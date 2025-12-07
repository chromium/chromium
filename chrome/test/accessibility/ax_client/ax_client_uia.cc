// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/ax_client/ax_client_uia.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/to_string.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#include <uiautomation.h>

using Microsoft::WRL::ComPtr;

namespace base::internal {

template <>
struct ToStringHelper<StructureChangeType> {
  static void Stringify(const StructureChangeType& type,
                        std::ostringstream& ss) {
    switch (type) {
      case StructureChangeType_ChildAdded:
        ss << "ChildAdded";
        break;
      case StructureChangeType_ChildRemoved:
        ss << "ChildRemoved";
        break;
      case StructureChangeType_ChildrenInvalidated:
        ss << "ChildrenInvalidated";
        break;
      case StructureChangeType_ChildrenBulkAdded:
        ss << "ChildrenBulkAdded";
        break;
      case StructureChangeType_ChildrenBulkRemoved:
        ss << "ChildrenBulkRemoved";
        break;
      case StructureChangeType_ChildrenReordered:
        ss << "ChildrenReordered";
        break;
      default:
        ss << "<UNKNOWN>";
        break;
    }
  }
};

}  // namespace base::internal

namespace ax_client {

namespace {

// Returns a copy of `array`.
base::HeapArray<int32_t> SafeArrayToIntArray(SAFEARRAY* array) {
  if (!array) {
    return {};
  }
  // Use ScopedSafearray for convenience; taking care to release it.
  base::win::ScopedSafearray safe_array(array);
  absl::Cleanup release_array = [&safe_array] { safe_array.Release(); };
  auto scope = safe_array.CreateLockScope<VT_I4>();
  CHECK(scope.has_value()) << "Unexpected VARTYPE in SAFEARRAY";
  return base::HeapArray<int32_t>::CopiedFrom(
      // SAFETY: The iterators are the range of ints held in the SAFEARRAY.
      UNSAFE_BUFFERS(base::span<int32_t>(scope->begin(), scope->end())));
}

// A UIA event handler that runs a callback for every event.
class StructureChangedEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIAutomationStructureChangedEventHandler> {
 public:
  using OnEvent =
      base::RepeatingCallback<void(ComPtr<IUIAutomationElement> sender,
                                   int change_type,
                                   base::HeapArray<int32_t> runtime_id)>;
  explicit StructureChangedEventHandler(OnEvent on_event)
      : on_event_(std::move(on_event)) {}

  // IUIAutomationStructureChangedEventHandler:
  IFACEMETHODIMP HandleStructureChangedEvent(IUIAutomationElement* sender,
                                             StructureChangeType change_type,
                                             SAFEARRAY* runtime_id) override;

 private:
  OnEvent on_event_;
};

HRESULT StructureChangedEventHandler::HandleStructureChangedEvent(
    IUIAutomationElement* sender,
    StructureChangeType change_type,
    SAFEARRAY* runtime_id) {
  on_event_.Run(ComPtr<IUIAutomationElement>(sender), change_type,
                SafeArrayToIntArray(runtime_id));
  return S_OK;
}

}  // namespace

AxClientUia::AxClientUia() = default;

AxClientUia::~AxClientUia() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

HRESULT AxClientUia::Initialize(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HRESULT hr = E_FAIL;

  // Connect to UI Automation.
  ComPtr<IUIAutomation> ui_automation;
  hr = ::CoCreateInstance(CLSID_CUIAutomation,
                          /*pUnkOuter=*/nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&ui_automation));
  if (FAILED(hr)) {
    return hr;
  }

  // Build a cache request.
  ComPtr<IUIAutomationCacheRequest> cache_request;
  hr = ui_automation->CreateCacheRequest(&cache_request);
  if (FAILED(hr)) {
    return hr;
  }
  cache_request->AddProperty(UIA_AutomationIdPropertyId);
  cache_request->AddProperty(UIA_ClassNamePropertyId);
  cache_request->AddProperty(UIA_NamePropertyId);
  cache_request->AddProperty(UIA_RuntimeIdPropertyId);
  cache_request->AddProperty(UIA_ValueValuePropertyId);

  // Find the desired window.
  ComPtr<IUIAutomationElement> main_window;
  hr = ui_automation->ElementFromHandleBuildCache(hwnd, cache_request.Get(),
                                                  &main_window);
  if (FAILED(hr)) {
    return hr;
  }
  auto main_window_props = GetProperties(main_window.Get());
  VLOG(1) << __func__ << " found browser window: " << main_window_props;

  // Register event handler(s).
  ComPtr<StructureChangedEventHandler> structure_changed_handler =
      Microsoft::WRL::Make<StructureChangedEventHandler>(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&AxClientUia::HandleStructureChangedEvent,
                              weak_ptr_factory_.GetWeakPtr())));
  hr = ui_automation->AddStructureChangedEventHandler(
      main_window.Get(), TreeScope_Subtree, cache_request.Get(),
      structure_changed_handler.Get());
  if (FAILED(hr)) {
    return hr;
  }

  // All operations succeeded. Save state in this instance.
  ui_automation_ = std::move(ui_automation);
  cache_request_ = std::move(cache_request);
  browser_window_ = std::move(main_window);
  browser_runtime_id_ = std::move(main_window_props.runtime_id);
  structure_changed_handler_ = std::move(structure_changed_handler);

  return S_OK;
}

HRESULT AxClientUia::FindAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HRESULT hr = E_FAIL;

  if (!ui_automation_) {
    return hr;
  }

  // Forget about all previously-discovered elements.
  elements_.clear();

  ComPtr<IUIAutomationCondition> condition;
  hr = ui_automation_->get_RawViewCondition(&condition);
  if (FAILED(hr)) {
    return hr;
  }

  ComPtr<IUIAutomationElementArray> element_array;
  hr = browser_window_->FindAllBuildCache(TreeScope_Subtree, condition.Get(),
                                          cache_request_.Get(), &element_array);
  if (FAILED(hr)) {
    LOG(ERROR) << __func__ << " " << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  int length;
  hr = element_array->get_Length(&length);
  if (FAILED(hr)) {
    LOG(ERROR) << __func__ << " " << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ComPtr<IUIAutomationElement> element;
  for (int i = 0; i < length; ++i) {
    hr = element_array->GetElement(i, &element);
    if (FAILED(hr)) {
      LOG(ERROR) << __func__ << " " << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    AddElement(std::move(element), __func__);
  }

  return S_OK;
}

void AxClientUia::Shutdown() {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Revoke all weak pointers so that any in-flight event callbacks are ignored.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (ui_automation_) {
    // Unregister all event handlers.
    HRESULT hr = ui_automation_->RemoveAllEventHandlers();
    LOG_IF(ERROR, FAILED(hr)) << "Error removing event handlers: "
                              << logging::SystemErrorCodeToString(hr);
  }

  // Release all references.
  elements_.clear();
  structure_changed_handler_.Reset();
  browser_window_.Reset();
  cache_request_.Reset();
  ui_automation_.Reset();
}

void AxClientUia::AddElement(ComPtr<IUIAutomationElement> element,
                             std::string_view context) {
  auto props = GetProperties(element.Get());
  if (elements_
          .try_emplace(
              base::HeapArray<int32_t>::CopiedFrom(props.runtime_id.as_span()),
              std::move(element))
          .second) {
    VLOG(1) << context << " added: " << props;
  } else {
    LOG(ERROR) << context << " ignored duplicate add for: " << props;
  }
}

void AxClientUia::RemoveElement(const base::HeapArray<int32_t>& runtime_id,
                                std::string_view context) {
  auto it = elements_.find(runtime_id);
  if (it != elements_.end()) {
    VLOG(1) << context << " removed: " << GetProperties(it->second.Get());
    elements_.erase(it);
  } else {
    LOG(ERROR) << context << " ignored removal of unknown element: "
               << base::ToString(runtime_id.as_span());
  }

  if (runtime_id.as_span() == browser_runtime_id_.as_span()) {
    VLOG(1) << context << " Releasing all elements for the main browser window";
    // The top-level browser window itself has been removed. Stop observing
    // changes beneath it and forget about it and all discovered elements.
    HRESULT hr = ui_automation_->RemoveStructureChangedEventHandler(
        browser_window_.Get(), structure_changed_handler_.Get());
    if (FAILED(hr)) {
      LOG(ERROR) << __func__ << " Failed to remove event handler "
                 << logging::SystemErrorCodeToString(hr);
    }
    structure_changed_handler_.Reset();
    browser_window_.Reset();
    browser_runtime_id_ = base::HeapArray<int32_t>();
    elements_.clear();
  }
}

void AxClientUia::HandleStructureChangedEvent(
    ComPtr<IUIAutomationElement> sender,
    int change_type_value,
    base::HeapArray<int32_t> runtime_id) {
  StructureChangeType change_type =
      static_cast<StructureChangeType>(change_type_value);
  switch (change_type) {
    case StructureChangeType_ChildAdded: {
      // `sender` is the element that was just added.
      AddElement(std::move(sender), __func__);
      break;
    }

    case StructureChangeType_ChildRemoved: {
      // `runtime_id` is the element that was just removed.
      RemoveElement(runtime_id, __func__);
      break;
    }

    case StructureChangeType_ChildrenInvalidated:
    case StructureChangeType_ChildrenBulkAdded:
    case StructureChangeType_ChildrenBulkRemoved:
    case StructureChangeType_ChildrenReordered:
      VLOG(1) << __func__ << " sender: " << GetProperties(sender.Get())
              << " change_type: " << base::ToString(change_type);
      break;

    default:
      NOTREACHED() << "Unexpected change type value " << change_type_value;
  }
}

// static
base::HeapArray<int32_t> AxClientUia::GetIntArrayProperty(
    IUIAutomationElement* element,
    int property_id) {
  base::win::ScopedVariant prop;
  if (SUCCEEDED(element->GetCachedPropertyValue(property_id, prop.Receive())) &&
      prop.type() == (VT_I4 | VT_ARRAY) && V_ARRAY(prop.ptr()) != nullptr) {
    return SafeArrayToIntArray(V_ARRAY(prop.ptr()));
  }
  return {};
}

// static
std::wstring AxClientUia::GetStringProperty(IUIAutomationElement* element,
                                            int property_id) {
  base::win::ScopedVariant prop;
  if (SUCCEEDED(element->GetCachedPropertyValue(property_id, prop.Receive())) &&
      prop.type() == VT_BSTR) {
    if (const BSTR str = V_BSTR(prop.ptr()); str) {
      return {str, ::SysStringLen(str)};
    }
  }
  return {};
}

AxClientUia::ElementProperties::ElementProperties(
    base::HeapArray<int32_t> the_runtime_id,
    std::wstring the_automation_id,
    std::wstring the_class_name,
    std::wstring the_name)
    : runtime_id(std::move(the_runtime_id)),
      automation_id(std::move(the_automation_id)),
      class_name(std::move(the_class_name)),
      name(std::move(the_name)) {}

AxClientUia::ElementProperties::~ElementProperties() = default;

// static
AxClientUia::ElementProperties AxClientUia::GetProperties(
    IUIAutomationElement* element) {
  return {
      /*runtime_id=*/GetIntArrayProperty(element, UIA_RuntimeIdPropertyId),
      /*automation_id=*/GetStringProperty(element, UIA_AutomationIdPropertyId),
      /*class_name=*/GetStringProperty(element, UIA_ClassNamePropertyId),
      /*name=*/GetStringProperty(element, UIA_NamePropertyId)};
}

}  // namespace ax_client

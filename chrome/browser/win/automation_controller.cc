// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/automation_controller.h"

#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/win/atl.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/win/ui_automation_util.h"
#include "ui/base/win/atl_module.h"

namespace {

// Configures a cache request so that it includes all properties used by the
// debug logging.
void ConfigureCacheRequestForLogging(IUIAutomationCacheRequest* cache_request) {
  DCHECK(cache_request);
#if DCHECK_IS_ON()
  cache_request->AddProperty(UIA_AutomationIdPropertyId);
  cache_request->AddProperty(UIA_ClassNamePropertyId);
  cache_request->AddProperty(UIA_ControlTypePropertyId);
  cache_request->AddProperty(UIA_IsPeripheralPropertyId);
  cache_request->AddProperty(UIA_NamePropertyId);
  cache_request->AddProperty(UIA_ProcessIdPropertyId);
  cache_request->AddProperty(UIA_RuntimeIdPropertyId);
  cache_request->AddProperty(UIA_ValueValuePropertyId);
#endif  // DCHECK_IS_ON()
}

// Safely keeps |delegate_| alive and available for the Automation context and
// the event handlers. This is safe because only its vtable is accessed on the
// various threads, which is const.
class RefCountedDelegate : public base::RefCounted<RefCountedDelegate> {
 public:
  explicit RefCountedDelegate(
      std::unique_ptr<AutomationController::Delegate> delegate);

  // These are forwarded to |delegate_|.
  void OnInitialized(HRESULT result);
  void ConfigureCacheRequest(IUIAutomationCacheRequest* cache_request);
  void OnAutomationEvent(IUIAutomation* automation,
                         IUIAutomationElement* sender,
                         EVENTID event_id);
  void OnFocusChangedEvent(IUIAutomation* automation,
                           IUIAutomationElement* sender);

 private:
  friend class base::RefCounted<RefCountedDelegate>;
  ~RefCountedDelegate();

  const std::unique_ptr<AutomationController::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedDelegate);
};

RefCountedDelegate::RefCountedDelegate(
    std::unique_ptr<AutomationController::Delegate> delegate)
    : delegate_(std::move(delegate)) {}

void RefCountedDelegate::OnInitialized(HRESULT result) {
  delegate_->OnInitialized(result);
}

void RefCountedDelegate::ConfigureCacheRequest(
    IUIAutomationCacheRequest* cache_request) {
  delegate_->ConfigureCacheRequest(cache_request);
}

void RefCountedDelegate::OnAutomationEvent(IUIAutomation* automation,
                                           IUIAutomationElement* sender,
                                           EVENTID event_id) {
  delegate_->OnAutomationEvent(automation, sender, event_id);
}

void RefCountedDelegate::OnFocusChangedEvent(IUIAutomation* automation,
                                             IUIAutomationElement* sender) {
  delegate_->OnFocusChangedEvent(automation, sender);
}

RefCountedDelegate::~RefCountedDelegate() = default;

}  // namespace

// This class lives in the automation sequence and is responsible for
// initializing the UIAutomation library and installing the event observers.
class AutomationController::Context {
 public:
  // Returns a new instance ready for initialization and use in another
  // sequence.
  static base::WeakPtr<Context> Create();

  // Deletes the instance.
  void DeleteInAutomationSequence();

  // Initializes the context, invoking the delegate's OnInitialized() method
  // when done. On success, the delegate's other On*() methods will be invoked
  // as events are observed. On failure, this instance self-destructs after
  // invoking OnInitialized().
  void Initialize(std::unique_ptr<Delegate> delegate);

 protected:
  class EventHandler;

  // The one and only method that may be called from outside of the automation
  // sequence.
  Context();
  ~Context();

  // Returns an event handler for all event types of interest.
  Microsoft::WRL::ComPtr<IUnknown> GetEventHandler();

  // Returns a pointer to the event handler's generic interface.
  Microsoft::WRL::ComPtr<IUIAutomationEventHandler> GetAutomationEventHandler();

  // Returns a pointer to the event handler's focus changed interface.
  Microsoft::WRL::ComPtr<IUIAutomationFocusChangedEventHandler>
  GetFocusChangedEventHandler();

  // Installs an event handler to observe events of interest.
  HRESULT InstallObservers();

  // Pointer to the delegate. Passed to event handlers.
  scoped_refptr<RefCountedDelegate> ref_counted_delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The automation client.
  Microsoft::WRL::ComPtr<IUIAutomation> automation_;

  // The event handler.
  Microsoft::WRL::ComPtr<IUnknown> event_handler_;

  // Weak pointers to the context are given to event handlers.
  base::WeakPtrFactory<Context> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Context);
};

class AutomationController::Context::EventHandler
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
      public IUIAutomationEventHandler,
      public IUIAutomationFocusChangedEventHandler {
 public:
  BEGIN_COM_MAP(AutomationController::Context::EventHandler)
  COM_INTERFACE_ENTRY(IUIAutomationEventHandler)
  COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
  END_COM_MAP()

  EventHandler();
  ~EventHandler();

  // Initializes the object. Events will be dispatched back to |context| via
  // |context_runner|.
  void Initialize(Microsoft::WRL::ComPtr<IUIAutomation> automation,
                  scoped_refptr<RefCountedDelegate> ref_counted_delegate);

  // IUIAutomationEventHandler:
  STDMETHOD(HandleAutomationEvent)
  (IUIAutomationElement* sender, EVENTID event_id) override;

  // IUIAutomationFocusChangedEventHandler:
  STDMETHOD(HandleFocusChangedEvent)(IUIAutomationElement* sender) override;

 private:
  Microsoft::WRL::ComPtr<IUIAutomation> automation_;

  // Pointer to the delegate.
  scoped_refptr<RefCountedDelegate> ref_counted_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EventHandler);
};

AutomationController::Context::EventHandler::EventHandler() = default;

AutomationController::Context::EventHandler::~EventHandler() = default;

void AutomationController::Context::EventHandler::Initialize(
    Microsoft::WRL::ComPtr<IUIAutomation> automation,
    scoped_refptr<RefCountedDelegate> ref_counted_delegate) {
  automation_ = automation;
  ref_counted_delegate_ = std::move(ref_counted_delegate);
}

HRESULT AutomationController::Context::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID event_id) {
  DVLOG(1)
      << "event id: " << GetEventName(event_id) << ", automation id: "
      << GetCachedBstrValue(sender, UIA_AutomationIdPropertyId)
      << ", name: " << GetCachedBstrValue(sender, UIA_NamePropertyId)
      << ", control type: "
      << GetControlType(GetCachedInt32Value(sender, UIA_ControlTypePropertyId))
      << ", is peripheral: "
      << GetCachedBoolValue(sender, UIA_IsPeripheralPropertyId)
      << ", class name: " << GetCachedBstrValue(sender, UIA_ClassNamePropertyId)
      << ", pid: " << GetCachedInt32Value(sender, UIA_ProcessIdPropertyId)
      << ", value: " << GetCachedBstrValue(sender, UIA_ValueValuePropertyId)
      << ", runtime id: "
      << IntArrayToString(
             GetCachedInt32ArrayValue(sender, UIA_RuntimeIdPropertyId));

  ref_counted_delegate_->OnAutomationEvent(automation_.Get(), sender, event_id);

  return S_OK;
}

HRESULT AutomationController::Context::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  DVLOG(1)
      << "focus changed for automation id: "
      << GetCachedBstrValue(sender, UIA_AutomationIdPropertyId)
      << ", name: " << GetCachedBstrValue(sender, UIA_NamePropertyId)
      << ", control type: "
      << GetControlType(GetCachedInt32Value(sender, UIA_ControlTypePropertyId))
      << ", is peripheral: "
      << GetCachedBoolValue(sender, UIA_IsPeripheralPropertyId)
      << ", class name: " << GetCachedBstrValue(sender, UIA_ClassNamePropertyId)
      << ", pid: " << GetCachedInt32Value(sender, UIA_ProcessIdPropertyId)
      << ", value: " << GetCachedBstrValue(sender, UIA_ValueValuePropertyId)
      << ", runtime id: "
      << IntArrayToString(
             GetCachedInt32ArrayValue(sender, UIA_RuntimeIdPropertyId));

  ref_counted_delegate_->OnFocusChangedEvent(automation_.Get(), sender);

  return S_OK;
}

// AutomationController::Context
// --------------------------------------------------

// static
base::WeakPtr<AutomationController::Context>
AutomationController::Context::Create() {
  Context* context = new Context();
  return context->weak_ptr_factory_.GetWeakPtr();
}

void AutomationController::Context::DeleteInAutomationSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete this;
}

void AutomationController::Context::Initialize(
    std::unique_ptr<Delegate> delegate) {
  // This and all other methods must be called in the automation sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ref_counted_delegate_ =
      base::MakeRefCounted<RefCountedDelegate>(std::move(delegate));

  HRESULT result =
      ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&automation_));
  if (SUCCEEDED(result))
    result = automation_ ? InstallObservers() : E_FAIL;

  // Now that the observers are installed, it's time to signal that the
  // initialization is done and that events will be received.
  ref_counted_delegate_->OnInitialized(result);

  // Self-destruct immediately if initialization failed to reduce overhead.
  if (FAILED(result))
    delete this;
}

AutomationController::Context::Context() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AutomationController::Context::~Context() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_handler_) {
    event_handler_.Reset();
    automation_->RemoveAllEventHandlers();
  }
}

Microsoft::WRL::ComPtr<IUnknown>
AutomationController::Context::GetEventHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event_handler_) {
    ATL::CComObject<EventHandler>* obj = nullptr;
    HRESULT result = ATL::CComObject<EventHandler>::CreateInstance(&obj);
    if (SUCCEEDED(result)) {
      obj->Initialize(automation_, ref_counted_delegate_);
      obj->QueryInterface(event_handler_.GetAddressOf());
    }
  }
  return event_handler_;
}

Microsoft::WRL::ComPtr<IUIAutomationEventHandler>
AutomationController::Context::GetAutomationEventHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Microsoft::WRL::ComPtr<IUIAutomationEventHandler> handler;
  GetEventHandler().CopyTo(handler.GetAddressOf());
  return handler;
}

Microsoft::WRL::ComPtr<IUIAutomationFocusChangedEventHandler>
AutomationController::Context::GetFocusChangedEventHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Microsoft::WRL::ComPtr<IUIAutomationFocusChangedEventHandler> handler;
  GetEventHandler().CopyTo(handler.GetAddressOf());
  return handler;
}

HRESULT AutomationController::Context::InstallObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(automation_);

  // Create a cache request so that elements received by way of events contain
  // all data needed for processing.
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  HRESULT result =
      automation_->CreateCacheRequest(cache_request.GetAddressOf());
  if (FAILED(result))
    return result;
  ConfigureCacheRequestForLogging(cache_request.Get());
  ref_counted_delegate_->ConfigureCacheRequest(cache_request.Get());

  // Observe changes in focus.
  result = automation_->AddFocusChangedEventHandler(
      cache_request.Get(), GetFocusChangedEventHandler().Get());
  if (FAILED(result))
    return result;

  // Observe invocations.
  Microsoft::WRL::ComPtr<IUIAutomationElement> desktop;
  result = automation_->GetRootElement(desktop.GetAddressOf());
  if (desktop) {
    result = automation_->AddAutomationEventHandler(
        UIA_Invoke_InvokedEventId, desktop.Get(), TreeScope_Subtree,
        cache_request.Get(), GetAutomationEventHandler().Get());
  }

  return result;
}

// AutomationController --------------------------------------------------------

AutomationController::AutomationController(std::unique_ptr<Delegate> delegate) {
  ui::win::CreateATLModuleIfNeeded();

  // Create the task runner on which the automation client lives.
  automation_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // Initialize the context on the automation task runner.
  context_ = Context::Create();
  automation_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AutomationController::Context::Initialize,
                                context_, std::move(delegate)));
}

AutomationController::~AutomationController() {
  // context_ is still valid when the caller destroys the instance before the
  // callback(s) have fired. In this case, delete the context in the automation
  // sequence before joining with it. DeleteSoon is not used because the monitor
  // has only a WeakPtr to the context that is bound to the automation sequence.
  automation_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AutomationController::Context::DeleteInAutomationSequence,
                     context_));
}

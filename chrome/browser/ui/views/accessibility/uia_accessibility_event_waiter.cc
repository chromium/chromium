// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/base/win/atl_module.h"

UiaAccessibilityEventWaiter::UiaAccessibilityEventWaiter(
    UiaAccessibilityWaiterInfo info) {
  // Create the event thread, and pump messages via |initialization_loop| until
  // initialization is complete.
  base::RunLoop initialization_loop;
  base::PlatformThread::Create(0, &thread_, &thread_handle_);
  thread_.Init(this, info, initialization_loop.QuitClosure(),
               shutdown_loop_.QuitClosure());
  initialization_loop.Run();
}

UiaAccessibilityEventWaiter::~UiaAccessibilityEventWaiter() {}

void UiaAccessibilityEventWaiter::Wait() {
  // Pump messages via |shutdown_loop_| until the thread is complete.
  shutdown_loop_.Run();
  base::PlatformThread::Join(thread_handle_);
}

void UiaAccessibilityEventWaiter::WaitWithTimeout(base::TimeDelta timeout) {
  // Pump messages via |shutdown_loop_| until the thread is complete.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, shutdown_loop_.QuitClosure(), timeout);
  shutdown_loop_.Run();
  base::PlatformThread::Join(thread_handle_);
}

UiaAccessibilityEventWaiter::Thread::Thread() {}

UiaAccessibilityEventWaiter::Thread::~Thread() {}

void UiaAccessibilityEventWaiter::Thread::SendShutdownSignal() {
  shutdown_signal_.Signal();
}

void UiaAccessibilityEventWaiter::Thread::Init(
    UiaAccessibilityEventWaiter* owner,
    const UiaAccessibilityWaiterInfo& info,
    base::OnceClosure initialization,
    base::OnceClosure shutdown) {
  owner_ = owner;
  info_ = info;
  initialization_complete_ = std::move(initialization);
  shutdown_complete_ = std::move(shutdown);
}

void UiaAccessibilityEventWaiter::Thread::ThreadMain() {
  // UIA calls must be made on an MTA thread to prevent random timeouts.
  base::win::ScopedCOMInitializer com_init{
      base::win::ScopedCOMInitializer::kMTA};

  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                   IID_PPV_ARGS(&uia_));
  CHECK(uia_.Get());

  // Find the IUIAutomationElement for the root content window.
  uia_->ElementFromHandle(info_.hwnd, &root_);
  CHECK(root_.Get());

  // Create the event handler.
  ui::win::CreateATLModuleIfNeeded();
  CHECK(
      SUCCEEDED(CComObject<EventHandler>::CreateInstance(&uia_event_handler_)));
  uia_event_handler_->AddRef();
  uia_event_handler_->Init(this, root_);

  // Create a cache request to avoid cross-thread issues when logging.
  CHECK(SUCCEEDED(uia_->CreateCacheRequest(&cache_request_)));
  CHECK(cache_request_.Get());
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_NamePropertyId)));
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_AriaRolePropertyId)));

  // Match AccEvent by using Raw View
  Microsoft::WRL::ComPtr<IUIAutomationCondition> pRawCond;
  CHECK(SUCCEEDED(uia_->get_RawViewCondition(&pRawCond)));
  CHECK(SUCCEEDED(cache_request_->put_TreeFilter(pRawCond.Get())));

  // Subscribe to focus events.
  uia_->AddFocusChangedEventHandler(cache_request_.Get(),
                                    uia_event_handler_.Get());

  // Subscribe to all property-change events.
  constexpr PROPERTYID kMinProp = UIA_RuntimeIdPropertyId;
  constexpr PROPERTYID kMaxProp = UIA_HeadingLevelPropertyId;
  std::array<PROPERTYID, (kMaxProp - kMinProp) + 1> property_list;
  std::iota(property_list.begin(), property_list.end(), kMinProp);
  uia_->AddPropertyChangedEventHandlerNativeArray(
      root_.Get(), TreeScope::TreeScope_Subtree, cache_request_.Get(),
      uia_event_handler_.Get(), &property_list[0], property_list.size());

  // Subscribe to all structure-change events.
  uia_->AddStructureChangedEventHandler(root_.Get(), TreeScope_Subtree,
                                        cache_request_.Get(),
                                        uia_event_handler_.Get());

  // Subscribe to all automation events (except structure-change events and
  // live-region events, which are handled elsewhere).
  constexpr EVENTID kMinEvent = UIA_ToolTipOpenedEventId;
  constexpr EVENTID kMaxEvent = UIA_NotificationEventId;
  for (EVENTID event_id = kMinEvent; event_id <= kMaxEvent; ++event_id) {
    if (event_id != UIA_StructureChangedEventId &&
        event_id != UIA_LiveRegionChangedEventId) {
      uia_->AddAutomationEventHandler(
          event_id, root_.Get(), TreeScope::TreeScope_Subtree,
          cache_request_.Get(), uia_event_handler_.Get());
    }
  }

  // Subscribe to live-region change events.  This must be the last event we
  // subscribe to, because |AXFragmentRootWin| will fire events when advised of
  // the subscription, and this can hang the test-process (on Windows 19H1+) if
  // we're simultaneously trying to subscribe to other events.
  uia_->AddAutomationEventHandler(
      UIA_LiveRegionChangedEventId, root_.Get(), TreeScope::TreeScope_Subtree,
      cache_request_.Get(), uia_event_handler_.Get());

  // Signal that initialization is complete; this will wake the main thread to
  // start executing the test code after this waiter has been constructed.
  std::move(initialization_complete_).Run();

  // Wait for shutdown signal.
  shutdown_signal_.Wait();

  // Cleanup
  uia_->RemoveAllEventHandlers();
  uia_event_handler_->CleanUp();
  uia_event_handler_.Reset();
  cache_request_.Reset();
  root_.Reset();
  uia_.Reset();

  std::move(shutdown_complete_).Run();
}

UiaAccessibilityEventWaiter::Thread::EventHandler::EventHandler() {}

UiaAccessibilityEventWaiter::Thread::EventHandler::~EventHandler() {}

void UiaAccessibilityEventWaiter::Thread::EventHandler::Init(
    UiaAccessibilityEventWaiter::Thread* owner,
    Microsoft::WRL::ComPtr<IUIAutomationElement> root) {
  owner_ = owner;
  root_ = root;
}

void UiaAccessibilityEventWaiter::Thread::EventHandler::CleanUp() {
  owner_ = nullptr;
  root_.Reset();
}

HRESULT
UiaAccessibilityEventWaiter::Thread::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  // Add focus changed event handling code here.
  return S_OK;
}

HRESULT
UiaAccessibilityEventWaiter::Thread::EventHandler::HandlePropertyChangedEvent(
    IUIAutomationElement* sender,
    PROPERTYID property_id,
    VARIANT new_value) {
  if (owner_ &&
      property_id ==
          ui::AXPlatformNodeWin::MojoEventToUIAProperty(owner_->info_.event) &&
      MatchesNameRole(sender)) {
    owner_->SendShutdownSignal();
  }
  return S_OK;
}

HRESULT
UiaAccessibilityEventWaiter::Thread::EventHandler::HandleStructureChangedEvent(
    IUIAutomationElement* sender,
    StructureChangeType change_type,
    SAFEARRAY* runtime_id) {
  // Add structure changed handling code here.
  return S_OK;
}

HRESULT
UiaAccessibilityEventWaiter::Thread::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID event_id) {
  if (owner_ &&
      event_id ==
          ui::AXPlatformNodeWin::MojoEventToUIAEvent(owner_->info_.event) &&
      MatchesNameRole(sender)) {
    owner_->SendShutdownSignal();
  }
  return S_OK;
}

bool UiaAccessibilityEventWaiter::Thread::EventHandler::MatchesNameRole(
    IUIAutomationElement* sender) {
  base::win::ScopedBstr aria_role;
  base::win::ScopedBstr name;
  sender->get_CachedAriaRole(aria_role.Receive());
  sender->get_CachedName(name.Receive());

  if (std::wstring(aria_role.Get(), SysStringLen(aria_role.Get())) ==
          owner_->info_.role &&
      std::wstring(name.Get(), SysStringLen(name.Get())) ==
          owner_->info_.name) {
    return true;
  }
  return false;
}

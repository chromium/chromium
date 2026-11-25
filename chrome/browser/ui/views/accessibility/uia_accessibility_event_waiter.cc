// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/timer/timer.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

UiaAccessibilityEventWaiter::UiaAccessibilityEventWaiter(
    UiaAccessibilityWaiterInfo info) {
  // Create the event thread, and pump messages via |initialization_loop| until
  // initialization is complete.
  base::RunLoop initialization_loop;
  thread_.Init(this, info, initialization_loop.QuitClosure(),
               event_loop_.QuitClosure(), shutdown_loop_.QuitClosure());
  base::PlatformThread::Create(0, &thread_, &thread_handle_);
  initialization_loop.Run();
}

UiaAccessibilityEventWaiter::~UiaAccessibilityEventWaiter() {
  ShutdownThread();
}

void UiaAccessibilityEventWaiter::Wait() {
  if (!wait_called_) {
    wait_called_ = true;
    // Pump messages via |event_loop_| until a matching UIA event is observed
    // (or the loop exits on timeout).
    event_loop_.Run();
  }
  ShutdownThread();
}

void UiaAccessibilityEventWaiter::WaitWithTimeout(base::TimeDelta timeout) {
  base::OneShotTimer timeout_timer;
  timeout_timer.Start(FROM_HERE, timeout, event_loop_.QuitClosure());
  Wait();
}

void UiaAccessibilityEventWaiter::ShutdownThread() {
  if (thread_joined_) {
    return;
  }
  // Disable RunLoop timeouts during cleanup so RemoveAllEventHandlers() can
  // finish marshaling back to this STA thread before Join(), even if
  // |event_loop_.Run()| exited early via ScopedRunLoopTimeout.
  base::test::ScopedDisableRunLoopTimeout disable_timeout;
  thread_.SendShutdownSignal();
  shutdown_loop_.Run();
  base::PlatformThread::Join(thread_handle_);
  thread_joined_ = true;
}

std::wstring UiaAccessibilityEventWaiter::GetNotificationString() const {
  base::AutoLock lock(notification_string_lock_);
  return notification_string_;
}

UiaAccessibilityEventWaiter::Thread::Thread() = default;

UiaAccessibilityEventWaiter::Thread::~Thread() = default;

void UiaAccessibilityEventWaiter::Thread::OnEventMatched() {
  event_matched_.Run();
}

void UiaAccessibilityEventWaiter::Thread::SendShutdownSignal() {
  shutdown_signal_.Signal();
}

void UiaAccessibilityEventWaiter::Thread::SetNotificationString(
    const std::wstring& str) {
  base::AutoLock lock(owner_->notification_string_lock_);
  owner_->notification_string_ = str;
}

void UiaAccessibilityEventWaiter::Thread::Init(
    UiaAccessibilityEventWaiter* owner,
    const UiaAccessibilityWaiterInfo& info,
    base::OnceClosure initialization,
    base::RepeatingClosure event_matched,
    base::OnceClosure shutdown) {
  owner_ = owner;
  info_ = info;
  initialization_complete_ = std::move(initialization);
  event_matched_ = std::move(event_matched);
  shutdown_complete_ = std::move(shutdown);
}

void UiaAccessibilityEventWaiter::Thread::ThreadMain() {
  // UIA calls must be made on an MTA thread to prevent random timeouts.
  base::win::ScopedCOMInitializer com_init{
      base::win::ScopedCOMInitializer::kMTA};

  // Use CLSID_CUIAutomation8 and query for IUIAutomation5 (notification
  // events). CLSID_CUIAutomation's object does not support IUIAutomation5,
  // without this the tests will not receive notification events.
  HRESULT hr = CoCreateInstance(CLSID_CUIAutomation8, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia_));
  CHECK(SUCCEEDED(hr)) << "CoCreateInstance failed: " << std::hex << hr;

  // Find the IUIAutomationElement for the root content window.
  uia_->ElementFromHandle(info_.hwnd, &root_);
  CHECK(root_.Get());

  // Create the event handler.
  uia_event_handler_ = Microsoft::WRL::Make<Thread::EventHandler>();
  CHECK(uia_event_handler_);
  uia_event_handler_->Init(this);

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
  constexpr EVENTID kMaxEvent = UIA_ActiveTextPositionChangedEventId;
  for (EVENTID event_id = kMinEvent; event_id <= kMaxEvent; ++event_id) {
    if (event_id != UIA_StructureChangedEventId &&
        event_id != UIA_LiveRegionChangedEventId &&
        event_id != UIA_NotificationEventId) {
      uia_->AddAutomationEventHandler(
          event_id, root_.Get(), TreeScope::TreeScope_Subtree,
          cache_request_.Get(), uia_event_handler_.Get());
    }
  }

  // Subscribe to notification events using the dedicated notification handler.
  // UIA_NotificationEventId requires IUIAutomationNotificationEventHandler,
  // not the generic IUIAutomationEventHandler.
  uia_->AddNotificationEventHandler(root_.Get(), TreeScope::TreeScope_Subtree,
                                    cache_request_.Get(),
                                    uia_event_handler_.Get());

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

UiaAccessibilityEventWaiter::Thread::EventHandler::EventHandler() = default;

UiaAccessibilityEventWaiter::Thread::EventHandler::~EventHandler() = default;

void UiaAccessibilityEventWaiter::Thread::EventHandler::Init(
    UiaAccessibilityEventWaiter::Thread* owner) {
  base::AutoLock lock(lock_);
  owner_ = owner;
}

void UiaAccessibilityEventWaiter::Thread::EventHandler::CleanUp() {
  base::AutoLock lock(lock_);
  owner_ = nullptr;
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
  std::wstring expected_role;
  std::wstring expected_name;
  {
    base::AutoLock lock(lock_);
    if (!owner_ || property_id != ui::AXPlatformNodeWin::MojoEventToUIAProperty(
                                      owner_->info_.event)) {
      return S_OK;
    }
    expected_role = owner_->info_.role;
    expected_name = owner_->info_.name;
  }

  if (!MatchesNameRole(sender, expected_role, expected_name)) {
    return S_OK;
  }

  base::AutoLock lock(lock_);
  if (owner_) {
    owner_->OnEventMatched();
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
  // Skip UIA_NotificationEventId since it's handled by
  // HandleNotificationEvent.
  if (event_id == UIA_NotificationEventId) {
    return S_OK;
  }

  std::wstring expected_role;
  std::wstring expected_name;
  {
    base::AutoLock lock(lock_);
    if (!owner_) {
      return S_OK;
    }
    // Check if we're waiting for a direct UIA event ID or an ax::mojom::Event.
    EVENTID expected_event_id = owner_->info_.uia_event_id;
    if (expected_event_id == 0) {
      // Fall back to mapping from ax::mojom::Event.
      auto mapped_event =
          ui::AXPlatformNodeWin::MojoEventToUIAEvent(owner_->info_.event);
      expected_event_id = mapped_event.value_or(0);
    }
    // Guard against matching event_id 0, which would indicate no valid event.
    if (expected_event_id == 0 || event_id != expected_event_id) {
      return S_OK;
    }
    expected_role = owner_->info_.role;
    expected_name = owner_->info_.name;
  }

  if (!MatchesNameRole(sender, expected_role, expected_name)) {
    return S_OK;
  }

  base::AutoLock lock(lock_);
  if (owner_) {
    owner_->OnEventMatched();
  }
  return S_OK;
}

HRESULT
UiaAccessibilityEventWaiter::Thread::EventHandler::HandleNotificationEvent(
    IUIAutomationElement* sender,
    NotificationKind notification_kind,
    NotificationProcessing notification_processing,
    BSTR display_string,
    BSTR activity_id) {
  base::AutoLock lock(lock_);
  if (owner_) {
    // Check if we're waiting for UIA_NotificationEventId.
    // For notification events, we don't check MatchesNameRole because the
    // cached properties may not be reliably available on the sender element.
    // The test should be structured to only trigger the expected notification.
    if (owner_->info_.uia_event_id == UIA_NotificationEventId) {
      if (display_string) {
        owner_->SetNotificationString(
            std::wstring(display_string, SysStringLen(display_string)));
      }
      owner_->OnEventMatched();
    }
  }
  return S_OK;
}

bool UiaAccessibilityEventWaiter::Thread::EventHandler::MatchesNameRole(
    IUIAutomationElement* sender,
    const std::wstring& expected_role,
    const std::wstring& expected_name) {
  if (!sender) {
    return false;
  }
  base::win::ScopedBstr aria_role;
  base::win::ScopedBstr name;
  sender->get_CachedAriaRole(aria_role.Receive());
  sender->get_CachedName(name.Receive());

  std::wstring aria_role_str(aria_role.Get() ? aria_role.Get() : L"",
                             aria_role.Length());
  std::wstring name_str(name.Get() ? name.Get() : L"", name.Length());
  return aria_role_str == expected_role && name_str == expected_name;
}

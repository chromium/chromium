// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/buildflags.h"

DEFINE_USER_DATA(BrowserActiveStateManager);

BrowserActiveStateManager::BrowserActiveStateManager(
    BrowserWindowInterface& browser,
    web_app::AppBrowserController* app_browser_controller)
    : browser_(browser),
      app_browser_controller_(app_browser_controller),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {}

BrowserActiveStateManager::~BrowserActiveStateManager() = default;

// static
BrowserActiveStateManager* BrowserActiveStateManager::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

// static
const BrowserActiveStateManager* BrowserActiveStateManager::From(
    const BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

bool BrowserActiveStateManager::IsActive() const {
// TODO(https://crbug.com/376306245): This is a temporary workaround for the
// fact that window_->IsActive() does not return the right result for macOS
// standalone PWA windows. This new behavior is still not technically correct,
// since it's checking that the last active window is `this`, as opposed to
// whether `this` is active.
#if BUILDFLAG(IS_MAC)
  // If this is a standalone PWA window, check BrowserList instead.
  if (app_browser_controller_) {
    return GetLastActiveBrowserWindowInterfaceWithAnyProfile() ==
           &browser_.get();
  }
#endif
  return is_active_;
}

void BrowserActiveStateManager::DidBecomeActive() {
  if (is_active_) {
    return;
  }

  is_active_ = true;

#if !BUILDFLAG(IS_CHROMEOS)
  // On platforms where BrowserView::Show() and BrowserView::Activate()
  // synchronously call DidBecomeActive() before the native window is
  // activated, another browser may still be marked active (either
  // speculatively from a prior Show()/Activate() or because the OS window
  // server has not yet dispatched its deactivation event). Ensure any other
  // active browser is marked inactive before notifying activation observers.
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this](BrowserWindowInterface* other) {
        if (other != &browser_.get()) {
          if (auto* other_manager = BrowserActiveStateManager::From(other)) {
            other_manager->DidBecomeInactive();
          }
        }
        return true;
      });
#endif

  did_become_active_callback_list_.Notify(&browser_.get());
  base::RecordAction(base::UserMetricsAction("ActiveBrowserChanged"));
}

void BrowserActiveStateManager::DidBecomeInactive() {
  if (is_active_) {
    is_active_ = false;
    did_become_inactive_callback_list_.Notify(&browser_.get());
  }
}

base::CallbackListSubscription
BrowserActiveStateManager::RegisterDidBecomeActive(
    BrowserWindowInterface::DidBecomeActiveCallback callback) {
  return did_become_active_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
BrowserActiveStateManager::RegisterDidBecomeInactive(
    BrowserWindowInterface::DidBecomeInactiveCallback callback) {
  return did_become_inactive_callback_list_.Add(std::move(callback));
}

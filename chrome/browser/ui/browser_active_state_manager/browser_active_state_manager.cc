// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DEFINE_USER_DATA(BrowserActiveStateManager);

BrowserActiveStateManager::BrowserActiveStateManager(
    BrowserWindowInterface& browser)
    : browser_(browser),
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
  return is_active_;
}

void BrowserActiveStateManager::DidBecomeActive() {
  if (!is_active_) {
    is_active_ = true;
    did_become_active_callback_list_.Notify(&browser_.get());
    base::RecordAction(base::UserMetricsAction("ActiveBrowserChanged"));
  }
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

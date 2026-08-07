// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTIVE_STATE_MANAGER_BROWSER_ACTIVE_STATE_MANAGER_H_
#define CHROME_BROWSER_UI_BROWSER_ACTIVE_STATE_MANAGER_BROWSER_ACTIVE_STATE_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace web_app {
class AppBrowserController;
}

// Feature controller responsible for managing the active state of a browser
// window and notifying registered observers when active state changes.
class BrowserActiveStateManager {
 public:
  DECLARE_USER_DATA(BrowserActiveStateManager);

  explicit BrowserActiveStateManager(
      BrowserWindowInterface& browser,
      web_app::AppBrowserController* app_browser_controller = nullptr);
  BrowserActiveStateManager(const BrowserActiveStateManager&) = delete;
  BrowserActiveStateManager& operator=(const BrowserActiveStateManager&) =
      delete;
  ~BrowserActiveStateManager();

  static BrowserActiveStateManager* From(BrowserWindowInterface* browser);
  static const BrowserActiveStateManager* From(
      const BrowserWindowInterface* browser);

  // Returns true if the browser window is active.
  bool IsActive() const;

  // Called when the browser window becomes active or inactive.
  void DidBecomeActive();
  void DidBecomeInactive();

  // Register callbacks for active/inactive state changes.
  base::CallbackListSubscription RegisterDidBecomeActive(
      BrowserWindowInterface::DidBecomeActiveCallback callback);
  base::CallbackListSubscription RegisterDidBecomeInactive(
      BrowserWindowInterface::DidBecomeInactiveCallback callback);

 private:
  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ptr<web_app::AppBrowserController> app_browser_controller_;

  // The active state of this browser window.
  bool is_active_ = false;

  using DidBecomeActiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeActiveCallbackList did_become_active_callback_list_;

  using DidBecomeInactiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeInactiveCallbackList did_become_inactive_callback_list_;

  ui::ScopedUnownedUserData<BrowserActiveStateManager>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTIVE_STATE_MANAGER_BROWSER_ACTIVE_STATE_MANAGER_H_

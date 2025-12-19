// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ui {
class BaseWindow;
}  // namespace ui

// Manages the initialization state of WebUI components that must be loaded
// before the browser window is shown.
class InitialWebUIManager {
 public:
  DECLARE_USER_DATA(InitialWebUIManager);

  explicit InitialWebUIManager(BrowserWindowInterface* browser);
  InitialWebUIManager(const InitialWebUIManager&) = delete;
  InitialWebUIManager& operator=(const InitialWebUIManager&) = delete;
  ~InitialWebUIManager();

  static InitialWebUIManager* From(
      BrowserWindowInterface* browser_window_interface);

  // Returns true if the browser window show should be deferred until
  // the web UI components are ready.
  // This is called when the `ui::BaseWindow` intends to invoke `Show()`, if the
  // show needs to be deferred, `is_show_pending_` will be set to true and
  // `Show()` will be called immediately when `is_initial_web_ui_pending_` is
  // changed to true from `OnReloadButtonLoaded()`.
  bool ShouldDeferShow();

  // Notifies that the reload button has finished loading. It will also call
  // `Show()` from the `window_` if `is_show_pending_` is true.
  void OnReloadButtonLoaded();

 private:
  // Shows the browser window if it was deferred.
  void MaybeShowBrowserWindow();

  const raw_ptr<ui::BaseWindow> window_;
  bool is_initial_web_ui_pending_;
  bool is_show_pending_ = false;

  ui::ScopedUnownedUserData<InitialWebUIManager> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

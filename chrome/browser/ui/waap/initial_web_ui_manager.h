// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

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
  bool ShouldDeferShow();

  // Notifies that the web UI toolbar has finished loading. It will also call
  // `Show()` from the `window_` if `is_show_pending_` is true.
  void OnWebUIToolbarLoaded();

  void SetWebUIReadyCallback(base::OnceClosure callback);

 private:
  // The callback is triggered when the initial WebUI is ready.
  base::OnceClosure web_ui_ready_callback_;
  bool is_initial_web_ui_pending_;

  ui::ScopedUnownedUserData<InitialWebUIManager> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

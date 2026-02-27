// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

#include "base/callback_list.h"
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

  // Requests to defer showing the browser window until the initial WebUI
  // finishes loading. Returns true if the deferral was successful and the state
  // was updated. In this case, `callback` will be invoked when the WebUI is
  // ready. Returns false if deferral is not applicable or not needed.
  // Note: the callback is unsafe since it will be added to the list and maybe
  // invoked after the caller is already destructed. The caller should ensure
  // the safety (e.g. by binding it to a weak pointer).
  bool RequestDeferShow(base::OnceClosure unsafe_callback);

  bool IsShowPending() const;

  // Notifies that the web UI toolbar has finished loading. It will also call
  // `Show()` from the `window_` if `is_show_pending_` is true.
  void OnWebUIToolbarLoaded();

 private:
  // These callbacks are triggered when the initial WebUI is ready.
  base::OnceClosureList web_ui_ready_callbacks_;

  // Tracks the physical loading state of the Initial WebUI.
  // True when the browser launches and the WebUI begins loading in the
  // background. Becomes strictly false when the WebUI finishes loading.
  // Note that a window can be created without intending to show it immediately,
  // e.g., a background window, so this flag alone cannot determine if a
  // window's visibility was artificially deferred.
  bool is_initial_web_ui_pending_;

  // Tracks the "Intent to Show" of the Browser Window.
  // True only if the browser explicitly attempted to show the window, but the
  // action was actively intercepted and blocked because
  // `is_initial_web_ui_pending_` was true.
  //
  // Why this is needed: We only want features like the metrics profiler to
  // attach to a window that *would* have been visible if we didn't pause it.
  bool is_show_pending_ = false;

  ui::ScopedUnownedUserData<InitialWebUIManager> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEB_UI_MANAGER_H_

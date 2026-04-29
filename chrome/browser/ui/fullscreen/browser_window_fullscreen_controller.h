// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_BROWSER_WINDOW_FULLSCREEN_CONTROLLER_H_
#define CHROME_BROWSER_UI_FULLSCREEN_BROWSER_WINDOW_FULLSCREEN_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Manages fullscreen state and UI policy for its associated Browser window.
class BrowserWindowFullscreenController {
 public:
  DECLARE_USER_DATA(BrowserWindowFullscreenController);

  explicit BrowserWindowFullscreenController(BrowserWindowInterface& browser);
  ~BrowserWindowFullscreenController();

  BrowserWindowFullscreenController(const BrowserWindowFullscreenController&) =
      delete;
  BrowserWindowFullscreenController& operator=(
      const BrowserWindowFullscreenController&) = delete;

  static BrowserWindowFullscreenController* From(
      BrowserWindowInterface* browser);
  static const BrowserWindowFullscreenController* From(
      const BrowserWindowInterface* browser);

  // Returns true if the UI should be hidden for fullscreen.
  bool ShouldHideUIForFullscreen() const;

  // True when we do not want to allow exiting fullscreen.
  bool IsForceFullscreen() const;

  void SetForceFullscreen(bool force_fullscreen);

  void set_should_hide_ui_for_fullscreen_for_testing(bool should_hide_ui) {
    should_hide_ui_for_fullscreen_for_testing_ = should_hide_ui;
  }

 private:
  const raw_ref<BrowserWindowInterface> browser_;
  bool force_fullscreen_ = false;

  std::optional<bool> should_hide_ui_for_fullscreen_for_testing_;

  ui::ScopedUnownedUserData<BrowserWindowFullscreenController>
      scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_BROWSER_WINDOW_FULLSCREEN_CONTROLLER_H_

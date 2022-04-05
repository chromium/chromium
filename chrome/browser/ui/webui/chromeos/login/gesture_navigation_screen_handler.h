// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class GestureNavigationScreen;
}

namespace chromeos {

// Interface between gesture navigation screen and its representation.
class GestureNavigationScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"gesture-navigation"};

  virtual ~GestureNavigationScreenView() {}

  virtual void Bind(ash::GestureNavigationScreen* screen) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
};

// WebUI implementation of GestureNavigationScreenView.
class GestureNavigationScreenHandler : public GestureNavigationScreenView,
                                       public BaseScreenHandler {
 public:
  using TView = GestureNavigationScreenView;

  GestureNavigationScreenHandler();
  ~GestureNavigationScreenHandler() override;

  GestureNavigationScreenHandler(const GestureNavigationScreenHandler&) =
      delete;
  GestureNavigationScreenHandler operator=(
      const GestureNavigationScreenHandler&) = delete;

  // GestureNavigationScreenView:
  void Bind(ash::GestureNavigationScreen* screen) override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;
  void RegisterMessages() override;

 private:
  // Called when the currently shown page for the gesture navigation screen is
  // changed.
  void HandleGesturePageChange(const std::string& new_page);

  ash::GestureNavigationScreen* screen_ = nullptr;

  // If true, InitializeDeprecated() will call Show().
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::GestureNavigationScreenHandler;
using ::chromeos::GestureNavigationScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GESTURE_NAVIGATION_SCREEN_HANDLER_H_

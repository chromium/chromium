// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_THEME_SELECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_THEME_SELECTION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class ThemeSelectionScreen;

// Interface between ThemeSelection screen and its representation,
// either WebUI or Views one.
class ThemeSelectionScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"theme-selection",
                                                "ThemeSelectionScreen"};

  inline constexpr static char kAutoMode[] = "auto";
  inline constexpr static char kDarkMode[] = "dark";
  inline constexpr static char kLightMode[] = "light";

  virtual ~ThemeSelectionScreenView() = default;

  virtual void Show(base::Value::Dict data) = 0;
  virtual base::WeakPtr<ThemeSelectionScreenView> AsWeakPtr() = 0;
};

class ThemeSelectionScreenHandler final : public ThemeSelectionScreenView,
                                          public BaseScreenHandler {
 public:
  using TView = ThemeSelectionScreenView;

  ThemeSelectionScreenHandler();

  ThemeSelectionScreenHandler(const ThemeSelectionScreenHandler&) = delete;
  ThemeSelectionScreenHandler& operator=(const ThemeSelectionScreenHandler&) =
      delete;

  ~ThemeSelectionScreenHandler() override;

  // ThemeSelectionScreenView implementation
  void Show(base::Value::Dict data) override;
  base::WeakPtr<ThemeSelectionScreenView> AsWeakPtr() override;

  // BaseScreenHandler implementation
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<ThemeSelectionScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_THEME_SELECTION_SCREEN_HANDLER_H_

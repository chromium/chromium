// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/theme_selection_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

namespace {
constexpr char kSelectedTheme[] = "selectedTheme";
}

constexpr StaticOobeScreenId ThemeSelectionScreenView::kScreenId;

ThemeSelectionScreenHandler::ThemeSelectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ThemeSelectionScreenHandler::~ThemeSelectionScreenHandler() = default;

void ThemeSelectionScreenHandler::Show(const std::string& mode) {
  base::Value::Dict data;
  data.Set(kSelectedTheme, mode);
  ShowInWebUI(std::move(data));
}

void ThemeSelectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("themeSelectionScreenTitle", IDS_THEME_SELECTION_TITLE);
  builder->Add("themeSelectionScreenDescriptionClamshell",
               IDS_THEME_SELECTION_DESCRIPTION_CLAMSHELL);
  builder->Add("themeSelectionScreenDescriptionTablet",
               IDS_THEME_SELECTION_DESCRIPTION_TABLET);
  builder->Add("lightThemeLabel", IDS_THEME_LIGHT_LABEL);
  builder->Add("lightThemeDescription", IDS_THEME_LIGHT_DESCRIPTION);
  builder->Add("darkThemeLabel", IDS_THEME_DARK_LABEL);
  builder->Add("darkThemeDescription", IDS_THEME_DARK_DESCRIPTION);
  builder->Add("autoThemeLabel", IDS_THEME_AUTO_LABEL);
  builder->Add("autoThemeDescription", IDS_THEME_AUTO_DESCRIPTION);
}

}  // namespace ash

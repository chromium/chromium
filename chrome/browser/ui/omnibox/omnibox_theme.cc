// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_theme.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

using TP = ThemeProperties;

namespace {

int GetThemePropertyId(OmniboxPart part, OmniboxPartState state) {
  const bool selected = state == OmniboxPartState::SELECTED;
  switch (part) {
    case OmniboxPart::LOCATION_BAR_BACKGROUND:
      return state == OmniboxPartState::HOVERED
                 ? TP::COLOR_OMNIBOX_BACKGROUND_HOVERED
                 : TP::COLOR_OMNIBOX_BACKGROUND;
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
      return TP::COLOR_OMNIBOX_SELECTED_KEYWORD;
    case OmniboxPart::RESULTS_BACKGROUND:
      switch (state) {
        case OmniboxPartState::NORMAL:
          return TP::COLOR_OMNIBOX_RESULTS_BG;
        case OmniboxPartState::HOVERED:
          return TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED;
        case OmniboxPartState::SELECTED:
          return TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED;
        default:
          NOTREACHED();
          return TP::COLOR_OMNIBOX_RESULTS_BG;
      }
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
      return TP::COLOR_OMNIBOX_TEXT;
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
      return selected ? TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED
                      : TP::COLOR_OMNIBOX_TEXT;
    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
      return TP::COLOR_OMNIBOX_TEXT_DIMMED;
    case OmniboxPart::RESULTS_TEXT_DIMMED:
      return selected ? TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED
                      : TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED;
    case OmniboxPart::RESULTS_ICON:
      return selected ? TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED
                      : TP::COLOR_OMNIBOX_RESULTS_ICON;
    case OmniboxPart::RESULTS_TEXT_URL:
      return selected ? TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED
                      : TP::COLOR_OMNIBOX_RESULTS_URL;
    case OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE:
      return OmniboxFieldTrial::IsExperimentalKeywordModeEnabled()
                 ? TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE
                 : TP::COLOR_OMNIBOX_BUBBLE_OUTLINE;
    default:
      NOTREACHED();
      return -1;
  }
}

}  // namespace

SkColor GetOmniboxColor(const ui::ThemeProvider* theme_provider,
                        OmniboxPart part,
                        OmniboxPartState state) {
  return theme_provider->GetColor(GetThemePropertyId(part, state));
}

SkColor GetOmniboxSecurityChipColor(
    const ui::ThemeProvider* theme_provider,
    security_state::SecurityLevel security_level) {
  if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT) {
    return GetOmniboxColor(theme_provider,
                           OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  }

  if (security_level == security_state::EV_SECURE ||
      security_level == security_state::SECURE) {
    return theme_provider->GetColor(TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE);
  }
  if (security_level == security_state::DANGEROUS)
    return theme_provider->GetColor(TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS);
  return theme_provider->GetColor(TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT);
}

float GetOmniboxStateOpacity(OmniboxPartState state) {
  constexpr float kOpacities[3] = {0.00f, 0.10f, 0.16f};
  return kOpacities[static_cast<size_t>(state)];
}

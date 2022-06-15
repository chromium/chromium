// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_theme.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"

using TP = ThemeProperties;

namespace {

ui::ColorId GetColorId(OmniboxPart part, OmniboxPartState state) {
  const bool selected = state == OmniboxPartState::SELECTED;
  switch (part) {
    case OmniboxPart::LOCATION_BAR_BACKGROUND:
      return state == OmniboxPartState::HOVERED ? kColorOmniboxBackgroundHovered
                                                : kColorOmniboxBackground;
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
      return kColorOmniboxKeywordSelected;
    case OmniboxPart::RESULTS_BACKGROUND:
      switch (state) {
        case OmniboxPartState::NORMAL:
          return kColorOmniboxResultsBackground;
        case OmniboxPartState::HOVERED:
          return kColorOmniboxResultsBackgroundHovered;
        case OmniboxPartState::SELECTED:
          return kColorOmniboxResultsBackgroundSelected;
        default:
          NOTREACHED();
          return kColorOmniboxResultsBackground;
      }
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
      return kColorOmniboxText;
    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
      return kColorOmniboxTextDimmed;
    case OmniboxPart::RESULTS_ICON:
      return selected ? kColorOmniboxResultsIconSelected
                      : kColorOmniboxResultsIcon;
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
      return selected ? kColorOmniboxResultsTextSelected : kColorOmniboxText;
    case OmniboxPart::RESULTS_TEXT_DIMMED:
      return selected ? kColorOmniboxResultsTextDimmedSelected
                      : kColorOmniboxResultsTextDimmed;
    case OmniboxPart::RESULTS_TEXT_NEGATIVE:
      return selected ? kColorOmniboxResultsTextNegativeSelected
                      : kColorOmniboxResultsTextNegative;
    case OmniboxPart::RESULTS_TEXT_POSITIVE:
      return selected ? kColorOmniboxResultsTextPositiveSelected
                      : kColorOmniboxResultsTextPositive;
    case OmniboxPart::RESULTS_TEXT_SECONDARY:
      return selected ? kColorOmniboxResultsTextSecondarySelected
                      : kColorOmniboxResultsTextSecondary;
    case OmniboxPart::RESULTS_TEXT_URL:
      return selected ? kColorOmniboxResultsUrlSelected
                      : kColorOmniboxResultsUrl;
    case OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE:
      return OmniboxFieldTrial::IsExperimentalKeywordModeEnabled()
                 ? kColorOmniboxBubbleOutlineExperimentalKeywordMode
                 : kColorOmniboxBubbleOutline;
    case OmniboxPart::RESULTS_BUTTON_BORDER:
      return kColorOmniboxResultsButtonBorder;
    case OmniboxPart::RESULTS_BUTTON_INK_DROP:
      return selected ? kColorOmniboxResultsButtonInkDropSelected
                      : kColorOmniboxResultsButtonInkDrop;
    default:
      NOTREACHED();
      return kColorOmniboxBackground;
  }
}

}  // namespace

SkColor GetOmniboxColor(const ui::ColorProvider* color_provider,
                        OmniboxPart part,
                        OmniboxPartState state) {
  return color_provider->GetColor(GetColorId(part, state));
}

SkColor GetOmniboxSecurityChipColor(
    const ui::ColorProvider* color_provider,
    security_state::SecurityLevel security_level) {
  if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT) {
    return GetOmniboxColor(color_provider,
                           OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  }

  if (security_level == security_state::SECURE) {
    return color_provider->GetColor(kColorOmniboxSecurityChipSecure);
  }
  if (security_level == security_state::DANGEROUS)
    return color_provider->GetColor(kColorOmniboxSecurityChipDangerous);
  return color_provider->GetColor(kColorOmniboxSecurityChipDefault);
}

float GetOmniboxStateOpacity(OmniboxPartState state) {
  constexpr float kOpacities[3] = {0.00f, 0.10f, 0.16f};
  return kOpacities[static_cast<size_t>(state)];
}

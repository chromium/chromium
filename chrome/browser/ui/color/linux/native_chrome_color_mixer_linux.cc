// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"
#include "components/search/ntp_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace {

ui::ColorTransform UseIfNonzeroAlpha(ui::ColorTransform transform) {
  const auto generator = [](ui::ColorTransform transform, SkColor input_color,
                            const ui::ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        SkColorGetA(transform_color) ? transform_color : input_color;
    DVLOG(2) << "ColorTransform UseIfNonzeroAlpha:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

// Candidate colors for active / inacitve toolbar top separator.
// Toolbar top separator is the separator between the toolbar and the tab strip.
// In GTK theme, it is computed from three sources in order of preference to
// meet the minimum contrast raio requirement:
//   a. kColorNativeHeaderSeparatorBorder
//   b. kColorNativeHeaderButtonBorder
//   c. fallback to the upstream mixer (i.e. chrome_color_mixer)
// Active and inactive colors are in a lock step. For example, if the active
// style chooses a. and the inactive chooses b., then both styles should choose
// b.
struct ToolbarTopSeparatorColorCandidates {
  enum Choice {
    kFirstChoice = 0,
    kSecondChoice,
    kFallbackChoice,
    kChoicesCount
  } choice;
  // Store all candidate colors since the active/inactive counterpart's `choice`
  // might be different than this `choice`.
  SkColor colors[kChoicesCount];
};

bool HasGoodConstrastForToolbarTopSeparator(SkColor forground,
                                            SkColor background) {
  const float kMinContrastRatio = 2.f;
  forground = color_utils::GetResultingPaintColor(forground, background);
  return color_utils::GetContrastRatio(background, forground) >=
         kMinContrastRatio;
}

ToolbarTopSeparatorColorCandidates GetToolbarTopSeparatorColorCandidates(
    ui::ColorTransform first_choice_transform,
    ui::ColorTransform second_choice_transform,
    ui::ColorTransform background_transform,
    SkColor input_color,
    const ui::ColorMixer& mixer) {
  SkColor first_choice_color = first_choice_transform.Run(input_color, mixer);
  SkColor second_choice_color = second_choice_transform.Run(input_color, mixer);
  SkColor background_color = background_transform.Run(input_color, mixer);
  auto choice = ToolbarTopSeparatorColorCandidates::kFallbackChoice;
  if (HasGoodConstrastForToolbarTopSeparator(first_choice_color,
                                             background_color))
    choice = ToolbarTopSeparatorColorCandidates::kFirstChoice;
  else if (HasGoodConstrastForToolbarTopSeparator(second_choice_color,
                                                  background_color))
    choice = ToolbarTopSeparatorColorCandidates::kSecondChoice;
  return {choice, {first_choice_color, second_choice_color, input_color}};
}

// kColorToolbarSeparatorFrame[Active/Inactive] selects the first color
// that satisfies the contrast ratio requirement from candidate colors.
// This logic is migrated from GtkUi::UpdateColors().
// TODO(crbug.com/1304441): re-examine the correctness in redirection tests.
ui::ColorTransform GetGtkToolbarTopSeparatorColorTransform(
    bool return_active_color) {
  using CandidatesGetter =
      base::RepeatingCallback<ToolbarTopSeparatorColorCandidates(
          SkColor input, const ui::ColorMixer& mixer)>;
  auto active_getter = base::BindRepeating(
      GetToolbarTopSeparatorColorCandidates,
      ui::kColorNativeHeaderSeparatorBorderActive,
      ui::kColorNativeHeaderButtonBorderActive, ui::kColorFrameActive);
  auto inactive_getter = base::BindRepeating(
      GetToolbarTopSeparatorColorCandidates,
      ui::kColorNativeHeaderSeparatorBorderInactive,
      ui::kColorNativeHeaderButtonBorderInactive, ui::kColorFrameInactive);
  const auto generator = [](bool return_active_color,
                            CandidatesGetter active_getter,
                            CandidatesGetter inactive_getter,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    ToolbarTopSeparatorColorCandidates active_candidates =
        active_getter.Run(input_color, mixer);
    ToolbarTopSeparatorColorCandidates inactive_candidates =
        inactive_getter.Run(input_color, mixer);
    // Active/inactive choices are in lockstep. They choose the most favored
    // candidate that satisfies the contrast ratio requirement.
    auto choice =
        std::max(active_candidates.choice, inactive_candidates.choice);
    return return_active_color ? active_candidates.colors[choice]
                               : inactive_candidates.colors[choice];
  };

  return base::BindRepeating(generator, return_active_color, active_getter,
                             inactive_getter);
}

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarForeground] = {kColorToolbarTextDefault};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] =
      UseIfNonzeroAlpha(ui::kColorNativeTextfieldBorderUnfocused);
  mixer[kColorNewTabPageBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorNewTabPageHeader] = {ui::kColorNativeButtonBorder};
  mixer[kColorNewTabPageLink] = {ui::kColorTextfieldSelectionBackground};
  mixer[kColorNewTabPageText] = {ui::kColorTextfieldForeground};
  mixer[kColorOmniboxText] = {ui::kColorTextfieldForeground};
  mixer[kColorToolbarBackgroundSubtleEmphasis] = {
      ui::kColorTextfieldBackground};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      ui::kColorNativeTabForegroundInactiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      ui::kColorNativeTabForegroundInactiveFrameInactive};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorToolbar] = {ui::kColorNativeToolbarBackground};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarSeparator};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeButtonBorder};
  mixer[kColorToolbarText] = {ui::kColorNativeLabelForeground};
  mixer[kColorToolbarTextDisabled] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetGtkToolbarTopSeparatorColorTransform(true);
  mixer[kColorToolbarTopSeparatorFrameInactive] =
      GetGtkToolbarTopSeparatorColorTransform(false);

  // Explicitly override certain colors for the NTP to those corresponding to
  // the light theme. See crbug.com/998903. This logic will be removed once the
  // NewTabPage comprehensive theming experiment has completed.
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpComprehensiveTheming))
    AddWebThemeNewTabPageColors(mixer, false);
}

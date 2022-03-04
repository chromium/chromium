// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "base/logging.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

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

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::ColorProviderManager::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] =
      UseIfNonzeroAlpha(ui::kColorNativeTextfieldBorderUnfocused);
  mixer[kColorNewTabPageBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorNewTabPageHeader] = {ui::kColorNativeButtonBorder};
  mixer[kColorNewTabPageText] = {ui::kColorTextfieldForeground};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarSeparator};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeButtonBorder};
  mixer[kColorToolbarText] = {ui::kColorNativeLabelForeground};
}

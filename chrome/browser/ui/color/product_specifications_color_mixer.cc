// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/product_specifications_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddProductSpecificationsColorMixer(ui::ColorProvider* provider,
                                        const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorProductSpecificationsButtonBackground] = {ui::kColorSysSurface2};
  mixer[kColorProductSpecificationsTonalButtonBackground] = {
      ui::kColorSysTonalContainer};
  mixer[kColorProductSpecificationsContentBackground] = {
      ui::kColorSysBaseContainer};
  mixer[kColorProductSpecificationsPageBackground] = {ui::kColorSysSurface2};
  mixer[kColorProductSpecificationsSummaryBackground] = {ui::kColorSysSurface};
  mixer[kColorProductSpecificationsPrimaryTitle] = {ui::kColorSysOnSurface};
  mixer[kColorProductSpecificationsSecondaryTitle] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorProductSpecificationsDivider] = {ui::kColorSysDivider};
}

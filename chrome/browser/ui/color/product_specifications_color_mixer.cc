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
  mixer[kColorProductSpecificationsCitationBackground] = {
      ui::kColorSysBaseContainer};
  mixer[kColorProductSpecificationsDetailChipBackground] = {
      ui::kColorSysBaseContainer};
  mixer[kColorProductSpecificationsDisclosureBackground] = {
      ui::kColorPrimaryBackground};
  mixer[kColorProductSpecificationsDisclosureForeground] = {
      ui::kColorPrimaryForeground};
  mixer[kColorProductSpecificationsDisclosureGradientEnd] = {
      ui::kColorSysGradientPrimary};
  mixer[kColorProductSpecificationsDisclosureGradientStart] = {
      ui::kColorSysGradientTertiary};
  mixer[kColorProductSpecificationsDisclosureSummaryBackground] = {
      ui::kColorSysSurface4};
  mixer[kColorProductSpecificationsDivider] = {ui::kColorSysDivider};
  mixer[kColorProductSpecificationsGradientIcon] = {
      ui::kColorSysOnSurfacePrimary};
  mixer[kColorProductSpecificationsHorizontalCarouselScrollbarThumb] = {
      ui::kColorSysTonalOutline};
  mixer[kColorProductSpecificationsIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorProductSpecificationsIconButtonBackground] = {
      ui::kColorSysSurface};
  mixer[kColorProductSpecificationsIconButtonHoveredBackground] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProductSpecificationsLink] = {ui::kColorSysPrimary};
  mixer[kColorProductSpecificationsPageBackground] = {ui::kColorSysSurface2};
  mixer[kColorProductSpecificationsPrimaryTitle] = {
      ui::kColorPrimaryForeground};
  mixer[kColorProductSpecificationsSecondaryTitle] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorProductSpecificationsSummaryBackground] = {
      ui::kColorPrimaryBackground};
  mixer[kColorProductSpecificationsSummaryBackgroundDragging] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProductSpecificationsTonalButtonBackground] = {
      ui::kColorSysTonalContainer};
}

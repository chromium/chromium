// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/icon_constants.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"

namespace app_list {

namespace {

constexpr SkColor kOmniboxGenericIconColor = gfx::kGoogleGrey700;

}  // namespace

int GetAnswerCardIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 28 : 24;
}

int GetAppIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 32 : 20;
}

int GetImageIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 28 : 32;
}

SkColor GetGenericIconColor() {
  if (ash::features::IsProductivityLauncherEnabled()) {
    return ash::ColorProvider::Get()->GetContentLayerColor(
        ash::ColorProvider::ContentLayerType::kButtonIconColor);
  }
  return kOmniboxGenericIconColor;
}

}  // namespace app_list

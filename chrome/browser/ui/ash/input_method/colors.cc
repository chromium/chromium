// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/colors.h"

#include "ash/style/dark_light_mode_controller_impl.h"

namespace ui {
namespace ime {

bool IsDarkModeEnabled() {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  if (!dark_light_mode_controller) {
    return false;
  }
  return dark_light_mode_controller->IsDarkModeEnabled();
}

SkColor ResolveSemanticColor(const cros_styles::ColorName& color_name) {
  return cros_styles::ResolveColor(color_name, IsDarkModeEnabled(), false);
}

}  // namespace ime
}  // namespace ui

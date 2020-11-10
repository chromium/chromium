// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"

#include "ui/base/resource/resource_bundle.h"

namespace web_app {

std::map<SquareSizePx, SkBitmap> LoadBundledIcons(
    const std::initializer_list<int>& icon_resource_ids) {
  std::map<SquareSizePx, SkBitmap> results;
  for (int id : icon_resource_ids) {
    const gfx::Image& image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(id);
    DCHECK_EQ(image.Width(), image.Height());
    results[image.Width()] = image.AsBitmap();
  }
  return results;
}

}  // namespace web_app

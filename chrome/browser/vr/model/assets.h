// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_ASSETS_H_
#define CHROME_BROWSER_VR_MODEL_ASSETS_H_

#include <memory>

#include "base/version.h"
#include "chrome/browser/vr/vr_base_export.h"

class SkBitmap;

namespace vr {

struct VR_BASE_EXPORT Assets {
  Assets();
  ~Assets();

  // This is required. This image (and the gradients below) are destroyed after
  // they are uploaded to the GPU. So while this image is never initially null
  // after a successful load, after the image has been consumed, it will be.
  std::unique_ptr<SkBitmap> background;

  // For backwards compatibility, we may not have these gradients in the
  // component. Receiving code must be able to cope with these gradient images
  // not existing.
  std::unique_ptr<SkBitmap> normal_gradient;
  std::unique_ptr<SkBitmap> incognito_gradient;
  std::unique_ptr<SkBitmap> fullscreen_gradient;

  std::unique_ptr<std::string> button_hover_sound;
  std::unique_ptr<std::string> button_click_sound;
  std::unique_ptr<std::string> back_button_click_sound;
  std::unique_ptr<std::string> inactive_button_click_sound;

  base::Version version;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_ASSETS_H_

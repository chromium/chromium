// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller_glue.h"

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

namespace lens {

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensOverlayControllerGlue);

LensOverlayControllerGlue::LensOverlayControllerGlue(
    content::WebContents* contents,
    LensOverlayController* controller)
    : content::WebContentsUserData<LensOverlayControllerGlue>(*contents),
      controller_(controller) {}

}  // namespace lens

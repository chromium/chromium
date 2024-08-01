// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_GLUE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_GLUE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace lens {

// When a WebUIController for lens overlay and search bubble is created, we
// need a mechanism to glue that instance to the LensOverlayController that
// spawned it. This class is that glue. The lifetime of this instance is scoped
// to the lifetime of the LensOverlayController, which semantically "owns" this
// instance.
class LensOverlayControllerGlue
    : public content::WebContentsUserData<LensOverlayControllerGlue> {
 public:
  ~LensOverlayControllerGlue() override = default;

  LensOverlayController* controller() { return controller_; }

 private:
  friend class content::WebContentsUserData<LensOverlayControllerGlue>;

  LensOverlayControllerGlue(content::WebContents* contents,
                            LensOverlayController* controller);

  // Semantically owns this class.
  raw_ptr<LensOverlayController> controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_GLUE_H_

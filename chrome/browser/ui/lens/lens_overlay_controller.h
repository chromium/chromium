// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

namespace tabs {
class TabModel;
}  // namespace tabs

// Manages all state associated with the lens overlay.
class LensOverlayController {
 public:
  explicit LensOverlayController(tabs::TabModel* tab_model);
  ~LensOverlayController();

 private:
  // Owns this class.
  raw_ptr<tabs::TabModel> tab_model_;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

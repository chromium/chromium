// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class TabStripModel;

namespace tabs {
class TabModel;
}  // namespace tabs

// Manages all state associated with the lens overlay.
class LensOverlayController : public TabStripModelObserver {
 public:
  explicit LensOverlayController(tabs::TabModel* tab_model);
  ~LensOverlayController() override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Called when the associated tab enters the foreground.
  void TabForegrounded();

  // Called when the associated tab enters the background.
  void TabBackgrounded();

  // Owns this class.
  raw_ptr<tabs::TabModel> tab_model_;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

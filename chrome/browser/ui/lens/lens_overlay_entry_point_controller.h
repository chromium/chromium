// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/actions/actions.h"

namespace lens {

// Class responsible for keeping Lens Overlay entry points in their correct
// state. This functionality needs to be separate from LensOverlayController,
// since LensOverlayController exist per tab, while entry points are per browser
// window.
class LensOverlayEntryPointController : public FullscreenObserver,
                                        public TemplateURLServiceObserver {
 public:
  explicit LensOverlayEntryPointController(Browser* browser);
  ~LensOverlayEntryPointController() override;

 private:
  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Updates the enable/disable state of entry points. If hide_if_needed is
  // true, instead of just disabling the entrypoint, we will also hide the
  // entrypoint from the user.
  void UpdateEntryPointsState(bool hide_if_needed);

  // Returns the ActionItem corresponding to our pinnable toolbar entrypoint.
  actions::ActionItem* GetToolbarEntrypoint();

  // Observer to check for browser window entering fullscreen.
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};

  // Observer to check for changes to the users DSE.
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  // Reference to the browser housing our entry points.
  raw_ptr<Browser> browser_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_

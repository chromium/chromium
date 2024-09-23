// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/actions/actions.h"

class BrowserWindowInterface;
class CommandUpdater;

namespace lens {

// Per-browser-window class responsible for keeping Lens Overlay entry points in
// their correct state. This functionality needs to be separate from
// LensOverlayController, since LensOverlayController exist per tab, while entry
// points are per browser window.
class LensOverlayEntryPointController : public FullscreenObserver,
                                        public TemplateURLServiceObserver {
 public:
  LensOverlayEntryPointController();
  ~LensOverlayEntryPointController() override;

  // This class does nothing if not initialized. IsEnabled returns false.
  void Initialize(BrowserWindowInterface* browser_window_interface,
                  CommandUpdater* command_updater);

  // Whether the entry points should be enabled.
  bool IsEnabled();

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

  // Used to change whether the lens entrypoint is enabled in the 3 dot menu.
  // The CommandUpdater is owned by the BrowserWindowInterface, which also owns
  // this, and thus is guaranteed to outlive this.
  raw_ptr<CommandUpdater> command_updater_;

  // Owns this.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_

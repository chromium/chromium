// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/tabs/public/tab_interface.h"

class LensOverlayController;

namespace lens {
class LensOverlaySidePanelCoordinator;
class LensSearchboxController;
}  // namespace lens

namespace variations {
class VariationsClient;
}  // namespace variations

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;
class ThemeService;

// Controller for all Lens Search features in Chrome. All external entry points
// should go through this controller.
// This migration is still in progress. Follow progress via crbug.com/404941800.
class LensSearchController {
 public:
  explicit LensSearchController(tabs::TabInterface* tab);
  virtual ~LensSearchController();

  // Initializes all the necessary dependencies for the LensSearchController.
  void Initialize(variations::VariationsClient* variations_client,
                  signin::IdentityManager* identity_manager,
                  PrefService* pref_service,
                  syncer::SyncService* sync_service,
                  ThemeService* theme_service);

  // A simple utility that gets the the LensSearchController TabFeature set by
  // the embedding tab of a lens WebUI hosted in `webui_web_contents`.
  // May return nullptr if no LensSearchController TabFeature is associated
  // with `webui_web_contents`.
  static LensSearchController* FromWebUIWebContents(
      content::WebContents* webui_web_contents);

  // A simple utility that gets the the LensSearchController TabFeature set by
  // the instances of WebContents associated with a tab.
  // May return nullptr if no LensSearchController TabFeature is associated
  // with `tab_web_contents`.
  static LensSearchController* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // This is an entry point for showing the overlay UI. This has no effect if
  // the overlay is not currently `kOff`.  This has no effect if the tab is not
  // in the foreground. If the overlay is successfully invoked, then the value
  // of `invocation_source` will be recorded in the relevant metrics. Virtual
  // for testing.
  virtual void OpenLensOverlay(
      lens::LensOverlayInvocationSource invocation_source);

  // Starts the closing process of the overlay. This is an asynchronous process
  // with the following sequence:
  //   (1) Close the side panel
  //   (2) Close the overlay.
  // Step (1) is asynchronous.
  virtual void CloseLensAsync(
      lens::LensOverlayDismissalSource dismissal_source);

  // Instantly closes all Lens components currently opened.This may not look
  // nice if the overlay is visible when this is called.
  virtual void CloseLensSync(lens::LensOverlayDismissalSource dismissal_source);

  // Returns the tab interface that owns this controller.
  tabs::TabInterface* GetTabInterface();

  // Returns the weak pointer to this class.
  base::WeakPtr<LensSearchController> GetWeakPtr();

  // Returns the LensOverlayController.
  LensOverlayController* lens_overlay_controller();

  // Returns the LensOverlaySidePanelCoordinator.
  lens::LensOverlaySidePanelCoordinator* lens_overlay_side_panel_coordinator();

  optimization_guide::PageContextEligibility* page_context_eligibility();

  // Testing function for setting the page context eligibility API for this
  // controller.
  void set_page_context_eligibility_for_testing(
      optimization_guide::PageContextEligibility* page_context_eligibility) {
    page_context_eligibility_ = page_context_eligibility;
  }

 protected:
  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      LensSearchController* lens_search_controller,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service);

  // Override these methods to be able to track calls made to the side panel
  // coordinator.
  virtual std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
  CreateLensOverlaySidePanelCoordinator();

  // Override these methods to be able to track calls made to the side panel
  // coordinator.
  virtual std::unique_ptr<lens::LensSearchboxController>
  CreateLensSearchboxController();

  // Override these methods to be able to track calls made to the page context
  // eligibility API.
  virtual void CreatePageContextEligibilityAPI();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. No feature is currently active or soon to be
    // active.
    kOff,

    // One or more Lens features are active on this tab.
    kActive,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

 private:
  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // Whether the LensSearchController has been initialized. Meaning, all the
  // dependencies have been initialized and the controller is ready to use.
  bool initialized_ = false;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // The overlay controller for the Lens Search feature on this tab.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  // The side panel coordinator for the Lens Search feature on this tab.
  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
      lens_overlay_side_panel_coordinator_;

  // The searchbox controller for the Lens Search feature on this tab.
  // TODO(crbug.com/413138792): Hook up this controller to handle searchbox
  // interactions, without a dependency on the overlay controller.
  std::unique_ptr<lens::LensSearchboxController> lens_searchbox_controller_;

  // The page context eligibility API if it has been fetched. Can be nullptr.
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

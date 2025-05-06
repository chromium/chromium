// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

class LensOverlayController;
class GURL;

namespace lens {
class LensOverlayGen204Controller;
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

  // Sets a region to search after the overlay loads, then calls ShowUI().
  // All units are in device pixels. region_bitmap contains the high definition
  // image bytes to use for the search instead of cropping the region from the
  // viewport. Virtual for testing.
  virtual void OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      lens::mojom::CenterRotatedBoxPtr region,
      const SkBitmap& region_bitmap);

  // Convenience method for calling OpenLensOverlayWithPendingRegion, that will
  // convert the bounds into a CenterRotated Box to pass to the overlay.
  void OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      const gfx::Rect& tab_bounds,
      const gfx::Rect& view_bounds,
      const gfx::Rect& region_bounds,
      const SkBitmap& region_bitmap);

  // Starts the contextualization flow without the overlay being shown to the
  // user. Virtual for testing.
  virtual void StartContextualization(
      lens::LensOverlayInvocationSource invocation_source);

  // Issues a contextual search request for Lens to fulfill. Starts
  // contextualization flow if its not already in progress. If the Lens Overlay
  // is in the process of opening, the request will be queued until the overlay
  // is fully opened.
  // TODO(crbug.com/403629222): Revisit if it makes sense to pass the
  // destination URL instead of the query text directly.
  void IssueContextualSearchRequest(const GURL& destination_url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion);

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

  // Returns the LensOverlayQueryController.
  lens::LensOverlayQueryController* lens_overlay_query_controller();

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
  friend class LensOverlayController;

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

  // Override these methods to stub out network requests for testing.
  virtual std::unique_ptr<lens::LensOverlayQueryController>
  CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      lens::UploadProgressCallback page_content_upload_progress_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller);

  // Override these methods to be able to track calls made to the side panel
  // coordinator.
  virtual std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
  CreateLensOverlaySidePanelCoordinator();

  // Override these methods to be able to track calls made to the side panel
  // coordinator.
  virtual std::unique_ptr<lens::LensSearchboxController>
  CreateLensSearchboxController();

  // Called by the Lens overlay when it has finished opening and has moved to
  // the kOverlay state. This is how this class knows it can move into kActive
  // state.
  // TODO(crbug.com/404941800): Make this more generic to allow for multiple
  // features to initialize at the same time.
  void NotifyOverlayOpened();

  // Shared logic for cleanup that is called after all features have finished
  // cleaning up.
  void CloseLensPart2();

  // Override these methods to be able to track calls made to the page context
  // eligibility API.
  virtual void CreatePageContextEligibilityAPI();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. No feature is currently active or soon to be
    // active.
    kOff,

    // The controller is in the process of starting up. Soon to be kActive.
    kInitializing,

    // One or more Lens features are active on this tab.
    kActive,

    // The controller is in the process of closing all dependencies and cleaning
    // up. Will soon be kOff.
    kClosing,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

 private:
  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // Passes the correct callbacks and dependencies to the protected
  // CreateLensQueryController method.
  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayInvocationSource invocation_source);

  // Callback used by the query controller to notify the search controller of
  // the response to the initial image upload request.
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text,
      bool is_error);

  // Callback used by the query controller to notify the search controller of
  // the URL response to the interaction request, aka, the URL that should be
  // opened in the results frame.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Callback used by the query controller to notify the search controller of
  // the response of an interaction request. If this is a visual interaction
  // request, the response will contain the text container within that image.
  void HandleInteractionResponse(lens::mojom::TextPtr text);

  // Callback used by the query controller to notify the search controller of
  // the suggest inputs response. This is used to update the searchbox with
  // the most recent suggest inputs.
  void HandleSuggestInputsResponse(
      lens::proto::LensOverlaySuggestInputs suggest_inputs);

  // Callback used by the query controller to pass the thumbnail bytes of a
  // visual interaction request to the searchbox.
  void HandleThumbnailCreated(const std::string& thumbnail_bytes);

  // Callback used by the query controller to notify the search controller of
  // the progress of the page content upload.
  void HandlePageContentUploadProgress(uint64_t position, uint64_t total);

  // Whether the LensSearchController has been initialized. Meaning, all the
  // dependencies have been initialized and the controller is ready to use.
  bool initialized_ = false;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // The query controller for the Lens Search feature on this tab. Lives for the
  // duration of a Lens feature being active on this tab.
  std::unique_ptr<lens::LensOverlayQueryController>
      lens_overlay_query_controller_;

  // The overlay controller for the Lens Search feature on this tab.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  // The controller for sending gen204 pings. Owned by this class so it can
  // outlive the query controller, allowing gen204 requests to be sent upon
  // query end.
  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  // The side panel coordinator for the Lens Search feature on this tab.
  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
      lens_overlay_side_panel_coordinator_;

  // The searchbox controller for the Lens Search feature on this tab.
  // TODO(crbug.com/413138792): Hook up this controller to handle searchbox
  // interactions, without a dependency on the overlay controller.
  std::unique_ptr<lens::LensSearchboxController> lens_searchbox_controller_;

  // The page context eligibility API if it has been fetched. Can be nullptr.
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The theme service associated with the current profile. Owned by Profile,
  // and thus guaranteed to outlive this instance.
  raw_ptr<ThemeService> theme_service_;

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTROLLER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/check.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace {

void CheckInitialized(bool initialized) {
  CHECK(initialized)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
}

LensSearchController* GetLensSearchControllerFromTabInterface(
    tabs::TabInterface* tab_interface) {
  return tab_interface
             ? tab_interface->GetTabFeatures()->lens_search_controller()
             : nullptr;
}
}  // namespace

LensSearchController::LensSearchController(tabs::TabInterface* tab)
    : tab_(tab) {
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(base::BindRepeating(
      &LensSearchController::TabForegrounded, weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&LensSearchController::TabWillEnterBackground,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&LensSearchController::WillDiscardContents,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &LensSearchController::WillDetach, weak_ptr_factory_.GetWeakPtr())));
}
LensSearchController::~LensSearchController() = default;

// TODO(crbug.com/404941800): Reconsider which of these controllers should be
// created in Initialize() vs created on demand when invoked.
void LensSearchController::Initialize(
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  CHECK(!initialized_);
  initialized_ = true;
  variations_client_ = variations_client;
  identity_manager_ = identity_manager;
  pref_service_ = pref_service;
  sync_service_ = sync_service;
  theme_service_ = theme_service;

  // Create Gen204 controller first as query controller depends on it.
  gen204_controller_ = std::make_unique<lens::LensOverlayGen204Controller>();

  lens_overlay_controller_ = CreateLensOverlayController(
      tab_, this, variations_client, identity_manager, pref_service,
      sync_service, theme_service);

  lens_overlay_side_panel_coordinator_ =
      CreateLensOverlaySidePanelCoordinator();

  lens_searchbox_controller_ = CreateLensSearchboxController();

  lens_contextualization_controller_ =
      CreateLensSearchContextualizationController();

  lens_overlay_event_handler_ =
      std::make_unique<lens::LensOverlayEventHandler>(this);

  lens_session_metrics_logger_ =
      std::make_unique<lens::LensSessionMetricsLogger>();

  CreatePageContextEligibilityAPI();
}

// static.
LensSearchController* LensSearchController::FromWebUIWebContents(
    content::WebContents* webui_web_contents) {
  return GetLensSearchControllerFromTabInterface(
      webui::GetTabInterface(webui_web_contents));
}

// static.
LensSearchController* LensSearchController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  return GetLensSearchControllerFromTabInterface(
      tabs::TabInterface::GetFromContents(tab_web_contents));
}

void LensSearchController::OpenLensOverlay(
    lens::LensOverlayInvocationSource invocation_source) {
  CheckInitialized(initialized_);

  // If the eligibility checks fail, do not procced with opening any UI.
  if (!IsOff() || !RunLensEligibilityChecks(
                      invocation_source,
                      /*permission_granted_callback=*/base::BindRepeating(
                          &LensSearchController::OpenLensOverlay,
                          weak_ptr_factory_.GetWeakPtr(), invocation_source))) {
    return;
  }

  // Setup all state necessary for this Lens session.
  StartLensSession(invocation_source);

  lens_overlay_controller_->ShowUI(invocation_source,
                                   lens_overlay_query_controller_.get());
}

void LensSearchController::OpenLensOverlayWithPendingRegionFromBounds(
    lens::LensOverlayInvocationSource invocation_source,
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    const gfx::Rect& region_bounds,
    const SkBitmap& region_bitmap) {
  OpenLensOverlayWithPendingRegion(
      invocation_source,
      lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
          tab_bounds, view_bounds, region_bounds),
      region_bitmap);
}

void LensSearchController::OpenLensOverlayWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    lens::mojom::CenterRotatedBoxPtr region,
    const SkBitmap& region_bitmap) {
  // If the eligibility checks fail, do not procced with opening any UI.
  if (!IsOff() ||
      !RunLensEligibilityChecks(
          invocation_source,
          /*permission_granted_callback=*/base::BindRepeating(
              &LensSearchController::OpenLensOverlayWithPendingRegion,
              weak_ptr_factory_.GetWeakPtr(), invocation_source,
              base::Passed(region.Clone()), region_bitmap))) {
    return;
  }

  // Setup all state necessary for this Lens session.
  StartLensSession(invocation_source);

  lens_overlay_controller_->ShowUIWithPendingRegion(
      lens_overlay_query_controller_.get(), invocation_source,
      std::move(region), region_bitmap);
}

void LensSearchController::StartContextualization(
    lens::LensOverlayInvocationSource invocation_source) {
  // If the eligibility checks fail, do not procced with opening any UI.
  if (!IsOff() || !RunLensEligibilityChecks(
                      invocation_source,
                      /*permission_granted_callback=*/base::BindRepeating(
                          &LensSearchController::StartContextualization,
                          weak_ptr_factory_.GetWeakPtr(), invocation_source))) {
    return;
  }

  // Setup all state necessary for this Lens session.
  StartLensSession(invocation_source);

  // TODO(crbug.com/418856988): Replace this with a call that starts
  // contextualization without the unneeded callback.
  lens_contextualization_controller_->StartContextualization(invocation_source,
                                                             base::DoNothing());
}

void LensSearchController::IssueContextualSearchRequest(
    lens::LensOverlayInvocationSource invocation_source,
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  // This method should only be used by the omnibox contextual suggestion flow.
  // There is no dependency on the omnibox, so this check is solely to ensure a
  // new flow is not accidentally added.
  CHECK(invocation_source ==
        lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion);

  // If the eligibility checks fail, do not procced with opening any UI.
  if (!RunLensEligibilityChecks(
          invocation_source,
          /*permission_granted_callback=*/base::BindRepeating(
              &LensSearchController::IssueContextualSearchRequest,
              weak_ptr_factory_.GetWeakPtr(), invocation_source,
              destination_url, match_type, is_zero_prefix_suggestion))) {
    return;
  }

  if (IsOff()) {
    // If the state is off, the Lens sessions needs to be initialized.
    StartLensSession(invocation_source);
  }

  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  lens_overlay_controller_->IssueContextualSearchRequest(
      destination_url, lens_overlay_query_controller_.get(), match_type,
      is_zero_prefix_suggestion, invocation_source);
}

void LensSearchController::CloseLensAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state() == State::kOff) {
    return;
  }

  // Close the side panel if it is showing. This provides a smooth closing
  // animation.
  auto* side_panel_coordinator =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  CHECK(side_panel_coordinator);
  if (state_ == State::kActive && side_panel_coordinator->GetCurrentEntryId() ==
                                      SidePanelEntry::Id::kLensOverlayResults) {
    // If a close was triggered while the Lens side panel is showing, instead of
    // just immediately closing all UI, the side panel should close to show a
    // smooth closing animation. Once the side panel deregisters, it will
    // recall the close method in OnSidePanelHidden() which will finish the
    // closing process.
    state_ = State::kClosingSidePanel;
    last_dismissal_source_ = dismissal_source;
    side_panel_coordinator->Close();
    // Also trigger the overlay fade out animation, but don't pass a callback
    // to finish the closing process since the side panel will call
    // the finish closing process callback in OnSidePanelHidden().
    lens_overlay_controller_->TriggerOverlayCloseAnimation(base::DoNothing());
    return;
  }
  state_ = State::kClosing;

  // If the overlay is showing, and the side panel is not, the overlay needs to
  // fade out. Play the fade out animation and then clean up the rest of the UI
  // afterwards.
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->TriggerOverlayCloseAnimation(
        base::BindOnce(&LensSearchController::CloseLensPart2,
                       weak_ptr_factory_.GetWeakPtr(), dismissal_source));
  } else {
    CloseLensPart2(dismissal_source);
  }
}

void LensSearchController::CloseLensSync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state() == State::kOff) {
    return;
  }
  state_ = State::kClosing;
  CloseLensPart2(dismissal_source);
}

void LensSearchController::MaybeLaunchSurvey() {
  if (!base::FeatureList::IsEnabled(lens::features::kLensOverlaySurvey)) {
    return;
  }
  if (hats_triggered_in_session_) {
    return;
  }
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      tab_->GetBrowserWindowInterface()->GetProfile(),
      /*create_if_necessary=*/true);
  if (!hats_service) {
    // HaTS may not be available in e.g. guest profile
    return;
  }
  hats_triggered_in_session_ = true;
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerLensOverlayResults, tab_->GetContents(),
      lens::features::GetLensOverlaySurveyResultsTime().InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      {{"ID that's tied to your Google Lens session",
        base::NumberToString(lens_overlay_query_controller_->gen204_id())}});
}

bool LensSearchController::IsActive() {
  return state_ == State::kActive;
}

bool LensSearchController::IsOff() {
  return state_ == State::kOff;
}

bool LensSearchController::IsClosing() {
  return state_ == State::kClosing || state_ == State::kClosingSidePanel;
}

bool LensSearchController::IsHandshakeComplete() {
  const auto& suggest_inputs =
      lens_searchbox_controller_->GetLensSuggestInputs();
  return AreLensSuggestInputsReady(suggest_inputs);
}

tabs::TabInterface* LensSearchController::GetTabInterface() {
  return tab_;
}

const GURL& LensSearchController::GetPageURL() const {
  if (lens::CanSharePageURLWithLensOverlay(pref_service_)) {
    return tab_->GetContents()->GetVisibleURL();
  }
  return GURL::EmptyGURL();
}

std::optional<std::string> LensSearchController::GetPageTitle() {
  std::optional<std::string> page_title;
  content::WebContents* active_web_contents = tab_->GetContents();
  if (lens::CanSharePageTitleWithLensOverlay(sync_service_, pref_service_)) {
    page_title = std::make_optional<std::string>(
        base::UTF16ToUTF8(active_web_contents->GetTitle()));
  }
  return page_title;
}

base::WeakPtr<LensSearchController> LensSearchController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

LensOverlayController* LensSearchController::lens_overlay_controller() {
  CheckInitialized(initialized_);
  return lens_overlay_controller_.get();
}

const LensOverlayController* LensSearchController::lens_overlay_controller()
    const {
  CheckInitialized(initialized_);
  return lens_overlay_controller_.get();
}

lens::LensOverlayQueryController*
LensSearchController::lens_overlay_query_controller() {
  CheckInitialized(initialized_);
  return lens_overlay_query_controller_.get();
}

lens::LensOverlaySidePanelCoordinator*
LensSearchController::lens_overlay_side_panel_coordinator() {
  CheckInitialized(initialized_);

  return lens_overlay_side_panel_coordinator_.get();
}

lens::LensSearchboxController*
LensSearchController::lens_searchbox_controller() {
  CheckInitialized(initialized_);
  return lens_searchbox_controller_.get();
}

lens::LensOverlayEventHandler*
LensSearchController::lens_overlay_event_handler() {
  CheckInitialized(initialized_);
  return lens_overlay_event_handler_.get();
}

optimization_guide::PageContextEligibility*
LensSearchController::page_context_eligibility() {
  CheckInitialized(initialized_);
  if (page_context_eligibility_) {
    return page_context_eligibility_;
  }

  return nullptr;
}

lens::LensSearchContextualizationController*
LensSearchController::lens_search_contextualization_controller() {
  CheckInitialized(initialized_);
  return lens_contextualization_controller_.get();
}

lens::LensSessionMetricsLogger*
LensSearchController::lens_session_metrics_logger() {
  CheckInitialized(initialized_);
  return lens_session_metrics_logger_.get();
}

std::unique_ptr<LensOverlayController>
LensSearchController::CreateLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  return std::make_unique<LensOverlayController>(
      tab, lens_search_controller, variations_client, identity_manager,
      pref_service, sync_service, theme_service);
}

std::unique_ptr<lens::LensOverlayQueryController>
LensSearchController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_callback,
    lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    lens::UploadProgressCallback upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller) {
  return std::make_unique<lens::LensOverlayQueryController>(
      std::move(full_image_callback), std::move(url_callback),
      std::move(interaction_callback), std::move(suggest_inputs_callback),
      std::move(thumbnail_created_callback),
      std::move(upload_progress_callback), variations_client, identity_manager,
      profile, invocation_source, use_dark_mode, gen204_controller);
}

std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
LensSearchController::CreateLensOverlaySidePanelCoordinator() {
  return std::make_unique<lens::LensOverlaySidePanelCoordinator>(this);
}

std::unique_ptr<lens::LensSearchboxController>
LensSearchController::CreateLensSearchboxController() {
  return std::make_unique<lens::LensSearchboxController>(this);
}

std::unique_ptr<lens::LensSearchContextualizationController>
LensSearchController::CreateLensSearchContextualizationController() {
  return std::make_unique<lens::LensSearchContextualizationController>(this);
}

void LensSearchController::CreatePageContextEligibilityAPI() {
  // Post to a background thread to avoid blocking the set up of the overlay.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(&LensSearchController::OnPageContextEligibilityAPILoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LensSearchController::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
}

std::unique_ptr<lens::LensOverlayQueryController>
LensSearchController::CreateLensQueryController(
    lens::LensOverlayInvocationSource invocation_source) {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return CreateLensQueryController(
      base::BindRepeating(&LensSearchController::HandleStartQueryResponse,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LensSearchController::HandleInteractionURLResponse,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LensSearchController::HandleInteractionResponse,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LensSearchController::HandleSuggestInputsResponse,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LensSearchController::HandleThumbnailCreated,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &LensSearchController::HandlePageContentUploadProgress,
          weak_ptr_factory_.GetWeakPtr()),
      variations_client_, identity_manager_, profile,
      /*invocation_source=*/invocation_source,
      /*use_dark_mode=*/lens::LensOverlayShouldUseDarkMode(theme_service_),
      gen204_controller_.get());
}

void LensSearchController::StartLensSession(
    lens::LensOverlayInvocationSource invocation_source) {
  state_ = State::kInitializing;

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);

  // Start the current metrics logger session.
  lens_session_metrics_logger_->OnSessionStart(invocation_source,
                                               tab_->GetContents());

  // Let the searchbox controller know that a new session has started so it can
  // initialize any data needed for the searchbox.
  lens_searchbox_controller_->OnSessionStart();

  // Reset session state.
  hats_triggered_in_session_ = false;
}

bool LensSearchController::RunLensEligibilityChecks(
    lens::LensOverlayInvocationSource invocation_source,
    base::RepeatingClosure permission_granted_callback) {
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return false;
  }

  // If the user hasn't granted permission, request user permission before
  // showing the UI.
  if (!lens::CanSharePageScreenshotWithLensOverlay(pref_service_) ||
      (lens::features::IsLensOverlayContextualSearchboxEnabled() &&
       !lens::CanSharePageContentWithLensOverlay(pref_service_))) {
    if (!lens_permission_bubble_controller_) {
      lens_permission_bubble_controller_ =
          std::make_unique<lens::LensPermissionBubbleController>(
              *tab_, pref_service_, invocation_source);
    }
    lens_permission_bubble_controller_->RequestPermission(
        tab_->GetContents(), std::move(permission_granted_callback));
    return false;
  }

  return true;
}

void LensSearchController::NotifyOverlayOpened() {
  CHECK(state() == State::kInitializing);
  state_ = State::kActive;

  // Record the UMA for lens overlay invocation.
  lens_session_metrics_logger_->RecordInvocation();
}

void LensSearchController::CloseLensPart2(
    lens::LensOverlayDismissalSource dismissal_source) {
  // Let the controllers know to cleanup.
  // TODO(crbug.com/404941800): Move logging to a shared location to not be
  // dependent on the overlay controller.
  lens_overlay_controller_->CloseUI(dismissal_source);
  lens_searchbox_controller_->CloseUI();
  lens_permission_bubble_controller_.reset();
  lens_contextualization_controller_->ResetState();
  lens_overlay_side_panel_coordinator_->DeregisterEntryAndCleanup();

  // Cleanup the query controller after the overlay controller to prevent
  // dangling ptrs.
  lens_overlay_query_controller_.reset();

  // Record end of session metrics.
  lens_session_metrics_logger_->RecordEndOfSessionMetrics(dismissal_source);

  state_ = State::kOff;
}

void LensSearchController::OnSidePanelWillHide(
    SidePanelEntryHideReason reason) {
  // If the tab is not in the foreground, this is not relevant.
  if (!tab_->IsActivated()) {
    return;
  }

  if (!IsClosing()) {
    if (reason == SidePanelEntryHideReason::kReplaced) {
      // If the Lens side panel is being replaced, don't close the side panel.
      // Instead, set the state and dismissal source and wait for
      // OnSidePanelHidden to be called.
      state_ = State::kClosingSidePanel;
      last_dismissal_source_ =
          lens::LensOverlayDismissalSource::kSidePanelEntryReplaced;
    } else {
      // Trigger the close animation and notify the overlay that the side
      // panel is closing so that it can fade out the UI.
      CloseLensAsync(lens::LensOverlayDismissalSource::kSidePanelCloseButton);
    }
  }
}

void LensSearchController::OnSidePanelHidden() {
  if (state_ != State::kClosingSidePanel) {
    return;
  }
  CHECK(last_dismissal_source_.has_value());
  CloseLensPart2(*last_dismissal_source_);
  last_dismissal_source_.reset();
}

void LensSearchController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
  lens_contextualization_controller_->SetText(text.Clone());
  lens_overlay_controller_->HandleStartQueryResponse(std::move(objects),
                                                     std::move(text), is_error);
}

void LensSearchController::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  lens_overlay_controller_->HandleInteractionURLResponse(response);
}

void LensSearchController::HandleInteractionResponse(
    lens::mojom::TextPtr text) {
  lens_overlay_controller_->HandleInteractionResponse(std::move(text));
}

void LensSearchController::HandleSuggestInputsResponse(
    lens::proto::LensOverlaySuggestInputs suggest_inputs) {
  lens_searchbox_controller_->HandleSuggestInputsResponse(suggest_inputs);
}

void LensSearchController::HandlePageContentUploadProgress(uint64_t position,
                                                           uint64_t total) {
  lens_overlay_controller_->HandlePageContentUploadProgress(position, total);
}

void LensSearchController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  lens_searchbox_controller_->HandleThumbnailCreated(thumbnail_bytes);
}

void LensSearchController::TabForegrounded(tabs::TabInterface* tab) {
  // Ignore the event if the overlay is not backgrounded.
  if (state_ != State::kBackground) {
    return;
  }

  // Notify the overlay controller of the tab foregrounded event so it can
  // restore to the previous state.
  lens_overlay_controller_->TabForegrounded(tab);

  state_ = State::kActive;
}

void LensSearchController::TabWillEnterBackground(tabs::TabInterface* tab) {
  if (state_ == State::kOff) {
    return;
  }

  // Ignore the event if the overlay is already backgrounded.
  if (state_ == State::kBackground) {
    return;
  }

  // Notify the overlay controller of the tab will enter background event so
  // it can hide the overlay.
  lens_overlay_controller_->TabWillEnterBackground(tab);

  state_ = State::kBackground;
}

void LensSearchController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Background tab contents discarded.
  CloseLensSync(lens::LensOverlayDismissalSource::kTabContentsDiscarded);
}

void LensSearchController::WillDetach(tabs::TabInterface* tab,
                                      tabs::TabInterface::DetachReason reason) {
  // When dragging a tab into a new window, all window-specific state must be
  // reset. As this flow is not fully functional, close the overlay regardless
  // of `reason`. https://crbug.com/342921671.
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      CloseLensSync(lens::LensOverlayDismissalSource::kTabClosed);
      return;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      CloseLensSync(lens::LensOverlayDismissalSource::kTabDragNewWindow);
      return;
  }
}

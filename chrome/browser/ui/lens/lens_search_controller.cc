// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/lens/lens_composebox_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_query_flow_router.h"
#include "chrome/browser/ui/lens/lens_results_panel_router.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/branded_strings.h"
#include "components/contextual_tasks/public/features.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/prefs/pref_service.h"
#include "skia/ext/codec_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {
// The size of the thumbnail to send to the searchbox.
inline constexpr float kMaxThumbnailWidth = 100.0f;
inline constexpr float kMaxThumbnailHeight = 100.0f;

void CheckInitialized(bool initialized) {
  CHECK(initialized)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
}

std::string ScaleBitmapAndEncodeToDataUri(SkBitmap bitmap) {
  float scale = std::min(kMaxThumbnailWidth / bitmap.width(),
                         kMaxThumbnailHeight / bitmap.height());
  int target_height = static_cast<int>(bitmap.height() * scale);
  int target_width = static_cast<int>(bitmap.width() * scale);

  SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, target_width, target_height);
  if (scaled_bitmap.drawsNothing()) {
    return std::string();
  }

  return skia::EncodePngAsDataUri(scaled_bitmap.pixmap());
}

bool UseNonBlockingPrivacyNotice(
    lens::LensOverlayInvocationSource invocation_source) {
  if (!lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled()) {
    return false;
  }
  // Invocation sources that simply open the overlay without submitting a query
  // are permitted to use the non-blocking privacy notice.
  return (invocation_source == lens::LensOverlayInvocationSource::kAppMenu ||
          invocation_source ==
              lens::LensOverlayInvocationSource::kContentAreaContextMenuPage ||
          invocation_source == lens::LensOverlayInvocationSource::kToolbar ||
          invocation_source == lens::LensOverlayInvocationSource::kOmnibox ||
          invocation_source ==
              lens::LensOverlayInvocationSource::kOmniboxPageAction ||
          invocation_source ==
              lens::LensOverlayInvocationSource::kHomeworkActionChip);
}

}  // namespace

DEFINE_USER_DATA(LensSearchController);

// static
LensSearchController* LensSearchController::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

LensSearchController::LensSearchController(tabs::TabInterface* tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
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

  lens_composebox_controller_ = CreateLensComposeboxController();

  lens_contextualization_controller_ =
      CreateLensSearchContextualizationController();
  // Create the page context eligibility API as soon as possible as it is needed
  // for every contextualization request.
  lens_contextualization_controller_->CreatePageContextEligibilityAPI();

  lens_overlay_event_handler_ =
      std::make_unique<lens::LensOverlayEventHandler>(this);

  lens_session_metrics_logger_ =
      std::make_unique<lens::LensSessionMetricsLogger>();
}

// static.
LensSearchController* LensSearchController::FromWebUIWebContents(
    content::WebContents* webui_web_contents) {
  return From(webui::GetTabInterface(webui_web_contents));
}

// static.
LensSearchController* LensSearchController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  if (!tab) {
    // TODO(crbug.com/444404134): Instead of calling MaybeGetFromContents(),
    // callers should be ensuring that the web contents is a tab. Dump to try to
    // identify when it is not.
    base::debug::DumpWithoutCrashing();
  }
  return From(tab);
}

void LensSearchController::OpenLensOverlay(
    lens::LensOverlayInvocationSource invocation_source) {
  CheckInitialized(initialized_);

  // The overlay can only be reinvoked if the feature is enabled.
  const bool allow_reinvoking_overlay =
      lens::features::GetEnableLensButtonInSearchbox() || IsOff();
  // If the eligibility checks fail, do not proceed with opening any UI.
  if (!allow_reinvoking_overlay ||
      !RunLensEligibilityChecks(
          invocation_source,
          /*permission_granted_callback=*/base::BindRepeating(
              &LensSearchController::OpenLensOverlay,
              weak_ptr_factory_.GetWeakPtr(), invocation_source))) {
    return;
  }

  // If flag enabled, perform an empty contextual query instead of opening the
  // overlay as normal. For internal debugging only.
  if (lens::features::IsLensOverlayForceEmptyCsbQueryEnabled()) {
    IssueTextSearchRequest(
        lens::LensOverlayInvocationSource::kContentAreaContextMenuText,
        /*query_text=*/"",
        /*additional_query_parameters=*/{},
        // TODO(crbug.com/432490312): Match type here is likely not ideal.
        // Investigate removing match type from this function.
        AutocompleteMatchType::Type::SEARCH_SUGGEST,
        /*is_zero_prefix_suggestion=*/false,
        /*suppress_contextualization=*/false);
    return;
  }

  if (lens::features::IsLensSearchZeroStateCsbEnabled() && IsOff()) {
    IssueZeroStateRequest(invocation_source);
    return;
  }

  // If the overlay is already active, don't start a new session. This can
  // happen if the side panel is open and the user reinvokes the overlay.
  if (IsOff()) {
    // Setup all state necessary for this Lens session.
    StartLensSession(invocation_source);
  }

  lens_overlay_controller_->ShowUI(invocation_source);
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
      invocation_source, std::move(region), region_bitmap);
}

void LensSearchController::OpenLensOverlayInCurrentSession() {
  if (IsOff() || lens_overlay_controller_->IsOverlayShowing()) {
    return;
  }

  // If the overlay was already initialized, but hidden, reshow the overlay.
  if (lens_overlay_controller_->state() ==
      LensOverlayController::State::kHidden) {
    lens_overlay_controller_->ReshowOverlay();
    return;
  }

  // Otherwise, the overlay must be fully closed. Open the overlay as normal.
  lens_overlay_controller_->ShowUI(
      lens_session_metrics_logger_->GetInvocationSource());
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

  std::string query_text =
      lens::ExtractTextQueryParameterValue(destination_url);
  std::map<std::string, std::string> additional_query_parameters =
      lens::GetParametersMapWithoutQuery(destination_url);

  IssueTextSearchRequest(
      invocation_source, query_text, additional_query_parameters, match_type,
      is_zero_prefix_suggestion, /*suppress_contextualization=*/false);
}

void LensSearchController::IssueTextSearchRequest(
    lens::LensOverlayInvocationSource invocation_source,
    std::string query_text,
    std::map<std::string, std::string> additional_query_parameters,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    bool suppress_contextualization) {
  // If the eligibility checks fail, do not procced with opening any UI.
  if (!RunLensEligibilityChecks(
          invocation_source,
          /*permission_granted_callback=*/base::BindRepeating(
              &LensSearchController::IssueTextSearchRequest,
              weak_ptr_factory_.GetWeakPtr(), invocation_source, query_text,
              additional_query_parameters, match_type,
              is_zero_prefix_suggestion, suppress_contextualization))) {
    return;
  }

  if (IsOff()) {
    // If the state is off, the Lens sessions needs to be initialized.
    StartLensSession(invocation_source, suppress_contextualization);
  }

  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  lens_overlay_controller_->IssueTextSearchRequest(
      query_text, additional_query_parameters, match_type,
      is_zero_prefix_suggestion, invocation_source);
}

void LensSearchController::IssueZeroStateRequest(
    lens::LensOverlayInvocationSource invocation_source) {
  CheckInitialized(initialized_);
  if (!RunLensEligibilityChecks(
          invocation_source,
          base::BindRepeating(&LensSearchController::IssueZeroStateRequest,
                              weak_ptr_factory_.GetWeakPtr(),
                              invocation_source))) {
    return;
  }

  auto query_start_time = base::Time::Now();
  if (IsOff()) {
    StartLensSession(invocation_source);
  }

  lens_contextualization_controller_->StartContextualization(
      invocation_source,
      base::BindOnce(
          &LensSearchController::OnPageContextUpdatedForZeroStateRequest,
          weak_ptr_factory_.GetWeakPtr(), invocation_source, query_start_time));
  // Show the side panel right away so the ghost loader is shown.
  lens_overlay_side_panel_coordinator()->RegisterEntryAndShow();
}

void LensSearchController::CloseLensAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state() == State::kOff) {
    return;
  }

  // While the overlay is closing, the session handle can be destroyed or
  // moved. It needs to be reset here to avoid a crash when the query router is
  // eventually destroyed.
  if (query_router_) {
    query_router_->reset_file_upload_status_observation();
  }

  // Close the side panel if it is showing. This provides a smooth closing
  // animation.
  auto* const side_panel_ui =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
  CHECK(side_panel_ui);
  if (state_ == State::kActive &&
      side_panel_ui->IsSidePanelEntryShowing(
          SidePanelEntryKey(SidePanelEntry::Id::kLensOverlayResults))) {
    // If a close was triggered while the Lens side panel is showing, instead of
    // just immediately closing all UI, the side panel should close to show a
    // smooth closing animation. Once the side panel deregisters, it will
    // recall the close method in OnSidePanelHidden() which will finish the
    // closing process.
    state_ = State::kClosingSidePanel;
    last_dismissal_source_ = dismissal_source;
    side_panel_ui->Close(lens_overlay_side_panel_coordinator_->GetPanelType());
    // Also trigger the overlay fade out animation, but don't pass a callback
    // to finish the closing process since the side panel will call
    // the finish closing process callback in OnSidePanelHidden().
    lens_overlay_controller_->TriggerOverlayFadeOutAnimation(base::DoNothing());
    return;
  }
  state_ = State::kClosing;

  // If the overlay is showing, and the side panel is not, the overlay needs to
  // fade out. Play the fade out animation and then clean up the rest of the UI
  // afterwards.
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->TriggerOverlayFadeOutAnimation(
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

void LensSearchController::HideOverlay(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state() == State::kOff) {
    return;
  }

  // If the overlay is showing, the overlay needs to fade out. Play the fade out
  // animation and then clean up the rest of the UI afterwards.
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->TriggerOverlayFadeOutAnimation(
        base::BindOnce(&LensSearchController::OnOverlayHidden,
                       weak_ptr_factory_.GetWeakPtr(), dismissal_source));
  }
}

void LensSearchController::HideOverlay() {
  if (state() == State::kOff || !lens_overlay_controller_->IsOverlayShowing()) {
    return;
  }

  // This method should only be called when the side panel is open.
  DCHECK(lens_overlay_side_panel_coordinator_->state() !=
         lens::LensOverlaySidePanelCoordinator::State::kOff);

  // If the overlay is showing, the overlay needs to fade out. Play the fade out
  // animation and then clean up the rest of the UI afterwards.
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->TriggerOverlayFadeOutAnimation(
        base::BindOnce(&LensSearchController::OnOverlayHidden,
                       weak_ptr_factory_.GetWeakPtr(), std::nullopt));
  }
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

bool LensSearchController::IsShowingUI() {
  CHECK(lens_overlay_controller_);
  CHECK(lens_overlay_side_panel_coordinator_);
  return lens_overlay_controller_->IsOverlayShowing() ||
         lens_overlay_side_panel_coordinator_->IsEntryShowing();
}

bool LensSearchController::IsOff() {
  return state_ == State::kOff;
}

bool LensSearchController::IsClosing() {
  return state_ == State::kClosing || state_ == State::kClosingSidePanel;
}

bool LensSearchController::IsHandshakeComplete() {
  auto suggest_inputs = query_router_->GetSuggestInputs();
  return suggest_inputs.has_value() &&
         AreLensSuggestInputsReady(*suggest_inputs);
}

bool LensSearchController::should_route_to_contextual_tasks() const {
  return should_route_to_contextual_tasks_;
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

void LensSearchController::ClearVisualSelectionThumbnail() {
  lens_searchbox_controller_->SetSearchboxThumbnail(std::string());
}

void LensSearchController::SetThumbnailCreatedCallback(
    base::RepeatingCallback<void(const std::string&)> callback) {
  thumbnail_created_callback_ = std::move(callback);
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

lens::LensQueryFlowRouter* LensSearchController::query_router() {
  CheckInitialized(initialized_);
  return query_router_.get();
}

lens::LensOverlaySidePanelCoordinator*
LensSearchController::lens_overlay_side_panel_coordinator() {
  CheckInitialized(initialized_);

  return lens_overlay_side_panel_coordinator_.get();
}

lens::LensResultsPanelRouter* LensSearchController::results_panel_router() {
  CheckInitialized(initialized_);
  return results_panel_router_.get();
}

lens::LensSearchboxController*
LensSearchController::lens_searchbox_controller() {
  CheckInitialized(initialized_);
  return lens_searchbox_controller_.get();
}

lens::LensComposeboxController*
LensSearchController::lens_composebox_controller() {
  CheckInitialized(initialized_);
  return lens_composebox_controller_.get();
}

lens::LensOverlayEventHandler*
LensSearchController::lens_overlay_event_handler() {
  CheckInitialized(initialized_);
  return lens_overlay_event_handler_.get();
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

lens::LensOverlayGen204Controller* LensSearchController::gen204_controller() {
  CheckInitialized(initialized_);
  return gen204_controller_.get();
}

std::optional<lens::LensOverlayInvocationSource>
LensSearchController::invocation_source() {
  return invocation_source_;
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
      std::move(interaction_callback), std::move(thumbnail_created_callback),
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

std::unique_ptr<lens::LensComposeboxController>
LensSearchController::CreateLensComposeboxController() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return std::make_unique<lens::LensComposeboxController>(this, profile);
}

std::unique_ptr<lens::LensSearchContextualizationController>
LensSearchController::CreateLensSearchContextualizationController() {
  return std::make_unique<lens::LensSearchContextualizationController>(this);
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
    lens::LensOverlayInvocationSource invocation_source,
    bool suppress_contextualization) {
  state_ = State::kInitializing;
  invocation_source_ = invocation_source;

  // Check if contextual tasks is currently available. If so, route through
  // results to the contextual tasks side panel.
  auto* const entry_point_eligibility_manager =
      contextual_tasks::EntryPointEligibilityManager::From(
          tab_->GetBrowserWindowInterface());
  should_route_to_contextual_tasks_ =
      contextual_tasks::GetEnableLensInContextualTasks() &&
      entry_point_eligibility_manager &&
      entry_point_eligibility_manager->AreEntryPointsEligible();

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);
  query_router_ = std::make_unique<lens::LensQueryFlowRouter>(this);
  query_router_->SetSuggestInputsReadyCallback(
      base::BindRepeating(&LensSearchController::OnSuggestInputsReady,
                          weak_ptr_factory_.GetWeakPtr()));

  // Start the current metrics logger session.
  lens_session_metrics_logger_->OnSessionStart(invocation_source,
                                               tab_->GetContents());

  // Let the searchbox controller know that a new session has started so it can
  // initialize any data needed for the searchbox.
  lens_searchbox_controller_->OnSessionStart(suppress_contextualization);

  // Set the results panel delegate to the side panel coordinator owned by
  // this controller.
  results_panel_router_ = std::make_unique<lens::LensResultsPanelRouter>(
      tab_->GetBrowserWindowInterface()->GetProfile(), this);

  // Reset session state.
  hats_triggered_in_session_ = false;
  is_handshake_complete_ = false;
}

bool LensSearchController::RunLensEligibilityChecks(
    lens::LensOverlayInvocationSource invocation_source,
    base::RepeatingClosure permission_granted_callback) {
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return false;
  }

  // The non-blocking privacy notice permits the overlay to open without
  // requesting user permission via the bubble.
  if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
      UseNonBlockingPrivacyNotice(invocation_source)) {
    return true;
  }

  // If the user hasn't granted permission, request user permission before
  // showing the UI.
  if (!lens::CanSharePageScreenshotWithLensOverlay(pref_service_) ||
      (lens::IsLensOverlayContextualSearchboxEnabled() &&
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
  CHECK(state() != State::kOff);
  // The search controller could be backgrounded as the overlay controller
  // is being initialized. If so, set the backgrounded state to active and
  // record the invocation instead of setting the current state to active.
  if (state_ == State::kBackground) {
    backgrounded_state_ = State::kActive;
  } else {
    state_ = State::kActive;
  }

  // Record the UMA for lens overlay invocation.
  lens_session_metrics_logger_->RecordInvocation();
}

void LensSearchController::OnThumbnailProcessed(
    bool is_region_selection,
    const std::string& thumbnail_uri) {
  if (should_route_to_contextual_tasks()) {
    // This function returns full viewport thumbnails and region selection
    // thumbnails. Only region search selections should trigger the thumbnail
    // created callback to be run.
    if (is_region_selection && thumbnail_created_callback_) {
      thumbnail_created_callback_.Run(thumbnail_uri);
    }
  }

  if (lens_searchbox_controller_) {
    lens_searchbox_controller_->SetSearchboxThumbnail(thumbnail_uri);
  }
  if (lens_composebox_controller_ && is_region_selection &&
      lens_overlay_controller_->use_aim_for_visual_search()) {
    lens_composebox_controller_->AddVisualSelectionContext(thumbnail_uri);
  }
}

void LensSearchController::CloseLensPart2(
    lens::LensOverlayDismissalSource dismissal_source) {
  // Let the controllers know to cleanup.
  // TODO(crbug.com/404941800): Move logging to a shared location to not be
  // dependent on the overlay controller.
  lens_overlay_controller_->CloseUI(dismissal_source);
  lens_searchbox_controller_->CloseUI();
  lens_composebox_controller_->CloseUI();
  lens_permission_bubble_controller_.reset();
  lens_contextualization_controller_->ResetState();
  lens_overlay_side_panel_coordinator_->DeregisterEntryAndCleanup();
  results_panel_router_.reset();

  // Cleanup the query controller after the overlay controller to prevent
  // dangling ptrs.
  lens_overlay_query_controller_.reset();
  query_router_.reset();
  invocation_source_.reset();
  thumbnail_created_callback_.Reset();

  // Record end of session metrics.
  lens_session_metrics_logger_->RecordEndOfSessionMetrics(dismissal_source);

  state_ = State::kOff;
}

void LensSearchController::OnOverlayHidden(
    std::optional<lens::LensOverlayDismissalSource> dismissal_source) {
  if (state_ == State::kOff) {
    return;
  }

  // If the side panel is not open, end the session.
  if (results_panel_router_ && !results_panel_router_->IsEntryShowing()) {
    // The caller should not have called this function without a dismissal
    // source if the side panel is not open, because it would leave the Lens
    // session in an inconsistent state.
    // TODO(crbug.com/440608864): Make this a CHECK once this is verified.
    if (!dismissal_source.has_value()) {
      // Exit early to avoid a crash.
      DCHECK(dismissal_source.has_value());
      return;
    }
    CloseLensPart2(dismissal_source.value());
    return;
  }

  if (should_route_to_contextual_tasks() &&
      dismissal_source ==
          lens::LensOverlayDismissalSource::kOverlayCloseButton) {
    CloseLensPart2(dismissal_source.value());
    return;
  }

  // Since the side panel is open and the overlay has smoothly faded out, hide
  // the overlay to restore state to the live page.
  lens_overlay_controller_->HideOverlayAndSetHiddenState();
  lens_overlay_controller_->ClearAllSelections();
  // Any pending visual selection that had not yet been submitted should be
  // cleared whenever the overlay is hidden.
  lens_composebox_controller_->ClearVisualSelectionContext();
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
    } else if (reason == SidePanelEntryHideReason::kBackgrounded) {
      TabWillEnterBackground(tab_);
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
  MaybeShowMobilePromo();
}

void LensSearchController::OnSuggestInputsReady() {
  if (IsOff()) {
    return;
  }

  auto suggest_inputs = query_router_->GetSuggestInputs();
  if (suggest_inputs.has_value()) {
    lens_searchbox_controller_->NotifySuggestInputsReady(*suggest_inputs);
  }

  // If the handshake was already complete, without the new suggest inputs,
  // exit early so that LensOverlayController::OnHandshakeComplete() isn't
  // called multiple times.
  if (is_handshake_complete_) {
    return;
  }

  // Check if the handshake with the server has been completed with the new
  // inputs. If so, this is the first time the suggest inputs satisfy the
  // handshake criteria, so notify the overlay that the handshake is complete.
  if (IsHandshakeComplete()) {
    is_handshake_complete_ = true;
    // Notify the overlay that it is now safe to query autocomplete.
    lens_overlay_controller()->OnHandshakeComplete();
  }
}

void LensSearchController::HandlePageContentUploadProgress(uint64_t position,
                                                           uint64_t total) {
  lens_overlay_controller_->HandlePageContentUploadProgress(position, total);
}

void LensSearchController::HandleThumbnailCreatedBitmap(
    const SkBitmap& thumbnail) {
  if (!lens::features::GetVisualSelectionUpdatesEnableCsbThumbnail() ||
      thumbnail.drawsNothing()) {
    return;
  }

  // SkBitmap is ref-counted, so a copy is cheap and safe for task posting.
  SkBitmap thumbnail_copy = thumbnail;

  // Downscale the bitmap to a size that is appropriate for the searchbox.
  // Keeping it full resolution will cause stuttering when the UI opens. Push
  // off the main thread to avoid blocking the overlay initialization.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ScaleBitmapAndEncodeToDataUri, std::move(thumbnail_copy)),
      base::BindOnce(&LensSearchController::OnThumbnailProcessed,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_region_selection=*/false));
}

void LensSearchController::HandleInteractionResponse(
    lens::mojom::TextPtr text) {
  lens_overlay_controller_->HandleInteractionResponse(std::move(text));
}

void LensSearchController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes,
    const SkBitmap& region_bitmap) {
  lens_overlay_controller()->HandleRegionBitmapCreated(region_bitmap);

  std::string thumbnail_uri =
      webui::MakeDataURIForImage(base::as_byte_span(thumbnail_bytes), "jpeg");
  OnThumbnailProcessed(/*is_region_selection=*/true, thumbnail_uri);
}

void LensSearchController::TabForegrounded(tabs::TabInterface* tab) {
  // Ignore the event if the search controller is not backgrounded.
  if (state_ != State::kBackground) {
    return;
  }

  // Notify the overlay controller of the tab foregrounded event so it can
  // restore to the previous state.
  lens_overlay_controller_->TabForegrounded(tab);

  state_ = backgrounded_state_;
}

void LensSearchController::TabWillEnterBackground(tabs::TabInterface* tab) {
  if (state_ == State::kOff) {
    return;
  }

  // Ignore the event if the overlay is already backgrounded.
  if (state_ == State::kBackground) {
    return;
  }

  // TODO(crbug.com/459478871): If the overlay is in an initializing state, then
  // the entire Lens session is closed as there is no way to currently recover
  // from this state. In the future, the side panel should remain open and the
  // overlay should close.
  if (lens_overlay_controller_->IsOverlayInitializing()) {
    CloseLensSync(
        lens::LensOverlayDismissalSource::kTabBackgroundedWhileInitializing);
    return;
  }

  // If no Lens UI is showing when the tab is backgrounded, then the entire Lens
  // session should be closed.
  if (!IsShowingUI()) {
    CloseLensSync(
        lens::LensOverlayDismissalSource::kTabBackgroundedWhileScreenshotting);
    return;
  }

  // Notify the overlay controller of the tab will enter background event so
  // it can hide the overlay.
  lens_overlay_controller_->TabWillEnterBackground(tab);

  backgrounded_state_ = state_;
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

void LensSearchController::OnPageContextUpdatedForZeroStateRequest(
    lens::LensOverlayInvocationSource invocation_source,
    base::Time query_start_time) {
  lens_searchbox_controller()->SetSearchboxInputText(std::string());
  if (lens_search_contextualization_controller()
          ->GetCurrentPageContextEligibility()) {
    // Create a region that consists of the entire viewport.
    auto full_viewport_region = lens::mojom::CenterRotatedBox::New();
    full_viewport_region->box =
        gfx::RectF(/*x=*/0.5, /*y=*/0.5, /*width=*/1.0, /*height=*/1.0);
    full_viewport_region->coordinate_type =
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;

    lens_overlay_query_controller()->SendRegionSearch(
        query_start_time, std::move(full_viewport_region),
        lens::LensOverlaySelectionType::REGION_SEARCH,
        /*additional_search_query_params=*/std::map<std::string, std::string>(),
        /*region_bytes=*/std::nullopt);
  }
}

void LensSearchController::MaybeShowMobilePromo() {
  if (MobilePromoOnDesktopTypeEnabled(
          MobilePromoOnDesktopPromoType::kLensPromo)) {
    IOSPromoTriggerService* service =
        IOSPromoTriggerServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                tab_->GetContents()->GetBrowserContext()));
    if (service) {
      service->NotifyPromoShouldBeShown(
          desktop_to_mobile_promos::PromoType::kLens);
    }
  }
}

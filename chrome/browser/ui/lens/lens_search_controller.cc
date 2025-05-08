// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/check.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace {
LensSearchController* GetLensSearchControllerFromTabInterface(
    tabs::TabInterface* tab_interface) {
  return tab_interface
             ? tab_interface->GetTabFeatures()->lens_search_controller()
             : nullptr;
}
}  // namespace

LensSearchController::LensSearchController(tabs::TabInterface* tab)
    : tab_(tab) {}
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
  theme_service_ = theme_service;

  // Create Gen204 controller first as query controller depends on it.
  gen204_controller_ = std::make_unique<lens::LensOverlayGen204Controller>();

  lens_overlay_controller_ = CreateLensOverlayController(
      tab_, this, variations_client, identity_manager, pref_service,
      sync_service, theme_service);

  lens_overlay_side_panel_coordinator_ =
      CreateLensOverlaySidePanelCoordinator();

  lens_searchbox_controller_ = CreateLensSearchboxController();

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
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";

  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // Exit early if the Lens feature is already active.
  if (state() != State::kOff) {
    return;
  }
  state_ = State::kInitializing;

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  lens_overlay_controller_->ShowUI(invocation_source,
                                   lens_overlay_query_controller_.get());
}

void LensSearchController::OpenLensOverlayWithPendingRegion(
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
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // Exit early if the Lens feature is already active.
  if (state() != State::kOff) {
    return;
  }
  state_ = State::kInitializing;

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  lens_overlay_controller_->ShowUIWithPendingRegion(
      lens_overlay_query_controller_.get(), invocation_source,
      std::move(region), region_bitmap);
}

void LensSearchController::StartContextualization(
    lens::LensOverlayInvocationSource invocation_source) {
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // Exit early if the Lens feature is already active.
  if (state() != State::kOff) {
    return;
  }
  state_ = State::kInitializing;

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  lens_overlay_controller_->StartContextualizationWithoutOverlay(
      invocation_source, lens_overlay_query_controller_.get());
}

void LensSearchController::IssueContextualSearchRequest(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // Exit early if the Lens feature is already active.
  if (state() != State::kOff) {
    return;
  }
  state_ = State::kInitializing;

  // TODO(crbug.com/402497756): For prototyping, reusing the existing
  // omnibox entry point. However, for production, create a new invocation
  // source for this new entry point.
  lens::LensOverlayInvocationSource invocation_source =
      lens::LensOverlayInvocationSource::kOmnibox;

  // Create the query controller to be used for the current invocation.
  CHECK(!lens_overlay_query_controller_);
  lens_overlay_query_controller_ = CreateLensQueryController(invocation_source);

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
  state_ = State::kClosing;

  // The overlay controller must be closed before the query controller so it
  // doesn't hold a dangling pointer. However, since the query controller
  // points to references owned by the overlay controller, those references
  // need to be invalidated before cleaning the overlay controller.
  // lens_overlay_query_controller_->ResetPageContentData();
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->CloseUIAsync(dismissal_source);
  } else {
    CloseLensPart2();
  }
}

void LensSearchController::CloseLensSync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state() == State::kOff) {
    return;
  }
  state_ = State::kClosing;
  // The overlay controller must be closed before the query controller so it
  // doesn't hold a dangling pointer. However, since the query controller
  // points to references owned by the overlay controller, those references
  // need to be invalidated before cleaning the overlay controller.
  // lens_overlay_query_controller_->ResetPageContentData();
  if (lens_overlay_controller_->state() != LensOverlayController::State::kOff) {
    lens_overlay_controller_->CloseUISync(dismissal_source);
  } else {
    CloseLensPart2();
  }
}

tabs::TabInterface* LensSearchController::GetTabInterface() {
  return tab_;
}

base::WeakPtr<LensSearchController> LensSearchController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

LensOverlayController* LensSearchController::lens_overlay_controller() {
  CHECK(initialized_) << "The LensSearchController has not been initialized. "
                         "Initialize() must "
                         "be called before using the LensSearchController.";
  return lens_overlay_controller_.get();
}

lens::LensOverlaySidePanelCoordinator*
LensSearchController::lens_overlay_side_panel_coordinator() {
  CHECK(initialized_) << "The LensSearchController has not been initialized. "
                         "Initialize() must "
                         "be called before using the LensSearchController.";
  return lens_overlay_side_panel_coordinator_.get();
}

optimization_guide::PageContextEligibility*
LensSearchController::page_context_eligibility() {
  CHECK(initialized_) << "The LensSearchController has not been initialized. "
                         "Initialize() must "
                         "be called before using the LensSearchController.";
  if (page_context_eligibility_) {
    return page_context_eligibility_;
  }

  return nullptr;
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

void LensSearchController::NotifyOverlayOpened() {
  CHECK(state() == State::kInitializing);
  state_ = State::kActive;
}

void LensSearchController::CloseLensPart2() {
  // Cleanup the query controller.
  lens_overlay_query_controller_.reset();
  state_ = State::kOff;
}

void LensSearchController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
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
  lens_overlay_controller_->HandleSuggestInputsResponse(suggest_inputs);
}

void LensSearchController::HandlePageContentUploadProgress(uint64_t position,
                                                           uint64_t total) {
  lens_overlay_controller_->HandlePageContentUploadProgress(position, total);
}

void LensSearchController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  lens_overlay_controller_->HandleThumbnailCreated(thumbnail_bytes);
}

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
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
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

void LensSearchController::Initialize(
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  CHECK(!initialized_);
  initialized_ = true;

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
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  lens_overlay_controller_->ShowUI(invocation_source);

  // TODO(crbug.com/404941800): This state should start with kInitializing and
  // then move to kActive once the overlay is fully initialized. Setting
  // straight to kActive for now to unblock development.
  state_ = State::kActive;
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

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  lens_overlay_controller_->ShowUIWithPendingRegion(
      invocation_source, std::move(region), region_bitmap);

  // TODO(crbug.com/404941800): This state should start with kInitializing and
  // then move to kActive once the overlay is fully initialized. Setting
  // straight to kActive for now to unblock development.
  state_ = State::kActive;
}

void LensSearchController::StartContextualization(
    lens::LensOverlayInvocationSource invocation_source) {
  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // TODO(crbug.com/404941800): Add logic based on this classes state once the
  // state machine is available.
  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  lens_overlay_controller_->StartContextualizationWithoutOverlay(
      invocation_source);

  // TODO(crbug.com/404941800): This state should start with kInitializing and
  // then move to kActive once the overlay is fully initialized. Setting
  // straight to kActive for now to unblock development.
  state_ = State::kActive;
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

  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  lens_overlay_controller_->IssueContextualSearchRequest(
      destination_url, match_type, is_zero_prefix_suggestion);

  // TODO(crbug.com/404941800): This state should start with kInitializing and
  // then move to kActive once the overlay is fully initialized. Setting
  // straight to kActive for now to unblock development.
  state_ = State::kActive;
}

void LensSearchController::CloseLensAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  lens_overlay_controller_->CloseUIAsync(dismissal_source);
  // TODO(crbug.com/404941800): This state should start with kClosing and
  // then move to kOff once all Lens feature have finished closing. Setting
  // straight to kOff for now to unblock development.
  state_ = State::kOff;
}

void LensSearchController::CloseLensSync(
    lens::LensOverlayDismissalSource dismissal_source) {
  lens_overlay_controller_->CloseUISync(dismissal_source);
  state_ = State::kOff;
}

tabs::TabInterface* LensSearchController::GetTabInterface() {
  return tab_;
}

base::WeakPtr<LensSearchController> LensSearchController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

LensOverlayController* LensSearchController::lens_overlay_controller() {
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
  return lens_overlay_controller_.get();
}

lens::LensOverlaySidePanelCoordinator*
LensSearchController::lens_overlay_side_panel_coordinator() {
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
         "be called before using the LensSearchController.";
  return lens_overlay_side_panel_coordinator_.get();
}

optimization_guide::PageContextEligibility*
LensSearchController::page_context_eligibility() {
  CHECK(initialized_)
      << "The LensSearchController has not been initialized. Initialize() must "
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

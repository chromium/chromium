// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"

#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/focus/focus_manager.h"

DEFINE_USER_DATA(LensOverlayHomeworkPageActionController);

LensOverlayHomeworkPageActionController::
    LensOverlayHomeworkPageActionController(
        tabs::TabInterface& tab_interface,
        Profile& profile,
        page_actions::PageActionController& page_action_controller)
    : profile_(profile),
      tab_(tab_interface),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

LensOverlayHomeworkPageActionController::
    ~LensOverlayHomeworkPageActionController() = default;

// static.
LensOverlayHomeworkPageActionController*
LensOverlayHomeworkPageActionController::From(tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void LensOverlayHomeworkPageActionController::UpdatePageActionIcon() {
  CHECK(tab_->GetContents());

  if (ShouldShow()) {
    if (!scoped_window_call_to_action_ptr_) {
      scoped_window_call_to_action_ptr_ =
          tab_->GetBrowserWindowInterface()->ShowCallToAction();
      lens::IncrementLensOverlayEduActionChipShownCount(
          base::to_address(profile_));
    }
    page_action_controller_->Show(kActionLensOverlayHomework);
    page_action_controller_->ShowSuggestionChip(kActionLensOverlayHomework);
  } else {
    scoped_window_call_to_action_ptr_.reset();
    page_action_controller_->HideSuggestionChip(kActionLensOverlayHomework);
    page_action_controller_->Hide(kActionLensOverlayHomework);
  }
}

void LensOverlayHomeworkPageActionController::HandlePageActionEvent(
    bool is_from_keyboard) {
  // If the user entered Lens through the keyboard and keyboard selection is not
  // enabled, we want to open Lens Web in a new tab.
  if (is_from_keyboard &&
      !lens::features::IsLensOverlayKeyboardSelectionEnabled()) {
    tab_->GetBrowserWindowInterface()
        ->GetFeatures()
        .lens_region_search_controller()
        ->Start(tab_->GetContents(), /*use_fullscreen_capture=*/true,
                /*is_google_default_search_provider=*/true,
                lens::AmbientSearchEntryPoint::
                    LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK);
    return;
  }

  LensSearchController* const controller =
      LensSearchController::FromTabWebContents(tab_->GetContents());
  CHECK(controller);

  if (lens::features::IsLensOverlayStraightToSrpEnabled()) {
    std::string query_text =
        lens::features::GetStraightToSrpQuery().empty()
            ? l10n_util::GetStringUTF8(IDS_LENS_CONTEXTUAL_SEARCH_DEFAULT_QUERY)
            : lens::features::GetStraightToSrpQuery();
    controller->IssueTextSearchRequest(
        lens::LensOverlayInvocationSource::kHomeworkActionChip, query_text,
        /*additional_query_parameters=*/{},
        AutocompleteMatchType::Type::SEARCH_SUGGEST,
        /*is_zero_prefix_suggestion=*/false,
        /*suppress_contextualization=*/false);
  } else {
    controller->OpenLensOverlay(
        lens::LensOverlayInvocationSource::kHomeworkActionChip);
  }
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      tab_->GetContents()->GetBrowserContext(), lens::features::kLensOverlay);

  page_action_controller_->HideSuggestionChip(kActionLensOverlayHomework);
  page_action_controller_->Hide(kActionLensOverlayHomework);
}

bool LensOverlayHomeworkPageActionController::ShouldShow() {
  if (!lens::ShouldShowLensOverlayEduActionChip(base::to_address(profile_))) {
    return false;
  }

  if (profile_->IsOffTheRecord()) {
    return false;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (lens::features::IsLensOverlayEduActionChipDisabledByGlic() &&
      glic::GlicEnabling::IsEligibleForGlicTieredRollout(
          base::to_address(profile_))) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  // Hide the homework chip if the broader lens feature is disabled.
  const auto* lens_overlay_entry_point_controller =
      tab_->GetBrowserWindowInterface()
          ->GetFeatures()
          .lens_overlay_entry_point_controller();
  if (!lens_overlay_entry_point_controller ||
      !lens_overlay_entry_point_controller->AreVisible()) {
    return false;
  }

  views::View* const location_bar_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->GetView(kLocationBarElementId);
  if (!location_bar_view) {
    return false;
  }

  // Hide the homework chip if the location bar is focused.
  const views::FocusManager* const focus_manager =
      location_bar_view->GetFocusManager();
  if (!focus_manager ||
      location_bar_view->Contains(focus_manager->GetFocusedView())) {
    return false;
  }

  // Treat the chip as a window-level call to action UI; only one such UI is
  // allowed to show at a time. Check if scoped_window_call_to_action_ptr_ is
  // already set (we are already showing the chip) before checking
  // CanShowCallToAction().
  if (!scoped_window_call_to_action_ptr_ &&
      !tab_->GetBrowserWindowInterface()->CanShowCallToAction()) {
    return false;
  }

  content::NavigationEntry* entry =
      tab_->GetContents()->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = tab_->GetContents()->GetController().GetVisibleEntry();
  }

  return lens_overlay_entry_point_controller->IsUrlEduEligible(entry->GetURL());
}

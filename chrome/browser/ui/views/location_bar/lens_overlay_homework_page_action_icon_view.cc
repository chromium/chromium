// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_icon_view.h"

#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metrics.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

LensOverlayHomeworkPageActionIconView::LensOverlayHomeworkPageActionIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    BrowserWindowInterface* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "LensOverlayHomework"),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);

  SetProperty(views::kElementIdentifierKey,
              kLensOverlayHomeworkPageActionIconElementId);

  SetLabel(l10n_util::GetStringUTF16(
      IDS_CONTENT_LENS_OVERLAY_ASK_GOOGLE_ENTRYPOINT_LABEL));
  // Elide behavior must be set to allow label to collapse.
  SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
}

LensOverlayHomeworkPageActionIconView::
    ~LensOverlayHomeworkPageActionIconView() = default;

void LensOverlayHomeworkPageActionIconView::UpdateImpl() {
  const bool should_show = ShouldShow();

  if (should_show) {
    // UpdateImpl() can be called multiple times, so make sure we don't call
    // ShowCallToAction() more than once while the chip is showing.
    if (!scoped_window_call_to_action_ptr_) {
      scoped_window_call_to_action_ptr_ = browser_->ShowCallToAction();
      lens::IncrementLensOverlayEduActionChipShownCount(browser_->GetProfile());
    }
  } else {
    scoped_window_call_to_action_ptr_.reset();
  }

  SetVisible(should_show);
  ResetSlideAnimation(true);
}

bool LensOverlayHomeworkPageActionIconView::ShouldShow() {
  if (!lens::ShouldShowLensOverlayEduActionChip(browser_->GetProfile())) {
    return false;
  }

  if (browser_->GetProfile()->IsOffTheRecord()) {
    return false;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (lens::features::IsLensOverlayEduActionChipDisabledByGlic() &&
      glic::GlicEnabling::IsEligibleForGlicTieredRollout(
          browser_->GetProfile())) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  // Hide the homework chip if the broader lens feature is disabled.
  const auto* controller =
      browser_->GetFeatures().lens_overlay_entry_point_controller();
  if (!controller || !controller->AreVisible()) {
    return false;
  }

  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }

  // Don't show the chip if the location bar isn't visible yet.
  // TODO(crbug.com/421963047): Investigate why we are getting two matching
  // views on ChromeOS.
  View* const location_bar_view =
      BrowserElementsViews::From(browser_)->GetView(kLocationBarElementId);
  if (!location_bar_view) {
    return false;
  }

  // Hide the homework chip if the location bar is focused.
  const views::FocusManager* const focus_manager = GetFocusManager();
  if (!focus_manager ||
      location_bar_view->Contains(focus_manager->GetFocusedView())) {
    return false;
  }

  // Treat the chip as a window-level call to action UI; only one such UI is
  // allowed to show at a time. Check if scoped_window_call_to_action_ptr_ is
  // already set (we are already showing the chip) before checking
  // CanShowCallToAction().
  if (!scoped_window_call_to_action_ptr_ && !browser_->CanShowCallToAction()) {
    return false;
  }

  // Use the committed entry (or the visible entry, if the committed entry is
  // the initial NavigationEntry) so the bookmarks bar disappears at the same
  // time the page does.
  CHECK(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = web_contents->GetController().GetVisibleEntry();
  }
  const GURL& url = entry->GetURL();

  return controller->IsUrlEduEligible(url);
}

void LensOverlayHomeworkPageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  // If the user entered Lens through the keyboard and keyboard selection is not
  // enabled, we want to open Lens Web in a new tab.
  if (source == PageActionIconView::EXECUTE_SOURCE_KEYBOARD &&
      !lens::features::IsLensOverlayKeyboardSelectionEnabled()) {
    browser_->GetFeatures().lens_region_search_controller()->Start(
        GetWebContents(), /*use_fullscreen_capture=*/true,
        /*is_google_default_search_provider=*/true,
        lens::AmbientSearchEntryPoint::
            LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK);
    return;
  }

  LensSearchController* const controller =
      LensSearchController::FromTabWebContents(GetWebContents());
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
      GetWebContents()->GetBrowserContext(), lens::features::kLensOverlay);

  // TODO(crbug.com/422844464): Fix Update() so that it correctly handles the
  // chip disappearing at this point, so that the following lines are no longer
  // needed.
  SetVisible(false);
  ResetSlideAnimation(true);
}

views::BubbleDialogDelegate* LensOverlayHomeworkPageActionIconView::GetBubble()
    const {
  return nullptr;
}

const gfx::VectorIcon& LensOverlayHomeworkPageActionIconView::GetVectorIcon()
    const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return vector_icons::kSearchChromeRefreshIcon;
#endif
}

void LensOverlayHomeworkPageActionIconView::
    ExecuteWithKeyboardSourceForTesting() {
  CHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_KEYBOARD);
}

BEGIN_METADATA(LensOverlayHomeworkPageActionIconView)
END_METADATA

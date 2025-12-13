// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metrics.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace {

// TODO(crbug.com/382494946): Similar bespoke checks are used throughout the
// codebase, this approach is taken from BookmarkTabHelper. This should be
// factored out as a common util and other callsites converted to use this.
bool IsNewTabPage(content::WebContents* const web_contents) {
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
  return NewTabUI::IsNewTab(url) || NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         search::NavEntryIsInstantNTP(web_contents, entry) ||
         search::IsSplitViewNewTabPage(url);
}

}  // namespace

LensOverlayPageActionIconView::LensOverlayPageActionIconView(
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "LensOverlay"),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);
  SetUpForInOutAnimation();

  SetProperty(views::kElementIdentifierKey,
              kLensOverlayPageActionIconElementId);

  if (!lens::features::IsOmniboxEntrypointAlwaysVisible()) {
    SetLabel(
        l10n_util::GetStringUTF16(IDS_CONTENT_LENS_OVERLAY_ENTRYPOINT_LABEL));
    SetUseTonalColorsWhenExpanded(true);
    SetBackgroundVisibility(BackgroundVisibility::kAlways);
  }

  // The accessible name should show the full text, independent of the what the
  // label text is set to.
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_LENS_OVERLAY),
      ax::mojom::NameFrom::kAttribute);
}

LensOverlayPageActionIconView::~LensOverlayPageActionIconView() = default;

bool LensOverlayPageActionIconView::ShouldShowLabel() const {
  // The persistent entrypoint should never show a label.
  return !lens::features::IsOmniboxEntrypointAlwaysVisible();
}

void LensOverlayPageActionIconView::UpdateImpl() {
  // There are 5 reasons why the lens page action may be hidden.
  // (1) It may be hidden by pref.
  bool enabled_by_pref = browser_->profile()->GetPrefs()->GetBoolean(
      omnibox::kShowGoogleLensShortcut);

  // (2) It will always be hidden if the location bar is not focused, or if a
  // feature flag has been set.
  bool location_bar_has_focus = false;
  if (BrowserView* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_);
      browser_view && browser_view->GetLocationBarView()) {
    if (const views::FocusManager* const focus_manager = GetFocusManager()) {
      location_bar_has_focus = browser_view->GetLocationBarView()->Contains(
          focus_manager->GetFocusedView());
    }
  }
  if (lens::features::IsOmniboxEntrypointAlwaysVisible()) {
    location_bar_has_focus = true;
  }

  // (3) It is always hidden in the NTP.
  // The overlay is unavailable on the NTP as it is unlikely to be useful to
  // users on the page, it would also appear immediately when a new tab or
  // window is created due to focus immediately jumping into the location bar.
  // `tab` is nullptr during construction of class Browser, during LocationBar
  // construction.
  auto* tab = browser_->GetActiveTabInterface();
  const bool is_ntp = tab && IsNewTabPage(tab->GetContents());

  // (4) The broader lens feature may be disabled for a number of reasons.
  // `controller` is nullptr during construction of class Browser, during
  // LocationBar construction.
  auto* controller =
      browser_->GetFeatures().lens_overlay_entry_point_controller();
  const bool is_broader_feature_enabled =
      controller && controller->AreVisible();

  // (5) The "AIM page action" feature in the Omnibox is enabled.
  // Since the on-focus behavior of the Lens overlay entrypoint and the AIM
  // page action could conflict, the Lens overlay entrypoint should be
  // suppressed when the "AIM page action" feature is enabled.
  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(browser_->profile());
  const bool is_aim_page_action_enabled =
      OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(aim_eligibility_service);

  const bool should_show_lens_overlay =
      enabled_by_pref && location_bar_has_focus && !is_ntp &&
      is_broader_feature_enabled && !is_aim_page_action_enabled;
  SetVisible(should_show_lens_overlay);
  ResetSlideAnimation(true);

  // TODO(pbos): Investigate why this call seems to be required to pick up that
  // this should still be painted in an expanded state. I.e. without this call
  // the last call to IconLabelBubbleView::UpdateBackground() seems to think
  // that the label isn't showing / shouldn't paint over a solid background.
  UpdateBackground();

  if (update_callback_for_testing_) {
    std::move(update_callback_for_testing_).Run();
  }
}

void LensOverlayPageActionIconView::OnExecuting(
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

  lens::RecordAmbientSearchQuery(
      lens::AmbientSearchEntryPoint::LENS_OVERLAY_LOCATION_BAR);
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kOmnibox);
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      GetWebContents()->GetBrowserContext(), lens::features::kLensOverlay);
}

views::BubbleDialogDelegate* LensOverlayPageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& LensOverlayPageActionIconView::GetVectorIcon() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return vector_icons::kSearchChromeRefreshIcon;
#endif
}


BEGIN_METADATA(LensOverlayPageActionIconView)
END_METADATA

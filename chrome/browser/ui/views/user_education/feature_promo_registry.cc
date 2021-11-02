// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"

#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/read_later/read_later_button.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

namespace {

// Functions to get an anchor view for an IPH should go here.

// kIPHDesktopPwaInstallFeature:
views::View* GetDesktopPwaInstallView(BrowserView* browser_view) {
  return browser_view->toolbar()
      ->location_bar()
      ->page_action_icon_controller()
      ->GetIconView(PageActionIconType::kPwaInstall);
}

// kIPHUpdatedConnectionSecurityIndicatorsFeature:
views::View* GetLocationIconView(BrowserView* browser_view) {
  return browser_view->toolbar()->location_bar()->location_icon_view();
}

// kIPHDesktopTabGroupsNewGroupFeature:
views::View* GetTabGroupsAnchorView(BrowserView* browser_view) {
  constexpr int kPreferredAnchorTab = 2;
  return browser_view->tabstrip()->GetTabViewForPromoAnchor(
      kPreferredAnchorTab);
}

// kIPHLiveCaptionFeature:
views::View* GetMediaButton(BrowserView* browser_view) {
  return browser_view->toolbar()->media_button();
}

// kIPHReadingListDiscoveryFeature:
views::View* GetReadingListButton(BrowserView* browser_view) {
  if (browser_view->right_aligned_side_panel())
    return browser_view->toolbar()->read_later_button();

  if (browser_view->bookmark_bar()->read_later_button()->IsDrawn())
    return browser_view->bookmark_bar()->read_later_button();

  return nullptr;
}

// kIPHReadingListEntryPointFeature:
views::View* GetReadingListStarView(BrowserView* browser_view) {
  return browser_view->toolbar()
      ->location_bar()
      ->page_action_icon_controller()
      ->GetIconView(PageActionIconType::kBookmarkStar);
}

// kIPHReadingListInSidePanelFeature:
views::View* GetSidePanelButton(BrowserView* browser_view) {
  return browser_view->toolbar()->read_later_button();
}

// kIPHReopenTabFeature:
views::View* GetAppMenuButton(BrowserView* browser_view) {
  return browser_view->toolbar()->app_menu_button();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// kIPHProfileSwitchFeature:
views::View* GetAvatarToolbarButton(BrowserView* browser_view) {
  return browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
// kIPHSideSearchFeature:
views::View* GetSideSearchButton(BrowserView* browser_view) {
  return browser_view->toolbar()->left_side_panel_button();
}
#endif

// kIPHTabSearchFeature:
views::View* GetTabSearchButton(BrowserView* browser_view) {
  auto* tab_search_host = browser_view->GetTabSearchBubbleHost();
  return tab_search_host ? tab_search_host->button() : nullptr;
}

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
// kIPHWebUITabStripFeature:
views::View* GetWebUITabStripAnchorView(BrowserView* browser_view) {
  WebUITabStripContainerView* const webui_tab_strip =
      browser_view->webui_tab_strip();
  if (!webui_tab_strip)
    return nullptr;
  return webui_tab_strip->tab_counter();
}
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

// kIPHDesktopSharedHighlightingFeature:
views::View* GetToolbarView(BrowserView* browser_view) {
  return browser_view->toolbar();
}

}  // namespace

FeaturePromoRegistry::FeaturePromoRegistry() {
  RegisterKnownFeatures();
}

FeaturePromoRegistry::~FeaturePromoRegistry() = default;

// static
FeaturePromoRegistry* FeaturePromoRegistry::GetInstance() {
  static base::NoDestructor<FeaturePromoRegistry> instance;
  return instance.get();
}

absl::optional<std::pair<FeaturePromoBubbleParams, views::View*>>
FeaturePromoRegistry::GetParamsForFeature(const base::Feature& iph_feature,
                                          BrowserView* browser_view) {
  auto data_it = feature_promo_data_.find(&iph_feature);
  DCHECK(data_it != feature_promo_data_.end());

  views::View* const anchor_view =
      data_it->second.get_anchor_view_callback.Run(browser_view);
  if (!anchor_view)
    return absl::nullopt;

  FeaturePromoBubbleParams params = data_it->second.params;

  if (params.feature_command_id) {
    // Only one of the two should be specified.
    DCHECK(!params.feature_accelerator);

    int command_id = params.feature_command_id.value();
    params.feature_command_id.reset();

    // Get the actual accelerator from |browser_view|.
    ui::Accelerator accelerator;
    if (browser_view->GetAccelerator(command_id, &accelerator))
      params.feature_accelerator = accelerator;
  }

  return std::make_pair(params, anchor_view);
}

void FeaturePromoRegistry::RegisterFeature(
    const base::Feature& iph_feature,
    const FeaturePromoBubbleParams& params,
    GetAnchorViewCallback get_anchor_view_callback) {
  FeaturePromoData data;
  data.params = params;
  data.get_anchor_view_callback = std::move(get_anchor_view_callback);
  feature_promo_data_.emplace(&iph_feature, std::move(data));
}

void FeaturePromoRegistry::ClearFeaturesForTesting() {
  feature_promo_data_.clear();
}

void FeaturePromoRegistry::ReinitializeForTesting() {
  ClearFeaturesForTesting();
  RegisterKnownFeatures();
}

void FeaturePromoRegistry::RegisterKnownFeatures() {
  {
    // kIPHDesktopPwaInstallFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_DESKTOP_PWA_INSTALL_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHDesktopPwaInstallFeature, params,
                    base::BindRepeating(GetDesktopPwaInstallView));
  }

  {
    // kIPHUpdatedConnectionSecurityIndicatorsFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier =
        IDS_UPDATED_CONNECTION_SECURITY_INDICATORS_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_LEFT;

    RegisterFeature(
        feature_engagement::kIPHUpdatedConnectionSecurityIndicatorsFeature,
        params, base::BindRepeating(GetLocationIconView));
  }

  {
    // kIPHDesktopTabGroupsNewGroupFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_TAB_GROUPS_NEW_GROUP_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_CENTER;

    // Turn on IPH Snooze for Tab Group.
    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHDesktopSnoozeFeature)) {
      params.focus_on_create = true;
      params.allow_snooze = true;
    }

    RegisterFeature(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
                    params, base::BindRepeating(GetTabGroupsAnchorView));
  }

  {
    // kIPHLiveCaptionFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_LIVE_CAPTION_PROMO;
    params.screenreader_string_specifier = IDS_LIVE_CAPTION_PROMO_SCREENREADER;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHLiveCaptionFeature, params,
                    base::BindRepeating(GetMediaButton));
  }

  {
    // kIPHGMCCastStartStopFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier =
        IDS_GLOBAL_MEDIA_CONTROLS_CONTROL_CAST_SESSIONS_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;
    RegisterFeature(feature_engagement::kIPHGMCCastStartStopFeature, params,
                    base::BindRepeating(GetMediaButton));
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  {
    // kIPHSwitchProfileFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_PROFILE_SWITCH_PROMO;
    params.screenreader_string_specifier =
        IDS_PROFILE_SWITCH_PROMO_SCREENREADER;
    params.feature_command_id = IDC_SHOW_AVATAR_MENU;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHProfileSwitchFeature, params,
                    base::BindRepeating(GetAvatarToolbarButton));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  {
    // kReadingListDiscoveryFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_READING_LIST_DISCOVERY_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHReadingListDiscoveryFeature, params,
                    base::BindRepeating(GetReadingListButton));
  }

  {
    // kIPHReadingListEntryPointFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_READING_LIST_ENTRY_POINT_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    // Turn on IPH Snooze for Read Later entry point.
    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHDesktopSnoozeFeature)) {
      params.focus_on_create = true;
      params.allow_snooze = true;
    }

    RegisterFeature(feature_engagement::kIPHReadingListEntryPointFeature,
                    params, base::BindRepeating(GetReadingListStarView));
  }

  {
    // kIPHReadingListInSidePanelFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_READING_LIST_IN_SIDE_PANEL_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHReadingListInSidePanelFeature,
                    params, base::BindRepeating(GetSidePanelButton));
  }

  {
    // kIPHReopenTabFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_REOPEN_TAB_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;
    params.feature_command_id = IDC_RESTORE_TAB;

    RegisterFeature(feature_engagement::kIPHReopenTabFeature, params,
                    base::BindRepeating(GetAppMenuButton));
  }

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
  {
    // kIPHSideSearchFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_SIDE_SEARCH_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_LEFT;

    RegisterFeature(feature_engagement::kIPHSideSearchFeature, params,
                    base::BindRepeating(GetSideSearchButton));
  }
#endif

  {
    // kIPHTabSearchFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_TAB_SEARCH_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHTabSearchFeature, params,
                    base::BindRepeating(GetTabSearchButton));
  }

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  {
    // kIPHWebUITabStripFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_WEBUI_TAB_STRIP_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHWebUITabStripFeature, params,
                    base::BindRepeating(GetWebUITabStripAnchorView));
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  {
    // kIPHDesktopSharedHighlightingFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_SHARED_HIGHLIGHTING_PROMO;
    params.arrow = FeaturePromoBubbleParams::Arrow::NONE;

    RegisterFeature(feature_engagement::kIPHDesktopSharedHighlightingFeature,
                    params, base::BindRepeating(GetToolbarView));
  }
}

FeaturePromoRegistry::FeaturePromoData::FeaturePromoData() = default;
FeaturePromoRegistry::FeaturePromoData::FeaturePromoData(FeaturePromoData&&) =
    default;
FeaturePromoRegistry::FeaturePromoData::~FeaturePromoData() = default;

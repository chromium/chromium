// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"

#include "base/no_destructor.h"
#include "base/optional.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/read_later/read_later_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
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
  if (browser_view->side_panel())
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

base::Optional<FeaturePromoBubbleParams>
FeaturePromoRegistry::GetParamsForFeature(const base::Feature& iph_feature,
                                          BrowserView* browser_view) {
  auto data_it = feature_promo_data_.find(&iph_feature);
  DCHECK(data_it != feature_promo_data_.end());

  views::View* const anchor_view =
      data_it->second.get_anchor_view_callback.Run(browser_view);
  if (!anchor_view)
    return base::nullopt;

  FeaturePromoBubbleParams params = data_it->second.params;
  params.anchor_view = anchor_view;

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

  return params;
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
    params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHDesktopPwaInstallFeature, params,
                    base::BindRepeating(GetDesktopPwaInstallView));
  }

  {
    // kIPHDesktopTabGroupsNewGroupFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_TAB_GROUPS_NEW_GROUP_PROMO;
    params.arrow = views::BubbleBorder::TOP_LEFT;

    // Turn on IPH Snooze for Tab Group.
    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHDesktopSnoozeFeature)) {
      params.allow_focus = true;
      params.persist_on_blur = true;
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
    params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHLiveCaptionFeature, params,
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
    params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHProfileSwitchFeature, params,
                    base::BindRepeating(GetAvatarToolbarButton));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  {
    // kReadingListDiscoveryFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_READING_LIST_DISCOVERY_PROMO;
    params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHReadingListDiscoveryFeature, params,
                    base::BindRepeating(GetReadingListButton));
  }

  {
    // kIPHReadingListEntryPointFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_READING_LIST_ENTRY_POINT_PROMO;
    params.arrow = views::BubbleBorder::TOP_RIGHT;

    // Turn on IPH Snooze for Read Later entry point.
    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHDesktopSnoozeFeature)) {
      params.allow_focus = true;
      params.persist_on_blur = true;
      params.allow_snooze = true;
    }

    RegisterFeature(feature_engagement::kIPHReadingListEntryPointFeature,
                    params, base::BindRepeating(GetReadingListStarView));
  }

  {
    // kIPHReopenTabFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_REOPEN_TAB_PROMO;
    params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;
    params.feature_command_id = IDC_RESTORE_TAB;

    RegisterFeature(feature_engagement::kIPHReopenTabFeature, params,
                    base::BindRepeating(GetAppMenuButton));
  }

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  {
    // kIPHWebUITabStripFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_WEBUI_TAB_STRIP_PROMO;
    params.arrow = views::BubbleBorder::TOP_RIGHT;

    RegisterFeature(feature_engagement::kIPHWebUITabStripFeature, params,
                    base::BindRepeating(GetWebUITabStripAnchorView));
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

FeaturePromoRegistry::FeaturePromoData::FeaturePromoData() = default;
FeaturePromoRegistry::FeaturePromoData::FeaturePromoData(FeaturePromoData&&) =
    default;
FeaturePromoRegistry::FeaturePromoData::~FeaturePromoData() = default;

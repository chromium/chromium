// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
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
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

namespace {

// kIPHDesktopTabGroupsNewGroupFeature:
ui::TrackedElement* GetTabGroupsAnchorView(
    const ui::ElementTracker::ElementList& elements) {
  if (elements.empty())
    return nullptr;
  TabStrip* const tab_strip = static_cast<TabStrip*>(
      elements[0]->AsA<views::TrackedElementViews>()->view());
  constexpr int kPreferredAnchorTab = 2;
  views::View* const tab =
      tab_strip->GetTabViewForPromoAnchor(kPreferredAnchorTab);
  return tab ? views::ElementTrackerViews::GetInstance()->GetElementForView(
                   tab, true)
             : nullptr;
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

const FeaturePromoSpecification* FeaturePromoRegistry::GetParamsForFeature(
    const base::Feature& iph_feature) {
  auto data_it = feature_promo_data_.find(&iph_feature);
  DCHECK(data_it != feature_promo_data_.end());
  return &data_it->second;
}

void FeaturePromoRegistry::RegisterFeature(FeaturePromoSpecification spec) {
  const base::Feature* const iph_feature = spec.feature();
  CHECK(iph_feature);
  const auto result = feature_promo_data_.emplace(iph_feature, std::move(spec));
  DCHECK(result.second) << "Duplicate IPH feature registered: "
                        << iph_feature->name;
}

void FeaturePromoRegistry::ClearFeaturesForTesting() {
  feature_promo_data_.clear();
}

void FeaturePromoRegistry::ReinitializeForTesting() {
  ClearFeaturesForTesting();
  RegisterKnownFeatures();
}

void FeaturePromoRegistry::RegisterKnownFeatures() {
  // kIPHDesktopPwaInstallFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHDesktopPwaInstallFeature, kInstallPwaElementId,
      IDS_DESKTOP_PWA_INSTALL_PROMO));

  // kIPHUpdatedConnectionSecurityIndicatorsFeature:
  RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHUpdatedConnectionSecurityIndicatorsFeature,
          kLocationIconElementId,
          IDS_UPDATED_CONNECTION_SECURITY_INDICATORS_PROMO)
          .SetBubbleArrow(FeaturePromoSpecification::BubbleArrow::kTopLeft)));

  // kIPHDesktopTabGroupsNewGroupFeature:
  RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
          kTabStripElementId, IDS_TAB_GROUPS_NEW_GROUP_PROMO)
          .SetBubbleArrow(FeaturePromoSpecification::BubbleArrow::kTopCenter)
          .SetAnchorElementFilter(
              base::BindRepeating(&GetTabGroupsAnchorView))));

  // kIPHLiveCaptionFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHLiveCaptionFeature, kMediaButtonElementId,
      IDS_LIVE_CAPTION_PROMO, IDS_LIVE_CAPTION_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHGMCCastStartStopFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHGMCCastStartStopFeature, kMediaButtonElementId,
      IDS_GLOBAL_MEDIA_CONTROLS_CONTROL_CAST_SESSIONS_PROMO));

  // kIPHSwitchProfileFeature:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHProfileSwitchFeature, kAvatarButtonElementId,
      IDS_PROFILE_SWITCH_PROMO, IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // kReadingListDiscoveryFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListDiscoveryFeature,
      kReadLaterButtonElementId, IDS_READING_LIST_DISCOVERY_PROMO));

  // kIPHReadingListEntryPointFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHReadingListInSidePanelFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      kReadLaterButtonElementId, IDS_READING_LIST_IN_SIDE_PANEL_PROMO));

  // kIPHReopenTabFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHReopenTabFeature, kAppMenuButtonElementId,
      IDS_REOPEN_TAB_PROMO, IDS_REOPEN_TAB_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_RESTORE_TAB)));

  // kIPHSideSearchFeature:
#if BUILDFLAG(ENABLE_SIDE_SEARCH)
  RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHSideSearchFeature,
          kSideSearchButtonElementId, IDS_SIDE_SEARCH_PROMO)
          .SetBubbleArrow(FeaturePromoSpecification::BubbleArrow::kTopLeft)));
#endif

  // kIPHTabSearchFeature:
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHTabSearchFeature, kTabSearchButtonElementId,
      IDS_TAB_SEARCH_PROMO));

  // kIPHWebUITabStripFeature:
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHWebUITabStripFeature, kTabCounterButtonElementId,
      IDS_WEBUI_TAB_STRIP_PROMO));
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // kIPHDesktopSharedHighlightingFeature:
  RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHDesktopSharedHighlightingFeature,
          kTopContainerElementId, IDS_SHARED_HIGHLIGHTING_PROMO)
          .SetBubbleArrow(FeaturePromoSpecification::BubbleArrow::kNone)));
}

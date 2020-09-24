// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_registry.h"

#include "base/no_destructor.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

namespace {

// Functions to get an anchor view for an IPH should go here.

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
    // kIPHDesktopTabGroupsNewGroupFeature:
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_TAB_GROUPS_NEW_GROUP_PROMO;
    params.arrow = views::BubbleBorder::TOP_LEFT;

    // Turn on IPH Snooze only for Tab Group.
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

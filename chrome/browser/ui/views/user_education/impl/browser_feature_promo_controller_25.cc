// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_25.h"

#include <string>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

BrowserFeaturePromoController25::BrowserFeaturePromoController25(
    BrowserView* browser_view,
    feature_engagement::Tracker* feature_engagement_tracker,
    user_education::FeaturePromoRegistry* registry,
    user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
    user_education::UserEducationStorageService* storage_service,
    user_education::FeaturePromoSessionPolicy* session_policy,
    user_education::TutorialService* tutorial_service,
    user_education::ProductMessagingController* messaging_controller)
    : FeaturePromoController25(feature_engagement_tracker,
                               registry,
                               help_bubble_registry,
                               storage_service,
                               session_policy,
                               tutorial_service,
                               messaging_controller),
      browser_view_(browser_view) {}

BrowserFeaturePromoController25::~BrowserFeaturePromoController25() = default;

ui::ElementContext BrowserFeaturePromoController25::GetAnchorContext() const {
  return views::ElementTrackerViews::GetContextForView(browser_view_);
}

const ui::AcceleratorProvider*
BrowserFeaturePromoController25::GetAcceleratorProvider() const {
  return browser_view_;
}

std::u16string BrowserFeaturePromoController25::GetTutorialScreenReaderHint()
    const {
  return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
      browser_view_);
}

std::u16string
BrowserFeaturePromoController25::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element) const {
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, browser_view_, anchor_element);
}

std::u16string BrowserFeaturePromoController25::GetBodyIconAltText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}

const base::Feature*
BrowserFeaturePromoController25::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char*
BrowserFeaturePromoController25::GetScreenReaderPromptPromoEventName() const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

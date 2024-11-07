// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_20.h"

#include <string>

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

BrowserFeaturePromoController20::BrowserFeaturePromoController20(
    BrowserView* browser_view,
    feature_engagement::Tracker* feature_engagement_tracker,
    user_education::FeaturePromoRegistry* registry,
    user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
    user_education::UserEducationStorageService* storage_service,
    user_education::FeaturePromoSessionPolicy* session_policy,
    user_education::TutorialService* tutorial_service,
    user_education::ProductMessagingController* messaging_controller)
    : FeaturePromoController20(feature_engagement_tracker,
                               registry,
                               help_bubble_registry,
                               storage_service,
                               session_policy,
                               tutorial_service,
                               messaging_controller),
      browser_view_(browser_view) {}

BrowserFeaturePromoController20::~BrowserFeaturePromoController20() = default;

ui::ElementContext BrowserFeaturePromoController20::GetAnchorContext() const {
  return views::ElementTrackerViews::GetContextForView(browser_view_);
}

bool BrowserFeaturePromoController20::CanShowPromoForElement(
    ui::TrackedElement* anchor_element) const {
  // Trying to show an IPH while the browser is closing can cause problems;
  // see crbug.com/346461762 for an example.
  if (browser_view_->browser()->IsBrowserClosing()) {
    return false;
  }

  auto* const profile = browser_view_->GetProfile();

  // Turn off IPH while a required privacy interstitial is visible or pending.
  // TODO(dfried): with Desktop User Education 2.0, filtering of IPH may need to
  // be more nuanced; also a contention scheme between required popups may be
  // required. See go/desktop-user-education-2.0 for details.
  auto* const privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (privacy_sandbox_service &&
      privacy_sandbox_service->GetRequiredPromptType(
          PrivacySandboxService::SurfaceType::kDesktop) !=
          PrivacySandboxService::PromptType::kNone) {
    return false;
  }

  // Turn off IPH while a required search engine choice dialog is visible or
  // pending.
  Browser& browser = *browser_view_->browser();
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(browser.profile());
  if (search_engine_choice_dialog_service &&
      search_engine_choice_dialog_service->HasPendingDialog(browser)) {
    return false;
  }

  // Don't show IPH if the toolbar is collapsed in Responsive Mode/the overflow
  // button is visible.
  //
  // TODO(dfried): make this more specific for certain types of promos. For
  // example, a toast IPH anchored to an element that's actually visible should
  // be fine, but we might want to avoid Tutorial and Custom Action IPH even if
  // the initial anchor is present.
  if (base::FeatureList::IsEnabled(features::kResponsiveToolbar)) {
    if (const auto* const controller =
            browser_view_->toolbar()->toolbar_controller()) {
      if (controller->InOverflowMode()) {
        return false;
      }
    }
  }

  // Don't show IPH if the anchor view is in an inactive window.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  auto* const anchor_widget = anchor_view ? anchor_view->view()->GetWidget()
                                          : browser_view_->GetWidget();
  if (!anchor_widget) {
    return false;
  }

  if (!active_window_check_blocked() && !anchor_widget->ShouldPaintAsActive()) {
    return false;
  }

  return true;
}

const ui::AcceleratorProvider*
BrowserFeaturePromoController20::GetAcceleratorProvider() const {
  return browser_view_;
}

std::u16string BrowserFeaturePromoController20::GetTutorialScreenReaderHint()
    const {
  return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
      browser_view_);
}

std::u16string
BrowserFeaturePromoController20::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element) const {
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, browser_view_, anchor_element);
}

std::u16string BrowserFeaturePromoController20::GetBodyIconAltText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}

const base::Feature*
BrowserFeaturePromoController20::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char*
BrowserFeaturePromoController20::GetScreenReaderPromptPromoEventName() const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

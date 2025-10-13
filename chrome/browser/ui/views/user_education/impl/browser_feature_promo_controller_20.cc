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
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

BrowserFeaturePromoController20::~BrowserFeaturePromoController20() = default;

user_education::FeaturePromoResult
BrowserFeaturePromoController20::CanShowPromoForElement(
    ui::TrackedElement* anchor_element,
    const user_education::UserEducationContextPtr& context) const {
  auto* const browser_context = context->AsA<BrowserUserEducationContext>();
  CHECK(browser_context && browser_context->IsValid());
  auto* const browser_view = &browser_context->GetBrowserView();

  // Trying to show an IPH while the browser is closing can cause problems;
  // see https://crbug.com/346461762 for an example. This can also crash
  // unit_tests that use a BrowserWindow but not a browser, so also check if
  // the browser view's widget is closing.
  if (browser_view->browser()->capabilities()->IsAttemptingToCloseBrowser() ||
      browser_view->GetWidget()->IsClosed()) {
    return user_education::FeaturePromoResult::kBlockedByContext;
  }

  auto* const profile = browser_view->GetProfile();

  // Turn off IPH while a required privacy interstitial is visible or pending.
  auto* const privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (privacy_sandbox_service &&
      privacy_sandbox_service->GetRequiredPromptType(
          PrivacySandboxService::SurfaceType::kDesktop) !=
          PrivacySandboxService::PromptType::kNone) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }

  Browser& browser = *browser_view->browser();

  // Turn off IPH while the browser is showing fullscreen content (like a
  // video). See https://crbug.com/411475424.
  auto* const fullscreen_controller =
      browser.GetFeatures().exclusive_access_manager()->fullscreen_controller();
  if (fullscreen_controller->IsWindowFullscreenForTabOrPending() ||
      fullscreen_controller->IsExtensionFullscreenOrPending()) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }

  // Turn off IPH while a required search engine choice dialog is visible or
  // pending.
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(browser.profile());
  if (search_engine_choice_dialog_service &&
      search_engine_choice_dialog_service->HasPendingDialog(browser)) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }

  // Don't show IPH if the toolbar is collapsed in Responsive Mode/the overflow
  // button is visible.
  if (const auto* const controller =
          browser_view->toolbar()->toolbar_controller()) {
    if (controller->InOverflowMode()) {
      return user_education::FeaturePromoResult::kWindowTooSmall;
    }
  }

  // Don't show IPH if the anchor view is in an inactive window.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  auto* const anchor_widget = anchor_view ? anchor_view->view()->GetWidget()
                                          : browser_view->GetWidget();
  if (!anchor_widget) {
    return user_education::FeaturePromoResult::kAnchorNotVisible;
  }

  if (!active_window_check_blocked() && !anchor_widget->ShouldPaintAsActive()) {
    return user_education::FeaturePromoResult::kAnchorSurfaceNotActive;
  }

  return FeaturePromoController20::CanShowPromoForElement(anchor_element,
                                                          context);
}

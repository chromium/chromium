// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kOmniboxNotOpenPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kToolbarNotCollapsedPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kBrowserNotClosingPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kNoCriticalNoticeShowingPrecondition);

WindowActivePrecondition::WindowActivePrecondition()
    : FeaturePromoPreconditionBase(kWindowActivePrecondition,
                                   "Target window is active") {}
WindowActivePrecondition::~WindowActivePrecondition() = default;

user_education::FeaturePromoResult WindowActivePrecondition::CheckPrecondition(
    ComputedData& data) const {
  auto& element_ref =
      data.Get(user_education::AnchorElementPrecondition::kAnchorElement);
  views::Widget* widget = nullptr;
  if (auto* const view_el = element_ref.get_as<views::TrackedElementViews>()) {
    widget = view_el->view()->GetWidget();
  } else if (auto* web_el =
                 element_ref.get_as<user_education::TrackedElementWebUI>()) {
    auto* const contents = web_el->handler()->GetWebContents();
    widget = views::Widget::GetWidgetForNativeWindow(
        contents->GetTopLevelNativeWindow());
  }
  return widget && widget->GetPrimaryWindowWidget()->ShouldPaintAsActive()
             ? user_education::FeaturePromoResult::Success()
             : user_education::FeaturePromoResult::kBlockedByUi;
}

OmniboxNotOpenPrecondition::OmniboxNotOpenPrecondition(
    const BrowserView& browser_view)
    : FeaturePromoPreconditionBase(kOmniboxNotOpenPrecondition,
                                   "Omnibox is not open"),
      browser_view_(browser_view) {}
OmniboxNotOpenPrecondition::~OmniboxNotOpenPrecondition() = default;

user_education::FeaturePromoResult
OmniboxNotOpenPrecondition::CheckPrecondition(ComputedData&) const {
  const OmniboxPopupView* const popup = browser_view_->GetLocationBarView()
                                            ->GetOmniboxView()
                                            ->model()
                                            ->get_popup_view();
  return popup && popup->IsOpen()
             ? user_education::FeaturePromoResult::kBlockedByUi
             : user_education::FeaturePromoResult::Success();
}

ToolbarNotCollapsedPrecondition::ToolbarNotCollapsedPrecondition(
    BrowserView& browser_view)
    : FeaturePromoPreconditionBase(kToolbarNotCollapsedPrecondition,
                                   "Toolbar is not collapsed"),
      browser_view_(browser_view) {}
ToolbarNotCollapsedPrecondition::~ToolbarNotCollapsedPrecondition() = default;

user_education::FeaturePromoResult
ToolbarNotCollapsedPrecondition::CheckPrecondition(ComputedData&) const {
  if (const auto* const controller =
          browser_view_->toolbar()->toolbar_controller()) {
    if (controller->InOverflowMode()) {
      return user_education::FeaturePromoResult::kBlockedByUi;
    }
  }
  return user_education::FeaturePromoResult::Success();
}

BrowserNotClosingPrecondition::BrowserNotClosingPrecondition(
    BrowserView& browser_view)
    : FeaturePromoPreconditionBase(kBrowserNotClosingPrecondition,
                                   "Browser is not closing"),
      browser_view_(browser_view) {}
BrowserNotClosingPrecondition::~BrowserNotClosingPrecondition() = default;

user_education::FeaturePromoResult
BrowserNotClosingPrecondition::CheckPrecondition(ComputedData&) const {
  if (browser_view_->browser()->IsBrowserClosing() ||
      browser_view_->GetWidget()->IsClosed()) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }
  return user_education::FeaturePromoResult::Success();
}

NoCriticalNoticeShowingPrecondition::NoCriticalNoticeShowingPrecondition(
    BrowserView& browser_view)
    : FeaturePromoPreconditionBase(kNoCriticalNoticeShowingPrecondition,
                                   "No critical notice is showing"),
      browser_view_(browser_view) {}
NoCriticalNoticeShowingPrecondition::~NoCriticalNoticeShowingPrecondition() =
    default;

user_education::FeaturePromoResult
NoCriticalNoticeShowingPrecondition::CheckPrecondition(ComputedData&) const {
  // Turn off IPH while a required privacy interstitial is visible or pending.
  auto* const privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser_view_->GetProfile());
  if (privacy_sandbox_service &&
      privacy_sandbox_service->GetRequiredPromptType(
          PrivacySandboxService::SurfaceType::kDesktop) !=
          PrivacySandboxService::PromptType::kNone) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }

  // Turn off IPH while a required search engine choice dialog is visible or
  // pending.
  SearchEngineChoiceDialogService* const search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          browser_view_->GetProfile());
  if (search_engine_choice_dialog_service &&
      search_engine_choice_dialog_service->HasPendingDialog(
          *browser_view_->browser())) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }

  return user_education::FeaturePromoResult::Success();
}

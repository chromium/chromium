// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"

#include "base/time/default_clock.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_help_bubble_webui_anchor.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/types/event_type.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kContentNotFullscreenPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kOmniboxNotOpenPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kToolbarNotCollapsedPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kBrowserNotClosingPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kNoCriticalNoticeShowingPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kUserNotActivePrecondition);

WindowActivePrecondition::WindowActivePrecondition()
    : FeaturePromoPreconditionBase(kWindowActivePrecondition,
                                   "Target window is active") {}
WindowActivePrecondition::~WindowActivePrecondition() = default;

user_education::FeaturePromoResult WindowActivePrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  if (user_education::FeaturePromoControllerCommon::
          active_window_check_blocked()) {
    return user_education::FeaturePromoResult::Success();
  }
  auto& element_ref =
      data[user_education::AnchorElementPrecondition::kAnchorElement];
  views::Widget* widget = nullptr;
  if (auto* const view_el = element_ref.get_as<views::TrackedElementViews>()) {
    widget = view_el->view()->GetWidget();
  } else if (auto* web_el =
                 element_ref.get_as<
                     user_education::TrackedElementHelpBubbleWebUIAnchor>()) {
    auto* const contents = web_el->handler()->GetWebContents();
    widget = views::Widget::GetWidgetForNativeWindow(
        contents->GetTopLevelNativeWindow());
  }
  if (widget) {
    // For some reason sometimes primary can be null;
    // see https://crbug.com/400921315
    if (auto* const primary = widget->GetPrimaryWindowWidget()) {
      widget = primary;
    }
  }
  return widget && widget->ShouldPaintAsActive()
             ? user_education::FeaturePromoResult::Success()
             : user_education::FeaturePromoResult::kAnchorSurfaceNotActive;
}

ContentNotFullscreenPrecondition::ContentNotFullscreenPrecondition(
    Browser& browser)
    : FeaturePromoPreconditionBase(kContentNotFullscreenPrecondition,
                                   "Content is not fullscreen"),
      browser_(browser) {}
ContentNotFullscreenPrecondition::~ContentNotFullscreenPrecondition() = default;

user_education::FeaturePromoResult
ContentNotFullscreenPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  auto* const fullscreen_controller = browser_->GetFeatures()
                                          .exclusive_access_manager()
                                          ->fullscreen_controller();
  if (fullscreen_controller->IsWindowFullscreenForTabOrPending() ||
      fullscreen_controller->IsExtensionFullscreenOrPending()) {
    return user_education::FeaturePromoResult::kBlockedByUi;
  }
  return user_education::FeaturePromoResult::Success();
}

OmniboxNotOpenPrecondition::OmniboxNotOpenPrecondition(
    const BrowserView& browser_view)
    : FeaturePromoPreconditionBase(kOmniboxNotOpenPrecondition,
                                   "Omnibox is not open"),
      browser_view_(browser_view) {}
OmniboxNotOpenPrecondition::~OmniboxNotOpenPrecondition() = default;

user_education::FeaturePromoResult
OmniboxNotOpenPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  return browser_view_->GetLocationBarView()
                 ->GetOmniboxController()
                 ->IsPopupOpen()
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
ToolbarNotCollapsedPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  if (const auto* const controller =
          browser_view_->toolbar()->toolbar_controller()) {
    if (controller->InOverflowMode()) {
      return user_education::FeaturePromoResult::kWindowTooSmall;
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
BrowserNotClosingPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  if (browser_view_->browser()->capabilities()->IsAttemptingToCloseBrowser() ||
      browser_view_->GetWidget()->IsClosed()) {
    return user_education::FeaturePromoResult::kBlockedByContext;
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
NoCriticalNoticeShowingPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
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

UserNotActivePrecondition::UserNotActivePrecondition(
    BrowserView& browser_view,
    const user_education::UserEducationTimeProvider& time_provider)
    : FeaturePromoPreconditionBase(kUserNotActivePrecondition,
                                   "The user is not actively sending input"),
      browser_view_(browser_view),
      time_provider_(time_provider) {
  if (browser_view.GetWidget()) {
    CreateEventMonitor();
  } else {
    browser_view_observation_.Observe(&browser_view);
  }
}

UserNotActivePrecondition::~UserNotActivePrecondition() = default;

void UserNotActivePrecondition::CreateEventMonitor() {
  // Note that null is a valid value for the second parameter here; if for
  // some reason there is no native window it simply falls back to
  // application-wide event-sniffing, which for this case is better than not
  // watching events at all.
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, browser_view_->GetWidget()->GetTopLevelWidget()->GetNativeWindow(),
      {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});
}

void UserNotActivePrecondition::OnEvent(const ui::Event& event) {
  // Delay heavyweight IPH for the prescribed amount of time.
  last_active_time_ = time_provider_->GetCurrentTime();
}

user_education::FeaturePromoResult UserNotActivePrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  // Only do check if min idle time is nonzero and positive; otherwise this is a
  // no-op. Explicitly verify this in case of non-monotonic clock weirdness.
  const auto min_idle_time =
      user_education::features::GetIdleTimeBeforeHeavyweightPromo();
  if (min_idle_time.is_positive()) {
    const auto elapsed = time_provider_->GetCurrentTime() - last_active_time_;
    return elapsed < min_idle_time
               ? user_education::FeaturePromoResult::kBlockedByUserActivity
               : user_education::FeaturePromoResult::Success();
  } else {
    return user_education::FeaturePromoResult::Success();
  }
}

void UserNotActivePrecondition::OnViewAddedToWidget(
    views::View* observed_view) {
  browser_view_observation_.Reset();
  CreateEventMonitor();
}

void UserNotActivePrecondition::OnViewIsDeleting(views::View* observed_view) {
  browser_view_observation_.Reset();
}

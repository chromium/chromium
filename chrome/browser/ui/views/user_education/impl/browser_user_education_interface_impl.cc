// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_user_education_interface_impl.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"

BrowserUserEducationInterfaceImpl::BrowserUserEducationInterfaceImpl(
    BrowserWindowInterface* browser)
    : BrowserUserEducationInterface(browser), profile_(browser->GetProfile()) {}
BrowserUserEducationInterfaceImpl::~BrowserUserEducationInterfaceImpl() {
  ClearQueuedPromos();
}

void BrowserUserEducationInterfaceImpl::Init(BrowserView* browser_view) {
  CHECK_EQ(State::kUninitialized, state_);
  state_ = State::kInitialized;

  // Only override the controller if the controller has not been overridden for
  // testing.
  if (!controller_) {
    // This returns a unique pointer to a `FeaturePromoControllerCommon`.
    controller_ = CreateUserEducationResources(browser_view);
  }

  if (!controller_) {
    ClearQueuedPromos(
        user_education::FeaturePromoResult::Failure::kBlockedByContext);
    return;
  }

  for (auto& params : queued_params_) {
    GetFeaturePromoControllerImpl()->MaybeShowStartupPromo(std::move(params));
  }
  queued_params_.clear();
}

void BrowserUserEducationInterfaceImpl::TearDown() {
  state_ = State::kTornDown;
  profile_ = nullptr;
  controller_.reset();
  ClearQueuedPromos();
}

UserEducationService*
BrowserUserEducationInterfaceImpl::GetUserEducationService() {
  return profile_ ? UserEducationServiceFactory::GetForBrowserContext(profile_)
                  : nullptr;
}

bool BrowserUserEducationInterfaceImpl::IsFeaturePromoQueued(
    const base::Feature& iph_feature) const {
  auto* const controller = GetFeaturePromoControllerImpl();
  return controller && controller->GetPromoStatus(iph_feature) ==
                           user_education::FeaturePromoStatus::kQueued;
}

bool BrowserUserEducationInterfaceImpl::IsFeaturePromoActive(
    const base::Feature& iph_feature) const {
  auto* const controller = GetFeaturePromoControllerImpl();
  return controller &&
         controller->IsPromoActive(
             iph_feature, user_education::FeaturePromoStatus::kContinued);
}

user_education::FeaturePromoResult
BrowserUserEducationInterfaceImpl::CanShowFeaturePromo(
    const base::Feature& iph_feature) const {
  if (state_ != State::kInitialized) {
    return user_education::FeaturePromoResult::kError;
  }

  if (auto* const controller = GetFeaturePromoControllerImpl()) {
    return controller->CanShowPromo(iph_feature);
  }
  return user_education::FeaturePromoResult::kBlockedByContext;
}

void BrowserUserEducationInterfaceImpl::MaybeShowFeaturePromo(
    user_education::FeaturePromoParams params) {
  // Trying to show a promo before the browser is initialized can result in a
  // failure to retrieve accelerators, which can cause issues for screen reader
  // users.
  if (state_ != State::kInitialized) {
    LOG(ERROR) << "Attempting to show IPH " << params.feature->name
               << (state_ == State::kUninitialized
                       ? " before browser initialization"
                       : " after browser shutdown")
               << "; IPH will not be shown.";
    user_education::FeaturePromoController::PostShowPromoResult(
        std::move(params.show_promo_result_callback),
        user_education::FeaturePromoResult::kError);
    return;
  }

  if (auto* const controller = GetFeaturePromoControllerImpl()) {
    controller->MaybeShowPromo(std::move(params));
    return;
  }

  user_education::FeaturePromoController::PostShowPromoResult(
      std::move(params.show_promo_result_callback),
      user_education::FeaturePromoResult::kBlockedByContext);
}

void BrowserUserEducationInterfaceImpl::MaybeShowStartupFeaturePromo(
    user_education::FeaturePromoParams params) {
  if (state_ == State::kUninitialized) {
    queued_params_.push_back(std::move(params));
    return;
  }

  if (state_ == State::kTornDown) {
    LOG(ERROR) << "Attempting to show IPH " << params.feature->name
               << " after browser shutdown; IPH will not be shown.";
    user_education::FeaturePromoController::PostShowPromoResult(
        std::move(params.show_promo_result_callback),
        user_education::FeaturePromoResult::kError);
    return;
  }

  if (auto* const controller = GetFeaturePromoControllerImpl()) {
    controller->MaybeShowStartupPromo(std::move(params));
    return;
  }

  user_education::FeaturePromoController::PostShowPromoResult(
      std::move(params.show_promo_result_callback),
      user_education::FeaturePromoResult::kBlockedByContext);
}

bool BrowserUserEducationInterfaceImpl::AbortFeaturePromo(
    const base::Feature& iph_feature) {
  auto* const controller = GetFeaturePromoControllerImpl();
  return controller &&
         controller->EndPromo(
             iph_feature, user_education::EndFeaturePromoReason::kAbortPromo);
}

user_education::FeaturePromoHandle
BrowserUserEducationInterfaceImpl::CloseFeaturePromoAndContinue(
    const base::Feature& iph_feature) {
  auto* const controller = GetFeaturePromoControllerImpl();
  if (!controller || controller->GetPromoStatus(iph_feature) !=
                         user_education::FeaturePromoStatus::kBubbleShowing) {
    return user_education::FeaturePromoHandle();
  }
  return controller->CloseBubbleAndContinuePromo(iph_feature);
}

bool BrowserUserEducationInterfaceImpl::NotifyFeaturePromoFeatureUsed(
    const base::Feature& feature,
    FeaturePromoFeatureUsedAction action) {
  auto* const controller = GetFeaturePromoControllerImpl();
  if (controller) {
    controller->NotifyFeatureUsedIfValid(feature);
    if (action == FeaturePromoFeatureUsedAction::kClosePromoIfPresent) {
      return controller->EndPromo(
          feature, user_education::EndFeaturePromoReason::kFeatureEngaged);
    }
  }
  return false;
}

void BrowserUserEducationInterfaceImpl::NotifyAdditionalConditionEvent(
    const char* event_name) {
  if (state_ == State::kInitialized) {
    if (auto* const tracker =
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile_)) {
      tracker->NotifyEvent(event_name);
    }
  }
}

user_education::DisplayNewBadge
BrowserUserEducationInterfaceImpl::MaybeShowNewBadgeFor(
    const base::Feature& feature) {
  auto* const service = GetUserEducationService();
  if (!service || !service->new_badge_controller()) {
    return user_education::DisplayNewBadge();
  }
  return service->new_badge_controller()->MaybeShowNewBadge(feature);
}

void BrowserUserEducationInterfaceImpl::NotifyNewBadgeFeatureUsed(
    const base::Feature& feature) {
  auto* const service = GetUserEducationService();
  if (service && service->new_badge_registry() &&
      service->new_badge_registry()->IsFeatureRegistered(feature)) {
    service->new_badge_controller()->NotifyFeatureUsedIfValid(feature);
  }
}

void BrowserUserEducationInterfaceImpl::SetFeaturePromoControllerForTesting(
    std::unique_ptr<user_education::FeaturePromoController> controller) {
  CHECK_NE(State::kTornDown, state_);
  if (state_ == State::kUninitialized) {
    CHECK(queued_params_.empty())
        << "Setting the controller from an uninitialized state is only allowed "
           "in unit tests, and only if no promos have been previously queued.";
    state_ = State::kInitialized;
  }
  controller_ = std::move(controller);
}

void BrowserUserEducationInterfaceImpl::ClearQueuedPromos(
    user_education::FeaturePromoResult::Failure failure) {
  for (auto& params : queued_params_) {
    user_education::FeaturePromoController::PostShowPromoResult(
        std::move(params.show_promo_result_callback), failure);
  }
  queued_params_.clear();
}

const user_education::FeaturePromoController*
BrowserUserEducationInterfaceImpl::GetFeaturePromoControllerImpl() const {
  return controller_.get();
}

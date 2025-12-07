// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_user_education_interface_impl.h"

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/user_education_storage_service.h"

BrowserUserEducationInterfaceImpl::BrowserUserEducationInterfaceImpl(
    BrowserWindowInterface* browser)
    : BrowserUserEducationInterface(browser), profile_(browser->GetProfile()) {}
BrowserUserEducationInterfaceImpl::~BrowserUserEducationInterfaceImpl() {
  ClearQueuedPromos();
}

void BrowserUserEducationInterfaceImpl::Init(BrowserView* browser_view) {
  CHECK_EQ(State::kUninitialized, state_);
  state_ = State::kInitializationPending;

  // Create the context.
  if (auto* const interface = GetUserEducationService()) {
    user_education_context_ = base::MakeRefCounted<BrowserUserEducationContext>(
        *browser_view, interface->user_education_storage_service());
  }

  // Need to wait for all of browser setup to complete before attempting to
  // launch startup promos. Since this method is called during feature/service
  // creation and before the browser view finishes attaching to the widget, it's
  // best to delay a minimum of one frame before attempting to queue promos.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUserEducationInterfaceImpl::CompleteInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserUserEducationInterfaceImpl::CompleteInitialization() {
  state_ = State::kInitialized;

  auto* const controller = GetFeaturePromoController();
  if (!controller) {
    ClearQueuedPromos(
        user_education::FeaturePromoResult::Failure::kBlockedByContext);
    return;
  }

  CHECK(user_education_context_)
      << "Should not have a controller but no service.";

  for (auto& params : queued_params_) {
    controller->MaybeShowStartupPromo(std::move(params),
                                      user_education_context_);
  }
  queued_params_.clear();
}

void BrowserUserEducationInterfaceImpl::TearDown() {
  if (user_education_context_) {
    static_cast<BrowserUserEducationContext*>(user_education_context_.get())
        ->Invalidate(base::PassKey<BrowserUserEducationInterfaceImpl>());
  }
  state_ = State::kTornDown;
  profile_ = nullptr;
  ClearQueuedPromos();
}

UserEducationService*
BrowserUserEducationInterfaceImpl::GetUserEducationService() {
  return profile_ ? UserEducationServiceFactory::GetForBrowserContext(profile_)
                  : nullptr;
}
const UserEducationService*
BrowserUserEducationInterfaceImpl::GetUserEducationService() const {
  return profile_ ? UserEducationServiceFactory::GetForBrowserContext(profile_)
                  : nullptr;
}

user_education::FeaturePromoController*
BrowserUserEducationInterfaceImpl::GetFeaturePromoController() {
  auto* const service = GetUserEducationService();
  return service ? service->GetFeaturePromoController(
                       base::PassKey<BrowserUserEducationInterfaceImpl>())
                 : nullptr;
}

const user_education::FeaturePromoController*
BrowserUserEducationInterfaceImpl::GetFeaturePromoController() const {
  auto* const service = GetUserEducationService();
  return service ? service->GetFeaturePromoController(
                       base::PassKey<BrowserUserEducationInterfaceImpl>())
                 : nullptr;
}

bool BrowserUserEducationInterfaceImpl::IsFeaturePromoQueued(
    const base::Feature& iph_feature) const {
  auto* const controller = GetFeaturePromoController();
  return controller && controller->GetPromoStatus(iph_feature) ==
                           user_education::FeaturePromoStatus::kQueued;
}

bool BrowserUserEducationInterfaceImpl::IsFeaturePromoActive(
    const base::Feature& iph_feature) const {
  auto* const controller = GetFeaturePromoController();
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

  if (auto* const controller = GetFeaturePromoController()) {
    return controller->CanShowPromo(iph_feature, user_education_context_);
  }
  return user_education::FeaturePromoResult::kBlockedByContext;
}

void BrowserUserEducationInterfaceImpl::MaybeShowFeaturePromo(
    user_education::FeaturePromoParams params) {
  // Trying to show a promo before the browser is initialized can result in a
  // failure to retrieve accelerators, which can cause issues for screen reader
  // users.
  if (state_ != State::kInitialized) {
    std::string_view state_desc;
    switch (state_) {
      case State::kUninitialized:
        state_desc = " before browser initialization";
        break;
      case State::kInitializationPending:
        state_desc = " before browser initialization complete";
        break;
      case State::kTornDown:
        state_desc = " after browser shutdown";
        break;
      case State::kInitialized:
        NOTREACHED();
    }
    LOG(ERROR) << "Attempting to show IPH " << params.feature->name
               << state_desc << "; IPH will not be shown.";
    user_education::FeaturePromoController::PostShowPromoResult(
        std::move(params.show_promo_result_callback),
        user_education::FeaturePromoResult::kError);
    return;
  }

  if (auto* const controller = GetFeaturePromoController()) {
    controller->MaybeShowPromo(std::move(params), user_education_context_);
    return;
  }

  user_education::FeaturePromoController::PostShowPromoResult(
      std::move(params.show_promo_result_callback),
      user_education::FeaturePromoResult::kBlockedByContext);
}

void BrowserUserEducationInterfaceImpl::MaybeShowStartupFeaturePromo(
    user_education::FeaturePromoParams params) {
  if (state_ == State::kUninitialized ||
      state_ == State::kInitializationPending) {
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

  if (auto* const controller = GetFeaturePromoController()) {
    controller->MaybeShowStartupPromo(std::move(params),
                                      user_education_context_);
    return;
  }

  user_education::FeaturePromoController::PostShowPromoResult(
      std::move(params.show_promo_result_callback),
      user_education::FeaturePromoResult::kBlockedByContext);
}

bool BrowserUserEducationInterfaceImpl::AbortFeaturePromo(
    const base::Feature& iph_feature) {
  auto* const controller = GetFeaturePromoController();
  return controller &&
         controller->EndPromo(
             iph_feature, user_education::EndFeaturePromoReason::kAbortPromo);
}

user_education::FeaturePromoHandle
BrowserUserEducationInterfaceImpl::CloseFeaturePromoAndContinue(
    const base::Feature& iph_feature) {
  auto* const controller = GetFeaturePromoController();
  if (!controller || controller->GetPromoStatus(iph_feature) !=
                         user_education::FeaturePromoStatus::kBubbleShowing) {
    return user_education::FeaturePromoHandle();
  }
  return controller->CloseBubbleAndContinuePromo(iph_feature);
}

bool BrowserUserEducationInterfaceImpl::NotifyFeaturePromoFeatureUsed(
    const base::Feature& feature,
    FeaturePromoFeatureUsedAction action) {
  auto* const controller = GetFeaturePromoController();
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

void BrowserUserEducationInterfaceImpl::ClearQueuedPromos(
    user_education::FeaturePromoResult::Failure failure) {
  for (auto& params : queued_params_) {
    user_education::FeaturePromoController::PostShowPromoResult(
        std::move(params.show_promo_result_callback), failure);
  }
  queued_params_.clear();
}

const user_education::UserEducationContextPtr&
BrowserUserEducationInterfaceImpl::GetUserEducationContextImpl() const {
  return user_education_context_;
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_education/user_education_handler.h"

#include "base/notreached.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"

UserEducationMixedTrustHandlerBase::UserEducationMixedTrustHandlerBase() =
    default;
UserEducationMixedTrustHandlerBase::~UserEducationMixedTrustHandlerBase() =
    default;

void UserEducationMixedTrustHandlerBase::NotifyFeaturePromoFeatureUsed(
    const std::string& feature_name,
    user_education::mojom::FeaturePromoFeatureUsedAction action) {
  auto* const controller = GetFeaturePromoController();
  if (!controller) {
    return;
  }
  auto* const feature = FeaturePromoFeatureFromName(feature_name);
  if (!feature) {
    ReportBadMessage("Unrecognized feature promo feature: " + feature_name);
    return;
  }
  controller->NotifyFeatureUsedIfValid(*feature);
  if (action == user_education::mojom::FeaturePromoFeatureUsedAction::
                    kClosePromoIfPresent) {
    controller->EndPromo(
        *feature, user_education::EndFeaturePromoReason::kFeatureEngaged);
  }
}

void UserEducationMixedTrustHandlerBase::NotifyAdditionalConditionEvent(
    const std::string& event_name) {
  auto* const tracker = GetFeatureEngagementTracker();
  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(event_name);
}

void UserEducationMixedTrustHandlerBase::NotifyNewBadgeFeatureUsed(
    const std::string& feature_name) {
  auto* const controller = GetNewBadgeController();
  if (!controller) {
    return;
  }
  auto* const feature = NewBadgeFeatureFromName(feature_name);
  if (!feature) {
    ReportBadMessage("Unrecognized New Badge feature: " + feature_name);
    return;
  }
  controller->NotifyFeatureUsed(*feature);
}

void UserEducationMixedTrustHandlerBase::MaybeShowNewBadgeFor(
    const std::string& feature_name,
    MaybeShowNewBadgeForCallback callback) {
  auto* const controller = GetNewBadgeController();
  if (!controller) {
    std::move(callback).Run(false);
    return;
  }
  auto* const feature = NewBadgeFeatureFromName(feature_name);
  if (!feature) {
    ReportBadMessage("Unrecognized New Badge feature: " + feature_name);
    return;
  }
  std::move(callback).Run(controller->MaybeShowNewBadge(*feature));
}

const base::Feature*
UserEducationMixedTrustHandlerBase::FeaturePromoFeatureFromName(
    const std::string& feature_name) const {
  for (auto& data : GetFeaturePromoRegistry()->feature_data()) {
    if (data.first->name == feature_name) {
      return data.first;
    }
  }
  return nullptr;
}

const base::Feature*
UserEducationMixedTrustHandlerBase::NewBadgeFeatureFromName(
    const std::string& feature_name) const {
  for (auto& data : GetNewBadgeRegistry()->feature_data()) {
    if (data.first->name == feature_name) {
      return data.first;
    }
  }
  return nullptr;
}

void UserEducationMixedTrustHandlerBase::ReportBadMessage(
    std::string_view error) {
  NOTREACHED() << error;
}

UserEducationMixedTrustHandler::UserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        pending_handler,
    content::BrowserContext& context)
    : receiver_(this, std::move(pending_handler)), context_(context) {}

UserEducationMixedTrustHandler::~UserEducationMixedTrustHandler() = default;

void UserEducationMixedTrustHandler::ReportBadMessage(std::string_view error) {
  receiver_.ReportBadMessage(std::move(error));
}

const user_education::NewBadgeRegistry*
UserEducationMixedTrustHandler::GetNewBadgeRegistry() const {
  return UserEducationServiceFactory::GetForBrowserContext(&*context_)
      ->new_badge_registry();
}

user_education::NewBadgeController*
UserEducationMixedTrustHandler::GetNewBadgeController() {
  return UserEducationServiceFactory::GetForBrowserContext(&*context_)
      ->new_badge_controller();
}

const user_education::FeaturePromoRegistry*
UserEducationMixedTrustHandler::GetFeaturePromoRegistry() const {
  return &UserEducationServiceFactory::GetForBrowserContext(&*context_)
              ->feature_promo_registry();
}

user_education::FeaturePromoController*
UserEducationMixedTrustHandler::GetFeaturePromoController() {
  return UserEducationServiceFactory::GetForBrowserContext(&*context_)
      ->GetFeaturePromoController(
          base::PassKey<UserEducationMixedTrustHandler>());
}

feature_engagement::Tracker*
UserEducationMixedTrustHandler::GetFeatureEngagementTracker() {
  return feature_engagement::TrackerFactory::GetForBrowserContext(&*context_);
}

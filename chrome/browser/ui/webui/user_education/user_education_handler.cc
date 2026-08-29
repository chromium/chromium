// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_education/user_education_handler.h"

#include "base/notreached.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "content/public/browser/web_contents.h"

UserEducationMixedTrustHandlerBase::UserEducationMixedTrustHandlerBase() =
    default;
UserEducationMixedTrustHandlerBase::~UserEducationMixedTrustHandlerBase() =
    default;

void UserEducationMixedTrustHandlerBase::MaybeShowFeaturePromo(
    user_education::mojom::FeaturePromoParamsPtr params) {
  auto* const interface = GetBrowserUserEducationInterface();
  if (!interface) {
    return;
  }

  auto* const feature = FeaturePromoFeatureFromName(params->feature_name);
  if (!feature) {
    return;
  }

  user_education::FeaturePromoParams iph_params(*feature);
  iph_params.key = params->key.value_or(std::string());
  interface->MaybeShowFeaturePromo(std::move(iph_params));
}

void UserEducationMixedTrustHandlerBase::NotifyFeaturePromoFeatureUsed(
    const std::string& feature_name,
    user_education::mojom::FeaturePromoFeatureUsedAction action) {
  auto* const controller = GetFeaturePromoController();
  if (!controller) {
    return;
  }
  auto* const feature = FeaturePromoFeatureFromName(feature_name);
  if (!feature) {
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
    return;
  }
  std::move(callback).Run(controller->MaybeShowNewBadge(*feature));
}

const base::Feature*
UserEducationMixedTrustHandlerBase::FeaturePromoFeatureFromName(
    const std::string& feature_name) {
  auto* const registry = GetFeaturePromoRegistry();
  if (!registry || registry->feature_data().empty()) {
    // Browser doesn't support feature promos, this is okay.
    return nullptr;
  }

  for (auto& data : registry->feature_data()) {
    if (data.first->name == feature_name) {
      return data.first;
    }
  }
  // Browser does support feature promos but the name was actually bad.
  ReportBadMessage("Unrecognized feature promo feature: " + feature_name);
  return nullptr;
}

const base::Feature*
UserEducationMixedTrustHandlerBase::NewBadgeFeatureFromName(
    const std::string& feature_name) {
  auto* const registry = GetNewBadgeRegistry();
  if (!registry) {
    // Browser doesn't support new badges, this is okay.
    return nullptr;
  }
  for (auto& data : registry->feature_data()) {
    if (data.first->name == feature_name) {
      return data.first;
    }
  }
  // Browser does support new badges but the name was actually bad.
  ReportBadMessage("Unrecognized New Badge feature: " + feature_name);
  return nullptr;
}

void UserEducationMixedTrustHandlerBase::ReportBadMessage(
    std::string_view error) {
  NOTREACHED() << error;
}

UserEducationMixedTrustHandler::UserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        pending_handler,
    content::WebContents* contents)
    : receiver_(this, std::move(pending_handler)), web_contents_(contents) {}

UserEducationMixedTrustHandler::~UserEducationMixedTrustHandler() = default;

void UserEducationMixedTrustHandler::ReportBadMessage(std::string_view error) {
  receiver_.ReportBadMessage(std::move(error));
}

const user_education::NewBadgeRegistry*
UserEducationMixedTrustHandler::GetNewBadgeRegistry() const {
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(GetBrowserContext());
  return service ? service->new_badge_registry() : nullptr;
}

user_education::NewBadgeController*
UserEducationMixedTrustHandler::GetNewBadgeController() {
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(GetBrowserContext());
  return service ? service->new_badge_controller() : nullptr;
}

const user_education::FeaturePromoRegistry*
UserEducationMixedTrustHandler::GetFeaturePromoRegistry() const {
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(GetBrowserContext());
  return service ? &service->feature_promo_registry() : nullptr;
}

user_education::FeaturePromoController*
UserEducationMixedTrustHandler::GetFeaturePromoController() {
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(GetBrowserContext());
  return service ? service->GetFeaturePromoController(
                       base::PassKey<UserEducationMixedTrustHandler>())
                 : nullptr;
}

feature_engagement::Tracker*
UserEducationMixedTrustHandler::GetFeatureEngagementTracker() {
  return feature_engagement::TrackerFactory::GetForBrowserContext(
      GetBrowserContext());
}

BrowserUserEducationInterface*
UserEducationMixedTrustHandler::GetBrowserUserEducationInterface() {
  auto* interface =
      BrowserUserEducationInterface::MaybeGetForWebContentsInTab(web_contents_);
  if (!interface) {
    interface =
        BrowserUserEducationInterface::MaybeGetForWebUiContents(web_contents_);
  }
  return interface;
}

content::BrowserContext* UserEducationMixedTrustHandler::GetBrowserContext()
    const {
  return web_contents_->GetBrowserContext();
}

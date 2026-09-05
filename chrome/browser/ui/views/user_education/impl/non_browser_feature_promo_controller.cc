// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/non_browser_feature_promo_controller.h"

#include "base/notreached.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"

NonBrowserFeaturePromoController::NonBrowserFeaturePromoController(
    PassKey,
    feature_engagement::Tracker* tracker_service,
    UserEducationService* user_education_service,
    user_education::UserEducationContextPtr context,
    const ui::AcceleratorProvider* accelerator_provider)
    : user_education::FeaturePromoControllerImpl(
          tracker_service,
          &user_education_service->feature_promo_registry(),
          &user_education_service->help_bubble_factory_registry(),
          &user_education_service->user_education_storage_service(),
          &user_education_service->feature_promo_session_policy(),
          user_education_service->tutorial_service(),
          user_education_service->product_messaging_controller()),
      context_(std::move(context)),
      accelerator_provider_(accelerator_provider) {
  MaybeRegisterChromeFeaturePromos(
      user_education_service->feature_promo_registry());
  RegisterChromeHelpBubbleFactories(
      user_education_service->help_bubble_factory_registry());
}

NonBrowserFeaturePromoController::~NonBrowserFeaturePromoController() {
  OnDestroying();
}

void NonBrowserFeaturePromoController::MaybeShowPromo(
    user_education::FeaturePromoParams params) {
  FeaturePromoControllerImpl::MaybeShowPromo(std::move(params), context_);
}

void NonBrowserFeaturePromoController::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  FeaturePromoControllerImpl::AddPreconditionProviders(to_add_to, priority,
                                                       required);
}

std::u16string NonBrowserFeaturePromoController::GetBodyIconAltText() const {
  NOTREACHED();
}

const base::Feature*
NonBrowserFeaturePromoController::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char*
NonBrowserFeaturePromoController::GetScreenReaderPromptPromoEventName() const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

std::u16string NonBrowserFeaturePromoController::GetTutorialScreenReaderHint(
    const ui::AcceleratorProvider*) const {
  NOTREACHED();
}

std::u16string
NonBrowserFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element,
    const ui::AcceleratorProvider* accelerator_provider) const {
  const ui::AcceleratorProvider* const effective_provider =
      accelerator_provider ? accelerator_provider : accelerator_provider_.get();
  if (!effective_provider) {
    return std::u16string();
  }
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, effective_provider, anchor_element);
}

user_education::UserEducationContextPtr
NonBrowserFeaturePromoController::GetContextForHelpBubble(
    const ui::TrackedElement* anchor_element) const {
  return context_;
}

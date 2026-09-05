// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_feature_promo_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/safe_castable.h"

namespace omnibox_everywhere {

namespace {

class OmniboxEverywhereUserEducationContext
    : public user_education::UserEducationContext {
 public:
  DECLARE_SAFE_CAST_TARGET()

  explicit OmniboxEverywhereUserEducationContext(
      base::WeakPtr<const OmniboxEverywhereService> service)
      : service_(std::move(service)) {}

  bool IsValid() const override {
    return service_ && service_->IsPopupVisibleForProfile();
  }

  ui::ElementContext GetElementContext() const override {
    return ui::ElementContext();
  }

  user_education::AnchorElementFilter GetDefaultElementFilter() const override {
    return base::BindRepeating(
        [](const ui::ElementTracker::ElementList& elements)
            -> ui::TrackedElement* {
          return elements.empty() ? nullptr : elements.front();
        });
  }

  const ui::AcceleratorProvider* GetAcceleratorProvider() const override {
    return nullptr;
  }

 protected:
  ~OmniboxEverywhereUserEducationContext() override = default;

 private:
  const base::WeakPtr<const OmniboxEverywhereService> service_;
};

DEFINE_SAFE_CAST_TARGET(OmniboxEverywhereUserEducationContext)

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kOmniboxEverywhereOpenAndActivePrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kOmniboxEverywhereOpenAndActivePrecondition);

class OmniboxEverywhereOpenAndActivePrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit OmniboxEverywhereOpenAndActivePrecondition(
      base::WeakPtr<const OmniboxEverywhereService> service)
      : FeaturePromoPreconditionBase(
            kOmniboxEverywhereOpenAndActivePrecondition,
            "Omnibox Everywhere is open and active"),
        service_(std::move(service)) {}
  ~OmniboxEverywhereOpenAndActivePrecondition() override = default;

  // user_education::FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      user_education::UnownedTypedDataCollection& data) const override {
    if (!service_ || !service_->IsPopupVisibleForProfile()) {
      return user_education::FeaturePromoResult::kBlockedByUi;
    }
    if (service_->profile() &&
        base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre) &&
        !service_->profile()->GetPrefs()->GetBoolean(
            omnibox_everywhere::prefs::kFreDismissed)) {
      return user_education::FeaturePromoResult::kBlockedByUi;
    }
    return user_education::FeaturePromoResult::Success();
  }

 private:
  const base::WeakPtr<const OmniboxEverywhereService> service_;
};

}  // namespace

OmniboxEverywhereFeaturePromoController::
    OmniboxEverywhereFeaturePromoController(
        feature_engagement::Tracker* tracker_service,
        UserEducationService* user_education_service,
        base::WeakPtr<OmniboxEverywhereService> service)
    : NonBrowserFeaturePromoController(
          base::PassKey<OmniboxEverywhereFeaturePromoController>(),
          tracker_service,
          user_education_service,
          base::MakeRefCounted<OmniboxEverywhereUserEducationContext>(service),
          /*accelerator_provider=*/nullptr),
      service_(std::move(service)) {}

OmniboxEverywhereFeaturePromoController::
    ~OmniboxEverywhereFeaturePromoController() = default;

void OmniboxEverywhereFeaturePromoController::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  NonBrowserFeaturePromoController::AddPreconditionProviders(
      to_add_to, priority, required);

  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<OmniboxEverywhereFeaturePromoController>
               promo_controller,
           const user_education::FeaturePromoSpecification& spec,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          user_education::FeaturePromoPreconditionList preconditions;
          if (promo_controller) {
            preconditions.AddPrecondition(
                std::make_unique<OmniboxEverywhereOpenAndActivePrecondition>(
                    promo_controller->service_));
          }
          return preconditions;
        },
        weak_factory_.GetWeakPtr()));
  }
}

}  // namespace omnibox_everywhere

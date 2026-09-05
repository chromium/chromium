// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_feature_promo_controller.h"

#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace {

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kProfilePickerOpenPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kProfilePickerOpenPrecondition);

class ProfilePickerOpenPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  ProfilePickerOpenPrecondition()
      : FeaturePromoPreconditionBase(kProfilePickerOpenPrecondition,
                                     "Profile picker is open") {}
  ~ProfilePickerOpenPrecondition() override = default;

  // user_education::FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      user_education::UnownedTypedDataCollection& data) const override {
    return ProfilePicker::IsOpen()
               ? user_education::FeaturePromoResult::Success()
               : user_education::FeaturePromoResult::kBlockedByUi;
  }
};

}  // namespace

ProfilePickerFeaturePromoController::ProfilePickerFeaturePromoController(
    feature_engagement::Tracker* tracker_service,
    UserEducationService* user_education_service,
    ProfilePickerView* profile_picker_view)
    : NonBrowserFeaturePromoController(
          base::PassKey<ProfilePickerFeaturePromoController>(),
          tracker_service,
          user_education_service,
          /*context=*/nullptr,
          /*accelerator_provider=*/profile_picker_view) {}

ProfilePickerFeaturePromoController::~ProfilePickerFeaturePromoController() =
    default;

void ProfilePickerFeaturePromoController::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  NonBrowserFeaturePromoController::AddPreconditionProviders(
      to_add_to, priority, required);

  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](const user_education::FeaturePromoSpecification& spec,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          user_education::FeaturePromoPreconditionList preconditions;
          preconditions.AddPrecondition(
              std::make_unique<ProfilePickerOpenPrecondition>());
          return preconditions;
        }));
  }
}


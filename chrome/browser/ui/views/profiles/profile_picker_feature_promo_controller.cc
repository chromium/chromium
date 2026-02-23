// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_feature_promo_controller.h"

#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/views/interaction/element_tracker_views.h"

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
      ui::UnownedTypedDataCollection& data) const override {
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
    : user_education::FeaturePromoControllerImpl(
          tracker_service,
          &user_education_service->feature_promo_registry(),
          &user_education_service->help_bubble_factory_registry(),
          &user_education_service->user_education_storage_service(),
          &user_education_service->feature_promo_session_policy(),
          &user_education_service->tutorial_service(),
          &user_education_service->product_messaging_controller()),
      profile_picker_view_(profile_picker_view) {
  MaybeRegisterChromeFeaturePromos(
      user_education_service->feature_promo_registry());
  RegisterChromeHelpBubbleFactories(
      user_education_service->help_bubble_factory_registry());
}

ProfilePickerFeaturePromoController::~ProfilePickerFeaturePromoController() {
  OnDestroying();
}

void ProfilePickerFeaturePromoController::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  FeaturePromoControllerImpl::AddPreconditionProviders(to_add_to, priority,
                                                       required);

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

std::u16string ProfilePickerFeaturePromoController::GetBodyIconAltText() const {
  NOTREACHED();
}

const base::Feature*
ProfilePickerFeaturePromoController::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char*
ProfilePickerFeaturePromoController::GetScreenReaderPromptPromoEventName()
    const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

std::u16string ProfilePickerFeaturePromoController::GetTutorialScreenReaderHint(
    const ui::AcceleratorProvider*) const {
  NOTREACHED();
}

std::u16string
ProfilePickerFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element,
    const ui::AcceleratorProvider* accelerator_provider) const {
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, profile_picker_view_, anchor_element);
}

user_education::UserEducationContextPtr
ProfilePickerFeaturePromoController::GetContextForHelpBubble(
    const ui::TrackedElement* anchor_element) const {
  return nullptr;
}

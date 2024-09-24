// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_feature_promo_controller.h"

#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/feature_engagement/public/event_constants.h"
#include "ui/views/interaction/element_tracker_views.h"

ProfilePickerFeaturePromoController::ProfilePickerFeaturePromoController(
    feature_engagement::Tracker* tracker_service,
    UserEducationService* user_education_service,
    ProfilePickerView* profile_picker_view)
    : user_education::FeaturePromoControllerCommon(
          tracker_service,
          &user_education_service->feature_promo_registry(),
          &user_education_service->help_bubble_factory_registry(),
          &user_education_service->feature_promo_storage_service(),
          &user_education_service->feature_promo_session_policy(),
          &user_education_service->tutorial_service(),
          &user_education_service->product_messaging_controller()),
      profile_picker_view_(profile_picker_view) {
  MaybeRegisterChromeFeaturePromos(
      user_education_service->feature_promo_registry());
  RegisterChromeHelpBubbleFactories(
      user_education_service->help_bubble_factory_registry());
}

ui::ElementContext ProfilePickerFeaturePromoController::GetAnchorContext()
    const {
  return views::ElementTrackerViews::GetContextForView(profile_picker_view_);
}

bool ProfilePickerFeaturePromoController::CanShowPromoForElement(
    ui::TrackedElement* anchor_element) const {
  return ProfilePicker::IsOpen();
}

const ui::AcceleratorProvider*
ProfilePickerFeaturePromoController::GetAcceleratorProvider() const {
  return profile_picker_view_;
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

std::u16string
ProfilePickerFeaturePromoController::GetTutorialScreenReaderHint() const {
  NOTREACHED();
}

std::u16string
ProfilePickerFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element) const {
  return GetFocusHelpBubbleScreenReaderHintCommon(
      promo_type, profile_picker_view_, anchor_element);
}

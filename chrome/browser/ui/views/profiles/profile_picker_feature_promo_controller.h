// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_20.h"
#include "components/user_education/common/user_education_context.h"

class UserEducationService;
class ProfilePickerView;

// Profile Picker implementation of `FeaturePromoControllerCommon`. There is a
// single instance owned by the Profile Picker, with the keyed services attached
// the OTR System Profile.
// The class allows the management of IPH that are displayed in the Profile
// Picker.
class ProfilePickerFeaturePromoController
    : public user_education::FeaturePromoController20 {
 public:
  ProfilePickerFeaturePromoController(
      feature_engagement::Tracker* tracker_service,
      UserEducationService* user_education_service,
      ProfilePickerView* profile_picker_view);

 private:
  // user_education::FeaturePromoControllerCommon:
  user_education::FeaturePromoResult CanShowPromoForElement(
      ui::TrackedElement* anchor_element,
      const user_education::UserEducationContextPtr&) const override;
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;
  std::u16string GetTutorialScreenReaderHint(
      const ui::AcceleratorProvider* accelerator_provider) const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      const ui::AcceleratorProvider* accelerator_provider) const override;
  user_education::UserEducationContextPtr GetContextForHelpBubble(
      const ui::TrackedElement* anchor_element) const override;

  const raw_ptr<ProfilePickerView> profile_picker_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_

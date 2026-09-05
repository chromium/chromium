// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_

#include "chrome/browser/ui/views/user_education/impl/non_browser_feature_promo_controller.h"

class ProfilePickerView;
class UserEducationService;

namespace feature_engagement {
class Tracker;
}

// Profile Picker implementation of `NonBrowserFeaturePromoController`. There is
// a single instance owned by the Profile Picker, with the keyed services
// attached the OTR System Profile. The class allows the management of IPH that
// are displayed in the Profile Picker.
class ProfilePickerFeaturePromoController
    : public NonBrowserFeaturePromoController {
 public:
  ProfilePickerFeaturePromoController(
      feature_engagement::Tracker* tracker_service,
      UserEducationService* user_education_service,
      ProfilePickerView* profile_picker_view);
  ~ProfilePickerFeaturePromoController() override;

 private:
  // NonBrowserFeaturePromoController:
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FEATURE_PROMO_CONTROLLER_H_

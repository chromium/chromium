// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

#include "chrome/browser/ui/user_education/feature_promo_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "components/keyed_service/core/keyed_service.h"

class UserEducationService : public KeyedService {
 public:
  UserEducationService();
  ~UserEducationService() override;

  TutorialRegistry& tutorial_registry() { return tutorial_registry_; }
  TutorialService& tutorial_service() { return tutorial_service_; }
  HelpBubbleFactoryRegistry& help_bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }
  FeaturePromoRegistry& feature_promo_registry() {
    return feature_promo_registry_;
  }

 private:
  TutorialRegistry tutorial_registry_;
  HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  FeaturePromoRegistry feature_promo_registry_;
  TutorialService tutorial_service_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

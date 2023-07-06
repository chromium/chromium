// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

#include "chrome/browser/ui/user_education/browser_tutorial_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_registry.h"

extern const char kTabGroupTutorialId[];
extern const char kTabGroupWithExistingGroupTutorialId[];
extern const char kSidePanelCustomizeChromeTutorialId[];
extern const char kPasswordManagerTutorialId[];

class UserEducationService : public KeyedService {
 public:
  UserEducationService();
  ~UserEducationService() override;

  user_education::TutorialRegistry& tutorial_registry() {
    return tutorial_registry_;
  }
  user_education::TutorialService& tutorial_service() {
    return tutorial_service_;
  }
  user_education::HelpBubbleFactoryRegistry& help_bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }
  user_education::FeaturePromoRegistry& feature_promo_registry() {
    return feature_promo_registry_;
  }

 private:
  user_education::TutorialRegistry tutorial_registry_;
  user_education::HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  user_education::FeaturePromoRegistry feature_promo_registry_;
  BrowserTutorialService tutorial_service_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

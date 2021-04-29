// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_FACTORY_H_

#include "chrome/browser/ui/user_education/feature_tutorial_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

class FeatureTutorialServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  FeatureTutorialServiceFactory();
  FeatureTutorialServiceFactory(const FeatureTutorialServiceFactory&) = delete;
  FeatureTutorialServiceFactory& operator=(
      const FeatureTutorialServiceFactory&) = delete;

  static FeatureTutorialServiceFactory* GetInstance();

  static FeatureTutorialService* GetForProfile(Profile* profile);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_FACTORY_H_

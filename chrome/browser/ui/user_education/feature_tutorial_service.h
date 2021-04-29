// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_H_

#include <memory>

#include "chrome/browser/ui/user_education/feature_tutorials.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

class FeatureTutorialService : public KeyedService {
 public:
  FeatureTutorialService();
  ~FeatureTutorialService() override;

  virtual bool StartTutorial(FeatureTutorial tutorial) = 0;

 private:
  friend class FeatureTutorialServiceFactory;

  static std::unique_ptr<FeatureTutorialService> MakeInstance(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_H_

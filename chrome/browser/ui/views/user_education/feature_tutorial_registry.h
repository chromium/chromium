// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_REGISTRY_H_

#include <map>

#include "chrome/browser/ui/user_education/feature_tutorials.h"
#include "chrome/browser/ui/views/user_education/feature_tutorial_description.h"

// Stores configurations for tutorials. Has one instance with statically
// registered tutorial configurations.
class FeatureTutorialRegistry {
 public:
  FeatureTutorialRegistry();
  ~FeatureTutorialRegistry();

  static const FeatureTutorialRegistry* GetInstance();

  const FeatureTutorialDescription* GetTutorialDescription(
      FeatureTutorial tutorial) const;

 private:
  std::map<FeatureTutorial, FeatureTutorialDescription> descriptions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_REGISTRY_H_

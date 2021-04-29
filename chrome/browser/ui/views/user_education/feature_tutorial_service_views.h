// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_VIEWS_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_tutorial_service.h"

class FeatureTutorialServiceViews : public FeatureTutorialService {
 public:
  explicit FeatureTutorialServiceViews(Profile* profile);
  ~FeatureTutorialServiceViews() override;

  bool StartTutorial(FeatureTutorial tutorial) override;

 private:
  // Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_SERVICE_VIEWS_H_

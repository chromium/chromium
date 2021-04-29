// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_tutorial_service_views.h"

#include <memory>

FeatureTutorialServiceViews::FeatureTutorialServiceViews(Profile* profile) {}
FeatureTutorialServiceViews::~FeatureTutorialServiceViews() = default;

bool FeatureTutorialServiceViews::StartTutorial(FeatureTutorial tutorial) {
  return true;
}

// Here we define the base class's MakeInstance function.
// static
std::unique_ptr<FeatureTutorialService> FeatureTutorialService::MakeInstance(
    Profile* profile) {
  return std::make_unique<FeatureTutorialServiceViews>(profile);
}

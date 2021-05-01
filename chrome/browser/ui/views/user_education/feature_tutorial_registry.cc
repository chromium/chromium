// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_tutorial_registry.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"

FeatureTutorialRegistry::FeatureTutorialRegistry() {
  // kTabGroups:
  {
    FeatureTutorialDescription description;

    FeatureTutorialDescription::Step step1;
    // Use a dummy string
    step1.bubble_body_string_specifier = IDS_TAB_GROUPS_NEW_GROUP_PROMO;
    description.steps.push_back(step1);
  }
}

FeatureTutorialRegistry::~FeatureTutorialRegistry() = default;

const FeatureTutorialDescription*
FeatureTutorialRegistry::GetTutorialDescription(
    FeatureTutorial tutorial) const {
  auto it = descriptions_.find(tutorial);
  CHECK(it != descriptions_.end());
  return &it->second;
}

// static
const FeatureTutorialRegistry* FeatureTutorialRegistry::GetInstance() {
  static base::NoDestructor<FeatureTutorialRegistry> instance;
  return instance.get();
}

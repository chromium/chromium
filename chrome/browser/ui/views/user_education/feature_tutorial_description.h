// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_DESCRIPTION_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_DESCRIPTION_H_

#include <vector>

// Describes a single tutorial. Contains a list of steps, each of which
// describes a help bubble, target element, and expected user interaction. A
// tutorial progresses to the next step when the expected user interaction
// happens.
struct FeatureTutorialDescription {
  FeatureTutorialDescription();
  ~FeatureTutorialDescription();

  struct Step {
    // String specifier for the bubble's body text
    int bubble_body_string_specifier;

    // Will contain an element ID (for anchoring and highlighting) and an
    // expected user interaction (a wait condition for the next step).
  };

  std::vector<Step> steps;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_TUTORIAL_DESCRIPTION_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_test_util.h"

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "ui/base/interaction/element_tracker.h"

std::unique_ptr<TutorialBubble> TestTutorialBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    absl::optional<std::u16string> title_text,
    absl::optional<std::u16string> body_text,
    TutorialDescription::Step::Arrow arrow,
    absl::optional<std::pair<int, int>> progress,
    base::RepeatingClosure abort_callback,
    bool is_last_step) {
  return std::make_unique<TestTutorialBubble>();
}

bool TestTutorialBubbleFactory::CanBuildBubbleForTrackedElement(
    ui::TrackedElement* element) {
  return true;
}

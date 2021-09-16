// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"

std::unique_ptr<TutorialBubble>
TutorialBubbleFactory::CreateBubbleIfElementIsValid(
    ui::TrackedElement* element,
    absl::optional<std::u16string> title_text,
    absl::optional<std::u16string> body_text,
    TutorialDescription::Step::Arrow arrow,
    absl::optional<std::pair<int, int>> progress) {
  if (CanBuildBubbleForTrackedElement(element))
    return CreateBubble(element, title_text, body_text, arrow, progress);
  return nullptr;
}

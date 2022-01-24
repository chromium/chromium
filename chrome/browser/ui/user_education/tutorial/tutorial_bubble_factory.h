// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"

// TutorialBubbleFactory is an interface for Opening and closing bubbles from a
// running Tutorial. Since some platforms/implementations of IPH bubbles will
// have their own reqirements/constraints for if/when/how to show a bubble,
// the TutorialService only calls into the implementation asking it to Show
// or Hide the bubble. The BubbleOwnerImplementation is responsible for handling
// state.
class TutorialBubbleFactory {
 public:
  virtual ~TutorialBubbleFactory() = default;

  std::unique_ptr<TutorialBubble> CreateBubbleIfElementIsValid(
      ui::TrackedElement* element,
      absl::optional<std::u16string> title_text,
      absl::optional<std::u16string> body_text,
      TutorialDescription::Step::Arrow arrow,
      absl::optional<std::pair<int, int>> progress,
      base::RepeatingClosure abort_callback,
      bool is_last_step = false);

 private:
  // Called by the Tutorial to show the bubble.
  virtual std::unique_ptr<TutorialBubble> CreateBubble(
      ui::TrackedElement* element,
      absl::optional<std::u16string> title_text,
      absl::optional<std::u16string> body_text,
      TutorialDescription::Step::Arrow arrow,
      absl::optional<std::pair<int, int>> progress,
      base::RepeatingClosure abort_callback,
      bool is_last_step = false) = 0;

  // Returns true iff the bubble owner can show a bubble for the TrackedElement.
  virtual bool CanBuildBubbleForTrackedElement(ui::TrackedElement* element) = 0;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_H_

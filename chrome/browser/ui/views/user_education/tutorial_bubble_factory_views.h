// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TUTORIAL_BUBBLE_FACTORY_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TUTORIAL_BUBBLE_FACTORY_VIEWS_H_

#include "base/token.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"

class TutorialBubbleViews : public TutorialBubble {
 public:
  explicit TutorialBubbleViews(absl::optional<base::Token> bubble_id);
  ~TutorialBubbleViews() override;

 private:
  absl::optional<base::Token> bubble_id_;
};

class TutorialBubbleFactoryViews : public TutorialBubbleFactory {
 public:
  TutorialBubbleFactoryViews();
  ~TutorialBubbleFactoryViews() override;

  std::unique_ptr<TutorialBubble> CreateBubble(
      ui::TrackedElement* element,
      absl::optional<std::u16string> title_text,
      absl::optional<std::u16string> body_text,
      TutorialDescription::Step::Arrow arrow,
      absl::optional<std::pair<int, int>> progress,
      base::RepeatingClosure abort_callback,
      bool is_last_step = false) override;

  bool CanBuildBubbleForTrackedElement(ui::TrackedElement* element) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TUTORIAL_BUBBLE_FACTORY_VIEWS_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_REGISTRY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_REGISTRY_H_

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"

// TutorialBubbleFactoryRegistry provides access to framework specific bubble
// factory implementations. It is used by the tutorial service manager to
// provide implementations. TutorialBubbleFactoryRegistry is not a singleton
// in order to allow for it to created multiple times in a test environment.
// in practice it should only be used by the TutorialServiceManger as a
// singleton.
class TutorialBubbleFactoryRegistry {
 public:
  TutorialBubbleFactoryRegistry();
  ~TutorialBubbleFactoryRegistry();
  TutorialBubbleFactoryRegistry(const TutorialBubbleFactoryRegistry&) = delete;
  TutorialBubbleFactoryRegistry& operator=(
      const TutorialBubbleFactoryRegistry&) = delete;

  // Adds the bubble factory to the list of bubble factories.
  void RegisterBubbleFactory(
      std::unique_ptr<TutorialBubbleFactory> bubble_factory);

  // gets the correct bubble factory for a given element if none are found a
  // nullptr is returned.
  std::unique_ptr<TutorialBubble> CreateBubbleForTrackedElement(
      ui::TrackedElement* element,
      absl::optional<std::u16string> title_text,
      absl::optional<std::u16string> body_text,
      TutorialDescription::Step::Arrow arrow,
      absl::optional<std::pair<int, int>> progress,
      base::RepeatingClosure abort_callback,
      bool is_last_step = false);

 private:
  // the list of registered bubble factories
  std::vector<std::unique_ptr<TutorialBubbleFactory>> bubble_factories_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_BUBBLE_FACTORY_REGISTRY_H_

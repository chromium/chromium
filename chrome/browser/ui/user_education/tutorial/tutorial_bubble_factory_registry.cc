// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"

#include <memory>

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "ui/base/interaction/element_tracker.h"

TutorialBubbleFactoryRegistry::TutorialBubbleFactoryRegistry() = default;
TutorialBubbleFactoryRegistry::~TutorialBubbleFactoryRegistry() = default;

void TutorialBubbleFactoryRegistry::RegisterBubbleFactory(
    std::unique_ptr<TutorialBubbleFactory> bubble_factory) {
  bubble_factories_.emplace_back(std::move(bubble_factory));
}

std::unique_ptr<TutorialBubble>
TutorialBubbleFactoryRegistry::CreateBubbleForTrackedElement(
    ui::TrackedElement* element,
    absl::optional<std::u16string> title_text,
    absl::optional<std::u16string> body_text,
    TutorialDescription::Step::Arrow arrow,
    absl::optional<std::pair<int, int>> progress,
    base::RepeatingClosure abort_callback,
    bool is_last_step) {
  for (const auto& bubble_factory : bubble_factories_) {
    std::unique_ptr<TutorialBubble> bubble =
        bubble_factory->CreateBubbleIfElementIsValid(
            element, title_text, body_text, arrow, progress, abort_callback,
            is_last_step);
    if (bubble)
      return bubble;
  }
  return nullptr;
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

TutorialDescription::TutorialDescription() = default;
TutorialDescription::~TutorialDescription() = default;
TutorialDescription::TutorialDescription(
    const TutorialDescription& description) = default;

TutorialDescription::Step::Step()
    : step_type(ui::InteractionSequence::StepType::kShown),
      arrow(TutorialDescription::Step::Arrow::NONE) {}
TutorialDescription::Step::~Step() = default;
TutorialDescription::Step::Step(
    absl::optional<std::u16string> title_text_,
    absl::optional<std::u16string> body_text_,
    ui::InteractionSequence::StepType step_type_,
    ui::ElementIdentifier element_id_,
    std::string element_name_,
    Arrow arrow_,
    absl::optional<bool> must_remain_visible_,
    bool transition_only_on_event_,
    TutorialDescription::NameElementsCallback name_elements_callback_)
    : title_text(title_text_),
      body_text(body_text_),
      step_type(step_type_),
      element_id(element_id_),
      element_name(element_name_),
      arrow(arrow_),
      must_remain_visible(must_remain_visible_),
      transition_only_on_event(transition_only_on_event_),
      name_elements_callback(name_elements_callback_) {}
TutorialDescription::Step::Step(const TutorialDescription::Step& description) =
    default;

bool TutorialDescription::Step::Step::ShouldShowBubble() const {
  return (element_id &&
          step_type != ui::InteractionSequence::StepType::kHidden &&
          (title_text || body_text));
}

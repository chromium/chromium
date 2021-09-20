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

TutorialDescription::Step::Step() = default;
TutorialDescription::Step::~Step() = default;
TutorialDescription::Step::Step(absl::optional<std::u16string> title_text_,
                                absl::optional<std::u16string> body_text_,
                                ui::InteractionSequence::StepType step_type_,
                                ui::ElementIdentifier element_id_,
                                Arrow arrow_)
    : title_text(title_text_),
      body_text(body_text_),
      step_type(step_type_),
      element_id(element_id_),
      arrow(arrow_) {}
TutorialDescription::Step::Step(const TutorialDescription::Step& description) =
    default;

bool TutorialDescription::Step::Step::ShouldShowBubble() const {
  return (element_id &&
          step_type != ui::InteractionSequence::StepType::kHidden &&
          (title_text || body_text));
}

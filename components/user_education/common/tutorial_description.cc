// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial_description.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace user_education {

TutorialDescription::TutorialDescription() = default;
TutorialDescription::~TutorialDescription() = default;
TutorialDescription::TutorialDescription(TutorialDescription&&) = default;
TutorialDescription& TutorialDescription::operator=(TutorialDescription&&) =
    default;

TutorialDescription::Step::Step()
    : step_type(ui::InteractionSequence::StepType::kShown),
      arrow(HelpBubbleArrow::kNone) {}
TutorialDescription::Step::~Step() = default;

TutorialDescription::Step::Step(
    int title_text_id_,
    int body_text_id_,
    ui::InteractionSequence::StepType step_type_,
    ui::ElementIdentifier element_id_,
    std::string element_name_,
    HelpBubbleArrow arrow_,
    ui::CustomElementEventType event_type_,
    absl::optional<bool> must_remain_visible_,
    bool transition_only_on_event_,
    TutorialDescription::NameElementsCallback name_elements_callback_,
    ContextMode context_mode_)
    : title_text_id(title_text_id_),
      body_text_id(body_text_id_),
      step_type(step_type_),
      event_type(event_type_),
      element_id(element_id_),
      element_name(element_name_),
      arrow(arrow_),
      must_remain_visible(must_remain_visible_),
      transition_only_on_event(transition_only_on_event_),
      name_elements_callback(name_elements_callback_),
      context_mode(context_mode_) {
  DCHECK(!title_text_id || body_text_id)
      << "Tutorial bubble should not have a title without body text.";
}

TutorialDescription::Step::Step(const TutorialDescription::Step&) = default;
TutorialDescription::Step& TutorialDescription::Step::operator=(
    const TutorialDescription::Step&) = default;

bool TutorialDescription::Step::Step::ShouldShowBubble() const {
  // Hide steps and steps with no body text are "hidden" steps.

  return body_text_id &&
         step_type != ui::InteractionSequence::StepType::kHidden;
}

}  // namespace user_education

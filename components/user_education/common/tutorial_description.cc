// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial_description.h"

#include <string_view>
#include <variant>

#include "components/user_education/common/events.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace user_education {

ScopedTutorialState::ScopedTutorialState(ui::ElementContext context)
    : context_(context) {}
ScopedTutorialState::~ScopedTutorialState() = default;

TutorialDescription::TutorialDescription() = default;
TutorialDescription::TutorialDescription(TutorialDescription&&) noexcept =
    default;
TutorialDescription& TutorialDescription::operator=(
    TutorialDescription&&) noexcept = default;
TutorialDescription::~TutorialDescription() = default;

TutorialDescription::Step::Step() = default;

TutorialDescription::Step::Step(ElementSpecifier element,
                                ui::InteractionSequence::StepType step_type,
                                HelpBubbleArrow arrow,
                                ui::CustomElementEventType event_type)
    : element_id_(std::holds_alternative<ui::ElementIdentifier>(element)
                      ? std::get<ui::ElementIdentifier>(element)
                      : ui::ElementIdentifier()),
      element_name_(std::holds_alternative<std::string>(element)
                        ? std::get<std::string>(element)
                        : std::string()),
      step_type_(step_type),
      event_type_(event_type) {}

TutorialDescription::Step::Step(const TutorialDescription::Step&) = default;
TutorialDescription::Step& TutorialDescription::Step::operator=(
    const TutorialDescription::Step&) = default;
TutorialDescription::Step::~Step() = default;

TutorialDescription::Step& TutorialDescription::Step::NameElement(
    std::string name) {
  return NameElements(base::BindRepeating(
      [](std::string name, ui::InteractionSequence* sequence,
         ui::TrackedElement* element) {
        sequence->NameElement(element, std::string_view(name));
        return true;
      },
      name));
}

bool TutorialDescription::Step::ShouldShowBubble() const {
  // Hide steps and steps with no body text are "hidden" steps.
  return body_text_id_ &&
         step_type_ != ui::InteractionSequence::StepType::kHidden &&
         step_type_ != ui::InteractionSequence::StepType::kSubsequence;
}

TutorialDescription::BubbleStep&
TutorialDescription::BubbleStep::AddDefaultNextButton() {
  return AddCustomNextButton(
      base::BindRepeating([](ui::TrackedElement* current_anchor) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
            current_anchor, kHelpBubbleNextButtonClickedEvent);
      }));
}

TutorialDescription::WaitForAnyOf::WaitForAnyOf(ElementSpecifier first,
                                                bool wait_for_event)
    : HiddenStep(ui::ElementIdentifier(),
                 ui::InteractionSequence::StepType::kSubsequence) {
  subsequence_mode_ = ui::InteractionSequence::SubsequenceMode::kAtLeastOne;
  Or(first, wait_for_event);
}

TutorialDescription::WaitForAnyOf& TutorialDescription::WaitForAnyOf::Or(
    ElementSpecifier element,
    bool wait_for_event) {
  branches_.emplace_back(
      AlwaysRun(), Steps(wait_for_event ? HiddenStep::WaitForShowEvent(element)
                                        : HiddenStep::WaitForShown(element)));
  return *this;
}

// static
TutorialDescription::ConditionalCallback TutorialDescription::AlwaysRun() {
  return base::BindRepeating([](const ui::TrackedElement*) { return true; });
}

// static
TutorialDescription::ConditionalCallback TutorialDescription::RunIfPresent() {
  return base::BindRepeating(
      [](const ui::TrackedElement* el) { return el != nullptr; });
}

}  // namespace user_education

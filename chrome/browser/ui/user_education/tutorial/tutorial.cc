// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"

#include "base/bind.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

Tutorial::StepBuilder::StepBuilder()
    : step_builder_(std::make_unique<ui::InteractionSequence::StepBuilder>()) {}
Tutorial::StepBuilder::~StepBuilder() = default;

// static
std::unique_ptr<ui::InteractionSequence::Step>
Tutorial::StepBuilder::BuildFromDescriptionStep(
    TutorialDescription::Step step,
    absl::optional<std::pair<int, int>> progress,
    TutorialService* tutorial_service,
    TutorialBubbleFactoryRegistry* bubble_factory_registry) {
  Tutorial::StepBuilder step_builder;

  step_builder.SetAnchorElementID(step.element_id)
      .SetTitleText(step.title_text)
      .SetBodyText(step.body_text)
      .SetStepType(step.step_type)
      .SetProgress(progress)
      .SetArrow(step.arrow);

  return step_builder.Build(tutorial_service, bubble_factory_registry);
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetAnchorElementID(
    ui::ElementIdentifier element_id_) {
  step_builder_->SetElementID(element_id_);
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetTitleText(
    absl::optional<std::u16string> title_text_) {
  this->title_text = title_text_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetBodyText(
    absl::optional<std::u16string> body_text_) {
  this->body_text = body_text_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetStepType(
    ui::InteractionSequence::StepType step_type_) {
  step_builder_->SetType(step_type_);
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetProgress(
    absl::optional<std::pair<int, int>> progress_) {
  this->progress = progress_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetArrow(
    TutorialDescription::Step::Arrow arrow_) {
  this->arrow = arrow_;
  return *this;
}

std::unique_ptr<ui::InteractionSequence::Step> Tutorial::StepBuilder::Build(
    TutorialService* tutorial_service,
    TutorialBubbleFactoryRegistry* bubble_factory_registry) {
  step_builder_->SetStartCallback(
      BuildShowBubbleCallback(tutorial_service, bubble_factory_registry));
  step_builder_->SetEndCallback(BuildHideBubbleCallback(tutorial_service));

  return step_builder_->Build();
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildShowBubbleCallback(
    TutorialService* tutorial_service,
    TutorialBubbleFactoryRegistry* bubble_factory_registry) {
  return base::BindOnce(
      [](TutorialService* tutorial_service,
         TutorialBubbleFactoryRegistry* bubble_factory_registry,
         absl::optional<std::u16string> title_text_,
         absl::optional<std::u16string> body_text_,
         TutorialDescription::Step::Arrow arrow_,
         absl::optional<std::pair<int, int>> progress_,
         ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        DCHECK(tutorial_service);
        DCHECK(bubble_factory_registry);

        std::unique_ptr<TutorialBubble> bubble =
            bubble_factory_registry->CreateBubbleForTrackedElement(
                element, title_text_, body_text_, arrow_, progress_);
        tutorial_service->SetCurrentBubble(std::move(bubble));
      },
      base::Unretained(tutorial_service),
      base::Unretained(bubble_factory_registry), title_text, body_text, arrow,
      progress);
}

ui::InteractionSequence::StepEndCallback
Tutorial::StepBuilder::BuildHideBubbleCallback(
    TutorialService* tutorial_service) {
  return base::BindOnce(
      [](TutorialService* tutorial_service, ui::TrackedElement* element) {
        tutorial_service->HideCurrentBubbleIfShowing();
      },
      base::Unretained(tutorial_service));
}

Tutorial::Builder::Builder()
    : builder_(std::make_unique<ui::InteractionSequence::Builder>()) {}
Tutorial::Builder::~Builder() = default;

// static
std::unique_ptr<Tutorial> Tutorial::Builder::BuildFromDescription(
    TutorialDescription description,
    TutorialService* tutorial_service,
    TutorialBubbleFactoryRegistry* bubble_factory_registry,
    ui::ElementContext context) {
  Tutorial::Builder builder;
  builder.SetContext(context);

  int visible_step_count = 0;
  for (const auto& step : description.steps) {
    if (step.ShouldShowBubble())
      visible_step_count++;
  }
  DCHECK(visible_step_count > 0);

  int current_step = 0;
  for (const auto& step : description.steps) {
    builder.AddStep(Tutorial::StepBuilder::BuildFromDescriptionStep(
        step, std::pair<int, int>(current_step, visible_step_count),
        tutorial_service, bubble_factory_registry));
    if (step.ShouldShowBubble())
      current_step++;
  }
  DCHECK(visible_step_count == current_step);

  builder.SetAbortedCallback(base::BindOnce(
      [](TutorialService* tutorial_service, ui::TrackedElement* last_element,
         ui::ElementIdentifier last_id,
         ui::InteractionSequence::StepType last_step_type,
         ui::InteractionSequence::AbortedReason aborted_reason) {
        tutorial_service->AbortTutorial();
      },
      tutorial_service));

  builder.SetCompletedCallback(base::BindOnce(
      [](TutorialService* tutorial_service) {
        tutorial_service->CompleteTutorial();
      },
      tutorial_service));

  return builder.Build();
}

Tutorial::Builder& Tutorial::Builder::AddStep(
    std::unique_ptr<ui::InteractionSequence::Step> step) {
  builder_->AddStep(std::move(step));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetAbortedCallback(
    ui::InteractionSequence::AbortedCallback callback) {
  builder_->SetAbortedCallback(std::move(callback));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetCompletedCallback(
    ui::InteractionSequence::CompletedCallback callback) {
  builder_->SetCompletedCallback(std::move(callback));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetContext(
    ui::ElementContext element_context) {
  builder_->SetContext(element_context);
  return *this;
}

std::unique_ptr<Tutorial> Tutorial::Builder::Build() {
  return absl::WrapUnique(new Tutorial(builder_->Build()));
}

Tutorial::Tutorial(
    std::unique_ptr<ui::InteractionSequence> interaction_sequence)
    : interaction_sequence_(std::move(interaction_sequence)) {}
Tutorial::~Tutorial() = default;

void Tutorial::Start() {
  DCHECK(interaction_sequence_);
  if (interaction_sequence_)
    interaction_sequence_->Start();
}

void Tutorial::Abort() {
  if (interaction_sequence_)
    interaction_sequence_.reset();
}

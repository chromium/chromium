// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

Tutorial::StepBuilder::StepBuilder() = default;
Tutorial::StepBuilder::StepBuilder(const TutorialDescription::Step& step)
    : step_(step) {}
Tutorial::StepBuilder::~StepBuilder() = default;

// static
std::unique_ptr<ui::InteractionSequence::Step>
Tutorial::StepBuilder::BuildFromDescriptionStep(
    const TutorialDescription::Step& step,
    absl::optional<std::pair<int, int>> progress,
    bool is_last_step,
    bool can_be_restarted,
    TutorialService* tutorial_service) {
  Tutorial::StepBuilder step_builder(step);
  step_builder.SetProgress(progress)
      .SetIsLastStep(is_last_step)
      .SetCanBeRestarted(can_be_restarted);

  return step_builder.Build(tutorial_service);
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetAnchorElementID(
    ui::ElementIdentifier anchor_element_id) {
  // Element ID and Element Name are mutually exclusive
  DCHECK(!anchor_element_id || step_.element_name.empty());

  step_.element_id = anchor_element_id;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetAnchorElementName(
    std::string anchor_element_name) {
  // Element ID and Element Name are mutually exclusive
  DCHECK(anchor_element_name.empty() || !step_.element_id);
  step_.element_name = anchor_element_name;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetTitleTextID(
    int title_text_id) {
  step_.title_text_id = title_text_id;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetBodyTextID(int body_text_id) {
  step_.body_text_id = body_text_id;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetStepType(
    ui::InteractionSequence::StepType step_type_,
    ui::CustomElementEventType event_type_) {
  DCHECK_EQ(step_type_ == ui::InteractionSequence::StepType::kCustomEvent,
            static_cast<bool>(event_type_))
      << "`event_type_` should be set if and only if `step_type_` is "
         "kCustomEvent.";
  step_.step_type = step_type_;
  step_.event_type = event_type_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetProgress(
    absl::optional<std::pair<int, int>> progress_) {
  progress = progress_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetArrow(HelpBubbleArrow arrow_) {
  step_.arrow = arrow_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetIsLastStep(
    bool is_last_step_) {
  is_last_step = is_last_step_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetMustRemainVisible(
    bool must_remain_visible_) {
  step_.must_remain_visible = must_remain_visible_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetTransitionOnlyOnEvent(
    bool transition_only_on_event_) {
  step_.transition_only_on_event = transition_only_on_event_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetNameElementsCallback(
    TutorialDescription::NameElementsCallback name_elements_callback_) {
  step_.name_elements_callback = name_elements_callback_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetCanBeRestarted(
    bool can_be_restarted_) {
  can_be_restarted = can_be_restarted_;
  return *this;
}

std::unique_ptr<ui::InteractionSequence::Step> Tutorial::StepBuilder::Build(
    TutorialService* tutorial_service) {
  std::unique_ptr<ui::InteractionSequence::StepBuilder>
      interaction_sequence_step_builder =
          std::make_unique<ui::InteractionSequence::StepBuilder>();

  interaction_sequence_step_builder->SetContext(step_.context_mode);

  if (step_.element_id)
    interaction_sequence_step_builder->SetElementID(step_.element_id);

  if (!step_.element_name.empty())
    interaction_sequence_step_builder->SetElementName(step_.element_name);

  interaction_sequence_step_builder->SetType(step_.step_type, step_.event_type);

  if (step_.must_remain_visible.has_value())
    interaction_sequence_step_builder->SetMustRemainVisible(
        step_.must_remain_visible.value());

  interaction_sequence_step_builder->SetTransitionOnlyOnEvent(
      step_.transition_only_on_event);

  interaction_sequence_step_builder->SetStartCallback(
      BuildStartCallback(tutorial_service));
  interaction_sequence_step_builder->SetEndCallback(
      BuildHideBubbleCallback(tutorial_service));

  return interaction_sequence_step_builder->Build();
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildStartCallback(TutorialService* tutorial_service) {
  // get show bubble callback
  ui::InteractionSequence::StepStartCallback maybe_show_bubble_callback =
      BuildMaybeShowBubbleCallback(tutorial_service);

  return base::BindOnce(
      [](TutorialDescription::NameElementsCallback name_elements_callback,
         ui::InteractionSequence::StepStartCallback maybe_show_bubble_callback,
         ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        if (name_elements_callback)
          name_elements_callback.Run(sequence, element);
        if (maybe_show_bubble_callback)
          std::move(maybe_show_bubble_callback).Run(sequence, element);
      },
      step_.name_elements_callback, std::move(maybe_show_bubble_callback));
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildMaybeShowBubbleCallback(
    TutorialService* tutorial_service) {
  if (!step_.ShouldShowBubble())
    return ui::InteractionSequence::StepStartCallback();

  const std::u16string title_text =
      step_.title_text_id ? l10n_util::GetStringUTF16(step_.title_text_id)
                          : std::u16string();

  const std::u16string body_text =
      step_.body_text_id ? l10n_util::GetStringUTF16(step_.body_text_id)
                         : std::u16string();

  return base::BindOnce(
      [](TutorialService* tutorial_service, std::u16string title_text_,
         std::u16string body_text_, HelpBubbleArrow arrow_,
         absl::optional<std::pair<int, int>> progress_, bool is_last_step_,
         bool can_be_restarted_, ui::InteractionSequence* sequence,
         ui::TrackedElement* element) {
        DCHECK(tutorial_service);

        tutorial_service->HideCurrentBubbleIfShowing();

        HelpBubbleParams params;
        params.title_text = title_text_;
        params.body_text = body_text_;
        params.progress = progress_;
        params.arrow = arrow_;
        params.timeout = base::TimeDelta();
        params.dismiss_callback = base::BindOnce(
            [](absl::optional<int> step_number,
               TutorialService* tutorial_service) {
              tutorial_service->AbortTutorial(step_number);
            },
            progress_.has_value() ? absl::make_optional(progress_.value().first)
                                  : absl::nullopt,
            base::Unretained(tutorial_service));

        if (is_last_step_) {
          params.body_icon = &vector_icons::kCelebrationIcon;
          params.body_icon_alt_text =
              tutorial_service->GetBodyIconAltText(true);
          params.dismiss_callback = base::BindOnce(
              [](TutorialService* tutorial_service) {
                tutorial_service->CompleteTutorial();
              },
              base::Unretained(tutorial_service));

          if (can_be_restarted_) {
            HelpBubbleButtonParams restart_button;
            restart_button.text =
                l10n_util::GetStringUTF16(IDS_TUTORIAL_RESTART_TUTORIAL);
            restart_button.is_default = false;
            restart_button.callback = base::BindOnce(
                [](TutorialService* tutorial_service) {
                  tutorial_service->RestartTutorial();
                },
                base::Unretained(tutorial_service));
            params.buttons.emplace_back(std::move(restart_button));
          }

          HelpBubbleButtonParams close_button;
          close_button.text =
              l10n_util::GetStringUTF16(IDS_TUTORIAL_CLOSE_TUTORIAL);
          close_button.is_default = true;
          close_button.callback = base::BindOnce(
              [](TutorialService* tutorial_service) {
                tutorial_service->CompleteTutorial();
              },
              base::Unretained(tutorial_service));
          params.buttons.emplace_back(std::move(close_button));
        }
        params.close_button_alt_text =
            l10n_util::GetStringUTF16(IDS_CLOSE_TUTORIAL);

        std::unique_ptr<HelpBubble> bubble =
            tutorial_service->bubble_factory_registry()->CreateHelpBubble(
                element, std::move(params));
        tutorial_service->SetCurrentBubble(std::move(bubble), is_last_step_);
      },
      base::Unretained(tutorial_service), title_text, body_text, step_.arrow,
      progress, is_last_step, can_be_restarted);
}

ui::InteractionSequence::StepEndCallback
Tutorial::StepBuilder::BuildHideBubbleCallback(
    TutorialService* tutorial_service) {
  return base::BindOnce(
      [](TutorialService* tutorial_service, ui::TrackedElement* element) {},
      base::Unretained(tutorial_service));
}

Tutorial::Builder::Builder()
    : builder_(std::make_unique<ui::InteractionSequence::Builder>()) {}
Tutorial::Builder::~Builder() = default;

// static
std::unique_ptr<Tutorial> Tutorial::Builder::BuildFromDescription(
    const TutorialDescription& description,
    TutorialService* tutorial_service,
    ui::ElementContext context) {
  Tutorial::Builder builder;
  builder.SetContext(context);

  // Last step doesn't have a progress counter.
  const int max_progress =
      base::ranges::count_if(description.steps,
                             &TutorialDescription::Step::ShouldShowBubble) -
      1;

  int current_step = 0;
  for (const auto& step : description.steps) {
    const bool is_last_step = &step == &description.steps.back();
    if (!is_last_step && step.ShouldShowBubble())
      ++current_step;
    const auto progress =
        !is_last_step && max_progress > 0
            ? absl::make_optional(std::make_pair(current_step, max_progress))
            : absl::nullopt;
    builder.AddStep(Tutorial::StepBuilder::BuildFromDescriptionStep(
        step, progress, is_last_step, description.can_be_restarted,
        tutorial_service));
  }
  DCHECK_EQ(current_step, max_progress);

  // Note that the step number we are using here is not the same as the the
  // InteractionSequence::AbortCallback step (`sequence_step`) which counts all
  // steps; `current_step` in this case is the visual bubble count, which does
  // not count hidden steps.
  builder.SetAbortedCallback(base::BindOnce(
      [](int step_number, TutorialService* tutorial_service, int sequence_step,
         ui::TrackedElement* last_element, ui::ElementIdentifier last_id,
         ui::InteractionSequence::StepType last_step_type,
         ui::InteractionSequence::AbortedReason aborted_reason,
         std::string description) {
        tutorial_service->AbortTutorial(step_number);
      },
      current_step, tutorial_service));

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

}  // namespace user_education

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
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
  step_builder.progress_ = progress;
  step_builder.is_last_step_ = is_last_step;
  step_builder.can_be_restarted_ = can_be_restarted;
  return step_builder.Build(tutorial_service);
}

std::unique_ptr<ui::InteractionSequence::Step> Tutorial::StepBuilder::Build(
    TutorialService* tutorial_service) {
  std::unique_ptr<ui::InteractionSequence::StepBuilder>
      interaction_sequence_step_builder =
          std::make_unique<ui::InteractionSequence::StepBuilder>();

  interaction_sequence_step_builder->SetContext(step_.context_mode_);

  if (step_.element_id_) {
    interaction_sequence_step_builder->SetElementID(step_.element_id_);
  }

  if (!step_.element_name_.empty()) {
    interaction_sequence_step_builder->SetElementName(step_.element_name_);
  }

  interaction_sequence_step_builder->SetType(step_.step_type_,
                                             step_.event_type_);

  if (step_.must_remain_visible_.has_value()) {
    interaction_sequence_step_builder->SetMustRemainVisible(
        step_.must_remain_visible_.value());
  }

  if (step_.must_be_visible_.has_value()) {
    interaction_sequence_step_builder->SetMustBeVisibleAtStart(
        step_.must_be_visible_.value());
  }

  interaction_sequence_step_builder->SetTransitionOnlyOnEvent(
      step_.transition_only_on_event_);

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
      step_.name_elements_callback_, std::move(maybe_show_bubble_callback));
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildMaybeShowBubbleCallback(
    TutorialService* tutorial_service) {
  if (!step_.ShouldShowBubble())
    return ui::InteractionSequence::StepStartCallback();

  const std::u16string title_text =
      step_.title_text_id_ ? l10n_util::GetStringUTF16(step_.title_text_id_)
                           : std::u16string();

  const std::u16string body_text =
      step_.body_text_id_ ? l10n_util::GetStringUTF16(step_.body_text_id_)
                          : std::u16string();

  return base::BindOnce(
      [](TutorialService* tutorial_service, std::u16string title_text_,
         std::u16string body_text_, HelpBubbleArrow arrow_,
         absl::optional<std::pair<int, int>> progress, bool is_last_step,
         bool can_be_restarted,
         TutorialDescription::NextButtonCallback next_button_callback,
         HelpBubbleParams::ExtendedProperties extended_properties,
         ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        DCHECK(tutorial_service);

        tutorial_service->HideCurrentBubbleIfShowing();

        HelpBubbleParams params;
        params.extended_properties = std::move(extended_properties);
        params.title_text = title_text_;
        params.body_text = body_text_;
        params.progress = progress;
        params.arrow = arrow_;
        params.timeout = base::TimeDelta();
        params.dismiss_callback = base::BindOnce(
            [](absl::optional<int> step_number,
               TutorialService* tutorial_service) {
              tutorial_service->AbortTutorial(step_number);
            },
            progress.has_value() ? absl::make_optional(progress.value().first)
                                 : absl::nullopt,
            base::Unretained(tutorial_service));

        if (is_last_step) {
          params.body_icon = &vector_icons::kCelebrationIcon;
          params.body_icon_alt_text =
              tutorial_service->GetBodyIconAltText(true);
          params.dismiss_callback = base::BindOnce(
              [](TutorialService* tutorial_service) {
                tutorial_service->CompleteTutorial();
              },
              base::Unretained(tutorial_service));

          if (can_be_restarted) {
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
        } else if (next_button_callback) {
          HelpBubbleButtonParams next_button;
          next_button.text =
              l10n_util::GetStringUTF16(IDS_TUTORIAL_NEXT_BUTTON);
          next_button.is_default = true;
          next_button.callback = base::BindOnce(
              [](TutorialDescription::NextButtonCallback next_button_callback,
                 ui::TrackedElement* current_anchor) {
                std::move(next_button_callback).Run(current_anchor);
              },
              std::move(next_button_callback), element);
          params.buttons.emplace_back(std::move(next_button));
        }

        params.close_button_alt_text =
            l10n_util::GetStringUTF16(IDS_CLOSE_TUTORIAL);

        std::unique_ptr<HelpBubble> bubble =
            tutorial_service->bubble_factory_registry()->CreateHelpBubble(
                element, std::move(params));
        tutorial_service->SetCurrentBubble(std::move(bubble), is_last_step);
      },
      base::Unretained(tutorial_service), title_text, body_text, step_.arrow_,
      progress_, is_last_step_, can_be_restarted_, step_.next_button_callback_,
      step_.extended_properties_);
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
      [](int step_number, TutorialService* tutorial_service,
         const ui::InteractionSequence::AbortedData&) {
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
  return base::WrapUnique(new Tutorial(builder_->Build()));
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

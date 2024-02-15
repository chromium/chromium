// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial.h"

#include <optional>

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
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

int CountProgress(const std::vector<TutorialDescription::Step>& steps) {
  int result = 0;
  for (const auto& step : steps) {
    if (step.ShouldShowBubble()) {
      ++result;
    } else if (step.step_type() ==
               ui::InteractionSequence::StepType::kSubsequence) {
      CHECK(!step.branches().empty());
      int to_add = 0;
      for (const auto& branch : step.branches()) {
        to_add = std::max(to_add, CountProgress(branch.second));
      }
      result += to_add;
    }
  }
  return result;
}

void CommonStepBuilderSetup(ui::InteractionSequence::StepBuilder& builder,
                            const TutorialDescription::Step& step) {
  builder.SetContext(step.context_mode());

  if (step.element_id()) {
    builder.SetElementID(step.element_id());
  }

  if (!step.element_name().empty()) {
    builder.SetElementName(step.element_name());
  }

  if (step.must_remain_visible().has_value()) {
    builder.SetMustRemainVisible(step.must_remain_visible().value());
  }

  if (step.must_be_visible().has_value()) {
    builder.SetMustBeVisibleAtStart(step.must_be_visible().value());
  }

  builder.SetTransitionOnlyOnEvent(step.transition_only_on_event());
}

// Adds a branch of a conditional to `builder` based on `condition` and `steps`.
void AddStepBuilderSubsequence(
    ui::InteractionSequence::StepBuilder& builder,
    ui::InteractionSequence::SubsequenceCondition condition,
    const std::vector<TutorialDescription::Step>& steps,
    int max_progress,
    int& current_progress,
    bool is_terminal,
    bool can_be_restarted,
    int complete_button_text_id,
    TutorialService* tutorial_service) {
  ui::InteractionSequence::Builder subsequence;
  for (const auto& step : steps) {
    subsequence.AddStep(Tutorial::Builder::BuildFromDescriptionStep(
        step, max_progress, current_progress,
        is_terminal && &step == &steps.back(), can_be_restarted,
        complete_button_text_id, tutorial_service));
  }
  builder.AddSubsequence(std::move(subsequence), std::move(condition));
}

}  // namespace

namespace internal {

// Step Builder provides an interface for constructing an
// InteractionSequence::Step from a TutorialDescription::Step.
// TutorialDescription is used as the basis for the TutorialStepBuilder since
// all parameters of the Description will be needed to create the bubble or
// build the interaction sequence step. In order to use the The
// TutorialStepBuilder should only be used by Tutorial::Builder to construct the
// steps in the tutorial.
class TutorialStepBuilder {
 public:
  explicit TutorialStepBuilder(const TutorialDescription::Step& step,
                               std::optional<std::pair<int, int>> progress,
                               bool is_last_step,
                               bool can_be_restarted,
                               int complete_button_text_id)
      : progress_(progress),
        is_last_step_(is_last_step),
        can_be_restarted_(can_be_restarted),
        complete_button_text_id_(complete_button_text_id),
        step_(step) {
    CHECK_NE(complete_button_text_id_, 0);
  }
  ~TutorialStepBuilder() = default;

  std::unique_ptr<ui::InteractionSequence::Step> Build(
      TutorialService* tutorial_service);

 private:
  ui::InteractionSequence::StepStartCallback BuildStartCallback(
      TutorialService* tutorial_service);

  ui::InteractionSequence::StepStartCallback BuildMaybeShowBubbleCallback(
      TutorialService* tutorial_service);

  ui::InteractionSequence::StepEndCallback BuildHideBubbleCallback(
      TutorialService* tutorial_service);

  const std::optional<std::pair<int, int>> progress_;
  const bool is_last_step_;
  const bool can_be_restarted_;
  const int complete_button_text_id_;
  const TutorialDescription::Step step_;
};

std::unique_ptr<ui::InteractionSequence::Step> TutorialStepBuilder::Build(
    TutorialService* tutorial_service) {
  ui::InteractionSequence::StepBuilder interaction_sequence_step_builder;

  interaction_sequence_step_builder.SetType(step_.step_type(),
                                            step_.event_type());

  CommonStepBuilderSetup(interaction_sequence_step_builder, step_);

  interaction_sequence_step_builder.SetStartCallback(
      BuildStartCallback(tutorial_service));
  interaction_sequence_step_builder.SetEndCallback(
      BuildHideBubbleCallback(tutorial_service));

  return interaction_sequence_step_builder.Build();
}

ui::InteractionSequence::StepStartCallback
TutorialStepBuilder::BuildStartCallback(TutorialService* tutorial_service) {
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
      step_.name_elements_callback(), std::move(maybe_show_bubble_callback));
}

ui::InteractionSequence::StepStartCallback
TutorialStepBuilder::BuildMaybeShowBubbleCallback(
    TutorialService* tutorial_service) {
  if (!step_.ShouldShowBubble())
    return ui::InteractionSequence::StepStartCallback();

  const std::u16string title_text =
      step_.title_text_id() ? l10n_util::GetStringUTF16(step_.title_text_id())
                            : std::u16string();

  const std::u16string body_text =
      step_.body_text_id() ? l10n_util::GetStringUTF16(step_.body_text_id())
                           : std::u16string();

  const std::u16string screenreader_text =
      step_.screenreader_text_id()
          ? l10n_util::GetStringUTF16(step_.screenreader_text_id())
          : std::u16string();

  return base::BindOnce(
      [](TutorialService* tutorial_service, std::u16string title_text_,
         std::u16string body_text_, std::u16string screenreader_text_,
         HelpBubbleArrow arrow_, std::optional<std::pair<int, int>> progress,
         bool is_last_step, bool can_be_restarted, int complete_button_text_id,
         TutorialDescription::NextButtonCallback next_button_callback,
         HelpBubbleParams::ExtendedProperties extended_properties,
         ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        DCHECK(tutorial_service);

        tutorial_service->HideCurrentBubbleIfShowing();

        HelpBubbleParams params;
        params.extended_properties = std::move(extended_properties);
        params.title_text = title_text_;
        params.body_text = body_text_;
        params.screenreader_text = screenreader_text_;
        params.progress = progress;
        params.arrow = arrow_;
        params.timeout = base::TimeDelta();
        params.dismiss_callback = base::BindOnce(
            [](std::optional<int> step_number,
               TutorialService* tutorial_service) {
              tutorial_service->AbortTutorial(step_number);
            },
            progress.has_value() ? std::make_optional(progress.value().first)
                                 : std::nullopt,
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

          HelpBubbleButtonParams complete_button;
          complete_button.text =
              l10n_util::GetStringUTF16(complete_button_text_id);
          complete_button.is_default = true;
          complete_button.callback = base::BindOnce(
              [](TutorialService* tutorial_service) {
                tutorial_service->CompleteTutorial();
              },
              base::Unretained(tutorial_service));
          params.buttons.emplace_back(std::move(complete_button));
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
      base::Unretained(tutorial_service), title_text, body_text,
      screenreader_text, step_.arrow(), progress_, is_last_step_,
      can_be_restarted_, complete_button_text_id_, step_.next_button_callback(),
      step_.extended_properties());
}

ui::InteractionSequence::StepEndCallback
TutorialStepBuilder::BuildHideBubbleCallback(
    TutorialService* tutorial_service) {
  return base::BindOnce(
      [](TutorialService* tutorial_service, ui::TrackedElement* element) {},
      base::Unretained(tutorial_service));
}

}  // namespace internal

// static
std::unique_ptr<ui::InteractionSequence::Step>
Tutorial::Builder::BuildFromDescriptionStep(
    const TutorialDescription::Step& step,
    int max_progress,
    int& current_progress,
    bool is_terminal,
    bool can_be_restarted,
    int complete_button_text_id,
    TutorialService* tutorial_service) {
  if (step.step_type() == ui::InteractionSequence::StepType::kSubsequence) {
    CHECK(!step.branches().empty());
    CHECK(!step.branches()[0].second.empty());
    ui::InteractionSequence::StepBuilder builder;
    builder.SetSubsequenceMode(step.subsequence_mode());
    CommonStepBuilderSetup(builder, step);
    const int prev_progress = current_progress;
    for (auto& branch : step.branches()) {
      int branch_progress = prev_progress;
      AddStepBuilderSubsequence(
          builder,
          base::BindOnce(
              [](TutorialDescription::ConditionalCallback callback,
                 const ui::InteractionSequence*,
                 const ui::TrackedElement* el) { return callback.Run(el); },
              std::move(branch.first)),
          std::move(branch.second), max_progress, branch_progress, is_terminal,
          can_be_restarted, complete_button_text_id, tutorial_service);
      current_progress = std::max(current_progress, branch_progress);
    }
    return builder.Build();
  } else {
    std::optional<std::pair<int, int>> progress;
    if (step.ShouldShowBubble()) {
      ++current_progress;
      if (!is_terminal) {
        DCHECK_LE(current_progress, max_progress)
            << "Intermediate/progress steps should never exceed the maximum "
               "progress.";
        progress = std::make_pair(current_progress, max_progress);
      } else {
        DCHECK_LE(current_progress, max_progress + 1)
            << "Terminal step should always be immediately after final "
               "progress step.";
        current_progress = max_progress + 1;
      }
    } else {
      DCHECK(!is_terminal)
          << "Hidden step should never be the last step in a sequence.";
    }
    internal::TutorialStepBuilder step_builder(
        step, progress, is_terminal, can_be_restarted, complete_button_text_id);
    return step_builder.Build(tutorial_service);
  }
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
  const int max_progress = CountProgress(description.steps) - 1;
  int current_progress = 0;
  for (const auto& step : description.steps) {
    builder.AddStep(BuildFromDescriptionStep(
        step, max_progress, current_progress,
        &step == &description.steps.back(), description.can_be_restarted,
        description.complete_button_text_id, tutorial_service));
  }
  DCHECK_EQ(current_progress, max_progress + 1);

  // Note that the step number we are using here is not the same as the the
  // InteractionSequence::AbortCallback step (`sequence_step`) which counts all
  // steps; `current_step` in this case is the visual bubble count, which does
  // not count hidden steps.
  builder.SetAbortedCallback(base::BindOnce(
      [](int step_number, TutorialService* tutorial_service,
         const ui::InteractionSequence::AbortedData&) {
        tutorial_service->AbortTutorial(step_number);
      },
      max_progress, tutorial_service));

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

void Tutorial::SetState(std::unique_ptr<ScopedTutorialState> tutorial_state) {
  CHECK(tutorial_state.get());
  tutorial_state_ = std::move(tutorial_state);
}

}  // namespace user_education

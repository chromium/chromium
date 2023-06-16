// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_

#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace user_education {

class TutorialService;

// Tutorials are a user initiated IPH which spans 1 or more Interactions.
// It utilizes the InteractionSequence Framework to provide a tracked list of
// interactions with tracked elements.
//
// Each tutorial consists of a list of InteractionSequence steps which, in the
// default case, create a HelpBubble which is implementation specific to
// the platform the tutorial is written for. It is possible to create custom
// InteractionSequenceSteps when using the traditional constructor and not
// using the TutorialStepBuilder.
//
// Because of implementation details in InteractionSequence, a tutorial can only
// be run once, see documentation for InteractionSequence.
//
// Basic tutorials use a TutorialDescription struct and the
// Builder::BuildFromDescription method to construct the tutorial.
//
// the end user of a Tutorial would define a tutorial description in a
// TutorialRegistry, for the platform the tutorial is implemented on. (see
// BrowserTutorialServiceFactory)
//
// TODO: Provide an in-depth readme.md for tutorials
//
class Tutorial {
 public:
  ~Tutorial();

  // Step Builder provides an interface for constructing an
  // InteractionSequence::Step from a TutorialDescription::Step.
  // TutorialDescription is used as the basis for the StepBuilder since all
  // parameters of the Description will be needed to create the bubble or build
  // the interaction sequence step. In order to use the The StepBuilder should
  // only be used by Tutorial::Builder to construct the steps in the tutorial.
  class StepBuilder {
   public:
    StepBuilder();
    StepBuilder(const StepBuilder&) = delete;
    StepBuilder& operator=(const StepBuilder&) = delete;
    ~StepBuilder();

    // Constructs the InteractionSequenceStepDirectly from the
    // TutorialDescriptionStep. This method is used by
    // Tutorial::Builder::BuildFromDescription to create tutorials.
    static std::unique_ptr<ui::InteractionSequence::Step>
    BuildFromDescriptionStep(const TutorialDescription::Step& step,
                             absl::optional<std::pair<int, int>> progress,
                             bool is_last_step,
                             bool can_be_restarted,
                             TutorialService* tutorial_service);

   private:
    explicit StepBuilder(const TutorialDescription::Step& step);

    std::unique_ptr<ui::InteractionSequence::Step> Build(
        TutorialService* tutorial_service);

    absl::optional<std::pair<int, int>> progress_;
    bool is_last_step_ = false;
    bool can_be_restarted_ = false;

    ui::InteractionSequence::StepStartCallback BuildStartCallback(
        TutorialService* tutorial_service);

    ui::InteractionSequence::StepStartCallback BuildMaybeShowBubbleCallback(
        TutorialService* tutorial_service);

    ui::InteractionSequence::StepEndCallback BuildHideBubbleCallback(
        TutorialService* tutorial_service);
    TutorialDescription::Step step_;
  };

  class Builder {
   public:
    Builder();
    ~Builder();

    static std::unique_ptr<Tutorial> BuildFromDescription(
        const TutorialDescription& description,
        TutorialService* tutorial_service,
        ui::ElementContext context);

    Builder(const Builder& other) = delete;
    void operator=(Builder& other) = delete;

    Builder& AddStep(std::unique_ptr<ui::InteractionSequence::Step> step);
    Builder& SetContext(ui::ElementContext element_context);

    Builder& SetAbortedCallback(
        ui::InteractionSequence::AbortedCallback callback);
    Builder& SetCompletedCallback(
        ui::InteractionSequence::CompletedCallback callback);

    std::unique_ptr<Tutorial> Build();

   private:
    std::unique_ptr<ui::InteractionSequence::Builder> builder_;
  };

  // Starts the Tutorial. has the same constraints as
  // InteractionSequence::Start.
  void Start();

  // Cancels the Tutorial. Calls the destructor of the InteractionSequence,
  // calling the abort callback if necessary.
  void Abort();

 private:
  // Tutorial Constructor that takes an InteractionSequence. Should only be
  // used in cases where custom step logic must be called
  explicit Tutorial(
      std::unique_ptr<ui::InteractionSequence> interaction_sequence);

  // The Interaction Sequence which controls the tutorial bubbles opening and
  // closing
  std::unique_ptr<ui::InteractionSequence> interaction_sequence_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_

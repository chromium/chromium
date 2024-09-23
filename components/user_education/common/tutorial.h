// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_

#include "base/gtest_prod_util.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
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

  class Builder {
   public:
    Builder();
    ~Builder();

    static std::unique_ptr<Tutorial> BuildFromDescription(
        const TutorialDescription& description,
        TutorialService* tutorial_service,
        ui::ElementContext context);

    // Constructs the InteractionSequenceStepDirectly from the
    // TutorialDescriptionStep. This method is used by
    // Tutorial::Builder::BuildFromDescription to create tutorials.
    //
    // Because tutorials can branch, this step may represent one or more
    // conditional subsequences. `current_progress` will be updated based on all
    // steps added as a result of adding this step, including in subsequences.
    // Also,
    static std::unique_ptr<ui::InteractionSequence::Step>
    BuildFromDescriptionStep(const TutorialDescription::Step& step,
                             int max_progress,
                             int& current_progress,
                             bool is_terminal,
                             bool can_be_restarted,
                             int complete_button_text_id,
                             TutorialService* tutorial_service);

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

  // Sets the temporary state associated with the tutorial.
  void SetState(std::unique_ptr<ScopedTutorialState> tutorial_state);

 private:
  // Tutorial Constructor that takes an InteractionSequence. Should only be
  // used in cases where custom step logic must be called
  explicit Tutorial(
      std::unique_ptr<ui::InteractionSequence> interaction_sequence);

  // The temporary state associated with the tutorial. The state is reset
  // which this goes out of scope
  std::unique_ptr<ScopedTutorialState> tutorial_state_;

  // The Interaction Sequence which controls the tutorial bubbles opening and
  // closing
  std::unique_ptr<ui::InteractionSequence> interaction_sequence_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_H_

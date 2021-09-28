// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

class TutorialService;

// Tutorials are a user initiated IPH which spans 1 or more Interactions.
// It utilizes the InteractionSequence Framework to provide a tracked list of
// interactions with tracked elements.
//
// Each tutorial consists of a list of InteractionSequence steps which, in the
// default case, create a TutorialBubble which is implementation specific to
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

  class StepBuilder : public TutorialDescription::Step {
   public:
    StepBuilder();
    ~StepBuilder();

    static std::unique_ptr<ui::InteractionSequence::Step>
    BuildFromDescriptionStep(
        TutorialDescription::Step step,
        absl::optional<std::pair<int, int>> progress,
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

    StepBuilder& SetAnchorElementID(ui::ElementIdentifier anchor_element_);
    StepBuilder& SetTitleText(absl::optional<std::u16string> title_text_);
    StepBuilder& SetBodyText(absl::optional<std::u16string> body_text_);
    StepBuilder& SetStepType(ui::InteractionSequence::StepType step_type_);
    StepBuilder& SetProgress(absl::optional<std::pair<int, int>> progress_);
    StepBuilder& SetArrow(TutorialDescription::Step::Arrow arrow_);

    std::unique_ptr<ui::InteractionSequence::Step> Build(
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

   private:
    absl::optional<std::pair<int, int>> progress;
    ui::InteractionSequence::StepStartCallback BuildShowBubbleCallback(
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);
    ui::InteractionSequence::StepEndCallback BuildHideBubbleCallback(
        TutorialService* tutorial_service);

    std::unique_ptr<ui::InteractionSequence::StepBuilder> step_builder_;
  };

  class Builder {
   public:
    Builder();
    ~Builder();

    static std::unique_ptr<Tutorial> BuildFromDescription(
        TutorialDescription description,
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry,
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

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_

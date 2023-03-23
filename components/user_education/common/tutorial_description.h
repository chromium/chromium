// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_

#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "components/user_education/common/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace user_education {

// Holds the data required to properly store histograms for a given tutorial.
// Abstract base class because best practice is to statically declare
// histograms and so we need some compile-time polymorphism to actually
// implement the RecordXXX() calls.
//
// Use MakeTutorialHistograms() below to create a concrete instance of this
// class.
class TutorialHistograms {
 public:
  TutorialHistograms() = default;
  TutorialHistograms(const TutorialHistograms& other) = delete;
  virtual ~TutorialHistograms() = default;
  void operator=(const TutorialHistograms& other) = delete;

  // Records whether the tutorial was completed or not.
  virtual void RecordComplete(bool value) = 0;

  // Records the step on which the tutorial was aborted.
  virtual void RecordAbortStep(int step) = 0;

  // Records whether, when an IPH offered the tutorial, the user opted into
  // seeing the tutorial or not.
  virtual void RecordIphLinkClicked(bool value) = 0;

  // Records whether, when an IPH offered the tutorial, the user opted into
  // seeing the tutorial or not.
  virtual void RecordStartedFromWhatsNewPage(bool value) = 0;

  // This is used for consistency-checking only.
  virtual const std::string& GetTutorialPrefix() const = 0;
};

namespace internal {

constexpr char kTutorialHistogramPrefix[] = "Tutorial.";

template <const char histogram_name[]>
class TutorialHistogramsImpl : public TutorialHistograms {
 public:
  explicit TutorialHistogramsImpl(int max_steps)
      : histogram_name_(histogram_name),
        completed_name_(kTutorialHistogramPrefix + histogram_name_ +
                        ".Completion"),
        aborted_name_(kTutorialHistogramPrefix + histogram_name_ +
                      ".AbortStep"),
        iph_link_clicked_name_(kTutorialHistogramPrefix + histogram_name_ +
                               ".IPHLinkClicked"),
        whats_new_page_name_(kTutorialHistogramPrefix + histogram_name_ +
                             ".StartedFromWhatsNewPage"),
        max_steps_(max_steps) {}
  ~TutorialHistogramsImpl() override = default;

 protected:
  void RecordComplete(bool value) override {
    UMA_HISTOGRAM_BOOLEAN(completed_name_, value);
  }

  void RecordAbortStep(int step) override {
    UMA_HISTOGRAM_EXACT_LINEAR(aborted_name_, step, max_steps_);
  }

  void RecordIphLinkClicked(bool value) override {
    UMA_HISTOGRAM_BOOLEAN(iph_link_clicked_name_, value);
  }

  void RecordStartedFromWhatsNewPage(bool value) override {
    UMA_HISTOGRAM_BOOLEAN(whats_new_page_name_, value);
  }

  const std::string& GetTutorialPrefix() const override {
    return histogram_name_;
  }

 private:
  const std::string histogram_name_;
  const std::string completed_name_;
  const std::string aborted_name_;
  const std::string iph_link_clicked_name_;
  const std::string whats_new_page_name_;
  const int max_steps_;
};

}  // namespace internal

// Call to create a tutorial-specific histograms object for use with the
// tutorial. The template parameter should be a reference to a const char[]
// that is a compile-time constant. Also remember to add a matching entry to
// the "TutorialID" variant in histograms.xml corresponding to your tutorial.
//
// Example:
//   const char kMyTutorialName[] = "MyTutorial";
//   tutorial_descriptions.histograms =
//       MakeTutorialHistograms<kMyTutorialName>(
//           tutorial_description.steps.size());
template <const char* histogram_name>
std::unique_ptr<TutorialHistograms> MakeTutorialHistograms(int max_steps) {
  return std::make_unique<internal::TutorialHistogramsImpl<histogram_name>>(
      max_steps);
}

// A Struct that provides all of the data necessary to construct a Tutorial.
// A Tutorial Description is a list of Steps for a tutorial. Each step has info
// for constructing the InteractionSequence::Step from the
// TutorialDescription::Step.
struct TutorialDescription {
  using NameElementsCallback =
      base::RepeatingCallback<bool(ui::InteractionSequence*,
                                   ui::TrackedElement*)>;

  TutorialDescription();
  ~TutorialDescription();
  TutorialDescription(TutorialDescription&& other);
  TutorialDescription& operator=(TutorialDescription&& other);

  using ContextMode = ui::InteractionSequence::ContextMode;
  using ElementSpecifier = absl::variant<ui::ElementIdentifier, std::string>;

  struct Step {
    Step();
    explicit Step(
        ElementSpecifier element_specifier,
        ui::InteractionSequence::StepType step_type_ =
            ui::InteractionSequence::StepType::kShown,
        ui::CustomElementEventType event_type_ = ui::CustomElementEventType());
    Step(int title_text_id_,
         int body_text_id_,
         ui::InteractionSequence::StepType step_type_,
         ui::ElementIdentifier element_id_,
         std::string element_name_,
         HelpBubbleArrow arrow_,
         ui::CustomElementEventType event_type_ = ui::CustomElementEventType(),
         absl::optional<bool> must_remain_visible_ = absl::nullopt,
         bool transition_only_on_event_ = false,
         NameElementsCallback name_elements_callback_ = NameElementsCallback(),
         ContextMode step_context = ContextMode::kInitial);
    Step(const Step& other);
    Step& operator=(const Step& other);
    ~Step();

    // The element used by interaction sequence to observe and attach a bubble.
    ui::ElementIdentifier element_id;

    // The element, referred to by name, used by the interaction sequence
    // to observe and potentially attach a bubble. must be non-empty.
    std::string element_name;

    // The step type for InteractionSequence::Step.
    ui::InteractionSequence::StepType step_type =
        ui::InteractionSequence::StepType::kShown;

    // The event type for the step if `step_type` is kCustomEvent.
    ui::CustomElementEventType event_type = ui::CustomElementEventType();

    // The title text to be populated in the bubble.
    int title_text_id = 0;

    // The body text to be populated in the bubble.
    int body_text_id = 0;

    // The positioning of the bubble arrow.
    HelpBubbleArrow arrow = HelpBubbleArrow::kTopRight;

    // Should the element remain visible through the entire step, this should be
    // set to false for hidden steps and for shown steps that precede hidden
    // steps on the same element. if left empty the interaction sequence will
    // decide what its value should be based on the generated
    // InteractionSequence::StepBuilder
    absl::optional<bool> must_remain_visible = absl::nullopt;

    // Should the step only be completed when an event like shown or hidden only
    // happens during current step. for more information on the implementation
    // take a look at transition_only_on_event in InteractionSequence::Step
    bool transition_only_on_event = false;

    // If set, determines whether the element in question must be visible at the
    // start of the step. If left empty the interaction sequence will choose a
    // reasonable default.
    absl::optional<bool> must_be_visible;

    // lambda which is called on the start callback of the InteractionSequence
    // which provides the interaction sequence and the current element that
    // belongs to the step. The intention for this functionality is to name one
    // or many elements using the Framework's Specific API finding an element
    // and naming it OR using the current element from the sequence as the
    // element for naming. The return value is a boolean which controls whether
    // the Interaction Sequence should continue or not. If false is returned
    // the tutorial will abort
    NameElementsCallback name_elements_callback = NameElementsCallback();

    // Where to search for the step's target element. Default is the context the
    // tutorial started in.
    ContextMode context_mode = ContextMode::kInitial;

    // returns true iff all of the required parameters exist to display a
    // bubble.
    bool ShouldShowBubble() const;

    Step& AbortIfVisibilityLost(bool must_remain_visible_) {
      must_remain_visible = must_remain_visible_;
      return *this;
    }

    Step& AbortIfNotVisible() {
      must_be_visible = true;
      return *this;
    }

    Step& NameElement(const char name_[]) {
      name_elements_callback = base::BindRepeating(
          [](const char name[], ui::InteractionSequence* sequence,
             ui::TrackedElement* element) {
            sequence->NameElement(element, base::StringPiece(name));
            return true;
          },
          name_);
      return *this;
    }

    Step& InAnyContext() {
      context_mode = TutorialDescription::ContextMode::kAny;
      return *this;
    }

    Step& InSameContext() {
      context_mode = TutorialDescription::ContextMode::kFromPreviousStep;
      return *this;
    }
  };

  // TutorialDescription::BubbleStep
  // A bubble step is a step which shows a bubble anchored to an element
  // This requires that the anchor element be visible, so this is always
  // a kShown step.
  //
  // - A bubble step must be passed an element_id or an element_name
  struct BubbleStep : public Step {
    // TutorialDescription::BubbleStep(element_id_)
    // TutorialDescription::BubbleStep(element_name_)
    explicit BubbleStep(ElementSpecifier element_specifier)
        : Step(element_specifier, ui::InteractionSequence::StepType::kShown) {}

    BubbleStep& SetBubbleTitleText(int title_text_) {
      title_text_id = title_text_;
      return *this;
    }

    BubbleStep& SetBubbleBodyText(int body_text_) {
      body_text_id = body_text_;
      return *this;
    }

    BubbleStep& SetBubbleArrow(HelpBubbleArrow arrow_) {
      arrow = arrow_;
      return *this;
    }
  };

  // TutorialDescription::HiddenStep
  // A hidden step has no bubble and waits for a UI event to occur on
  // a particular element.
  //
  // - A hidden step must be passed an element_id or an element_name
  struct HiddenStep : public Step {
    // HiddenStep::WaitForShowEvent(element_id_)
    // HiddenStep::WaitForShowEvent(element_name_)
    // Transition to the next step after a show event occurs
    static HiddenStep WaitForShowEvent(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kShown);
      step.transition_only_on_event = true;
      return step;
    }

    // HiddenStep::WaitForHideEvent(element_id_)
    // HiddenStep::WaitForHideEvent(element_name_)
    // Transition to the next step after a hide event occurs
    static HiddenStep WaitForHideEvent(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kHidden);
      step.transition_only_on_event = true;
      return step;
    }

    // HiddenStep::WaitForActivateEvent(element_id_)
    // HiddenStep::WaitForActivateEvent(element_name_)
    // Transition to the next step after an activation event occurs
    static HiddenStep WaitForActivateEvent(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kActivated);
      step.transition_only_on_event = true;
      return step;
    }

    // HiddenStep::WaitForShown(element_id_)
    // HiddenStep::WaitForShown(element_name_)
    // Transition to the next step if anchor is, or becomes, visible
    static HiddenStep WaitForShown(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kShown);
      step.transition_only_on_event = false;
      return step;
    }

    // HiddenStep::WaitForHidden(element_id_)
    // HiddenStep::WaitForHidden(element_name_)
    // Transition to the next step if anchor is, or becomes, hidden
    static HiddenStep WaitForHidden(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kHidden);
      step.transition_only_on_event = false;
      return step;
    }

    // HiddenStep::WaitForActivated(element_id_)
    // HiddenStep::WaitForActivated(element_name_)
    // Transition to the next step if anchor is, or becomes, activated
    static HiddenStep WaitForActivated(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kActivated);
      step.transition_only_on_event = false;
      return step;
    }

   private:
    explicit HiddenStep(ElementSpecifier element_specifier,
                        ui::InteractionSequence::StepType step_type)
        : Step(element_specifier, step_type) {}
  };

  // TutorialDescription::EventStep
  // An event step is a special case of a HiddenStep that waits for
  // a custom event to be fired programmatically.
  //
  // - This step must be passed an event_id
  // - Additionally, you can also pass an element_id or element_name if
  // the event should occur specifically on a given element
  struct EventStep : public Step {
    // TutorialDescription::EventStep(event_id_)
    explicit EventStep(ui::CustomElementEventType event_type_)
        : Step(ui::ElementIdentifier(),
               ui::InteractionSequence::StepType::kCustomEvent,
               event_type_) {}

    // TutorialDescription::EventStep(event_id_, element_id_)
    // TutorialDescription::EventStep(event_id_, element_name_)
    EventStep(ui::CustomElementEventType event_type_,
              ElementSpecifier element_specifier)
        : Step(element_specifier,
               ui::InteractionSequence::StepType::kCustomEvent,
               event_type_) {}
  };

  // the list of TutorialDescription steps
  std::vector<Step> steps;

  // The histogram data to use. Use MakeTutorialHistograms() above to create a
  // value to use, if you want to record specific histograms for this tutorial.
  std::unique_ptr<TutorialHistograms> histograms;

  // The ability for the tutorial to be restarted. In some cases tutorials can
  // leave the UI in a state where it can not re-run the tutorial. In these
  // cases this flag should be set to false so that the restart tutorial button
  // is not displayed.
  bool can_be_restarted = false;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_

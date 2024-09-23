// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/user_education_metadata.h"
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
  void operator=(const TutorialHistograms& other) = delete;
  virtual ~TutorialHistograms() = default;

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

// A class that manages a temporary state associated with the tutorial.
// A new object via the temporary setup callback when a new tutorial is started
// and maintained through the lifetime of the tutorial.
class ScopedTutorialState {
 public:
  explicit ScopedTutorialState(ui::ElementContext context);
  virtual ~ScopedTutorialState();

  ui::ElementContext context() const { return context_; }

 private:
  const ui::ElementContext context_;
};

// A Struct that provides all of the data necessary to construct a Tutorial.
// A Tutorial Description is a list of Steps for a tutorial. Each step has info
// for constructing the InteractionSequence::Step from the
// TutorialDescription::Step.
struct TutorialDescription {
  using NameElementsCallback =
      base::RepeatingCallback<bool(ui::InteractionSequence*,
                                   ui::TrackedElement*)>;
  using NextButtonCallback =
      base::RepeatingCallback<void(ui::TrackedElement* current_anchor)>;

  using TemporaryStateCallback =
      base::RepeatingCallback<std::unique_ptr<ScopedTutorialState>(
          ui::ElementContext)>;

  TutorialDescription();
  TutorialDescription(TutorialDescription&& other) noexcept;
  TutorialDescription& operator=(TutorialDescription&& other) noexcept;
  ~TutorialDescription();

  using ContextMode = ui::InteractionSequence::ContextMode;
  using ElementSpecifier = std::variant<ui::ElementIdentifier, std::string>;

  // Callback used to determine if the "then" branch of a conditional should be
  // followed. Note that `element` may be null if no matching element exists.
  using ConditionalCallback =
      base::RepeatingCallback<bool(const ui::TrackedElement* element)>;

  class Step {
   public:
    Step();
    Step(const Step& other);
    Step& operator=(const Step& other);
    ~Step();

    // returns true iff all of the required parameters exist to display a
    // bubble.
    bool ShouldShowBubble() const;

    Step& AbortIfVisibilityLost(bool must_remain_visible) {
      must_remain_visible_ = must_remain_visible;
      return *this;
    }

    Step& AbortIfNotVisible() {
      must_be_visible_ = true;
      return *this;
    }

    Step& NameElement(std::string name);

    Step& NameElements(NameElementsCallback name_elements_callback) {
      name_elements_callback_ = std::move(name_elements_callback);
      return *this;
    }

    Step& InAnyContext() {
      context_mode_ = ContextMode::kAny;
      return *this;
    }

    Step& InSameContext() {
      context_mode_ = ContextMode::kFromPreviousStep;
      return *this;
    }

    ui::ElementIdentifier element_id() const { return element_id_; }
    std::string element_name() const { return element_name_; }
    ui::InteractionSequence::StepType step_type() const { return step_type_; }
    ui::CustomElementEventType event_type() const { return event_type_; }
    int title_text_id() const { return title_text_id_; }
    int body_text_id() const { return body_text_id_; }
    int screenreader_text_id() const { return screenreader_text_id_; }
    HelpBubbleArrow arrow() const { return arrow_; }
    std::optional<bool> must_remain_visible() const {
      return must_remain_visible_;
    }
    std::optional<bool> must_be_visible() const { return must_be_visible_; }
    bool transition_only_on_event() const { return transition_only_on_event_; }
    const NameElementsCallback& name_elements_callback() const {
      return name_elements_callback_;
    }
    ContextMode context_mode() const { return context_mode_; }
    const NextButtonCallback& next_button_callback() const {
      return next_button_callback_;
    }
    const HelpBubbleParams::ExtendedProperties& extended_properties() const {
      return extended_properties_;
    }
    ui::InteractionSequence::SubsequenceMode subsequence_mode() const {
      return subsequence_mode_;
    }
    const auto& branches() const { return branches_; }

   protected:
    Step(ElementSpecifier element,
         ui::InteractionSequence::StepType step_type,
         HelpBubbleArrow arrow = HelpBubbleArrow::kNone,
         ui::CustomElementEventType event_type = ui::CustomElementEventType());

    // The element used by interaction sequence to observe and attach a bubble.
    ui::ElementIdentifier element_id_;

    // The element, referred to by name, used by the interaction sequence
    // to observe and potentially attach a bubble. must be non-empty.
    std::string element_name_;

    // The step type for InteractionSequence::Step.
    ui::InteractionSequence::StepType step_type_ =
        ui::InteractionSequence::StepType::kShown;

    // The event type for the step if `step_type` is kCustomEvent.
    ui::CustomElementEventType event_type_ = ui::CustomElementEventType();

    // The title text to be populated in the bubble.
    int title_text_id_ = 0;

    // The body text to be populated in the bubble.
    int body_text_id_ = 0;

    // The screenreader text to be populated in the bubble.
    int screenreader_text_id_ = 0;

    // The positioning of the bubble arrow.
    HelpBubbleArrow arrow_ = HelpBubbleArrow::kNone;

    // Should the element remain visible through the entire step, this should be
    // set to false for hidden steps and for shown steps that precede hidden
    // steps on the same element. if left empty the interaction sequence will
    // decide what its value should be based on the generated
    // InteractionSequence::StepBuilder
    std::optional<bool> must_remain_visible_ = std::nullopt;

    // If set, determines whether the element in question must be visible at the
    // start of the step. If left empty the interaction sequence will choose a
    // reasonable default.
    std::optional<bool> must_be_visible_;

    // Should the step only be completed when an event like shown or hidden only
    // happens during current step. for more information on the implementation
    // take a look at transition_only_on_event in InteractionSequence::Step
    bool transition_only_on_event_ = false;

    // lambda which is called on the start callback of the InteractionSequence
    // which provides the interaction sequence and the current element that
    // belongs to the step. The intention for this functionality is to name one
    // or many elements using the Framework's Specific API finding an element
    // and naming it OR using the current element from the sequence as the
    // element for naming. The return value is a boolean which controls whether
    // the Interaction Sequence should continue or not. If false is returned
    // the tutorial will abort
    NameElementsCallback name_elements_callback_ = NameElementsCallback();

    // Where to search for the step's target element. Default is the context the
    // tutorial started in.
    ContextMode context_mode_ = ContextMode::kInitial;

    // Lambda which is called when the "Next" button is clicked in the help
    // bubble associated with this step. Note that a "Next" button won't render:
    // 1. if `next_button_callback` is null
    // 2. if this step is the last step of a tutorial
    NextButtonCallback next_button_callback_ = NextButtonCallback();

    // Platform-specific properties that can be set for a bubble step. If an
    // extended property evolves to warrant cross-platform support, it should be
    // promoted out of extended properties.
    HelpBubbleParams::ExtendedProperties extended_properties_;

    // Used for if-then-else conditionals.
    ui::InteractionSequence::SubsequenceMode subsequence_mode_ =
        ui::InteractionSequence::SubsequenceMode::kAtMostOne;
    std::vector<std::pair<ConditionalCallback, std::vector<Step>>> branches_;

   private:
    friend class Tutorial;
  };

  // TutorialDescription::BubbleStep
  // A bubble step is a step which shows a bubble anchored to an element
  // This requires that the anchor element be visible, so this is always
  // a kShown step.
  //
  // - A bubble step must be passed an element_id or an element_name
  class BubbleStep : public Step {
   public:
    explicit BubbleStep(ElementSpecifier element_specifier)
        : Step(element_specifier, ui::InteractionSequence::StepType::kShown) {}

    BubbleStep& SetBubbleTitleText(int title_text) {
      title_text_id_ = title_text;
      return *this;
    }

    BubbleStep& SetBubbleBodyText(int body_text_id) {
      body_text_id_ = body_text_id;
      return *this;
    }

    BubbleStep& SetBubbleScreenreaderText(int screenreader_text_id) {
      screenreader_text_id_ = screenreader_text_id;
      return *this;
    }

    BubbleStep& SetBubbleArrow(HelpBubbleArrow arrow) {
      arrow_ = arrow;
      return *this;
    }

    BubbleStep& SetExtendedProperties(
        HelpBubbleParams::ExtendedProperties extended_properties) {
      extended_properties_ = std::move(extended_properties);
      return *this;
    }

    BubbleStep& AddCustomNextButton(NextButtonCallback next_button_callback) {
      next_button_callback_ = std::move(next_button_callback);
      return *this;
    }

    BubbleStep& AddDefaultNextButton();
  };

  // TutorialDescription::HiddenStep
  // A hidden step has no bubble and waits for a UI event to occur on
  // a particular element.
  //
  // - A hidden step must be passed an element_id or an element_name
  class HiddenStep : public Step {
   public:
    // Transition to the next step after a show event occurs
    static HiddenStep WaitForShowEvent(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kShown);
      step.transition_only_on_event_ = true;
      return step;
    }

    // Transition to the next step after a hide event occurs
    static HiddenStep WaitForHideEvent(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kHidden);
      step.transition_only_on_event_ = true;
      return step;
    }

    // Transition to the next step if anchor is, or becomes, visible
    static HiddenStep WaitForShown(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kShown);
      step.transition_only_on_event_ = false;
      return step;
    }

    // Transition to the next step if anchor is, or becomes, hidden
    static HiddenStep WaitForHidden(ElementSpecifier element_specifier) {
      HiddenStep step(element_specifier,
                      ui::InteractionSequence::StepType::kHidden);
      step.transition_only_on_event_ = false;
      return step;
    }

    // Transition to the next step if anchor is, or becomes, activated
    static HiddenStep WaitForActivated(ElementSpecifier element_specifier) {
      return HiddenStep(element_specifier,
                        ui::InteractionSequence::StepType::kActivated);
    }

    template <typename... Args>
    static HiddenStep WaitForOneOf(Args&&... args) {}

   protected:
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
  class EventStep : public Step {
   public:
    explicit EventStep(ui::CustomElementEventType event_type)
        : Step(ui::ElementIdentifier(),
               ui::InteractionSequence::StepType::kCustomEvent,
               HelpBubbleArrow::kNone,
               event_type) {}

    EventStep(ui::CustomElementEventType event_type,
              ElementSpecifier element_specifier)
        : Step(element_specifier,
               ui::InteractionSequence::StepType::kCustomEvent,
               HelpBubbleArrow::kNone,
               event_type) {}
  };

  // TutorialDescription::Create<"Prefix">(step1, step2, ...)
  //
  // Create a tutorial description with the given steps
  // This will also generate the histograms with the given prefix
  template <const char histogram_name[], typename... Args>
  static TutorialDescription Create(Args... steps) {
    TutorialDescription description;
    description.steps = Steps(std::move(steps)...);
    description.histograms =
        user_education::MakeTutorialHistograms<histogram_name>(
            description.steps.size());
    return description;
  }

  // TutorialDescription::Steps(step1, step2, {step3, step4}, ...)
  //
  // Turn steps and step vectors into a flattened vector of steps
  template <typename... Args>
  static std::vector<Step> Steps(Args... steps) {
    std::vector<Step> flat_steps;
    (AddStep(flat_steps, std::move(steps)), ...);
    return flat_steps;
  }

  // Creates a conditional step. Syntax is:
  // ```
  //   If(element[, condition])
  //       .Then(steps...)
  //       [.Else(steps...)]
  // ```
  //
  // The `element` is retrieved from the current context (it will be null if it
  // is not present). The `Then()` or branch is conditionally executed based on:
  //  * The result of calling `condition`, if specified (please note that the
  //    parameter may be null!)
  //  * Whether `element` exists, if `condition` is not specified.
  //
  // If `Else()` is specified, it will be executed if the `Then()` branch is
  // not. At most one branch can execute, and if it fails, the tutorial fails.
  //
  // Because the step does not wait for `element` to become present if it is
  // not yet visible, you may want to insert a `HiddenStep` before your
  // conditional to ensure the correct state:
  // ```
  //   HiddenStep::WaitForShown(kElementId),
  //   If(kElementId, should_do_optional_steps)
  //       .Then(<optional-steps>),
  // ```
  //
  // If you want to wait for one of several elements to be visible (e.g. when
  // the browser might show different variations of a WebUI page based on
  // viewport size), use `WaitForAnyOf()`:
  // ```
  //   // Wait for either version of the surface to appear:
  //   WaitForAnyOf(kPageVariation1ElementId)
  //       .Or(kPageVariation2ElementId),
  //
  //   // Show different Tutorial steps based on which variation appeared:
  //   If(kPageVariation1ElementId)
  //       .Then(<then-steps>)
  //       .Else(<else-steps>),
  // ```
  //
  // Without the WaitForAnyOf, it is possible that neither element would have
  // yet become visible, resulting in the condition failing and the Else()
  // branch always running.
  class If : public Step {
   public:
    // Executes the `Then()` part of this conditional step if `element`
    // satisfies `if_condition`. If `if_condition` is not specified, the
    // default is to execute the `Then()` portion if `element` is visible.
    explicit If(ElementSpecifier element,
                ConditionalCallback if_condition = RunIfPresent())
        : Step(element, ui::InteractionSequence::StepType::kSubsequence) {
      branches_.emplace_back(std::move(if_condition), std::vector<Step>());
    }

    // These tutorial `then_steps` are executed if `if_condition` returns true.
    // Otherwise they are ignored (and the `Else()` steps will be executed if
    // present).
    template <typename... Args>
    If& Then(Args... then_steps) {
      CHECK_EQ(1U, branches_.size());
      CHECK(branches_[0].second.empty());
      branches_[0].second = Steps(std::move(then_steps)...);
      return *this;
    }

    // The tutorial `else_steps` are executed if `if_condition` returns false.
    // This is optional; if not specified and the condition returns false,
    // nothing happens.
    template <typename... Args>
    If& Else(Args... else_steps) {
      CHECK_EQ(1U, branches_.size());
      branches_.emplace_back(AlwaysRun(), Steps(std::move(else_steps)...));
      subsequence_mode_ = ui::InteractionSequence::SubsequenceMode::kExactlyOne;
      return *this;
    }

    // Provides mutable access to the branches in case steps need to be added
    // individually.
    using Step::branches;
    auto& branches() { return branches_; }
  };

  // Constructs a hidden step that waits for at least one of `first` and any
  // additional elements added through `Or()`.
  //
  // This is usually used before an `If()` to ensure that one of several
  // different elements is present. See documentation for `If()` for more
  // information.
  class WaitForAnyOf : public HiddenStep {
   public:
    explicit WaitForAnyOf(ElementSpecifier first, bool wait_for_event = false);
    WaitForAnyOf& Or(ElementSpecifier element, bool wait_for_event = false);
  };

  // The list of TutorialDescription steps.
  std::vector<Step> steps;

  // The histogram data to use. Use MakeTutorialHistograms() above to create a
  // value to use, if you want to record specific histograms for this tutorial.
  std::unique_ptr<TutorialHistograms> histograms;

  // The callback for the tutorial which returns a scoped object which
  // manages temporary state that is maintained through the lifetime of the
  // tutorial.
  TemporaryStateCallback temporary_state_callback;

  // The ability for the tutorial to be restarted. In some cases tutorials can
  // leave the UI in a state where it can not re-run the tutorial. In these
  // cases this flag should be set to false so that the restart tutorial button
  // is not displayed.
  bool can_be_restarted = false;

  // The text ID to use for the complete button at the end of the tutorial.
  int complete_button_text_id = IDS_TUTORIAL_CLOSE_TUTORIAL;

  // Holds metadata about the tutorial.
  Metadata metadata;

 private:
  static void AddStep(std::vector<Step>& dest, Step step) {
    dest.emplace_back(std::move(step));
  }
  static void AddStep(std::vector<Step>& dest, const std::vector<Step>& src) {
    for (auto& step : src) {
      dest.emplace_back(step);
    }
  }

  static ConditionalCallback AlwaysRun();
  static ConditionalCallback RunIfPresent();
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_DESCRIPTION_H_

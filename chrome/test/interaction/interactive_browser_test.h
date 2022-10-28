// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_

#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interaction_test_util_mouse.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ui {
class TrackedElement;
}

namespace views {
class ViewsDelegate;
}

class Browser;

// Base class for tests that supports common InteractionSequence-related testing
// utils.
class InteractiveBrowserTest : public InProcessBrowserTest {
 public:
  InteractiveBrowserTest();
  ~InteractiveBrowserTest() override;

#if defined(TOOLKIT_VIEWS)
  // |views_delegate| is used for tests that want to use a derived class of
  // ViewsDelegate to observe or modify things like window placement and Widget
  // params.
  explicit InteractiveBrowserTest(
      std::unique_ptr<views::ViewsDelegate> views_delegate);
#endif

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
  using InputType = ui::test::InteractionTestUtil::InputType;
  using MultiStep = std::vector<ui::InteractionSequence::StepBuilder>;
  using StateChange = WebContentsInteractionTestUtil::StateChange;
  using StepBuilder = ui::InteractionSequence::StepBuilder;

  // Construct a MultiStep from one or more StepBuilders and/or MultiSteps.
  template <typename... Args>
  static MultiStep Steps(Args&&... args);

  // Returns an interaction simulator for things like clicking buttons.
  // Generally, prefer to use functions like PressButton() to directly using the
  // InteractionTestUtil.
  ui::test::InteractionTestUtil& test_util() { return test_util_; }

  // Returns an object that can be used to inject mouse inputs. Generally,
  // prefer to use methods like MoveMouseTo, MouseClick, and DragMouseTo.
  InteractionTestUtilMouse& mouse_util() { return *mouse_util_.get(); }

  // Runs a test InteractionSequence from a series of Steps or StepBuilders with
  // RunSynchronouslyForTesting(). Hooks both the completed and aborted
  // callbacks to ensure completion, and prints an error on failure. The context
  // will be pulled from `browser()`.
  template <typename... Args>
  bool RunTestSequence(Args&&... steps);

  // Runs a test InteractionSequence in `context` from a series of Steps or
  // StepBuilders with RunSynchronouslyForTesting(). Hooks both the completed
  // and aborted callbacks to ensure completion, and prints an error on failure.
  template <typename... Args>
  bool RunTestSequenceInContext(ui::ElementContext context, Args&&... steps);

  // Shorthand to convert a tracked element into a View. The element should be
  // a views::TrackedElementViews and of type `T`.
  template <typename T = views::View>
  static T* AsView(ui::TrackedElement* el);

  // Shorthand to convert a tracked element into a instrumented WebContents.
  // The element should be a TrackedElementWebContents.
  static WebContentsInteractionTestUtil* AsInstrumentedWebContents(
      ui::TrackedElement* el);

  // Retrieves an instrumented WebContents with identifier `id`, or null if the
  // contents has not been instrumented.
  WebContentsInteractionTestUtil* GetInstrumentedWebContents(
      ui::ElementIdentifier id);

  // Instruments an existing tab in `browser`. If `tab_index` is not specified,
  // the active tab is instrumented.
  WebContentsInteractionTestUtil* InstrumentTab(
      Browser* browser,
      ui::ElementIdentifier id,
      absl::optional<int> tab_index = absl::nullopt);

  // Instruments the next tab to open in `browser`, or if not specified, in any
  // browser.
  WebContentsInteractionTestUtil* InstrumentNextTab(
      absl::optional<Browser*> browser,
      ui::ElementIdentifier id);

  // Instruments a non-tab `web_view`.
  WebContentsInteractionTestUtil* InstrumentNonTabWebView(
      views::WebView* web_view,
      ui::ElementIdentifier id);

  // Specifies an element either by ID or by name.
  using ElementSpecifier =
      absl::variant<ui::ElementIdentifier, base::StringPiece>;

  // Naming views:
  //
  // The following methods name a view (to be referred to later in the test
  // sequence by name) based on some kind of rule or relationship. The View need
  // not have an ElementIdentifier assigned ahead of time, so this is useful for
  // finding random or dynamically-created views.
  //
  // For example:
  //
  //   RunTestSequence(
  //     ...
  //     NameView(kThirdTabName,
  //              base::BindLambdaForTesting([&](){
  //                return browser_view->tabstrip()->tab_at(3);
  //              }))
  //     WithElement(kThirdTabName, ...)
  //     ...
  //   );
  //
  // How the view is named will depend on which version of the method you use;
  // the

  // Determines if a view matches some predicate.
  using ViewMatcher = base::RepeatingCallback<bool(const views::View*)>;

  // Specifies a View not relative to any particular other View.
  using AbsoluteViewSpecifier = absl::variant<
      // Specify a view that is known at the time the sequence is created. The
      // View must persist until the step executes.
      views::View*,
      // Specify a view that will be valid by the time the step executes (i.e.
      // is set in a previous step callback) but not at the time the test
      // sequence is built. The view will be read from the target variable,
      // which must point to a valid view.
      views::View**,
      // Find and return a view based on an arbitrary rule.
      base::OnceCallback<views::View*()>>;

  // Specifies a view relative to its parent.
  using ChildViewSpecifier = absl::variant<
      // The index of the child in the parent view. An out of bounds index will
      // generate an error.
      size_t,
      // Specifies a filter that is applied to the children; the first child
      // view to satisfy the filter (i.e. return true) is named.
      ViewMatcher>;

  // Specifies a view relative to another view `relative_to` based on an
  // arbitrary rule. The resulting view does not need to be a descendant (or
  // even an ancestor) of `relative_to`.
  using FindViewCallback =
      base::OnceCallback<views::View*(views::View* relative_to)>;

  // Methods that name views.
  [[nodiscard]] static StepBuilder NameView(base::StringPiece name,
                                            AbsoluteViewSpecifier spec);
  [[nodiscard]] static StepBuilder NameViewRelative(
      ElementSpecifier relative_to,
      base::StringPiece name,
      FindViewCallback find_callback);
  [[nodiscard]] static StepBuilder NameChildView(ElementSpecifier parent,
                                                 base::StringPiece name,
                                                 ChildViewSpecifier spec);
  [[nodiscard]] static StepBuilder NameDescendantView(ElementSpecifier ancestor,
                                                      base::StringPiece name,
                                                      ViewMatcher matcher);

  // Convenience methods for creating interaction steps of type kShown. The
  // resulting step's start callback is already set; therefore, do not try to
  // add additional logic. However, any other parameter on the step may be set,
  // such as SetMustBeVisibleAtStart(), SetFindElementInAnyContext(),
  // SetTransitionOnlyOnEvent(), etc.
  //
  // TODO(dfried): in the future, these will be supplanted/supplemented by more
  // flexible primitives that allow multiple actions in the same step in the
  // future.
  [[nodiscard]] StepBuilder PressButton(
      ElementSpecifier button,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectMenuItem(
      ElementSpecifier menu_item,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder DoDefaultAction(
      ElementSpecifier element,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectTab(
      ElementSpecifier tab_collection,
      size_t tab_index,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder Screenshot(ElementSpecifier element,
                                       const std::string& screenshot_name,
                                       const std::string& baseline);

  // These convenience methods wait for page navigation/ready. If you specify
  // `expected_url`, the test will fail if that is not the loaded page. If you
  // do not, there is no step start callback and you can add your own logic.
  [[nodiscard]] static StepBuilder WaitForWebContentsReady(
      ui::ElementIdentifier webcontents_id,
      absl::optional<GURL> expected_url = absl::nullopt);
  [[nodiscard]] static StepBuilder WaitForWebContentsNavigation(
      ui::ElementIdentifier webcontents_id,
      absl::optional<GURL> expected_url = absl::nullopt);

  // This convenience method navigates the page at `webcontents_id` to
  // `new_url`, which must be different than its current URL. The sequence will
  // not proceed until navigation completes, and will fail if the wrong URL is
  // loaded.
  [[nodiscard]] static MultiStep NavigateWebContents(
      ui::ElementIdentifier webcontents_id,
      GURL new_url);

  // Waits for the given `state_change` in `webcontents_id`. The sequence will
  // fail if the change times out, unless `expect_timeout` is true, in which
  // case the StateChange *must* timeout, and |state_change.timeout_event| must
  // be set.
  [[nodiscard]] static MultiStep WaitForStateChange(
      ui::ElementIdentifier webcontents_id,
      StateChange state_change,
      bool expect_timeout = false);

  // Indicates that the center point of the target element should be used for a
  // mouse move.
  struct CenterPoint {};

  // Function that returns a destination for a move or drag.
  using AbsolutePositionCallback = base::OnceCallback<gfx::Point()>;

  // Specifies an absolute position for a mouse move or drag that does not need
  // a reference element.
  using AbsolutePositionSpecifier = absl::variant<
      // Use this specific position. This value is stored when the sequence is
      // created; use gfx::Point* if you want to capture a point during sequence
      // execution.
      gfx::Point,
      // As above, but the position is read from the memory address on execution
      // instead of copied when the test sequence is constructed. Use this when
      // you want to calculate and cache a point during test execution for later
      // use. The pointer must remain valid through the end of the test.
      gfx::Point*,
      // Use the return value of the supplied callback
      AbsolutePositionCallback>;

  // Specifies how the `reference_element` should be used (or not) to generate a
  // target point for a mouse move.
  using RelativePositionCallback =
      base::OnceCallback<gfx::Point(ui::TrackedElement* reference_element)>;

  // Specifies how the target position of a mouse operation (in screen
  // coordinates) will be determined.
  using RelativePositionSpecifier = absl::variant<
      // Default to the centerpoint of the reference element, which should be a
      // views::View.
      CenterPoint,
      // Use the return value of the supplied callback.
      RelativePositionCallback,
      // Find the DOM element at the given path in the reference element, which
      // should be an instrumented WebContents; see Instrument*(). The exact
      // position used is the element's center point in screen coordinates.
      DeepQuery>;

  // Move the mouse to the specified `position` in screen coordinates. The
  // `reference` element will be used based on how `position` is specified.
  [[nodiscard]] MultiStep MoveMouseTo(AbsolutePositionSpecifier position);
  [[nodiscard]] MultiStep MoveMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint());

  // Clicks mouse button `button` at the current cursor position.
  [[nodiscard]] MultiStep ClickMouse(
      ui_controls::MouseButton button = ui_controls::LEFT,
      bool release = true);

  // Depresses the left mouse button at the current cursor position and drags to
  // the target `position`. The `reference` element will be used based on how
  // `position` is specified.
  [[nodiscard]] MultiStep DragMouseTo(AbsolutePositionSpecifier position,
                                      bool release = true);
  [[nodiscard]] MultiStep DragMouseTo(
      ElementSpecifier reference,
      RelativePositionSpecifier position = CenterPoint(),
      bool release = true);

  // Releases the specified mouse button. Use when you previously called
  // ClickMouse() or DragMouseTo() with `release` = false.
  [[nodiscard]] MultiStep ReleaseMouse(
      ui_controls::MouseButton button = ui_controls::LEFT);

  // Specifies a test action that is not tied to any one UI element.
  // Returns true on success, false on failure (which will fail the test).
  using CheckCallback = base::OnceCallback<bool()>;

  // Does an action at this point in the test sequence.
  [[nodiscard]] static StepBuilder Do(base::OnceClosure action);

  // Performs a check and fails the test if `check_callback` returns false.
  [[nodiscard]] static StepBuilder Check(CheckCallback check_callback);

  // Calls `function` and applies `matcher` to the result. If the matcher does
  // not match, an appropriate error message is printed and the test fails.
  template <template <typename...> class C, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckResult(C<T()> function, U&& matcher);

  // Checks that `check` returns true for element `element`. will fail the test
  // sequence if `check` returns false - the callback should log any specific
  // error before returning.
  //
  // Note that unless you add .SetMustBeVisibleAtStart(true), this test step
  // will wait for `element` to be shown before proceeding.
  [[nodiscard]] static StepBuilder CheckElement(
      ElementSpecifier element,
      base::OnceCallback<bool(ui::TrackedElement* el)> check);

  // As CheckElement(), but checks that the result of calling `function` on
  // `element` matches `matcher`. If not, the mismatch is printed and the test
  // fails.
  template <template <typename...> class C, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckElement(
      ElementSpecifier element,
      C<T(ui::TrackedElement*)> function,
      U&& matcher);

  // As CheckElement(), but `view` should resolve to a TrackedElementViews
  // wrapping a view of type `V`.
  template <class V>
  [[nodiscard]] static StepBuilder CheckView(
      ElementSpecifier view,
      base::OnceCallback<bool(V* view)> check);

  // As CheckView(), but checks that the result of calling `function` on `view`
  // matches `matcher`. If not, the mismatch is printed and the test fails.
  template <template <typename...> class C, class V, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckView(ElementSpecifier view,
                                             C<T(V*)> function,
                                             U&& matcher);

  // As CheckView() but checks that `matcher` matches the value returned by
  // calling `property` on `view`. On failure, logs the matcher error and fails
  // the test.
  template <class V, typename T, typename U>
  [[nodiscard]] static StepBuilder CheckViewProperty(ElementSpecifier view,
                                                     T (V::*property)() const,
                                                     U&& matcher);

  // Shorthand methods for working with basic ElementTracker events. The element
  // will have `step_callback` called on it. You may specify additional
  // constraints such as SetMustBeVisibleAtStart(),
  // SetFindElementInAnyContext(), SetTransitionOnlyOnEvent(), etc.
  template <class T>
  [[nodiscard]] static StepBuilder AfterShow(ElementSpecifier element,
                                             T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterActivate(ElementSpecifier element,
                                                 T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterEvent(
      ElementSpecifier element,
      ui::CustomElementEventType event_type,
      T&& step_callback);
  template <class T>
  [[nodiscard]] static StepBuilder AfterHide(ElementSpecifier element,
                                             T&& step_callback);

  // Versions of the above that have no step callback; included for clarity and
  // brevity.
  [[nodiscard]] static StepBuilder WaitForShow(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForHide(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForActivate(ElementSpecifier element);
  [[nodiscard]] static StepBuilder WaitForEvent(
      ElementSpecifier element,
      ui::CustomElementEventType event);

  // Equivalent to AfterShow() but the element must already be present.
  template <class T>
  [[nodiscard]] static StepBuilder WithElement(ElementSpecifier element,
                                               T&& step_callback);

  // Adds steps to the sequence that ensure that `element_to_check` is not
  // present. Flushes the current message queue to ensure that if e.g. the
  // previous step was responding to elements being added, the
  // `element_to_check` may not have had its shown event called yet.
  [[nodiscard]] static MultiStep EnsureNotPresent(
      ui::ElementIdentifier element_to_check,
      bool in_any_context = false);

  // Provides syntactic sugar so you can put "in any context" before an action
  // or test step rather than after. For example the following are equivalent:
  //
  //    PressButton(kElementIdentifier)
  //        .SetFindElementInAnyContext(true)
  //
  //    InAnyContext(PressButton(kElementIdentifier))
  //
  // Note: does not work with EnsureNotPresent; use the `in_any_context`
  // parameter.
  //
  // TODO(dfried): consider if we should have a version that takes variadic
  // arguments and applies "in any context" to all of them?
  [[nodiscard]] static MultiStep InAnyContext(MultiStep steps);
  template <typename T>
  static StepBuilder InAnyContext(T&& step);

 private:
  // Helper method to add a step or steps to a sequence builder.
  static void AddStep(ui::InteractionSequence::Builder& builder,
                      MultiStep steps);
  template <typename T>
  static void AddStep(ui::InteractionSequence::Builder& builder, T&& step);

  static void AddStep(MultiStep& dest, StepBuilder src);
  static void AddStep(MultiStep& dest, MultiStep src);

  // Applies `matcher` to `value` and returns the result; on failure a useful
  // error message is printed using `test_name`, `value`, and `matcher`.
  //
  // Steps which use this method will fail if it returns false, printing out the
  // details of the step in the usual way.
  template <typename T>
  static bool MatchAndExplain(const base::StringPiece& test_name,
                              testing::Matcher<T>& matcher,
                              T&& value);

  // Converts an ElementSpecifier to an element ID or name and sets it onto
  // `builder`.
  static void SpecifyElement(StepBuilder& builder, ElementSpecifier element);

  // Converts a *PositionSpecifier to an appropriate *PositionCallback.
  static RelativePositionCallback GetPositionCallback(
      AbsolutePositionSpecifier spec);
  static RelativePositionCallback GetPositionCallback(
      RelativePositionSpecifier spec);

  static FindViewCallback GetFindViewCallback(AbsoluteViewSpecifier spec);
  static FindViewCallback GetFindViewCallback(ChildViewSpecifier spec);

  // Recursively finds an element that matches `matcher` starting with (but
  // not including) `from`. If `recursive` is true, searches all descendants,
  // otherwise searches children.
  static views::View* FindMatchingView(const views::View* from,
                                       ViewMatcher& matcher,
                                       bool recursive);

  // Creates the follow-up step for a mouse action.
  StepBuilder CreateMouseFollowUpStep();

  // Implementation for RunTestSequence*().
  bool RunTestSequenceImpl(ui::ElementContext context,
                           ui::InteractionSequence::Builder builder);
  void OnSequenceComplete();
  void OnSequenceAborted(int active_step,
                         ui::TrackedElement* last_element,
                         ui::ElementIdentifier last_id,
                         ui::InteractionSequence::StepType last_step_type,
                         ui::InteractionSequence::AbortedReason aborted_reason);

  // Tracks whether a sequence succeeded or failed.
  bool success_ = false;

  // Provides simulated interaction with UI elements as well as screenshots.
  InteractionTestUtilBrowser test_util_;

  // Provides mouse interaction.
  std::unique_ptr<InteractionTestUtilMouse> mouse_util_;

  // Tracks failures when a mouse operation fails.
  std::string mouse_error_message_;

  // Provides instrumentation for WebContents and WebUI.
  std::map<ui::ElementIdentifier,
           std::unique_ptr<WebContentsInteractionTestUtil>>
      instrumented_web_contents_;

  // Provides an element to bounce events off of during tests.
  std::unique_ptr<ui::TrackedElement> pivot_element_;
};

// Template definitions.

// static
template <typename... Args>
InteractiveBrowserTest::MultiStep InteractiveBrowserTest::Steps(
    Args&&... args) {
  MultiStep result;
  (AddStep(result, std::forward<Args>(args)), ...);
  return result;
}

// static
template <class T>
T* InteractiveBrowserTest::AsView(ui::TrackedElement* el) {
  auto* const views_el = el->AsA<views::TrackedElementViews>();
  CHECK(views_el);
  T* const view = views::AsViewClass<T>(views_el->view());
  CHECK(view);
  return view;
}

template <typename... Args>
bool InteractiveBrowserTest::RunTestSequence(Args&&... steps) {
  return RunTestSequenceInContext(browser()->window()->GetElementContext(),
                                  std::forward<Args>(steps)...);
}

template <typename... Args>
bool InteractiveBrowserTest::RunTestSequenceInContext(
    ui::ElementContext context,
    Args&&... steps) {
  // TODO(dfried): is there any additional automation we need to do in order to
  // get proper error scoping, RunLoop timeout handling, etc.? We may have to
  // inject information directly into the steps or step callbacks; it's unclear.
  ui::InteractionSequence::Builder builder;
  (AddStep(builder, std::move(steps)), ...);
  return RunTestSequenceImpl(context, std::move(builder));
}

// static
template <typename T>
void InteractiveBrowserTest::AddStep(ui::InteractionSequence::Builder& builder,
                                     T&& step) {
  builder.AddStep(std::move(step));
}

// static
template <typename T>
bool InteractiveBrowserTest::MatchAndExplain(const base::StringPiece& test_name,
                                             testing::Matcher<T>& matcher,
                                             T&& value) {
  if (matcher.Matches(value))
    return true;
  std::ostringstream oss;
  oss << test_name << "failed.\nExpected: ";
  matcher.DescribeTo(&oss);
  oss << "\nActual: " << testing::PrintToString(value);
  LOG(ERROR) << oss.str();
  return false;
}

// static
template <class T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::AfterShow(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<ui::InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::AfterActivate(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetType(ui::InteractionSequence::StepType::kActivated);
  builder.SetStartCallback(
      base::RectifyCallback<ui::InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::AfterEvent(
    ElementSpecifier element,
    ui::CustomElementEventType event_type,
    T&& step_callback) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetType(ui::InteractionSequence::StepType::kCustomEvent, event_type);
  builder.SetStartCallback(
      base::RectifyCallback<ui::InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::AfterHide(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetType(ui::InteractionSequence::StepType::kHidden);
  builder.SetStartCallback(
      base::RectifyCallback<ui::InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  return builder;
}

// static
template <class T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::WithElement(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<ui::InteractionSequence::StepStartCallback>(
          std::forward<T>(step_callback)));
  builder.SetMustBeVisibleAtStart(true);
  return builder;
}

// static
template <typename T>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::InAnyContext(
    T&& step) {
  return std::move(step.SetFindElementInAnyContext(true));
}

// static
template <template <typename...> typename C, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckResult(
    C<T()> function,
    U&& matcher) {
  return Check(base::BindOnce(
      [](base::OnceCallback<T()> function, testing::Matcher<T> matcher) {
        return MatchAndExplain("CheckResult()", matcher,
                               std::move(function).Run());
      },
      base::OnceCallback<T()>(std::move(function)),
      testing::Matcher<T>(std::forward<U>(matcher))));
}

// static
template <template <typename...> typename C, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckElement(
    ElementSpecifier element,
    C<T(ui::TrackedElement*)> function,
    U&& matcher) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<T(ui::TrackedElement*)> function,
         testing::Matcher<T> matcher, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        if (!MatchAndExplain("CheckElement()", matcher,
                             std::move(function).Run(el))) {
          seq->FailForTesting();
        }
      },
      base::OnceCallback<T(ui::TrackedElement*)>(std::move(function)),
      testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

// static
template <typename V>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckView(
    ElementSpecifier view,
    base::OnceCallback<bool(V* view)> check) {
  return CheckView(view, std::move(check), true);
}

// static
template <template <typename...> typename C, class V, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckView(
    ElementSpecifier view,
    C<T(V*)> function,
    U&& matcher) {
  StepBuilder builder;
  SpecifyElement(builder, view);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<T(V*)> function, testing::Matcher<T> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!MatchAndExplain("CheckView()", matcher,
                             std::move(function).Run(AsView<V>(el)))) {
          seq->FailForTesting();
        }
      },
      base::OnceCallback<T(V*)>(std::move(function)),
      testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

// static
template <class V, typename T, typename U>
ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckViewProperty(
    ElementSpecifier view,
    T (V::*property)() const,
    U&& matcher) {
  StepBuilder builder;
  SpecifyElement(builder, view);
  builder.SetStartCallback(base::BindOnce(
      [](T (V::*property)() const, testing::Matcher<T> matcher,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!MatchAndExplain("CheckViewProperty()", matcher,
                             (AsView<V>(el)->*property)())) {
          seq->FailForTesting();
        }
      },
      property, testing::Matcher<T>(std::forward<U>(matcher))));
  return builder;
}

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_

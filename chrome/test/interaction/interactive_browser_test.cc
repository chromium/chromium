// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_tracker.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/views_delegate.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPivotElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kEnsureNotPresentCheckEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMouseGestureCompleteEvent);
}  // namespace

InteractiveBrowserTest::InteractiveBrowserTest() = default;
InteractiveBrowserTest::~InteractiveBrowserTest() = default;

#if defined(TOOLKIT_VIEWS)
InteractiveBrowserTest::InteractiveBrowserTest(
    std::unique_ptr<views::ViewsDelegate> views_delegate)
    : InProcessBrowserTest(std::move(views_delegate)) {}
#endif

void InteractiveBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  mouse_util_ = std::make_unique<InteractionTestUtilMouse>(browser());
}

void InteractiveBrowserTest::TearDownOnMainThread() {
  // Release the mouse util object and cancel any pending actions.
  mouse_util_.reset();

  // Release any remaining instrumented WebContents.
  instrumented_web_contents_.clear();

  InProcessBrowserTest::TearDownOnMainThread();
}

// static
WebContentsInteractionTestUtil*
InteractiveBrowserTest::AsInstrumentedWebContents(ui::TrackedElement* el) {
  auto* const web_el = el->AsA<TrackedElementWebContents>();
  CHECK(web_el);
  return web_el->owner();
}

WebContentsInteractionTestUtil*
InteractiveBrowserTest::GetInstrumentedWebContents(ui::ElementIdentifier id) {
  const auto it = instrumented_web_contents_.find(id);
  return it == instrumented_web_contents_.end() ? nullptr : it->second.get();
}

WebContentsInteractionTestUtil* InteractiveBrowserTest::InstrumentTab(
    Browser* browser,
    ui::ElementIdentifier id,
    absl::optional<int> tab_index) {
  auto instrument = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser, id, tab_index);
  const auto result =
      instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

WebContentsInteractionTestUtil* InteractiveBrowserTest::InstrumentNextTab(
    absl::optional<Browser*> browser,
    ui::ElementIdentifier id) {
  auto instrument =
      browser.has_value()
          ? WebContentsInteractionTestUtil::ForNextTabInBrowser(browser.value(),
                                                                id)
          : WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(id);
  const auto result =
      instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

WebContentsInteractionTestUtil* InteractiveBrowserTest::InstrumentNonTabWebView(
    views::WebView* web_view,
    ui::ElementIdentifier id) {
  auto instrument =
      WebContentsInteractionTestUtil::ForNonTabWebView(web_view, id);
  const auto result =
      instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::NameView(
    base::StringPiece name,
    AbsoluteViewSpecifier spec) {
  return NameViewRelative(kPivotElementId, name,
                          GetFindViewCallback(std::move(spec)));
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::NameViewRelative(
    ElementSpecifier relative_to,
    base::StringPiece name,
    FindViewCallback find_callback) {
  StepBuilder builder;
  SpecifyElement(builder, relative_to);
  builder.SetMustBeVisibleAtStart(true);
  builder.SetStartCallback(base::BindOnce(
      [](FindViewCallback find_callback, std::string name,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        views::View* relative_to = nullptr;
        if (el->identifier() != kPivotElementId) {
          if (!el->IsA<views::TrackedElementViews>()) {
            LOG(ERROR) << "NameView(): Target element is not a View.";
            seq->FailForTesting();
            return;
          }
          relative_to = AsView<views::View>(el);
        }
        views::View* const result = std::move(find_callback).Run(relative_to);
        if (!result) {
          LOG(ERROR) << "NameView(): No View found.";
          seq->FailForTesting();
          return;
        }
        seq->NameElement(
            views::ElementTrackerViews::GetInstance()->GetElementForView(
                result, /* assign_temporary_id =*/true),
            name);
      },
      std::move(find_callback), std::string(name)));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::NameChildView(
    ElementSpecifier parent,
    base::StringPiece name,
    ChildViewSpecifier spec) {
  return NameViewRelative(parent, name, GetFindViewCallback(std::move(spec)));
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::NameDescendantView(
    ElementSpecifier parent,
    base::StringPiece name,
    ViewMatcher matcher) {
  return NameViewRelative(
      parent, name,
      base::BindOnce(
          [](ViewMatcher matcher, views::View* ancestor) -> views::View* {
            auto* const result =
                FindMatchingView(ancestor, matcher, /* recursive =*/true);
            if (!result)
              LOG(ERROR)
                  << "NameDescendantView(): No descendant matches matcher.";
            return result;
          },
          matcher));
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::PressButton(
    ElementSpecifier button,
    InputType input_type) {
  StepBuilder builder;
  SpecifyElement(builder, button);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveBrowserTest* test,
         ui::InteractionSequence*, ui::TrackedElement* el) {
        test->test_util().PressButton(el, input_type);
      },
      input_type, base::Unretained(this)));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::SelectMenuItem(
    ElementSpecifier menu_item,
    InputType input_type) {
  StepBuilder builder;
  SpecifyElement(builder, menu_item);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveBrowserTest* test,
         ui::InteractionSequence*, ui::TrackedElement* el) {
        test->test_util().SelectMenuItem(el, input_type);
      },
      input_type, base::Unretained(this)));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::DoDefaultAction(
    ElementSpecifier element,
    InputType input_type) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveBrowserTest* test,
         ui::InteractionSequence*, ui::TrackedElement* el) {
        test->test_util().DoDefaultAction(el, input_type);
      },
      input_type, base::Unretained(this)));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::SelectTab(
    ElementSpecifier tab_collection,
    size_t tab_index,
    InputType input_type) {
  StepBuilder builder;
  SpecifyElement(builder, tab_collection);
  builder.SetStartCallback(base::BindOnce(
      [](size_t index, InputType input_type, InteractiveBrowserTest* test,
         ui::InteractionSequence*, ui::TrackedElement* el) {
        test->test_util().SelectTab(el, index, input_type);
      },
      tab_index, input_type, base::Unretained(this)));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::Screenshot(
    ElementSpecifier element,
    const std::string& screenshot_name,
    const std::string& baseline) {
  StepBuilder builder;
  SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](std::string screenshot_name, std::string baseline,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!InteractionTestUtilBrowser::CompareScreenshot(el, screenshot_name,
                                                           baseline)) {
          LOG(ERROR) << "Screenshot failed: " << screenshot_name;
          seq->FailForTesting();
        }
      },
      screenshot_name, baseline));
  return builder;
}

ui::InteractionSequence::StepBuilder
InteractiveBrowserTest::WaitForWebContentsReady(
    ui::ElementIdentifier webcontents_id,
    absl::optional<GURL> expected_url) {
  StepBuilder builder;
  builder.SetElementID(webcontents_id);
  if (expected_url.has_value()) {
    builder.SetStartCallback(base::BindOnce(
        [](GURL expected_url, ui::InteractionSequence* seq,
           ui::TrackedElement* el) {
          auto* const contents =
              el->AsA<TrackedElementWebContents>()->owner()->web_contents();
          if (expected_url != contents->GetURL()) {
            LOG(ERROR) << "Loaded wrong URL; got " << contents->GetURL()
                       << " but expected " << expected_url;
            seq->FailForTesting();
          }
        },
        expected_url.value()));
  }
  return builder;
}

ui::InteractionSequence::StepBuilder
InteractiveBrowserTest::WaitForWebContentsNavigation(
    ui::ElementIdentifier webcontents_id,
    absl::optional<GURL> expected_url) {
  StepBuilder builder;
  builder.SetElementID(webcontents_id);
  builder.SetTransitionOnlyOnEvent(true);
  if (expected_url.has_value()) {
    builder.SetStartCallback(base::BindOnce(
        [](GURL expected_url, ui::InteractionSequence* seq,
           ui::TrackedElement* el) {
          auto* const contents =
              el->AsA<TrackedElementWebContents>()->owner()->web_contents();
          if (expected_url != contents->GetURL()) {
            LOG(ERROR) << "Loaded wrong URL; got " << contents->GetURL()
                       << " but expected " << expected_url;
            seq->FailForTesting();
          }
        },
        expected_url.value()));
  }
  return builder;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::NavigateWebContents(
    ui::ElementIdentifier webcontents_id,
    GURL target_url) {
  MultiStep steps;
  steps.emplace_back(std::move(
      StepBuilder()
          .SetElementID(webcontents_id)
          .SetStartCallback(base::BindOnce(
              [](GURL url, ui::InteractionSequence* seq,
                 ui::TrackedElement* el) {
                auto* const owner =
                    el->AsA<TrackedElementWebContents>()->owner();
                if (url.EqualsIgnoringRef(owner->web_contents()->GetURL())) {
                  LOG(ERROR) << "Trying to load URL " << url
                             << " but WebContents URL is already "
                             << owner->web_contents()->GetURL();
                  seq->FailForTesting();
                }
                owner->LoadPage(url);
              },
              target_url))));
  steps.emplace_back(WaitForWebContentsNavigation(webcontents_id, target_url));
  return steps;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::WaitForStateChange(
    ui::ElementIdentifier webcontents_id,
    StateChange state_change,
    bool expect_timeout) {
  MultiStep steps;
  ui::CustomElementEventType event_type =
      expect_timeout ? state_change.timeout_event : state_change.event;
  CHECK(event_type);
  steps.emplace_back(
      std::move(StepBuilder()
                    .SetElementID(webcontents_id)
                    .SetStartCallback(base::BindOnce(
                        [](StateChange state_change, ui::TrackedElement* el) {
                          el->AsA<TrackedElementWebContents>()
                              ->owner()
                              ->SendEventOnStateChange(state_change);
                        },
                        std::move(state_change)))));
  steps.emplace_back(
      std::move(StepBuilder()
                    .SetElementID(webcontents_id)
                    .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                             event_type)));
  return steps;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::MoveMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position) {
  StepBuilder step;
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserTest* test, RelativePositionCallback pos_callback,
         ui::TrackedElement* el) {
        test->mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveBrowserTest* test, bool success) {
                  if (!success)
                    test->mouse_error_message_ = "MoreMouseTo() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->pivot_element_.get(), kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            InteractionTestUtilMouse::MoveTo(std::move(pos_callback).Run(el)));
      },
      base::Unretained(this), GetPositionCallback(std::move(position))));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::MoveMouseTo(
    AbsolutePositionSpecifier position) {
  return MoveMouseTo(kPivotElementId, GetPositionCallback(std::move(position)));
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::ClickMouse(
    ui_controls::MouseButton button,
    bool release) {
  StepBuilder step;
  step.SetElementID(kPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserTest* test, ui_controls::MouseButton button,
         bool release) {
        test->mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveBrowserTest* test, bool success) {
                  if (!success)
                    test->mouse_error_message_ = "ClickMouse() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->pivot_element_.get(), kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            release ? InteractionTestUtilMouse::Click(button)
                    : InteractionTestUtilMouse::MouseGestures{
                          InteractionTestUtilMouse::MouseDown(button)});
      },
      base::Unretained(this), button, release));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::DragMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position,
    bool release) {
  StepBuilder step;
  SpecifyElement(step, reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserTest* test, RelativePositionCallback pos_callback,
         bool release, ui::TrackedElement* el) {
        test->mouse_error_message_.clear();
        const gfx::Point target = std::move(pos_callback).Run(el);
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveBrowserTest* test, bool success) {
                  if (!success)
                    test->mouse_error_message_ = "DragMouseTo() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->pivot_element_.get(), kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            release ? InteractionTestUtilMouse::DragAndRelease(target)
                    : InteractionTestUtilMouse::DragAndHold(target));
      },
      base::Unretained(this), GetPositionCallback(std::move(position)),
      release));

  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::DragMouseTo(
    AbsolutePositionSpecifier position,
    bool release) {
  return DragMouseTo(kPivotElementId, GetPositionCallback(std::move(position)),
                     release);
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::ReleaseMouse(
    ui_controls::MouseButton button) {
  StepBuilder step;
  step.SetElementID(kPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserTest* test, ui_controls::MouseButton button) {
        test->mouse_error_message_.clear();
        test->mouse_util().PerformGestures(
            base::BindOnce(
                [](InteractiveBrowserTest* test, bool success) {
                  if (!success)
                    test->mouse_error_message_ = "ReleaseMouse() failed.";
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      test->pivot_element_.get(), kMouseGestureCompleteEvent);
                },
                base::Unretained(test)),
            InteractionTestUtilMouse::MouseUp(button));
      },
      base::Unretained(this), button));
  MultiStep result;
  result.emplace_back(std::move(step));
  result.emplace_back(CreateMouseFollowUpStep());
  return result;
}

InteractiveBrowserTest::StepBuilder InteractiveBrowserTest::Check(
    CheckCallback check_callback) {
  StepBuilder builder;
  builder.SetElementID(kPivotElementId);
  builder.SetStartCallback(base::BindOnce(
      [](CheckCallback check_callback, ui::InteractionSequence* seq,
         ui::TrackedElement*) {
        const bool result = std::move(check_callback).Run();
        if (!result)
          seq->FailForTesting();
      },
      std::move(check_callback)));
  return builder;
}

InteractiveBrowserTest::StepBuilder InteractiveBrowserTest::Do(
    base::OnceClosure action) {
  StepBuilder builder;
  builder.SetElementID(kPivotElementId);
  builder.SetStartCallback(std::move(action));
  return builder;
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTest::CheckElement(
    ElementSpecifier element,
    base::OnceCallback<bool(ui::TrackedElement* el)> check) {
  StepBuilder step;
  SpecifyElement(step, element);
  step.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<bool(ui::TrackedElement * el)> check,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        if (!std::move(check).Run(el))
          seq->FailForTesting();
      },
      std::move(check)));
  return step;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::EnsureNotPresent(
    ui::ElementIdentifier element_to_check,
    bool in_any_context) {
  MultiStep steps;
  steps.emplace_back(WithElement(
      kPivotElementId, base::BindOnce([](ui::TrackedElement* element) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](ui::ElementIdentifier id, ui::ElementContext context) {
                  auto* const element =
                      ui::ElementTracker::GetElementTracker()
                          ->GetFirstMatchingElement(id, context);
                  if (element) {
                    ui::ElementTracker::GetFrameworkDelegate()
                        ->NotifyCustomEvent(element,
                                            kEnsureNotPresentCheckEvent);
                  }
                  // Note: if the element is no longer present, the sequence was
                  // already aborted; there is no need to send further errors.
                },
                element->identifier(), element->context()));
      })));
  steps.emplace_back(AfterEvent(
      kPivotElementId, kEnsureNotPresentCheckEvent,
      base::BindOnce(
          [](ui::ElementIdentifier element_to_check, bool in_any_context,
             ui::InteractionSequence* seq, ui::TrackedElement* reference) {
            auto* const element =
                in_any_context
                    ? ui::ElementTracker::GetElementTracker()
                          ->GetElementInAnyContext(element_to_check)
                    : ui::ElementTracker::GetElementTracker()
                          ->GetFirstMatchingElement(element_to_check,
                                                    reference->context());
            if (element) {
              LOG(ERROR) << "Expected element " << element_to_check
                         << " not to be present but it was present.";
              seq->FailForTesting();
            }
          },
          element_to_check, in_any_context)));
  return steps;
}

InteractiveBrowserTest::MultiStep InteractiveBrowserTest::InAnyContext(
    MultiStep steps) {
  for (auto& step : steps)
    step.SetFindElementInAnyContext(true);
  return steps;
}

bool InteractiveBrowserTest::RunTestSequenceImpl(
    ui::ElementContext context,
    ui::InteractionSequence::Builder builder) {
  builder.SetContext(context);

  // Pivot element also serves as a re-entrancy guard.
  CHECK(!pivot_element_);
  auto pivot_element =
      std::make_unique<ui::test::TestElement>(kPivotElementId, context);
  pivot_element->Show();
  base::AutoReset<std::unique_ptr<ui::TrackedElement>> pivot_element_reset(
      &pivot_element_, std::move(pivot_element));

  success_ = false;
  builder.SetCompletedCallback(base::BindOnce(
      &InteractiveBrowserTest::OnSequenceComplete, base::Unretained(this)));
  builder.SetAbortedCallback(base::BindOnce(
      &InteractiveBrowserTest::OnSequenceAborted, base::Unretained(this)));
  auto sequence = builder.Build();
  sequence->RunSynchronouslyForTesting();
  return success_;
}

void InteractiveBrowserTest::OnSequenceComplete() {
  mouse_util_->CancelAllGestures();
  success_ = true;
}

void InteractiveBrowserTest::OnSequenceAborted(
    int active_step,
    ui::TrackedElement* last_element,
    ui::ElementIdentifier last_id,
    ui::InteractionSequence::StepType last_step_type,
    ui::InteractionSequence::AbortedReason aborted_reason) {
  mouse_util_->CancelAllGestures();
  GTEST_FAIL() << "Interactive test failed on step " << active_step
               << " for reason " << aborted_reason << ". Step type was "
               << last_step_type << " with element " << last_id;
}

// static
void InteractiveBrowserTest::AddStep(ui::InteractionSequence::Builder& builder,
                                     MultiStep multi_step) {
  for (auto& step : multi_step)
    builder.AddStep(step);
}

// static
void InteractiveBrowserTest::SpecifyElement(StepBuilder& builder,
                                            ElementSpecifier element) {
  if (auto* id = absl::get_if<ui::ElementIdentifier>(&element)) {
    builder.SetElementID(*id);
  } else {
    CHECK(absl::holds_alternative<base::StringPiece>(element));
    builder.SetElementName(absl::get<base::StringPiece>(element));
  }
}

// static
InteractiveBrowserTest::RelativePositionCallback
InteractiveBrowserTest::GetPositionCallback(AbsolutePositionSpecifier spec) {
  if (auto* point = absl::get_if<gfx::Point>(&spec)) {
    return base::BindOnce([](gfx::Point p, ui::TrackedElement*) { return p; },
                          *point);
  }

  if (auto** point = absl::get_if<gfx::Point*>(&spec)) {
    return base::BindOnce([](gfx::Point* p, ui::TrackedElement*) { return *p; },
                          base::Unretained(*point));
  }

  CHECK(absl::holds_alternative<AbsolutePositionCallback>(spec));
  return base::RectifyCallback<RelativePositionCallback>(
      std::move(absl::get<AbsolutePositionCallback>(spec)));
}

// static
InteractiveBrowserTest::RelativePositionCallback
InteractiveBrowserTest::GetPositionCallback(RelativePositionSpecifier spec) {
  if (auto* cb = absl::get_if<RelativePositionCallback>(&spec)) {
    return std::move(*cb);
  }

  if (auto* deep_query = absl::get_if<DeepQuery>(&spec)) {
    return base::BindOnce(
        [](DeepQuery q, ui::TrackedElement* el) {
          return el->AsA<TrackedElementWebContents>()
              ->owner()
              ->GetElementBoundsInScreen(q)
              .CenterPoint();
        },
        std::move(*deep_query));
  }

  CHECK(absl::holds_alternative<CenterPoint>(spec));
  return base::BindOnce([](ui::TrackedElement* el) {
    return el->AsA<views::TrackedElementViews>()
        ->view()
        ->GetBoundsInScreen()
        .CenterPoint();
  });
}

// static
InteractiveBrowserTest::FindViewCallback
InteractiveBrowserTest::GetFindViewCallback(AbsoluteViewSpecifier spec) {
  if (views::View** view = absl::get_if<views::View*>(&spec)) {
    CHECK(*view) << "NameView(View*): view must be set.";
    return base::BindOnce(
        [](const std::unique_ptr<views::ViewTracker>& ref, views::View*) {
          LOG_IF(ERROR, !ref->view()) << "NameView(View*): view ceased to be "
                                         "valid before step was executed.";
          return ref->view();
        },
        std::make_unique<views::ViewTracker>(*view));
  }

  if (views::View*** view = absl::get_if<views::View**>(&spec)) {
    CHECK(*view) << "NameView(View**): view pointer is null.";
    return base::BindOnce(
        [](views::View** view, views::View*) {
          LOG_IF(ERROR, !*view) << "NameView(View**): view pointer is null.";
          return *view;
        },
        base::Unretained(*view));
  }

  return base::RectifyCallback<FindViewCallback>(
      std::move(absl::get<base::OnceCallback<views::View*()>>(spec)));
}

// static
InteractiveBrowserTest::FindViewCallback
InteractiveBrowserTest::GetFindViewCallback(ChildViewSpecifier spec) {
  if (size_t* index = absl::get_if<size_t>(&spec)) {
    return base::BindOnce(
        [](size_t index, views::View* parent) -> views::View* {
          if (index >= parent->children().size()) {
            LOG(ERROR) << "NameChildView(int): Child index out of bounds; got "
                       << index << " but only " << parent->children().size()
                       << " children.";
            return nullptr;
          }
          return parent->children()[index];
        },
        *index);
  }

  return base::BindOnce(
      [](ViewMatcher matcher, views::View* parent) -> views::View* {
        auto* const result =
            FindMatchingView(parent, matcher, /*recursive =*/false);
        LOG_IF(ERROR, !result)
            << "NameChildView(ViewMatcher): No child matches matcher.";
        return result;
      },
      absl::get<ViewMatcher>(spec));
}

// static
views::View* InteractiveBrowserTest::FindMatchingView(const views::View* from,
                                                      ViewMatcher& matcher,
                                                      bool recursive) {
  for (auto* const child : from->children()) {
    if (matcher.Run(child))
      return child;
    if (recursive) {
      auto* const result = FindMatchingView(child, matcher, true);
      if (result)
        return result;
    }
  }
  return nullptr;
}

InteractiveBrowserTest::StepBuilder
InteractiveBrowserTest::CreateMouseFollowUpStep() {
  return std::move(
      StepBuilder()
          .SetElementID(kPivotElementId)
          .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                   kMouseGestureCompleteEvent)
          .SetStartCallback(base::BindOnce(
              [](InteractiveBrowserTest* test, ui::InteractionSequence* seq,
                 ui::TrackedElement*) {
                if (!test->mouse_error_message_.empty()) {
                  LOG(ERROR) << test->mouse_error_message_;
                  seq->FailForTesting();
                }
              },
              base::Unretained(this))));
}

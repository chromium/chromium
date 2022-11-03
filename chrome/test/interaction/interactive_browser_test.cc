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
#include "chrome/browser/ui/views/frame/browser_view.h"
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
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_delegate.h"

InteractiveBrowserTestApi::InteractiveBrowserTestApi()
    : InteractiveBrowserTestApi(
          std::make_unique<internal::InteractiveBrowserTestPrivate>(
              std::make_unique<InteractionTestUtilBrowser>())) {}

InteractiveBrowserTestApi::InteractiveBrowserTestApi(
    std::unique_ptr<internal::InteractiveBrowserTestPrivate> private_test_impl)
    : InteractiveViewsTestApi(std::move(private_test_impl)) {}

InteractiveBrowserTestApi::~InteractiveBrowserTestApi() = default;

// static
WebContentsInteractionTestUtil*
InteractiveBrowserTestApi::AsInstrumentedWebContents(ui::TrackedElement* el) {
  auto* const web_el = el->AsA<TrackedElementWebContents>();
  CHECK(web_el);
  return web_el->owner();
}

WebContentsInteractionTestUtil*
InteractiveBrowserTestApi::GetInstrumentedWebContents(
    ui::ElementIdentifier id) {
  const auto it = test_impl().instrumented_web_contents_.find(id);
  return it == test_impl().instrumented_web_contents_.end() ? nullptr
                                                            : it->second.get();
}

WebContentsInteractionTestUtil* InteractiveBrowserTestApi::InstrumentTab(
    Browser* browser,
    ui::ElementIdentifier id,
    absl::optional<int> tab_index) {
  auto instrument = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser, id, tab_index);
  const auto result =
      test_impl().instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

WebContentsInteractionTestUtil* InteractiveBrowserTestApi::InstrumentNextTab(
    absl::optional<Browser*> browser,
    ui::ElementIdentifier id) {
  auto instrument =
      browser.has_value()
          ? WebContentsInteractionTestUtil::ForNextTabInBrowser(browser.value(),
                                                                id)
          : WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(id);
  const auto result =
      test_impl().instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

WebContentsInteractionTestUtil*
InteractiveBrowserTestApi::InstrumentNonTabWebView(views::WebView* web_view,
                                                   ui::ElementIdentifier id) {
  auto instrument =
      WebContentsInteractionTestUtil::ForNonTabWebView(web_view, id);
  const auto result =
      test_impl().instrumented_web_contents_.emplace(id, std::move(instrument));
  CHECK(result.second);
  return result.first->second.get();
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::Screenshot(
    ElementSpecifier element,
    const std::string& screenshot_name,
    const std::string& baseline) {
  StepBuilder builder;
  ui::test::internal::SpecifyElement(builder, element);
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

// static
ui::InteractionSequence::StepBuilder
InteractiveBrowserTestApi::WaitForWebContentsReady(
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

// static
ui::InteractionSequence::StepBuilder
InteractiveBrowserTestApi::WaitForWebContentsNavigation(
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

// static
InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::NavigateWebContents(
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

// static
InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::WaitForStateChange(
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

InteractiveBrowserTestApi::MultiStep InteractiveBrowserTestApi::MoveMouseTo(
    ElementSpecifier web_contents,
    DeepQuery where) {
  return MoveMouseTo(web_contents,
                     DeepQueryToRelativePosition(std::move(where)));
}

InteractiveBrowserTestApi::MultiStep InteractiveBrowserTestApi::DragMouseTo(
    ElementSpecifier web_contents,
    DeepQuery where,
    bool release) {
  return DragMouseTo(web_contents,
                     DeepQueryToRelativePosition(std::move(where)), release);
}

// static
InteractiveBrowserTestApi::RelativePositionCallback
InteractiveBrowserTestApi::DeepQueryToRelativePosition(DeepQuery query) {
  return base::BindOnce(
      [](DeepQuery q, ui::TrackedElement* el) {
        return el->AsA<TrackedElementWebContents>()
            ->owner()
            ->GetElementBoundsInScreen(q)
            .CenterPoint();
      },
      std::move(query));
}

InteractiveBrowserTest::InteractiveBrowserTest() = default;

InteractiveBrowserTest::InteractiveBrowserTest(
    std::unique_ptr<views::ViewsDelegate> views_delegate)
    : InProcessBrowserTest(std::move(views_delegate)) {}

InteractiveBrowserTest::~InteractiveBrowserTest() = default;

void InteractiveBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  private_test_impl().DoTestSetUp();
  SetContextWidget(
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
}

void InteractiveBrowserTest::TearDownOnMainThread() {
  SetContextWidget(nullptr);
  private_test_impl().DoTestTearDown();
  InProcessBrowserTest::TearDownOnMainThread();
}

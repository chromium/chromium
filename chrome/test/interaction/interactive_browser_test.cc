// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include <utility>

#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/views_delegate.h"

namespace {
// Since we enforce a 1:1 correspondence between ElementIdentifiers and
// WebContents defaulting to ContextMode::kAny prevents accidentally missing the
// correct context, which is a common mistake that causes tests to mysteriously
// time out looking in the wrong place.
constexpr ui::InteractionSequence::ContextMode kDefaultWebContentsContextMode =
    ui::InteractionSequence::ContextMode::kAny;
}  // namespace

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

void InteractiveBrowserTestApi::EnableWebUICodeCoverage() {
  test_impl().MaybeStartWebUICodeCoverage();
}

ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::Screenshot(
    ElementSpecifier element,
    const std::string& screenshot_name,
    const std::string& baseline) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf("Screenshot( \"%s\", \"%s\" )",
                                            screenshot_name.c_str(),
                                            baseline.c_str()));
  ui::test::internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](InteractiveBrowserTestApi* test, std::string screenshot_name,
         std::string baseline, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        const auto result = InteractionTestUtilBrowser::CompareScreenshot(
            el, screenshot_name, baseline);
        test->test_impl().HandleActionResult(seq, el, "Screenshot", result);
      },
      base::Unretained(this), screenshot_name, baseline));
  return builder;
}

InteractiveBrowserTestApi::MultiStep InteractiveBrowserTestApi::InstrumentTab(
    ui::ElementIdentifier id,
    absl::optional<int> tab_index,
    BrowserSpecifier in_browser,
    bool wait_for_ready) {
  const auto desc =
      base::StringPrintf("InstrumentTab( %s, %d, %d )", id.GetName().c_str(),
                         tab_index.value_or(-1), wait_for_ready);
  auto steps = Steps(std::move(
      WithElement(ui::test::internal::kInteractiveTestPivotElementId,
                  base::BindLambdaForTesting([this, id, tab_index, in_browser](
                                                 ui::TrackedElement* el) {
                    Browser* const browser =
                        GetBrowserFor(el->context(), in_browser);
                    CHECK(browser)
                        << "InstrumentTab(): a specific browser is required.";
                    test_impl().AddInstrumentedWebContents(
                        WebContentsInteractionTestUtil::ForExistingTabInBrowser(
                            browser, id, tab_index));
                  }))
          .SetDescription(base::StrCat({desc, ": Instrument"}))));
  if (wait_for_ready) {
    steps.emplace_back(std::move(WaitForWebContentsReady(id).FormatDescription(
        base::StrCat({desc, ": %s"}))));
  }
  return steps;
}

ui::InteractionSequence::StepBuilder
InteractiveBrowserTestApi::InstrumentNextTab(ui::ElementIdentifier id,
                                             BrowserSpecifier in_browser) {
  return std::move(
      WithElement(
          ui::test::internal::kInteractiveTestPivotElementId,
          base::BindLambdaForTesting([this, id,
                                      in_browser](ui::TrackedElement* el) {
            Browser* const browser = GetBrowserFor(el->context(), in_browser);
            test_impl().AddInstrumentedWebContents(
                browser
                    ? WebContentsInteractionTestUtil::ForNextTabInBrowser(
                          browser, id)
                    : WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
                          id));
          }))
          .SetDescription(
              base::StringPrintf("InstrumentTab( %s )", id.GetName().c_str())));
}

InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::AddInstrumentedTab(ui::ElementIdentifier id,
                                              GURL url,
                                              absl::optional<int> at_index,
                                              BrowserSpecifier in_browser) {
  const auto desc = base::StringPrintf("AddInstrumentedTab( %s, %s, %d, )",
                                       id.GetName().c_str(), url.spec().c_str(),
                                       at_index.value_or(-1));
  return Steps(
      std::move(
          InstrumentNextTab(id, in_browser)
              .SetDescription(base::StrCat({desc, ": Instrument Next Tab"}))),
      std::move(
          WithElement(
              ui::test::internal::kInteractiveTestPivotElementId,
              base::BindLambdaForTesting([this, url, at_index,
                                          in_browser](ui::TrackedElement* el) {
                Browser* const browser =
                    GetBrowserFor(el->context(), in_browser);
                CHECK(browser)
                    << "AddInstrumentedTab(): a browser is required.";
                NavigateParams navigate_params(
                    browser, url, ui::PageTransition::PAGE_TRANSITION_TYPED);
                navigate_params.tabstrip_index = at_index.value_or(-1);
                navigate_params.disposition =
                    WindowOpenDisposition::NEW_FOREGROUND_TAB;
                CHECK(Navigate(&navigate_params));
              }))
              .SetDescription(base::StrCat({desc, ": Navigate"}))),
      std::move(WaitForWebContentsReady(id).FormatDescription(
          base::StrCat({desc, ": %s"}))));
}

InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::InstrumentNonTabWebView(ui::ElementIdentifier id,
                                                   ElementSpecifier web_view,
                                                   bool wait_for_ready) {
  const auto desc = base::StringPrintf("InstrumentNonTabWebView( %s, %d, )",
                                       id.GetName().c_str(), wait_for_ready);
  auto steps = Steps(std::move(
      AfterShow(web_view,
                base::BindLambdaForTesting([this, id](ui::TrackedElement* el) {
                  test_impl().AddInstrumentedWebContents(
                      WebContentsInteractionTestUtil::ForNonTabWebView(
                          AsView<views::WebView>(el), id));
                }))
          .SetDescription(base::StrCat({desc, ": Instrument WebView"}))));
  if (wait_for_ready) {
    steps.emplace_back(std::move(WaitForWebContentsReady(id).FormatDescription(
        base::StrCat({desc, ": %s"}))));
  }
  return steps;
}

InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::InstrumentNonTabWebView(
    ui::ElementIdentifier id,
    AbsoluteViewSpecifier web_view,
    bool wait_for_ready) {
  constexpr char kTemporaryElementName[] =
      "__InstrumentNonTabWebViewTemporaryElementName__";
  return Steps(
      std::move(NameView(kTemporaryElementName, std::move(web_view))
                    .FormatDescription("InstrumentNonTabWebView(): %s")),
      InstrumentNonTabWebView(id, kTemporaryElementName, wait_for_ready));
}

// static
ui::InteractionSequence::StepBuilder
InteractiveBrowserTestApi::WaitForWebContentsReady(
    ui::ElementIdentifier webcontents_id,
    absl::optional<GURL> expected_url) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("WaitForWebContentsReady( %s )",
                         expected_url.value_or(GURL()).spec().c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
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
  builder.SetDescription(
      base::StringPrintf("WaitForWebContentsNavigation( %s )",
                         expected_url.value_or(GURL()).spec().c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
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
  const auto desc = base::StringPrintf("NavigateWebContents( %s )",
                                       target_url.spec().c_str());
  return Steps(
      std::move(StepBuilder()
                    .SetDescription(base::StrCat({desc, ": Navigate"}))
                    .SetElementID(webcontents_id)
                    .SetContext(kDefaultWebContentsContextMode)
                    .SetStartCallback(base::BindOnce(
                        [](GURL url, ui::InteractionSequence* seq,
                           ui::TrackedElement* el) {
                          auto* const owner =
                              el->AsA<TrackedElementWebContents>()->owner();
                          if (url.EqualsIgnoringRef(
                                  owner->web_contents()->GetURL())) {
                            LOG(ERROR) << "Trying to load URL " << url
                                       << " but WebContents URL is already "
                                       << owner->web_contents()->GetURL();
                            seq->FailForTesting();
                          }
                          owner->LoadPage(url);
                        },
                        target_url))),
      std::move(WaitForWebContentsNavigation(webcontents_id, target_url)
                    .FormatDescription(base::StrCat({desc, ": %s"}))));
}

// static
InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::WaitForStateChange(
    ui::ElementIdentifier webcontents_id,
    const StateChange& state_change,
    bool expect_timeout) {
  ui::CustomElementEventType event_type =
      expect_timeout ? state_change.timeout_event : state_change.event;
  CHECK(event_type);
  const auto desc =
      base::StringPrintf("WaitForStateChange( %d )", expect_timeout);
  return Steps(
      std::move(StepBuilder()
                    .SetDescription(base::StrCat({desc, ": Queue Event"}))
                    .SetElementID(webcontents_id)
                    .SetContext(kDefaultWebContentsContextMode)
                    .SetStartCallback(base::BindOnce(
                        [](StateChange state_change, ui::TrackedElement* el) {
                          el->AsA<TrackedElementWebContents>()
                              ->owner()
                              ->SendEventOnStateChange(state_change);
                        },
                        state_change))),
      std::move(StepBuilder()
                    .SetDescription(base::StrCat({desc, ": Wait For Event"}))
                    .SetElementID(webcontents_id)
                    .SetContext(
                        ui::InteractionSequence::ContextMode::kFromPreviousStep)
                    .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                             event_type)));
}

// static
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::EnsurePresent(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf(
      "EnsurePresent( %s, %s )", webcontents_id.GetName().c_str(),
      internal::InteractiveBrowserTestPrivate::DeepQueryToString(where)
          .c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](DeepQuery where, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        if (!AsInstrumentedWebContents(el)->Exists(where)) {
          LOG(ERROR) << "Expected DOM element to be present: " << where;
          seq->FailForTesting();
        }
      },
      where));
  return builder;
}

// static
ui::InteractionSequence::StepBuilder
InteractiveBrowserTestApi::EnsureNotPresent(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf(
      "EnsureNotPresent( %s, %s )", webcontents_id.GetName().c_str(),
      internal::InteractiveBrowserTestPrivate::DeepQueryToString(where)
          .c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](DeepQuery where, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        if (AsInstrumentedWebContents(el)->Exists(where)) {
          LOG(ERROR) << "Expected DOM element not to be present: " << where;
          seq->FailForTesting();
        }
      },
      where));
  return builder;
}

// static
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::ExecuteJs(
    ui::ElementIdentifier webcontents_id,
    const std::string& function) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("ExecuteJs(\"\n%s\n\")", function.c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](std::string function, ui::TrackedElement* el) {
        AsInstrumentedWebContents(el)->Execute(function);
      },
      function));
  return builder;
}

// static
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::ExecuteJsAt(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where,
    const std::string& function) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf(
      "ExecuteJsAt( %s, \"\n%s\n\")",
      internal::InteractiveBrowserTestPrivate::DeepQueryToString(where).c_str(),
      function.c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](DeepQuery where, std::string function, ui::TrackedElement* el) {
        AsInstrumentedWebContents(el)->ExecuteAt(where, function);
      },
      where, function));
  return builder;
}

// static
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::CheckJsResult(
    ui::ElementIdentifier webcontents_id,
    const std::string& function) {
  StepBuilder builder;
  builder.SetDescription(
      base::StringPrintf("CheckJsResult(\"\n%s\n\")", function.c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](std::string function, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        const auto result = AsInstrumentedWebContents(el)->Evaluate(function);
        if (!WebContentsInteractionTestUtil::IsTruthy(result)) {
          LOG(ERROR) << "CheckJsResult(): result of function is falsy: "
                     << result;
          seq->FailForTesting();
        }
      },
      function));
  return builder;
}

// static
ui::InteractionSequence::StepBuilder InteractiveBrowserTestApi::CheckJsResultAt(
    ui::ElementIdentifier webcontents_id,
    const DeepQuery& where,
    const std::string& function) {
  StepBuilder builder;
  builder.SetDescription(base::StringPrintf(
      "CheckJsResultAt( %s, \"\n%s\n\")",
      internal::InteractiveBrowserTestPrivate::DeepQueryToString(where).c_str(),
      function.c_str()));
  builder.SetElementID(webcontents_id);
  builder.SetContext(kDefaultWebContentsContextMode);
  builder.SetStartCallback(base::BindOnce(
      [](DeepQuery where, std::string function, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        const auto result =
            AsInstrumentedWebContents(el)->EvaluateAt(where, function);
        if (!WebContentsInteractionTestUtil::IsTruthy(result)) {
          LOG(ERROR) << "CheckJsResultAt(): result of function is falsy: "
                     << result;
          seq->FailForTesting();
        }
      },
      where, function));
  return builder;
}

InteractiveBrowserTestApi::StepBuilder InteractiveBrowserTestApi::MoveMouseTo(
    ElementSpecifier web_contents,
    const DeepQuery& where) {
  return MoveMouseTo(web_contents, DeepQueryToRelativePosition(where));
}

InteractiveBrowserTestApi::StepBuilder InteractiveBrowserTestApi::DragMouseTo(
    ElementSpecifier web_contents,
    const DeepQuery& where,
    bool release) {
  return DragMouseTo(web_contents, DeepQueryToRelativePosition(where), release);
}

InteractiveBrowserTestApi::StepBuilder
InteractiveBrowserTestApi::ScrollIntoView(ui::ElementIdentifier web_contents,
                                          const DeepQuery& where) {
  return std::move(
      ExecuteJsAt(web_contents, where,
                  "(el) => { el.scrollIntoView({ behavior: 'instant' }); }")
          .SetDescription("ScrollIntoView()"));
}

// static
InteractiveBrowserTestApi::RelativePositionCallback
InteractiveBrowserTestApi::DeepQueryToRelativePosition(const DeepQuery& query) {
  return base::BindOnce(
      [](DeepQuery q, ui::TrackedElement* el) {
        auto* const contents = el->AsA<TrackedElementWebContents>();
        const gfx::Rect container_bounds = contents->GetScreenBounds();
        const gfx::Rect element_bounds =
            contents->owner()->GetElementBoundsInScreen(q);
        CHECK(!element_bounds.IsEmpty())
            << "Cannot target DOM element at " << q << " in "
            << el->identifier() << " because its screen bounds are emtpy.";
        gfx::Rect intersect_bounds = element_bounds;
        intersect_bounds.Intersect(container_bounds);
        CHECK(!intersect_bounds.IsEmpty())
            << "Cannot target DOM element at " << q << " in "
            << el->identifier() << " because its screen bounds "
            << element_bounds.ToString()
            << " are outside the screen bounds of the containing WebView, "
            << container_bounds.ToString()
            << ". Did you forget to scroll the element into view? See "
               "ScrollToVisible().";
        return intersect_bounds.CenterPoint();
      },
      query);
}

Browser* InteractiveBrowserTestApi::GetBrowserFor(
    ui::ElementContext current_context,
    BrowserSpecifier spec) {
  return absl::visit(
      base::Overloaded{[](AnyBrowser) -> Browser* { return nullptr; },
                       [current_context](CurrentBrowser) {
                         Browser* const browser =
                             InteractionTestUtilBrowser::GetBrowserFromContext(
                                 current_context);
                         CHECK(browser) << "Current context is not a browser.";
                         return browser;
                       },
                       [](Browser* browser) {
                         CHECK(browser)
                             << "BrowserSpecifier: Browser* is null.";
                         return browser;
                       },
                       [](std::reference_wrapper<Browser*> browser) {
                         CHECK(browser.get())
                             << "BrowserSpecifier: Browser* is null.";
                         return browser.get();
                       }},
      spec);
}

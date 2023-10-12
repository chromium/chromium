// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithLinks[] = "/links.html";
constexpr char kScrollableDocument[] =
    "/scroll/scrollable_page_with_content.html";
}  // namespace

class InteractiveBrowserTestBrowsertest : public InteractiveBrowserTest {
 public:
  InteractiveBrowserTestBrowsertest() = default;
  ~InteractiveBrowserTestBrowsertest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       EnsurePresentNotPresent) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(InstrumentTab(kWebContentsId),
                  NavigateWebContents(kWebContentsId, url),
                  EnsurePresent(kWebContentsId, DeepQuery({"#select"})),
                  EnsureNotPresent(kWebContentsId, DeepQuery{"#doesNotExist"}));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       EnsureNotPresent_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InstrumentTab(kWebContentsId),
                      NavigateWebContents(kWebContentsId, url),
                      EnsureNotPresent(kWebContentsId, DeepQuery{"#select"})));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, EnsurePresent_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          EnsurePresent(kWebContentsId, DeepQuery{"#doesNotExist"})));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, ExecuteJs) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJs(kWebContentsId, "() => { window.value = 1; }"),
      WithElement(kWebContentsId, base::BindOnce([](ui::TrackedElement* el) {
                    const auto result = AsInstrumentedWebContents(el)->Evaluate(
                        "() => window.value");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       ExecuteJsFireAndForget) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJs(kWebContentsId, "() => { window.value = 1; }",
                ExecuteJsMode::kFireAndForget),
      WithElement(kWebContentsId, base::BindOnce([](ui::TrackedElement* el) {
                    const auto result = AsInstrumentedWebContents(el)->Evaluate(
                        "() => window.value");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       ExecuteJsFailsOnThrow) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          ExecuteJs(kWebContentsId, "() => { throw new Error('an error'); }")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResult) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const std::string str("a string");
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJs(kWebContentsId,
                R"(() => {
            window.intValue = 1;
            window.boolValue = true;
            window.doubleValue = 2.0;
            window.stringValue = 'a string';
          })"),
      CheckJsResult(kWebContentsId, "() => window.intValue"),
      CheckJsResult(kWebContentsId, "() => window.intValue", 1),
      CheckJsResult(kWebContentsId, "() => window.intValue", testing::Lt(2)),
      CheckJsResult(kWebContentsId, "() => window.boolValue"),
      CheckJsResult(kWebContentsId, "() => window.boolValue", true),
      CheckJsResult(kWebContentsId, "() => window.boolValue",
                    testing::Ne(false)),
      CheckJsResult(kWebContentsId, "() => window.doubleValue"),
      CheckJsResult(kWebContentsId, "() => window.doubleValue", 2.0),
      CheckJsResult(kWebContentsId, "() => window.doubleValue",
                    testing::Gt(1.5)),
      CheckJsResult(kWebContentsId, "() => window.stringValue"),
      CheckJsResult(kWebContentsId, "() => window.stringValue", "a string"),
      CheckJsResult(kWebContentsId, "() => window.stringValue", str),
      CheckJsResult(kWebContentsId, "() => window.stringValue",
                    std::string("a string")),
      CheckJsResult(kWebContentsId, "() => window.stringValue",
                    testing::Ne(std::string("another string"))));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultWithPromise) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(InstrumentTab(kWebContentsId),
                  NavigateWebContents(kWebContentsId, url),
                  CheckJsResult(kWebContentsId,
                                "() => new Promise((resolve, reject) => "
                                "setTimeout(() => resolve(true), 100))"),
                  CheckJsResult(kWebContentsId,
                                "() => new Promise((resolve, reject) => "
                                "setTimeout(() => resolve(1), 100))",
                                1));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultWithPromiseFailsOnReject) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          CheckJsResult(kWebContentsId,
                        "() => new Promise((resolve, reject) => "
                        "setTimeout(() => reject('rejected'), 100))")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResult_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InstrumentTab(kWebContentsId),
                      NavigateWebContents(kWebContentsId, url),
                      ExecuteJs(kWebContentsId, "() => { window.value = 1; }"),
                      CheckJsResult(kWebContentsId, "() => window.value", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResult_ThrowError_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          CheckJsResult(kWebContentsId,
                        "() => { throw new Error('an error'); }", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResult_NoArgument_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InstrumentTab(kWebContentsId),
                      NavigateWebContents(kWebContentsId, url),
                      ExecuteJs(kWebContentsId, "() => { window.value = 0; }"),
                      CheckJsResult(kWebContentsId, "() => window.value")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, ExecuteJsAt) {
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }"),
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&kWhere](ui::TrackedElement* el) {
                    const auto result =
                        AsInstrumentedWebContents(el)->EvaluateAt(
                            kWhere, "(el) => el.intValue");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       ExecuteJsAtFireAndForget) {
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }",
                  ExecuteJsMode::kFireAndForget),
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&kWhere](ui::TrackedElement* el) {
                    const auto result =
                        AsInstrumentedWebContents(el)->EvaluateAt(
                            kWhere, "(el) => el.intValue");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       ExecuteJsAtFailsIfElementNotPresent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#aaaaa"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       ExecuteJsAtFailsOnThrow) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InstrumentTab(kWebContentsId),
                      NavigateWebContents(kWebContentsId, url),
                      ExecuteJsAt(kWebContentsId, kWhere,
                                  "(el) => { throw new Error('an error'); }")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResultAt) {
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const std::string str("a string");
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJsAt(kWebContentsId, kWhere,
                  R"((el) => {
            el.intValue = 1;
            el.boolValue = true;
            el.doubleValue = 2.0;
            el.stringValue = 'a string';
          })"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue", 1),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue",
                      testing::Lt(2)),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.boolValue"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.boolValue", true),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.boolValue",
                      testing::Ne(false)),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.doubleValue"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.doubleValue", 2.0),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.doubleValue",
                      testing::Gt(1.5)),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue",
                      "a string"),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue", str),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue",
                      std::string("a string")),
      CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue",
                      testing::Ne(std::string("another string"))));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAtWithPromise) {
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      ExecuteJsAt(kWebContentsId, kWhere,
                  R"((el) => {
            el.intValue = 1;
            el.stringValue = 'a string';
          })"),
      CheckJsResultAt(
          kWebContentsId, kWhere,
          "(el) => new Promise((resolve, reject) => resolve(el.intValue))"),
      CheckJsResultAt(
          kWebContentsId, kWhere,
          "(el) => new Promise((resolve, reject) => resolve(el.intValue))", 1),
      CheckJsResultAt(
          kWebContentsId, kWhere,
          "(el) => new Promise((resolve, reject) => resolve(el.stringValue))"),
      CheckJsResultAt(
          kWebContentsId, kWhere,
          "(el) => new Promise((resolve, reject) => resolve(el.stringValue))",
          "a string"));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAtWithPromiseFailsOnReject) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InstrumentTab(kWebContentsId),
                      NavigateWebContents(kWebContentsId, url),
                      CheckJsResultAt(kWebContentsId, kWhere,
                                      "(el) => new Promise((resolve, reject) "
                                      "=> reject('rejected!'))")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAt_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }"),
          CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAt_ThrowsError_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          CheckJsResultAt(kWebContentsId, kWhere,
                          "(el) => { throw new Error('an error'); }", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAt_BadPath_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#aaaaa"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       CheckJsResultAt_NoArgument_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          InstrumentTab(kWebContentsId),
          NavigateWebContents(kWebContentsId, url),
          ExecuteJsAt(kWebContentsId, kWhere,
                      "(el) => { el.stringValue = ''; }"),
          CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.stringValue")));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       InstrumentTabsAsTestSteps) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab3Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIncognito1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIncognito2Id);
  const char kIncognitoNtbName[] = "Incognito NTB";

  auto verify_is_at_tab_index = [](Browser* where, ui::ElementIdentifier id,
                                   int expected_index) {
    return CheckElement(
        id, base::BindLambdaForTesting([where](ui::TrackedElement* el) {
          return where->tab_strip_model()->GetIndexOfWebContents(
              AsInstrumentedWebContents(el)->web_contents());
        }),
        expected_index);
  };

  const GURL url1 = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithLinks);

  Browser* incognito_browser = CreateIncognitoBrowser();

  RunTestSequence(
      // Instrument an existing tab.
      InstrumentTab(kTab1Id), verify_is_at_tab_index(browser(), kTab1Id, 0),

      // Instrument the next tab, then insert a tab and verify it's there.
      InstrumentNextTab(kTab2Id), PressButton(kNewTabButtonElementId),
      NavigateWebContents(kTab2Id, url1),
      verify_is_at_tab_index(browser(), kTab2Id, 1),

      // Add and instrument tab all in one fell swoop.
      AddInstrumentedTab(kTab3Id, url2),
      verify_is_at_tab_index(browser(), kTab3Id, 2),

      // Instrument the next tab in any browser, then insert the tab and verify
      // it's there.
      InstrumentNextTab(kIncognito1Id, AnyBrowser()),
      NameView(kIncognitoNtbName,
               base::BindLambdaForTesting([incognito_browser]() {
                 return AsView(
                     ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kNewTabButtonElementId,
                         incognito_browser->window()->GetElementContext()));
               })),
      PressButton(kIncognitoNtbName),
      InAnyContext(verify_is_at_tab_index(incognito_browser, kIncognito1Id, 1)),

      Do(base::BindOnce([]() { LOG(WARNING) << 1; })),

      // Instrument a final tab by inserting it. Specify an index so the other
      // tabs are re-ordered.
      AddInstrumentedTab(kIncognito2Id, url2, 1, incognito_browser),
      InAnyContext(verify_is_at_tab_index(incognito_browser, kIncognito2Id, 1)),
      InAnyContext(
          verify_is_at_tab_index(incognito_browser, kIncognito1Id, 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, ScrollIntoView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  const GURL url = embedded_test_server()->GetURL(kScrollableDocument);
  const DeepQuery kLink{"#link"};
  const DeepQuery kText{"#text"};

  constexpr char kElementIsInViewport[] = R"(
    (el) => {
      const bounds = el.getBoundingClientRect();
      return bounds.right >= 0 && bounds.bottom >= 0 &&
             bounds.x < window.innerWidth && bounds.y < window.innerHeight;
    }
  )";

  RunTestSequence(InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
                  CheckJsResultAt(kTabId, kLink, kElementIsInViewport, true),
                  CheckJsResultAt(kTabId, kText, kElementIsInViewport, false),
                  ScrollIntoView(kTabId, kText),
                  CheckJsResultAt(kTabId, kLink, kElementIsInViewport, false),
                  CheckJsResultAt(kTabId, kText, kElementIsInViewport, true));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       WaitForStateChangeAcrossNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFoundElementEvent);
  const GURL url1 = embedded_test_server()->GetURL(kDocumentWithLinks);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  StateChange state_change;
  state_change.type = StateChange::Type::kExists;
  state_change.where = {"#select"};
  state_change.continue_across_navigation = true;
  state_change.event = kFoundElementEvent;

  RunTestSequence(
      InstrumentTab(kTabId),
      // This is needed to prevent subsequent navigation from causing the
      // previous step to fail due to the element immediately losing visibility.
      FlushEvents(),
      InParallel(Steps(NavigateWebContents(kTabId, url1),
                       NavigateWebContents(kTabId, url2)),
                 WaitForStateChange(kTabId, state_change)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       WaitForStateChangeWithConditionAcrossNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFoundElementEvent);
  const GURL url1 = embedded_test_server()->GetURL(kDocumentWithLinks);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  StateChange state_change;
  state_change.type = StateChange::Type::kExistsAndConditionTrue;
  state_change.where = {"#select option[selected]"};
  state_change.test_function = "(el) => (el.innerText === 'Apple')";
  state_change.continue_across_navigation = true;
  state_change.event = kFoundElementEvent;

  RunTestSequence(
      InstrumentTab(kTabId),
      // This is needed to prevent subsequent navigation from causing the
      // previous step to fail due to the element immediately losing visibility.
      FlushEvents(),
      InParallel(Steps(NavigateWebContents(kTabId, url1),
                       NavigateWebContents(kTabId, url2)),
                 WaitForStateChange(kTabId, state_change)));
}

// Parameter for WebUI coverage tests.
struct CoverageConfig {
  // Whether to set the --devtools-code-coverage flag. If it's not set, nothing
  // should be captured, and the test is simply verifying that no errors are
  // generated as a result.
  bool command_line_flag = false;

  // Whether coverage is actively enabled. If the command line flag is also set,
  // the test will check whether data got written to the code coverage folder.
  bool enable_coverage = false;
};

// Test fixture to verify that when EnableWebUICodeCoverage() is called with the
// correct command-line arguments, coverage data actually gets written out. It
// also verifies that
class InteractiveBrowserTestCodeCoverageBrowsertest
    : public InteractiveBrowserTestBrowsertest,
      public testing::WithParamInterface<CoverageConfig> {
 public:
  void SetUp() override {
    {
      // This is required for file IO.
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(tmp_dir_.CreateUniqueTempDir());
      ASSERT_TRUE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));
    }
    InteractiveBrowserTestBrowsertest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().command_line_flag) {
      command_line->AppendSwitchPath(switches::kDevtoolsCodeCoverage,
                                     tmp_dir_.GetPath());
    } else {
      command_line->RemoveSwitch(switches::kDevtoolsCodeCoverage);
    }
    InteractiveBrowserTestBrowsertest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTestBrowsertest::SetUpOnMainThread();
    if (GetParam().enable_coverage) {
      EnableWebUICodeCoverage();
    }
  }

  // This is where we actually verify that the data has been written out, since
  // coverage output doesn't happen until teardown.
  void TearDownOnMainThread() override {
    InteractiveBrowserTestBrowsertest::TearDownOnMainThread();

    if (GetParam().command_line_flag) {
      // This is required for file IO.
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (GetParam().enable_coverage) {
        // Scripts and tests are special directories under the WebUI specific
        // directory, ensure they have been created and are not empty.
        base::FilePath coverage_dir =
            tmp_dir_.GetPath().AppendASCII("webui_javascript_code_coverage");
        EXPECT_FALSE(
            base::IsDirectoryEmpty(coverage_dir.AppendASCII("scripts")));
        EXPECT_FALSE(base::IsDirectoryEmpty(coverage_dir.AppendASCII("tests")));
      } else {
        EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_.GetPath()));
      }
    }
  }

 protected:
  base::ScopedTempDir tmp_dir_;
};

INSTANTIATE_TEST_SUITE_P(,
                         InteractiveBrowserTestCodeCoverageBrowsertest,
                         testing::Values(CoverageConfig{false, false},
                                         CoverageConfig{false, true},
                                         CoverageConfig{true, false},
                                         CoverageConfig{true, true}));

IN_PROC_BROWSER_TEST_P(InteractiveBrowserTestCodeCoverageBrowsertest,
                       TestCoverageEmits) {
  // Navigate and load the New Tab Page, which we know works with code coverage.
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, GURL("chrome://history")));
}

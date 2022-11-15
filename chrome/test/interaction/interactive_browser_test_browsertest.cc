// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/test/interaction/interactive_browser_test.h"

#include "base/bind.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
constexpr char kDocumentWithNamedElement[] = "/select.html";
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

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, EnsureNotPresent) {
  InstrumentTab(browser(), kWebContentsId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(NavigateWebContents(kWebContentsId, url),
                  EnsureNotPresent(kWebContentsId, DeepQuery{"#doesNotExist"}));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest,
                       EnsureNotPresent_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  InstrumentTab(browser(), kWebContentsId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(NavigateWebContents(kWebContentsId, url),
                      EnsureNotPresent(kWebContentsId, DeepQuery{"#select"})));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, ExecuteJs) {
  InstrumentTab(browser(), kWebContentsId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      NavigateWebContents(kWebContentsId, url),
      ExecuteJs(kWebContentsId, "() => { window.value = 1; }"),
      WithElement(kWebContentsId, base::BindOnce([](ui::TrackedElement* el) {
                    const auto result = AsInstrumentedWebContents(el)->Evaluate(
                        "() => window.value");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResult) {
  InstrumentTab(browser(), kWebContentsId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const std::string str("a string");
  RunTestSequence(
      NavigateWebContents(kWebContentsId, url),
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

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResult_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  InstrumentTab(browser(), kWebContentsId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(NavigateWebContents(kWebContentsId, url),
                      ExecuteJs(kWebContentsId, "() => { window.value = 1; }"),
                      CheckJsResult(kWebContentsId, "() => window.value", 2)));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, ExecuteJsAt) {
  InstrumentTab(browser(), kWebContentsId);
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  RunTestSequence(
      NavigateWebContents(kWebContentsId, url),
      ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }"),
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&kWhere](ui::TrackedElement* el) {
                    const auto result =
                        AsInstrumentedWebContents(el)->EvaluateAt(
                            kWhere, "(el) => el.intValue");
                    EXPECT_EQ(1, result.GetInt());
                  })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckJsResultAt) {
  InstrumentTab(browser(), kWebContentsId);
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const std::string str("a string");
  RunTestSequence(
      NavigateWebContents(kWebContentsId, url),
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
                       CheckJsResultAt_Fails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  InstrumentTab(browser(), kWebContentsId);
  const DeepQuery kWhere{"#select"};
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          NavigateWebContents(kWebContentsId, url),
          ExecuteJsAt(kWebContentsId, kWhere, "(el) => { el.intValue = 1; }"),
          CheckJsResultAt(kWebContentsId, kWhere, "(el) => el.intValue", 2)));
}

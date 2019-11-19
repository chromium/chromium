// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/actions/click_action.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::AnyOf;
using ::testing::IsEmpty;

// Flag to enable site per process to enforce OOPIFs.
const char* kSitePerProcess = "site-per-process";
const char* kTargetWebsitePath = "/autofill_assistant_target_website.html";

class WebControllerBrowserTest : public content::ContentBrowserTest,
                                 public content::WebContentsObserver {
 public:
  WebControllerBrowserTest() {}
  ~WebControllerBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Start a mock server for hosting an OOPIF.
    http_server_iframe_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_iframe_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant/html_iframe");
    ASSERT_TRUE(http_server_iframe_->Start(8081));

    // Start the main server hosting the test page.
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant/html");
    ASSERT_TRUE(http_server_->Start(8080));
    ASSERT_TRUE(
        NavigateToURL(shell(), http_server_->GetURL(kTargetWebsitePath)));
    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &settings_);
    Observe(shell()->web_contents());
  }

  void WaitTillPageIsIdle(base::TimeDelta continuous_paint_timeout) {
    base::TimeTicks finished_load_time = base::TimeTicks::Now();
    while (true) {
      content::RenderFrameSubmissionObserver frame_submission_observer(
          web_contents());
      // Runs a loop for 3 seconds to see if the renderer is idle.
      {
        base::RunLoop heart_beat;
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, heart_beat.QuitClosure(),
            base::TimeDelta::FromSeconds(3));
        heart_beat.Run();
      }
      bool page_is_loading =
          web_contents()->IsWaitingForResponse() || web_contents()->IsLoading();
      if (page_is_loading) {
        finished_load_time = base::TimeTicks::Now();
      } else if ((base::TimeTicks::Now() - finished_load_time) >
                 continuous_paint_timeout) {
        // |continuous_paint_timeout| has expired since Chrome loaded the page.
        // During this period of time, Chrome has been continuously painting
        // the page. In this case, the page is probably idle, but a bug, a
        // blinking caret or a persistent animation is making Chrome paint at
        // regular intervals. Exit.
        break;
      } else if (frame_submission_observer.render_frame_count() == 0) {
        // If the renderer has stopped submitting frames for 3 seconds then
        // we're done.
        break;
      }
    }
  }

  void RunStrictElementCheck(const Selector& selector, bool result) {
    RunElementCheck(/* strict= */ true, selector, result);
  }

  void RunLaxElementCheck(const Selector& selector, bool result) {
    RunElementCheck(/* strict= */ false, selector, result);
  }

  void RunElementCheck(bool strict, const Selector& selector, bool result) {
    std::vector<Selector> selectors{selector};
    std::vector<bool> results{result};
    RunElementChecks(strict, selectors, results);
  }

  void RunElementChecks(bool strict,
                        const std::vector<Selector>& selectors,
                        const std::vector<bool> results) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), results.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->ElementCheck(
          selectors[i], strict,
          base::BindOnce(&WebControllerBrowserTest::CheckElementVisibleCallback,
                         base::Unretained(this), run_loop.QuitClosure(),
                         selectors[i], &pending_number_of_checks, results[i]));
    }
    run_loop.Run();
  }

  void CheckElementVisibleCallback(const base::Closure& done_callback,
                                   const Selector& selector,
                                   size_t* pending_number_of_checks_output,
                                   bool expected_result,
                                   const ClientStatus& result) {
    EXPECT_EQ(expected_result, result.ok()) << "selector: " << selector;
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      done_callback.Run();
    }
  }

  void ClickElementCallback(const base::Closure& done_callback,
                            const ClientStatus& status) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    done_callback.Run();
  }

  void ClickOrTapElement(const Selector& selector,
                         ClickAction::ClickType click_type) {
    base::RunLoop run_loop;
    web_controller_->ClickOrTapElement(
        selector, click_type,
        base::BindOnce(&WebControllerBrowserTest::ClickElementCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void WaitForElementRemove(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->ElementCheck(
        selector, /* strict= */ false,
        base::BindOnce(&WebControllerBrowserTest::OnWaitForElementRemove,
                       base::Unretained(this), run_loop.QuitClosure(),
                       selector));
    run_loop.Run();
  }

  void OnWaitForElementRemove(const base::Closure& done_callback,
                              const Selector& selector,
                              const ClientStatus& result) {
    done_callback.Run();
    if (result.ok()) {
      WaitForElementRemove(selector);
    }
  }

  void FocusElement(const Selector& selector, const TopPadding top_padding) {
    base::RunLoop run_loop;
    web_controller_->FocusElement(
        selector, top_padding,
        base::BindOnce(&WebControllerBrowserTest::OnFocusElement,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnFocusElement(base::OnceClosure done_callback,
                      const ClientStatus& status) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    std::move(done_callback).Run();
  }

  ClientStatus SelectOption(const Selector& selector,
                            const std::string& option) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SelectOption(
        selector, option,
        base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnClientStatus(base::Closure done_callback,
                      ClientStatus* result_output,
                      const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  void OnClientStatusAndReadyState(base::Closure done_callback,
                                   ClientStatus* result_output,
                                   DocumentReadyState* ready_state_out,
                                   const ClientStatus& status,
                                   DocumentReadyState ready_state) {
    *result_output = status;
    *ready_state_out = ready_state;
    std::move(done_callback).Run();
  }

  ClientStatus HighlightElement(const Selector& selector) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->HighlightElement(
        selector, base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                                 base::Unretained(this), run_loop.QuitClosure(),
                                 &result));
    run_loop.Run();
    return result;
  }

  ClientStatus GetOuterHtml(const Selector& selector,
                            std::string* html_output) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->GetOuterHtml(
        selector, base::BindOnce(&WebControllerBrowserTest::OnGetOuterHtml,
                                 base::Unretained(this), run_loop.QuitClosure(),
                                 &result, html_output));
    run_loop.Run();
    return result;
  }

  void OnGetOuterHtml(const base::Closure& done_callback,
                      ClientStatus* successful_output,
                      std::string* html_output,
                      const ClientStatus& status,
                      const std::string& html) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    *successful_output = status;
    *html_output = html;
    done_callback.Run();
  }

  void FindElement(const Selector& selector,
                   ClientStatus* status_out,
                   ElementFinder::Result* result_out) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                       base::Unretained(this), run_loop.QuitClosure(),
                       base::Unretained(status_out),
                       base::Unretained(result_out)));
    run_loop.Run();
  }

  void OnFindElement(const base::Closure& done_callback,
                     ClientStatus* status_out,
                     ElementFinder::Result* result_out,
                     const ClientStatus& status,
                     std::unique_ptr<ElementFinder::Result> result) {
    ASSERT_TRUE(result);
    done_callback.Run();
    if (status_out)
      *status_out = status;

    if (result_out)
      *result_out = *result;
  }

  void FindElementAndCheck(const Selector& selector,
                           size_t expected_index,
                           bool is_main_frame) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    ElementFinder::Result result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    CheckFindElementResult(result, expected_index, is_main_frame);
  }

  void FindElementExpectEmptyResult(const Selector& selector) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    ElementFinder::Result result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
    EXPECT_THAT(result.object_id, IsEmpty());
  }

  void CheckFindElementResult(const ElementFinder::Result& result,
                              size_t expected_index,
                              bool is_main_frame) {
    if (is_main_frame) {
      EXPECT_EQ(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
    } else {
      EXPECT_NE(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
    }
    EXPECT_EQ(result.container_frame_selector_index, expected_index);
    EXPECT_FALSE(result.object_id.empty());
  }

  void GetFieldsValue(const std::vector<Selector>& selectors,
                      const std::vector<std::string>& expected_values) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), expected_values.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->GetFieldValue(
          selectors[i],
          base::BindOnce(&WebControllerBrowserTest::OnGetFieldValue,
                         base::Unretained(this), run_loop.QuitClosure(),
                         &pending_number_of_checks, expected_values[i]));
    }
    run_loop.Run();
  }

  void OnGetFieldValue(const base::Closure& done_callback,
                       size_t* pending_number_of_checks_output,
                       const std::string& expected_value,
                       const ClientStatus& status,
                       const std::string& value) {
    // Don't use ASSERT_EQ here: if the check fails, this would result in
    // an endless loop without meaningful test results.
    EXPECT_EQ(expected_value, value);
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  ClientStatus SetFieldValue(const Selector& selector,
                             const std::string& value,
                             bool simulate_key_presses) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SetFieldValue(
        selector, value, simulate_key_presses, 0,
        base::BindOnce(&WebControllerBrowserTest::OnSetFieldValue,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSetFieldValue(const base::Closure& done_callback,
                       ClientStatus* result_output,
                       const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints,
                                 int delay_in_milli) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SendKeyboardInput(
        selector, codepoints, delay_in_milli,
        base::BindOnce(&WebControllerBrowserTest::OnSendKeyboardInput,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints) {
    return SendKeyboardInput(selector, codepoints, -1);
  }

  void OnSendKeyboardInput(const base::Closure& done_callback,
                           ClientStatus* result_output,
                           const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attribute,
                            const std::string& value) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SetAttribute(
        selector, attribute, value,
        base::BindOnce(&WebControllerBrowserTest::OnSetAttribute,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSetAttribute(const base::Closure& done_callback,
                      ClientStatus* result_output,
                      const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  // Make sure scrolling is necessary for #scroll_container , no matter the
  // screen height
  void SetupScrollContainerHeights() {
    EXPECT_TRUE(content::ExecJs(shell(),
                                R"(
           let before = document.querySelector("#before_scroll_container");
           before.style.height = window.innerHeight + "px";
           let after = document.querySelector("#after_scroll_container");
           after.style.height = window.innerHeight + "px";)"));
  }

  // Scrolls #scroll_container to the given y position.
  void ScrollContainerTo(int y) {
    EXPECT_TRUE(content::ExecJs(shell(), base::StringPrintf(
                                             R"(
           let container = document.querySelector("#scroll_container");
           container.scrollTo(0, %d);)",
                                             y)));
  }

  // Scrolls the window to the given y position.
  void ScrollWindowTo(int y) {
    EXPECT_TRUE(content::ExecJs(
        shell(), base::StringPrintf("window.scrollTo(0, %d);", y)));
  }

  // Scroll an element into view that's within a container element. This
  // requires scrolling the container, then the window, to get the element to
  // the desired y position.
  void TestScrollIntoView(int initial_window_scroll_y,
                          int initial_container_scroll_y) {
    Selector selector;
    selector.selectors.emplace_back("#scroll_item_5");

    SetupScrollContainerHeights();
    ScrollWindowTo(initial_window_scroll_y);
    ScrollContainerTo(initial_window_scroll_y);

    TopPadding top_padding{0.25, TopPadding::Unit::RATIO};
    FocusElement(selector, top_padding);
    base::ListValue eval_result = content::EvalJs(shell(), R"(
      let item = document.querySelector("#scroll_item_5");
      let itemRect = item.getBoundingClientRect();
      let container = document.querySelector("#scroll_container");
      let containerRect = container.getBoundingClientRect();
      [itemRect.top, itemRect.bottom, window.innerHeight,
           containerRect.top, containerRect.bottom])")
                                      .ExtractList();
    double top = eval_result.GetList()[0].GetDouble();
    double bottom = eval_result.GetList()[1].GetDouble();
    double window_height = eval_result.GetList()[2].GetDouble();
    double container_top = eval_result.GetList()[3].GetDouble();
    double container_bottom = eval_result.GetList()[4].GetDouble();

    // Element is at the desired position. (top is relative to the viewport)
    EXPECT_NEAR(top, window_height * 0.25, 0.5);

    // Element is within the visible portion of its container.
    EXPECT_GT(bottom, container_top);
    EXPECT_LT(top, container_bottom);
  }

 protected:
  std::unique_ptr<WebController> web_controller_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_iframe_;
  ClientSettings settings_;

  DISALLOW_COPY_AND_ASSIGN(WebControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementExistenceCheck) {
  // A visible element
  RunLaxElementCheck(Selector({"#button"}), true);

  // A hidden element.
  RunLaxElementCheck(Selector({"#hidden"}), true);

  // A nonexistent element.
  RunLaxElementCheck(Selector({"#doesnotexist"}), false);

  // A pseudo-element
  RunLaxElementCheck(Selector({"#terms-and-conditions"}, BEFORE), true);

  // An invisible pseudo-element
  //
  // TODO(b/129461999): This is wrong; it should exist. Fix it.
  RunLaxElementCheck(Selector({"#button"}, BEFORE), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}, AFTER), false);

  // An iFrame.
  RunLaxElementCheck(Selector({"#iframe"}), true);

  // An element in a same-origin iFrame.
  RunLaxElementCheck(Selector({"#iframe", "#button"}), true);

  // An OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal"}), true);

  // An element in an OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal", "#button"}), true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, VisibilityRequirementCheck) {
  // A visible element
  RunLaxElementCheck(Selector({"#button"}).MustBeVisible(), true);

  // A hidden element.
  RunLaxElementCheck(Selector({"#hidden"}).MustBeVisible(), false);

  // A non-existent element
  RunLaxElementCheck(Selector({"#doesnotexist"}).MustBeVisible(), false);

  // A pseudo-element
  RunLaxElementCheck(
      Selector({"#terms-and-conditions"}, BEFORE).MustBeVisible(), true);

  // An invisible pseudo-element
  RunLaxElementCheck(Selector({"#button"}, BEFORE).MustBeVisible(), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}, AFTER).MustBeVisible(), false);

  // An iFrame.
  RunLaxElementCheck(Selector({"#iframe"}).MustBeVisible(), true);

  // An element in a same-origin iFrame.
  RunLaxElementCheck(Selector({"#iframe", "#button"}).MustBeVisible(), true);

  // An OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal"}).MustBeVisible(), true);

  // An element in an OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal", "#button"}).MustBeVisible(),
                     true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, MultipleVisibleElementCheck) {
  // both visible
  RunLaxElementCheck(Selector({"#button,#select"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#button,#select"}).MustBeVisible(), false);

  // one visible (first non-visible)
  RunLaxElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);

  // one visible (first visible)
  RunLaxElementCheck(Selector({"#button,#hidden"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);

  // one invisible, one non-existent
  RunLaxElementCheck(Selector({"#doesnotexist,#hidden"}).MustBeVisible(),
                     false);
  RunStrictElementCheck(Selector({"#doesnotexist,#hidden"}).MustBeVisible(),
                        false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, InnerTextCondition) {
  Selector selector({"#with_inner_text span"});
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector.MustBeVisible(), false);

  // No matches
  selector.inner_text_pattern = "no match";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, false);

  // Matches exactly one visible element.
  selector.inner_text_pattern = "hello, world";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);

  // Matches two visible elements
  selector.inner_text_pattern = "^hello";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);

  // Matches one visible, one invisible element
  selector.inner_text_pattern = "world$";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);

  // Inner text conditions are applied before looking for the pseudo-type.
  selector.pseudo_type = PseudoType::BEFORE;
  selector.inner_text_pattern = "world";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  selector.inner_text_pattern = "before";  // matches :before content
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ValueCondition) {
  // One match
  RunLaxElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"), true);
  RunStrictElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"),
                        true);

  // No matches
  RunLaxElementCheck(Selector({"#input2"}).MatchingValue("doesnotmatch"),
                     false);
  RunStrictElementCheck(Selector({"#input2"}).MatchingValue("doesnotmatch"),
                        false);

  // Multiple matches
  RunLaxElementCheck(Selector({"#input1,#input2"}).MatchingValue("^hello"),
                     true);
  RunStrictElementCheck(Selector({"#input1,#input2"}).MatchingValue("^hello"),
                        false);

  // Multiple selector matches, one value match
  RunLaxElementCheck(Selector({"#input1,#input2"}).MatchingValue("helloworld1"),
                     true);
  RunStrictElementCheck(
      Selector({"#input1,#input2"}).MatchingValue("helloworld1"), true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ConcurrentElementsVisibilityCheck) {
  std::vector<Selector> selectors;
  std::vector<bool> results;

  Selector a_selector;
  a_selector.must_be_visible = true;
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("[name=name]");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // OOPIF.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframeExternal");
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // Shadow DOM.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#shadowsection");
  a_selector.selectors.emplace_back("#shadowbutton");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame inside IFrame.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // Hidden element.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#hidden");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  RunElementChecks(/* strict= */ false, selectors, results);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElement) {
  Selector selector;
  selector.selectors.emplace_back("#button");
  ClickOrTapElement(selector, ClickAction::CLICK);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElementInIFrame) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#shadowsection");
  selector.selectors.emplace_back("#shadowbutton");
  ClickOrTapElement(selector, ClickAction::CLICK);

  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElementInOOPIF) {
  Selector selector;
  selector.selectors.emplace_back("#iframeExternal");
  selector.selectors.emplace_back("#button");
  ClickOrTapElement(selector, ClickAction::CLICK);

  selector.selectors.clear();
  selector.selectors.emplace_back("#iframeExternal");
  selector.selectors.emplace_back("#div");
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ClickElementInScrollContainer) {
  // Make sure #scroll_item_3 is not visible, no matter the screen height. It
  // also makes sure that there's enough room on the visual viewport to scroll
  // everything to the center.
  SetupScrollContainerHeights();
  ScrollWindowTo(0);
  ScrollContainerTo(0);

  EXPECT_TRUE(content::ExecJs(shell(),
                              R"(var scrollItem3WasClicked = false;
           let item = document.querySelector("#scroll_item_3");
           item.addEventListener("click", function() {
             scrollItem3WasClicked = true;
           });)"));

  Selector selector;
  selector.selectors.emplace_back("#scroll_item_3");
  ClickOrTapElement(selector, ClickAction::CLICK);

  EXPECT_TRUE(content::EvalJs(shell(), "scrollItem3WasClicked").ExtractBool());

  // TODO(b/135909926): Find a reliable way of verifying that the button was
  // mover roughly to the center.
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElement) {
  Selector selector;
  selector.selectors.emplace_back("#touch_area_two");
  ClickOrTapElement(selector, ClickAction::TAP);
  WaitForElementRemove(selector);

  selector.selectors.clear();
  selector.selectors.emplace_back("#touch_area_one");
  ClickOrTapElement(selector, ClickAction::TAP);
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElementMovingOutOfView) {
  Selector selector;
  selector.selectors.emplace_back("#touch_area_three");
  ClickOrTapElement(selector, ClickAction::TAP);
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElementAfterPageIsIdle) {
  // Set a very long timeout to make sure either the page is idle or the test
  // timeout.
  WaitTillPageIsIdle(base::TimeDelta::FromHours(1));

  Selector selector;
  selector.selectors.emplace_back("#touch_area_one");
  ClickOrTapElement(selector, ClickAction::TAP);

  WaitForElementRemove(selector);
}

// TODO(crbug.com/920948) Disabled for strong flakiness.
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, DISABLED_TapElementInIFrame) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#touch_area");
  ClickOrTapElement(selector, ClickAction::TAP);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       TapRandomMovingElementRepeatedly) {
  Selector button_selector({"#random_moving_button"});
  int num_clicks = 100;
  for (int i = 0; i < num_clicks; ++i) {
    ClickOrTapElement(button_selector, ClickAction::JAVASCRIPT);
  }

  std::vector<Selector> click_counter_selectors;
  std::vector<std::string> expected_values;
  expected_values.emplace_back(base::NumberToString(num_clicks));
  Selector click_counter_selector({"#random_moving_click_counter"});
  click_counter_selectors.emplace_back(click_counter_selector);
  GetFieldsValue(click_counter_selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapMovingElementRepeatedly) {
  Selector button_selector({"#moving_button"});
  int num_clicks = 100;
  for (int i = 0; i < num_clicks; ++i) {
    ClickOrTapElement(button_selector, ClickAction::JAVASCRIPT);
  }

  std::vector<Selector> click_counter_selectors;
  std::vector<std::string> expected_values;
  expected_values.emplace_back(base::NumberToString(num_clicks));
  Selector click_counter_selector({"#moving_click_counter"});
  click_counter_selectors.emplace_back(click_counter_selector);
  GetFieldsValue(click_counter_selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapStaticElementRepeatedly) {
  Selector button_selector;
  button_selector.selectors.emplace_back("#static_button");

  int num_clicks = 100;
  for (int i = 0; i < num_clicks; ++i) {
    ClickOrTapElement(button_selector, ClickAction::JAVASCRIPT);
  }

  std::vector<Selector> click_counter_selectors;
  std::vector<std::string> expected_values;
  expected_values.emplace_back(base::NumberToString(num_clicks));
  Selector click_counter_selector;
  click_counter_selector.selectors.emplace_back("#static_click_counter");
  click_counter_selectors.emplace_back(click_counter_selector);
  GetFieldsValue(click_counter_selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickPseudoElement) {
  const std::string javascript = R"(
    document.querySelector("#terms-and-conditions").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), javascript).ExtractBool());
  Selector selector({R"(label[for="terms-and-conditions"])"},
                    PseudoType::BEFORE);
  ClickOrTapElement(selector, ClickAction::CLICK);
  EXPECT_TRUE(content::EvalJs(shell(), javascript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElement) {
  Selector selector;
  selector.selectors.emplace_back("#button");
  FindElementAndCheck(selector, 0, true);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, true);

  // IFrame.
  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 0, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, false);

  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("[name=name]");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 0, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, false);

  // IFrame inside IFrame.
  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 1, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 1, false);

  // OutOfProcessIFrame.
  selector.selectors.clear();
  selector.selectors.emplace_back("#iframeExternal");
  selector.selectors.emplace_back("#button");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 1, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 1, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElementNotFound) {
  FindElementExpectEmptyResult(Selector({"#notfound"}));
  FindElementExpectEmptyResult(Selector({"#hidden"}).MustBeVisible());
  FindElementExpectEmptyResult(Selector({"#iframe", "#iframe", "#notfound"}));
  FindElementExpectEmptyResult(
      Selector({"#iframe", "#iframe", "#hidden"}).MustBeVisible());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElementErrorStatus) {
  ClientStatus status;

  FindElement(
      Selector(ElementReferenceProto::default_instance()).MustBeVisible(),
      &status, nullptr);
  EXPECT_EQ(INVALID_SELECTOR, status.proto_status());

  FindElement(Selector({"#doesnotexist"}).MustBeVisible(), &status, nullptr);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());

  FindElement(Selector({"div"}), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());

  FindElement(Selector({"div"}).MustBeVisible(), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusElement) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#focus");

  const std::string checkVisibleScript = R"(
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      iframeRect.y + divRect.y < window.innerHeight;
  )";
  EXPECT_EQ(false, content::EvalJs(shell(), checkVisibleScript));
  TopPadding top_padding;
  FocusElement(selector, top_padding);
  EXPECT_EQ(true, content::EvalJs(shell(), checkVisibleScript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       FocusElementWithScrollIntoViewNeeded) {
  TestScrollIntoView(/* initial_window_scroll_y= */ 0,
                     /* initial_container_scroll_y=*/0);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       FocusElementWithScrollIntoViewNotNeeded) {
  TestScrollIntoView(/* initial_window_scroll_y= */ 0,
                     /* initial_container_scroll_y=*/200);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       FocusElement_WithPaddingInPixels) {
  Selector selector;
  selector.selectors.emplace_back("#scroll-me");

  const std::string checkScrollDifferentThanTargetScript = R"(
      window.scrollTo(0, 0);
      let scrollTarget = document.querySelector("#scroll-me");
      let scrollTargetRect = scrollTarget.getBoundingClientRect();
      scrollTargetRect.y > 360;
  )";

  EXPECT_EQ(true,
            content::EvalJs(shell(), checkScrollDifferentThanTargetScript));

  // Scroll 360px from the top.
  TopPadding top_padding{/* value= */ 360, TopPadding::Unit::PIXELS};
  FocusElement(selector, top_padding);

  double eval_result = content::EvalJs(shell(), R"(
      let scrollTarget = document.querySelector("#scroll-me");
      let scrollTargetRect = scrollTarget.getBoundingClientRect();
      scrollTargetRect.top;
  )")
                           .ExtractDouble();

  EXPECT_NEAR(360, eval_result, 1);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       FocusElement_WithPaddingInRatio) {
  Selector selector;
  selector.selectors.emplace_back("#scroll-me");

  const std::string checkScrollDifferentThanTargetScript = R"(
      window.scrollTo(0, 0);
      let scrollTarget = document.querySelector("#scroll-me");
      let scrollTargetRect = scrollTarget.getBoundingClientRect();
      let targetScrollY = window.innerHeight * 0.7;
      scrollTargetRect.y > targetScrollY;
  )";

  EXPECT_EQ(true,
            content::EvalJs(shell(), checkScrollDifferentThanTargetScript));

  // Scroll 70% from the top.
  TopPadding top_padding{/* value= */ 0.7, TopPadding::Unit::RATIO};
  FocusElement(selector, top_padding);

  base::ListValue eval_result = content::EvalJs(shell(), R"(
      let scrollTarget = document.querySelector("#scroll-me");
      let scrollTargetRect = scrollTarget.getBoundingClientRect();
      [scrollTargetRect.top, window.innerHeight]
  )")
                                    .ExtractList();

  double top = eval_result.GetList()[0].GetDouble();
  double window_inner_height = eval_result.GetList()[1].GetDouble();

  EXPECT_NEAR(top, window_inner_height * 0.7, 1);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOption) {
  Selector selector;
  selector.selectors.emplace_back("#select");
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "incorrect_label").proto_status());
  EXPECT_EQ(ACTION_APPLIED, SelectOption(selector, "two").proto_status());

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("Two", content::EvalJs(shell(), javascript));

  EXPECT_EQ(ACTION_APPLIED, SelectOption(selector, "one").proto_status());
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  selector.selectors.clear();
  selector.selectors.emplace_back("#incorrect_selector");
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED,
            SelectOption(selector, "two").proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIFrame) {
  Selector select_selector;

  // IFrame.
  select_selector.selectors.clear();
  select_selector.selectors.emplace_back("#iframe");
  select_selector.selectors.emplace_back("select[name=state]");
  EXPECT_EQ(ACTION_APPLIED, SelectOption(select_selector, "NY").proto_status());

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));

  // OOPIF.
  // Checking elements through EvalJs in OOPIF is blocked by cross-site.
  select_selector.selectors.clear();
  select_selector.selectors.emplace_back("#iframeExternal");
  select_selector.selectors.emplace_back("select[name=pet]");
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(select_selector, "Cat").proto_status());

  Selector result_selector;
  result_selector.selectors.emplace_back("#iframeExternal");
  result_selector.selectors.emplace_back("#myPet");
  GetFieldsValue({result_selector}, {"Cat"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtml) {
  std::string html;

  // Div.
  Selector div_selector;
  div_selector.selectors.emplace_back("#testOuterHtml");
  ASSERT_EQ(ACTION_APPLIED, GetOuterHtml(div_selector, &html).proto_status());
  EXPECT_EQ(
      R"(<div id="testOuterHtml"><span>Span</span><p>Paragraph</p></div>)",
      html);

  // IFrame.
  Selector iframe_selector;
  iframe_selector.selectors.emplace_back("#iframe");
  iframe_selector.selectors.emplace_back("#input");
  ASSERT_EQ(ACTION_APPLIED,
            GetOuterHtml(iframe_selector, &html).proto_status());
  EXPECT_EQ(R"(<input id="input" type="text">)", html);

  // OOPIF.
  Selector oopif_selector;
  oopif_selector.selectors.emplace_back("#iframeExternal");
  oopif_selector.selectors.emplace_back("#divToRemove");
  ASSERT_EQ(ACTION_APPLIED, GetOuterHtml(oopif_selector, &html).proto_status());
  EXPECT_EQ(R"(<div id="divToRemove">Text</div>)", html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValue) {
  std::vector<Selector> selectors;
  std::vector<std::string> expected_values;

  Selector a_selector;
  a_selector.selectors.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "foo", /* simulate_key_presses= */ false)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("foo");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#uppercase_input");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, /* Zürich */ "Z\xc3\xbcrich",
                          /* simulate_key_presses= */ true)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back(/* ZÜRICH */ "Z\xc3\x9cRICH");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#invalid_selector");
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(
      ELEMENT_RESOLUTION_FAILED,
      SetFieldValue(a_selector, "foobar", /* simulate_key_presses= */ false)
          .proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValueInIFrame) {
  Selector a_selector;

  // IFrame.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#input");
  EXPECT_EQ(ACTION_APPLIED, SetFieldValue(a_selector, "text",
                                          /* simulate_key_presses= */ false)
                                .proto_status());
  GetFieldsValue({a_selector}, {"text"});

  // OOPIF.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframeExternal");
  a_selector.selectors.emplace_back("#input");
  EXPECT_EQ(ACTION_APPLIED, SetFieldValue(a_selector, "text",
                                          /* simulate_key_presses= */ false)
                                .proto_status());
  GetFieldsValue({a_selector}, {"text"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendKeyboardInput) {
  auto input = UTF8ToUnicode("Zürich");
  std::string expected_output = "Zürich";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input6");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input).proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputSetsKeyProperty) {
  auto input = UTF8ToUnicode("Zürich\r");
  std::string expected_output = "ZürichEnter";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input_js_event_listener");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input).proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputSetsKeyPropertyWithTimeout) {
  // Sends input keys to a field where JS will intercept KeyDown and
  // at index 3 it will set a timeout whick inserts an space after the
  // timeout. If key press delay is enabled, it should handle this
  // correctly.
  auto input = UTF8ToUnicode("012345");
  std::string expected_output = "012 345";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input_js_event_with_timeout");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input, /*delay_in_milli*/ 100)
                .proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SetAttribute) {
  Selector selector;
  std::vector<std::string> attribute;

  selector.selectors.emplace_back("#full_height_section");
  attribute.emplace_back("style");
  attribute.emplace_back("backgroundColor");
  std::string value = "red";

  EXPECT_EQ(ACTION_APPLIED,
            SetAttribute(selector, attribute, value).proto_status());
  const std::string javascript = R"(
    document.querySelector("#full_height_section").style.backgroundColor;
  )";
  EXPECT_EQ(value, content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ConcurrentGetFieldsValue) {
  std::vector<Selector> selectors;
  std::vector<std::string> expected_values;

  Selector a_selector;
  a_selector.selectors.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input2");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld2");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input3");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld3");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input4");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld4");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input5");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld5");

  GetFieldsValue(selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, NavigateToUrl) {
  EXPECT_EQ(kTargetWebsitePath,
            shell()->web_contents()->GetLastCommittedURL().path());
  web_controller_->LoadURL(GURL(url::kAboutBlankURL));
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(url::kAboutBlankURL,
            shell()->web_contents()->GetLastCommittedURL().spec());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, HighlightElement) {
  Selector selector;
  selector.selectors.emplace_back("#select");

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.style.boxShadow;
  )";
  EXPECT_EQ("", content::EvalJs(shell(), javascript));
  EXPECT_EQ(ACTION_APPLIED, HighlightElement(selector).proto_status());
  // We only make sure that the element has a non-empty boxShadow style without
  // requiring an exact string match.
  EXPECT_NE("", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, WaitForHeightChange) {
  base::RunLoop run_loop;
  ClientStatus result;
  web_controller_->WaitForWindowHeightChange(
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(), &result));

  EXPECT_TRUE(
      content::ExecJs(shell(), "window.dispatchEvent(new Event('resize'))"));
  run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, result.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitMainDocumentReadyStateInteractive) {
  ClientStatus status;
  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      Selector(), DOCUMENT_INTERACTIVE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_THAT(end_state, AnyOf(DOCUMENT_INTERACTIVE, DOCUMENT_COMPLETE));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitMainDocumentReadyStateComplete) {
  ClientStatus status;
  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      Selector(), DOCUMENT_COMPLETE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_EQ(DOCUMENT_COMPLETE, end_state);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitFrameDocumentReadyStateLoaded) {
  ClientStatus status;
  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      Selector({"#iframe"}), DOCUMENT_LOADED,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_THAT(end_state,
              AnyOf(DOCUMENT_LOADED, DOCUMENT_INTERACTIVE, DOCUMENT_COMPLETE));
}

}  // namespace autofill_assistant

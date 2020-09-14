// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
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

  void CheckElementVisibleCallback(base::OnceClosure done_callback,
                                   const Selector& selector,
                                   size_t* pending_number_of_checks_output,
                                   bool expected_result,
                                   const ClientStatus& result) {
    EXPECT_EQ(expected_result, result.ok())
        << "selector: " << selector << " status: " << result;
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  void ElementRetainingCallback(std::unique_ptr<ElementFinder::Result> element,
                                base::OnceClosure done_callback,
                                ClientStatus* result_output,
                                const ClientStatus& status) {
    EXPECT_TRUE(element != nullptr);
    *result_output = status;
    std::move(done_callback).Run();
  }

  void ClickOrTapElement(const Selector& selector, ClickType click_type) {
    base::RunLoop run_loop;
    ClientStatus result_output;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(&WebControllerBrowserTest::FindClickOrTapElementCallback,
                       base::Unretained(this), click_type,
                       run_loop.QuitClosure(), &result_output));

    run_loop.Run();
    EXPECT_EQ(ACTION_APPLIED, result_output.proto_status());
  }

  void FindClickOrTapElementCallback(
      ClickType click_type,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    ASSERT_TRUE(element_result != nullptr);
    PerformClickOrTap(
        click_type, *element_result,
        base::BindOnce(&WebControllerBrowserTest::ElementRetainingCallback,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output));
  }

  void PerformClickOrTap(
      ClickType click_type,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) {
    web_controller_->ScrollIntoView(
        element,
        base::BindOnce(&WebControllerBrowserTest::OnScrollIntoViewForClickOrTap,
                       base::Unretained(this), click_type, element,
                       std::move(callback)));
  }

  void OnScrollIntoViewForClickOrTap(
      ClickType click_type,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& scroll_status) {
    if (!scroll_status.ok()) {
      std::move(callback).Run(scroll_status);
      return;
    }

    web_controller_->ClickOrTapElement(element, click_type,
                                       std::move(callback));
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

  void OnWaitForElementRemove(base::OnceClosure done_callback,
                              const Selector& selector,
                              const ClientStatus& result) {
    std::move(done_callback).Run();
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
                            const std::string& value,
                            DropdownSelectStrategy select_strategy) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &WebControllerBrowserTest::FindSelectOptionElementCallback,
            base::Unretained(this), value, select_strategy,
            run_loop.QuitClosure(), &result));

    run_loop.Run();
    return result;
  }

  void FindSelectOptionElementCallback(
      const std::string& value,
      DropdownSelectStrategy select_strategy,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    if (!status.ok()) {
      *result_output = status;
      std::move(done_callback).Run();
      return;
    }

    ASSERT_TRUE(element_result != nullptr);
    web_controller_->SelectOption(
        *element_result, value, select_strategy,
        base::BindOnce(&WebControllerBrowserTest::ElementRetainingCallback,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output));
  }

  void OnClientStatus(base::OnceClosure done_callback,
                      ClientStatus* result_output,
                      const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  void OnClientStatusAndReadyState(base::OnceClosure done_callback,
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

  void OnGetOuterHtml(base::OnceClosure done_callback,
                      ClientStatus* successful_output,
                      std::string* html_output,
                      const ClientStatus& status,
                      const std::string& html) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    *successful_output = status;
    *html_output = html;
    std::move(done_callback).Run();
  }

  ClientStatus GetElementTag(const Selector& selector,
                             std::string* element_tag_output) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindElement(
        selector, /* strict= */ true,
        base::BindOnce(
            &WebControllerBrowserTest::FindGetElementTagElementCallback,
            base::Unretained(this), run_loop.QuitClosure(), &result,
            element_tag_output));

    run_loop.Run();
    return result;
  }

  void FindGetElementTagElementCallback(
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      std::string* element_tag_output,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());
    ASSERT_TRUE(element_result != nullptr);
    web_controller_->GetElementTag(
        *element_result,
        base::BindOnce(&WebControllerBrowserTest::OnGetElementTag,
                       base::Unretained(this), std::move(done_callback),
                       result_output, element_tag_output,
                       std::move(element_result)));
  }

  void OnGetElementTag(base::OnceClosure done_callback,
                       ClientStatus* successful_output,
                       std::string* element_tag_output,
                       std::unique_ptr<ElementFinder::Result> element,
                       const ClientStatus& status,
                       const std::string& element_tag) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    ASSERT_TRUE(element != nullptr);
    *successful_output = status;
    *element_tag_output = element_tag;
    std::move(done_callback).Run();
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

  void OnFindElement(base::OnceClosure done_callback,
                     ClientStatus* status_out,
                     ElementFinder::Result* result_out,
                     const ClientStatus& status,
                     std::unique_ptr<ElementFinder::Result> result) {
    ASSERT_TRUE(result);
    std::move(done_callback).Run();
    if (status_out)
      *status_out = status;

    if (result_out)
      *result_out = *result;
  }

  void FindElementAndCheck(const Selector& selector, bool is_main_frame) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    ElementFinder::Result result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    CheckFindElementResult(result, is_main_frame);
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
                              bool is_main_frame) {
    if (is_main_frame) {
      EXPECT_EQ(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
      EXPECT_EQ(result.frame_stack.size(), 0u);
    } else {
      EXPECT_NE(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
      EXPECT_GE(result.frame_stack.size(), 1u);
    }
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

  void OnGetFieldValue(base::OnceClosure done_callback,
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
                             KeyboardValueFillStrategy fill_strategy) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &WebControllerBrowserTest::FindSetFieldValueElementCallback,
            base::Unretained(this), value, fill_strategy,
            run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void FindSetFieldValueElementCallback(
      const std::string& value,
      KeyboardValueFillStrategy fill_strategy,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    if (!element_status.ok()) {
      *result_output = element_status;
      std::move(done_callback).Run();
      return;
    }

    EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());
    ASSERT_TRUE(element_result != nullptr);
    web_controller_->SetFieldValue(
        *element_result, value, fill_strategy,
        /* key_press_delay_in_millisecond= */ 0,
        base::BindOnce(&WebControllerBrowserTest::SetFieldValueCallback,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output));
  }

  void SetFieldValueCallback(std::unique_ptr<ElementFinder::Result> element,
                             base::OnceClosure done_callback,
                             ClientStatus* result_output,
                             const ClientStatus& status) {
    EXPECT_TRUE(element != nullptr);
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints,
                                 int delay_in_milli) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &WebControllerBrowserTest::FindSendKeyboardInputElementCallback,
            base::Unretained(this), codepoints, delay_in_milli,
            run_loop.QuitClosure(), &result));

    run_loop.Run();
    return result;
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints) {
    return SendKeyboardInput(selector, codepoints, -1);
  }

  void FindSendKeyboardInputElementCallback(
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());
    ASSERT_TRUE(element_result != nullptr);
    PerformSendKeyboardInput(
        codepoints, delay_in_milli, *element_result,
        base::BindOnce(&WebControllerBrowserTest::ElementRetainingCallback,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output));
  }

  void PerformSendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) {
    PerformClickOrTap(
        ClickType::CLICK, element,
        base::BindOnce(
            &WebControllerBrowserTest::OnClickOrTapForSendKeyboardInput,
            base::Unretained(this), codepoints, delay_in_milli, element,
            std::move(callback)));
  }

  void OnClickOrTapForSendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& click_status) {
    if (!click_status.ok()) {
      std::move(callback).Run(click_status);
      return;
    }

    web_controller_->SendKeyboardInput(element, codepoints, delay_in_milli,
                                       std::move(callback));
  }

  ClientStatus SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attributes,
                            const std::string& value) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &WebControllerBrowserTest::FindSetAttributeElementCallback,
            base::Unretained(this), attributes, value, run_loop.QuitClosure(),
            &result));

    run_loop.Run();
    return result;
  }

  void FindSetAttributeElementCallback(
      const std::vector<std::string>& attributes,
      const std::string& value,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result) {
    if (!status.ok()) {
      *result_output = status;
      std::move(done_callback).Run();
      return;
    }

    ASSERT_TRUE(element_result != nullptr);
    web_controller_->SetAttribute(
        *element_result, attributes, value,
        base::BindOnce(&WebControllerBrowserTest::ElementRetainingCallback,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output));
  }

  bool GetElementPosition(const Selector& selector, RectF* rect_output) {
    base::RunLoop run_loop;
    bool result;
    web_controller_->GetElementPosition(
        selector,
        base::BindOnce(&WebControllerBrowserTest::OnGetElementPosition,
                       base::Unretained(this), run_loop.QuitClosure(), &result,
                       rect_output));
    run_loop.Run();
    return result;
  }

  void OnGetElementPosition(base::OnceClosure done_callback,
                            bool* result_output,
                            RectF* rect_output,
                            bool non_empty,
                            const RectF& rect) {
    if (non_empty) {
      *rect_output = rect;
    }
    *result_output = non_empty;
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
    Selector selector({"#scroll_item_5"});

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

  // Send a Runtime.Evaluate protocol message. Useful for evaluating JS in the
  // page as there is no ordering guarantee between protocol messages and e.g.
  // ExecJs().
  void RuntimeEvaluate(const std::string& code) {
    web_controller_->devtools_client_->GetRuntime()->Evaluate(
        code,
        /* node_frame_id= */ std::string());
  }

 protected:
  std::unique_ptr<WebController> web_controller_;
  ClientSettings settings_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_iframe_;

  DISALLOW_COPY_AND_ASSIGN(WebControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementExistenceCheck) {
  // A visible element
  RunLaxElementCheck(Selector({"#button"}), true);

  // A hidden element.
  RunLaxElementCheck(Selector({"#hidden"}), true);

  // A nonexistent element.
  RunLaxElementCheck(Selector({"#doesnotexist"}), false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoElementChecks) {
  // A pseudo-element
  RunLaxElementCheck(Selector({"#terms-and-conditions"}).SetPseudoType(BEFORE),
                     true);

  // An invisible pseudo-element
  //
  // TODO(b/129461999): This is wrong; it should exist. Fix it.
  RunLaxElementCheck(Selector({"#button"}).SetPseudoType(BEFORE), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}).SetPseudoType(AFTER), false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementInFrameChecks) {
  // An iFrame.
  RunLaxElementCheck(Selector({"#iframe"}), true);

  // An element in a same-origin iFrame.
  RunLaxElementCheck(Selector({"#iframe", "#button"}), true);

  // An element in a same-origin iFrame.
  RunLaxElementCheck(Selector({"#iframe", "#doesnotexist"}), false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementInExternalFrameChecks) {
  // An OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal"}), true);

  // An element in an OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal", "#button"}), true);

  // An element in an OOPIF.
  RunLaxElementCheck(Selector({"#iframeExternal", "#doesnotexist"}), false);
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
      Selector({"#terms-and-conditions"}).MustBeVisible().SetPseudoType(BEFORE),
      true);

  // An invisible pseudo-element
  RunLaxElementCheck(
      Selector({"#button"}).MustBeVisible().SetPseudoType(BEFORE), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}).MustBeVisible().SetPseudoType(AFTER),
                     false);

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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SearchMultipleIframes) {
  // There are two "iframe" elements in the document so the selector would need
  // to search in both iframes, which isn't supported.
  SelectorProto proto;
  proto.add_filters()->set_css_selector("iframe");
  proto.add_filters()->mutable_enter_frame();
  proto.add_filters()->set_css_selector("#element_in_iframe_two");

  ClientStatus status;
  FindElement(Selector(proto), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, InnerTextCondition) {
  const Selector base_selector({"#with_inner_text span"});
  Selector selector = base_selector;
  selector.MustBeVisible();
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector.MustBeVisible(), false);

  // No matches
  selector = base_selector;
  selector.MatchingInnerText("no match");
  RunLaxElementCheck(selector, false);
  selector.MustBeVisible();
  RunLaxElementCheck(selector, false);

  // Matches exactly one visible element.
  selector = base_selector;
  selector.MatchingInnerText("hello, world");
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);
  selector.MustBeVisible();
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);

  // Matches case (in)sensitive.
  selector = base_selector;
  selector.MatchingInnerText("HELLO, WORLD", /* case_sensitive=*/false);
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);
  selector = base_selector;
  selector.MatchingInnerText("HELLO, WORLD", /* case_sensitive=*/true);
  RunLaxElementCheck(selector, false);
  RunStrictElementCheck(selector, false);

  // Matches two visible elements
  selector = base_selector;
  selector.MatchingInnerText("^hello");
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
  selector.MustBeVisible();
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);

  // Matches one visible, one invisible element
  selector = base_selector;
  selector.MatchingInnerText("world$");
  RunLaxElementCheck(selector, true);
  selector.MustBeVisible();
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeAndInnerText) {
  // Inner text conditions then pseudo type vs pseudo type then inner text
  // condition.
  Selector selector({"#with_inner_text span"});
  selector.MatchingInnerText("world");
  selector.SetPseudoType(PseudoType::BEFORE);
  RunLaxElementCheck(selector, true);

  // "before" is the content of the :before, checking the text of pseudo-types
  // doesn't work.
  selector = Selector({"#with_inner_text span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.MatchingInnerText("before");
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, MultipleBefore) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);

  // There's more than one "span" with a before, so only a lax check can
  // succeed.
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeThenBoundingBox) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.proto.add_filters()->mutable_bounding_box();

  RunLaxElementCheck(selector, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeThenPickOne) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.proto.add_filters()->mutable_pick_one();

  RunStrictElementCheck(selector, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeThenCss) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.proto.add_filters()->set_css_selector("div");

  // This makes no sense, but shouldn't return an unexpected error.
  ClientStatus status;
  ElementFinder::Result result;
  FindElement(selector, &status, &result);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeThenInnerText) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.proto.add_filters()->mutable_inner_text()->set_re2("before");

  // This isn't supported yet.
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeContent) {
  Selector selector({"#with_inner_text span"});
  auto* content =
      selector.proto.add_filters()->mutable_pseudo_element_content();
  content->set_pseudo_type(PseudoType::BEFORE);
  content->mutable_content()->set_re2("before");
  RunLaxElementCheck(selector, true);

  content->mutable_content()->set_re2("nomatch");
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, InnerTextThenCss) {
  // There are two divs containing "Section with text", but only one has a
  // button, which removes #button.
  SelectorProto proto;
  proto.add_filters()->set_css_selector("div");
  proto.add_filters()->mutable_inner_text()->set_re2("Section with text");
  proto.add_filters()->set_css_selector("button");

  ClickOrTapElement(Selector(proto), ClickType::CLICK);
  WaitForElementRemove(Selector({"#button"}));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindFormInputByLabel) {
  // #option1_label refers to the labelled control by id.
  Selector option1;
  option1.proto.add_filters()->set_css_selector("#option1_label");
  option1.proto.add_filters()->mutable_labelled();

  const std::string option1_checked = R"(
    document.querySelector("#option1").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), option1_checked).ExtractBool());
  ClickOrTapElement(option1, ClickType::CLICK);
  EXPECT_TRUE(content::EvalJs(shell(), option1_checked).ExtractBool());

  // #option2 contains the labelled control.
  Selector option2;
  option2.proto.add_filters()->set_css_selector("#option2_label");
  option2.proto.add_filters()->mutable_labelled();

  const std::string option2_checked = R"(
    document.querySelector("#option2").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), option2_checked).ExtractBool());
  ClickOrTapElement(option2, ClickType::CLICK);
  EXPECT_TRUE(content::EvalJs(shell(), option2_checked).ExtractBool());

  // #button is not a label.
  Selector not_a_label;
  not_a_label.proto.add_filters()->set_css_selector("#button");
  not_a_label.proto.add_filters()->mutable_labelled();

  // #bad_label1 and #bad_label2 are labels that don't reference a valid
  // element. They must not cause JavaScript errors.
  Selector bad_label1;
  bad_label1.proto.add_filters()->set_css_selector("#bad_label1");
  bad_label1.proto.add_filters()->mutable_labelled();

  ClientStatus status;
  FindElement(bad_label1, &status, nullptr);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());

  Selector bad_label2;
  bad_label2.proto.add_filters()->set_css_selector("#bad_label2");
  bad_label2.proto.add_filters()->mutable_labelled();

  FindElement(bad_label2, &status, nullptr);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ValueCondition) {
  // One match
  RunLaxElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"), true);
  RunStrictElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"),
                        true);

  // Case (in)sensitive match
  RunLaxElementCheck(Selector({"#input1"}).MatchingValue("HELLOWORLD1", false),
                     true);
  RunLaxElementCheck(Selector({"#input1"}).MatchingValue("HELLOWORLD1", true),
                     false);
  RunStrictElementCheck(
      Selector({"#input1"}).MatchingValue("HELLOWORLD1", false), true);
  RunStrictElementCheck(
      Selector({"#input1"}).MatchingValue("HELLOWORLD1", true), false);

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

  Selector visible_button({"#button"});
  visible_button.MustBeVisible();
  selectors.emplace_back(visible_button);
  results.emplace_back(true);

  Selector visible_with_iframe({"#button", "#watever"});
  visible_with_iframe.MustBeVisible();
  selectors.emplace_back(visible_with_iframe);
  results.emplace_back(false);

  // IFrame.
  selectors.emplace_back(Selector({"#iframe", "#button"}));
  results.emplace_back(true);

  selectors.emplace_back(Selector({"#iframe", "#button", "#whatever"}));
  results.emplace_back(false);

  selectors.emplace_back(Selector({"#iframe", "[name=name]"}));
  results.emplace_back(true);

  // OOPIF.
  selectors.emplace_back(Selector({"#iframeExternal", "#button"}));
  results.emplace_back(true);

  // Shadow DOM.
  selectors.emplace_back(
      Selector({"#iframe", "#shadowsection", "#shadowbutton"}));
  results.emplace_back(true);

  selectors.emplace_back(
      Selector({"#iframe", "#shadowsection", "#shadowbutton", "#whatever"}));
  results.emplace_back(false);

  // IFrame inside IFrame.
  selectors.emplace_back(Selector({"#iframe", "#iframe", "#button"}));
  results.emplace_back(true);

  selectors.emplace_back(
      Selector({"#iframe", "#iframe", "#button", "#whatever"}));
  results.emplace_back(false);

  // Hidden element.
  selectors.emplace_back(Selector({"#hidden"}).MustBeVisible());
  results.emplace_back(false);

  RunElementChecks(/* strict= */ false, selectors, results);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElement) {
  Selector selector({"#button"});
  ClickOrTapElement(selector, ClickType::CLICK);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElementInIFrame) {
  ClickOrTapElement(Selector({"#iframe", "#shadowsection", "#shadowbutton"}),
                    ClickType::CLICK);

  WaitForElementRemove(Selector({"#iframe", "#button"}));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElementInOOPIF) {
  ClickOrTapElement(Selector({"#iframeExternal", "#button"}), ClickType::CLICK);

  WaitForElementRemove(Selector({"#iframeExternal", "#div"}));
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

  Selector selector({"#scroll_item_3"});
  ClickOrTapElement(selector, ClickType::CLICK);

  EXPECT_TRUE(content::EvalJs(shell(), "scrollItem3WasClicked").ExtractBool());

  // TODO(b/135909926): Find a reliable way of verifying that the button was
  // mover roughly to the center.
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElement) {
  Selector area_two({"#touch_area_two"});
  ClickOrTapElement(area_two, ClickType::TAP);
  WaitForElementRemove(area_two);

  Selector area_one({"#touch_area_one"});
  ClickOrTapElement(area_one, ClickType::TAP);
  WaitForElementRemove(area_one);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_TapElementMovingOutOfView) {
  Selector selector({"#touch_area_three"});
  ClickOrTapElement(selector, ClickType::TAP);
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_TapElementAfterPageIsIdle) {
  // Set a very long timeout to make sure either the page is idle or the test
  // timeout.
  WaitTillPageIsIdle(base::TimeDelta::FromHours(1));

  Selector selector({"#touch_area_one"});
  ClickOrTapElement(selector, ClickType::TAP);

  WaitForElementRemove(selector);
}

// TODO(crbug.com/920948) Disabled for strong flakiness.
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, DISABLED_TapElementInIFrame) {
  Selector selector({"#iframe", "#touch_area"});
  ClickOrTapElement(selector, ClickType::TAP);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_TapRandomMovingElementRepeatedly) {
  Selector button_selector({"#random_moving_button"});
  int num_clicks = 100;
  for (int i = 0; i < num_clicks; ++i) {
    ClickOrTapElement(button_selector, ClickType::JAVASCRIPT);
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
    ClickOrTapElement(button_selector, ClickType::JAVASCRIPT);
  }

  std::vector<Selector> click_counter_selectors;
  std::vector<std::string> expected_values;
  expected_values.emplace_back(base::NumberToString(num_clicks));
  Selector click_counter_selector({"#moving_click_counter"});
  click_counter_selectors.emplace_back(click_counter_selector);
  GetFieldsValue(click_counter_selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapStaticElementRepeatedly) {
  Selector button_selector({"#static_button"});

  int num_clicks = 100;
  for (int i = 0; i < num_clicks; ++i) {
    ClickOrTapElement(button_selector, ClickType::JAVASCRIPT);
  }

  std::vector<Selector> click_counter_selectors;
  std::vector<std::string> expected_values;
  expected_values.emplace_back(base::NumberToString(num_clicks));
  Selector click_counter_selector({"#static_click_counter"});
  click_counter_selectors.emplace_back(click_counter_selector);
  GetFieldsValue(click_counter_selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickPseudoElement) {
  const std::string javascript = R"(
    document.querySelector("#terms-and-conditions").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), javascript).ExtractBool());
  Selector selector({R"(label[for="terms-and-conditions"])"});
  selector.SetPseudoType(PseudoType::BEFORE);
  ClickOrTapElement(selector, ClickType::CLICK);
  EXPECT_TRUE(content::EvalJs(shell(), javascript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElement) {
  Selector selector({"#button"});
  FindElementAndCheck(selector, true);
  selector.MustBeVisible();
  FindElementAndCheck(selector, true);

  // IFrame.
  selector = Selector({"#iframe", "#button"});
  FindElementAndCheck(selector, false);
  selector.MustBeVisible();
  FindElementAndCheck(selector, false);

  selector = Selector({"#iframe", "[name=name]"});
  FindElementAndCheck(selector, false);
  selector.MustBeVisible();
  FindElementAndCheck(selector, false);

  // IFrame inside IFrame.
  selector = Selector({"#iframe", "#iframe", "#button"});
  FindElementAndCheck(selector, false);
  selector.MustBeVisible();
  FindElementAndCheck(selector, false);

  // OutOfProcessIFrame.
  selector = Selector({"#iframeExternal", "#button"});
  FindElementAndCheck(selector, false);
  selector.MustBeVisible();
  FindElementAndCheck(selector, false);
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

  FindElement(Selector(SelectorProto::default_instance()), &status, nullptr);
  EXPECT_EQ(INVALID_SELECTOR, status.proto_status());

  FindElement(Selector({"#doesnotexist"}), &status, nullptr);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());

  FindElement(Selector({"div"}), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusElement) {
  Selector selector({"#iframe", "#focus"});

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
  Selector selector({"#scroll-me"});

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
  Selector selector({"#scroll-me"});

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
  Selector selector({"#select"});

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.options[select.selectedIndex].label;
  )";

  // Select value not matching anything.
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "incorrect label", LABEL_STARTS_WITH)
                .proto_status());

  // Selects nothing if no strategy is set.
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "one", UNSPECIFIED_SELECT_STRATEGY)
                .proto_status());

  // Select value matching the option's label.
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "ZÜRICH", LABEL_STARTS_WITH).proto_status());
  EXPECT_EQ("Zürich Hauptbahnhof", content::EvalJs(shell(), javascript));

  // Select value matching the option's value.
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "Aü万𠜎", VALUE_MATCH).proto_status());
  EXPECT_EQ("Character Test Entry", content::EvalJs(shell(), javascript));

  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED,
            SelectOption(Selector({"#incorrect_selector"}), "not important",
                         LABEL_STARTS_WITH)
                .proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIFrame) {
  // IFrame.
  Selector select_selector({"#iframe", "select[name=state]"});
  EXPECT_EQ(
      ACTION_APPLIED,
      SelectOption(select_selector, "NY", LABEL_STARTS_WITH).proto_status());

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));

  // OOPIF.
  // Checking elements through EvalJs in OOPIF is blocked by cross-site.
  select_selector = Selector({"#iframeExternal", "select[name=pet]"});
  EXPECT_EQ(
      ACTION_APPLIED,
      SelectOption(select_selector, "Cat", LABEL_STARTS_WITH).proto_status());

  Selector result_selector({"#iframeExternal", "#myPet"});
  GetFieldsValue({result_selector}, {"Cat"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtml) {
  std::string html;

  // Div.
  Selector div_selector({"#testOuterHtml"});
  ASSERT_EQ(ACTION_APPLIED, GetOuterHtml(div_selector, &html).proto_status());
  EXPECT_EQ(
      R"(<div id="testOuterHtml"><span>Span</span><p>Paragraph</p></div>)",
      html);

  // IFrame.
  Selector iframe_selector({"#iframe", "#input"});
  ASSERT_EQ(ACTION_APPLIED,
            GetOuterHtml(iframe_selector, &html).proto_status());
  EXPECT_EQ(R"(<input id="input" type="text">)", html);

  // OOPIF.
  Selector oopif_selector({"#iframeExternal", "#divToRemove"});
  ASSERT_EQ(ACTION_APPLIED, GetOuterHtml(oopif_selector, &html).proto_status());
  EXPECT_EQ(R"(<div id="divToRemove">Text</div>)", html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetElementTag) {
  std::string element_tag;

  // Div.
  ASSERT_EQ(
      ACTION_APPLIED,
      GetElementTag(Selector({"#testOuterHtml"}), &element_tag).proto_status());
  EXPECT_EQ("DIV", element_tag);

  // Select.
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(Selector({"#select"}), &element_tag).proto_status());
  EXPECT_EQ("SELECT", element_tag);

  // Input.
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(Selector({"#input1"}), &element_tag).proto_status());
  EXPECT_EQ("INPUT", element_tag);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValue) {
  std::vector<Selector> selectors;
  std::vector<std::string> expected_values;

  Selector a_selector({"body"});  //  Body has 'undefined' value
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input1"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld1");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "foo", SET_VALUE).proto_status());
  expected_values.clear();
  expected_values.emplace_back("foo");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#uppercase_input"});
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, /* Zürich */ "Z\xc3\xbcrich",
                          SIMULATE_KEY_PRESSES)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back(/* ZÜRICH */ "Z\xc3\x9cRICH");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input2"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld2");
  GetFieldsValue(selectors, expected_values);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, /* value= */ "", SIMULATE_KEY_PRESSES)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input3"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld3");
  GetFieldsValue(selectors, expected_values);
  EXPECT_EQ(ACTION_APPLIED, SetFieldValue(a_selector, /* value= */ "",
                                          SIMULATE_KEY_PRESSES_SELECT_VALUE)
                                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#invalid_selector"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED,
            SetFieldValue(a_selector, "foobar", SET_VALUE).proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValueInIFrame) {
  // IFrame.
  Selector a_selector({"#iframe", "#input"});
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "text", SET_VALUE).proto_status());
  GetFieldsValue({a_selector}, {"text"});

  // OOPIF.
  a_selector = Selector({"#iframeExternal", "#input"});
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "text", SET_VALUE).proto_status());
  GetFieldsValue({a_selector}, {"text"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendKeyboardInput) {
  auto input = UTF8ToUnicode("Zürich");
  std::string expected_output = "Zürich";

  std::vector<Selector> selectors;
  Selector a_selector({"#input6"});
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
  Selector a_selector({"#input_js_event_listener"});
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
  Selector a_selector({"#input_js_event_with_timeout"});
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input, /*delay_in_milli*/ 100)
                .proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SetAttribute) {
  std::vector<std::string> attribute;

  Selector selector({"#full_height_section"});
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

  Selector a_selector({"#input1"});
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");

  a_selector = Selector({"#input2"});
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld2");

  a_selector = Selector({"#input3"});
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld3");

  a_selector = Selector({"#input4"});
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld4");

  a_selector = Selector({"#input5"});
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld5");

  GetFieldsValue(selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, NavigateToUrl) {
  EXPECT_EQ(kTargetWebsitePath,
            shell()->web_contents()->GetLastCommittedURL().path());
  web_controller_->LoadURL(GURL(url::kAboutBlankURL));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url::kAboutBlankURL,
            shell()->web_contents()->GetLastCommittedURL().spec());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, HighlightElement) {
  Selector selector({"#select"});

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

  RuntimeEvaluate("window.dispatchEvent(new Event('resize'))");
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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetElementPosition) {
  RectF document_element_rect;
  Selector document_element({"#full_height_section"});
  EXPECT_TRUE(GetElementPosition(document_element, &document_element_rect));

  // The iFrame must be after the #full_height_section element to check that
  // the resulting rect is global.
  RectF iframe_element_rect;
  Selector iframe_element({"#iframe", "#touch_area_1"});
  EXPECT_TRUE(GetElementPosition(iframe_element, &iframe_element_rect));

  EXPECT_GT(iframe_element_rect.top, document_element_rect.bottom);

  // Make sure the element is within the iframe.
  RectF iframe_rect;
  Selector iframe({"#iframe"});
  EXPECT_TRUE(GetElementPosition(iframe, &iframe_rect));

  EXPECT_GT(iframe_element_rect.left, iframe_rect.left);
  EXPECT_LT(iframe_element_rect.right, iframe_rect.right);
  EXPECT_GT(iframe_element_rect.top, iframe_rect.top);
  EXPECT_LT(iframe_element_rect.bottom, iframe_rect.bottom);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetElementByProximity) {
  Selector input1_selector({"input"});
  auto* input1_closest = input1_selector.proto.add_filters()->mutable_closest();
  input1_closest->add_target()->set_css_selector("label");
  input1_closest->add_target()->mutable_inner_text()->set_re2("Input1");

  GetFieldsValue({input1_selector}, {"helloworld1"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       GetElementByProximityWithTooManyCandidates) {
  Selector selector({"input.pairs"});
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("label.pairs");
  closest->set_max_pairs(24);

  ClientStatus status;
  ElementFinder::Result result;
  FindElement(selector, &status, &result);
  EXPECT_EQ(TOO_MANY_CANDIDATES, status.proto_status());

  closest->set_max_pairs(25);
  FindElement(selector, &status, &result);
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ProximityRelative_Position) {
  Selector selector({"#at_center"});
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("table.proximity td");
  auto* inner_text = closest->add_target()->mutable_inner_text();

  // The cells of the table look like the following:
  //
  // One    Two     Three
  // Four   Center  Five
  // Six    Seven   Eight
  //
  // The element is "Center", the target is "One" to "Eight". The
  // relative_position specify that the element should be below|above|... the
  // target.

  closest->set_relative_position(SelectorProto::ProximityFilter::BELOW);
  inner_text->set_re2("One");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, false);

  closest->set_relative_position(SelectorProto::ProximityFilter::ABOVE);
  inner_text->set_re2("One");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, false);

  closest->set_relative_position(SelectorProto::ProximityFilter::LEFT);
  inner_text->set_re2("One");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, false);

  closest->set_relative_position(SelectorProto::ProximityFilter::RIGHT);
  inner_text->set_re2("One");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ProximityAlignment) {
  Selector selector({"#at_center"});
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("table.proximity td");
  auto* inner_text = closest->add_target()->mutable_inner_text();

  closest->set_in_alignment(true);
  inner_text->set_re2("One");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ProximityAlignmentWithPosition) {
  Selector selector({"#at_center"});
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("table.proximity td");
  auto* inner_text = closest->add_target()->mutable_inner_text();

  closest->set_in_alignment(true);
  closest->set_relative_position(SelectorProto::ProximityFilter::LEFT);

  inner_text->set_re2("One");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Two");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Three");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Four");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Five");
  RunStrictElementCheck(selector, true);
  inner_text->set_re2("Six");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Seven");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Eight");
  RunStrictElementCheck(selector, false);
  inner_text->set_re2("Center");
  RunStrictElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       FindPseudoElementToClickByProximity) {
  const std::string javascript = R"(
    document.querySelector("#terms-and-conditions").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), javascript).ExtractBool());

  // This test clicks on the before pseudo-element that's closest to
  // #terms-and-conditions - this has the same effect as clicking on
  // #terms-and-conditions. This checks that pseudo-elements have positions and
  // that we can go through an array of pseudo-elements and choose the closest
  // one.
  Selector selector({"label, span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("#terms-and-conditions");

  ClickOrTapElement(selector, ClickType::CLICK);
  EXPECT_TRUE(content::EvalJs(shell(), javascript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       GetElementByProximityDifferentFrames) {
  Selector selector({"input"});
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("#iframe");
  closest->add_target()->mutable_pick_one();
  closest->add_target()->mutable_enter_frame();
  closest->add_target()->set_css_selector("div");

  // Cannot compare position of elements on different frames.
  ClientStatus status;
  FindElement(Selector(SelectorProto::default_instance()), &status, nullptr);
  EXPECT_EQ(INVALID_SELECTOR, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       GetElementByProximitySameFrame) {
  Selector selector({"#iframe", "input[name='email']"});

  // The target is searched within #iframe.
  auto* closest = selector.proto.add_filters()->mutable_closest();
  closest->add_target()->set_css_selector("span");
  closest->add_target()->mutable_inner_text()->set_re2("Email");

  RunLaxElementCheck(selector, true);
  GetFieldsValue({selector}, {"email@example.com"});
}

}  // namespace autofill_assistant

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill_assistant {

const char* kTargetWebsitePath = "/autofill_assistant_target_website.html";

class WebControllerBrowserTest : public content::ContentBrowserTest {
 public:
  WebControllerBrowserTest() {}
  ~WebControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant");
    ASSERT_TRUE(http_server_->Start());
    ASSERT_TRUE(
        NavigateToURL(shell(), http_server_->GetURL(kTargetWebsitePath)));
    web_controller_ =
        WebController::CreateForWebContents(shell()->web_contents());
  }

  void RunElementChecks(ElementCheckType check_type,
                        const std::vector<std::vector<std::string>>& selectors,
                        const std::vector<bool> results) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), results.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->ElementCheck(
          check_type, selectors[i],
          base::BindOnce(&WebControllerBrowserTest::CheckElementVisibleCallback,
                         base::Unretained(this), run_loop.QuitClosure(),
                         &pending_number_of_checks, results[i]));
    }
    run_loop.Run();
  }

  void CheckElementVisibleCallback(const base::Closure& done_callback,
                                   size_t* pending_number_of_checks_output,
                                   bool expected_result,
                                   bool result) {
    ASSERT_EQ(expected_result, result);
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      done_callback.Run();
    }
  }

  void ClickElement(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->ClickElement(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::ClickElementCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void ClickElementCallback(const base::Closure& done_callback,
                            bool result) {
    ASSERT_TRUE(result);
    done_callback.Run();
  }

  void WaitForElementRemove(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->ElementCheck(
        kExistenceCheck, selectors,
        base::BindOnce(&WebControllerBrowserTest::OnWaitForElementRemove,
                       base::Unretained(this), run_loop.QuitClosure(),
                       selectors));
    run_loop.Run();
  }

  void OnWaitForElementRemove(const base::Closure& done_callback,
                              const std::vector<std::string>& selectors,
                              bool result) {
    done_callback.Run();
    if (result) {
      WaitForElementRemove(selectors);
    }
  }

  void FocusElement(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->FocusElement(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::OnFocusElement,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnFocusElement(base::Closure done_callback, bool result) {
    ASSERT_TRUE(result);
    std::move(done_callback).Run();
  }

  bool SelectOption(const std::vector<std::string>& selectors,
                    const std::string& option) {
    base::RunLoop run_loop;
    bool result;
    web_controller_->SelectOption(
        selectors, option,
        base::BindOnce(&WebControllerBrowserTest::OnSelectOption,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSelectOption(base::Closure done_callback,
                      bool* result_output,
                      bool result) {
    *result_output = result;
    std::move(done_callback).Run();
  }

  bool HighlightElement(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    bool result;
    web_controller_->HighlightElement(
        selectors, base::BindOnce(&WebControllerBrowserTest::OnHighlightElement,
                                  base::Unretained(this),
                                  run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void OnHighlightElement(base::Closure done_callback,
                          bool* result_output,
                          bool result) {
    *result_output = result;
    std::move(done_callback).Run();
  }

  bool GetOuterHtml(const std::vector<std::string>& selectors,
                    std::string* html_output) {
    base::RunLoop run_loop;
    bool result;
    web_controller_->GetOuterHtml(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::OnGetOuterHtml,
                       base::Unretained(this), run_loop.QuitClosure(), &result,
                       html_output));
    run_loop.Run();
    return result;
  }

  void OnGetOuterHtml(const base::Closure& done_callback,
                      bool* successful_output,
                      std::string* html_output,
                      bool successful,
                      const std::string& html) {
    *successful_output = successful;
    *html_output = html;
    done_callback.Run();
  }

  void FindElement(const std::vector<std::string>& selectors,
                   size_t expected_index,
                   bool is_main_frame) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selectors,
        /* strict_mode= */ true,
        base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                       base::Unretained(this), run_loop.QuitClosure(),
                       expected_index, is_main_frame));
    run_loop.Run();
  }

  void OnFindElement(const base::Closure& done_callback,
                     size_t expected_index,
                     bool is_main_frame,
                     std::unique_ptr<WebController::FindElementResult> result) {
    done_callback.Run();
    if (is_main_frame) {
      ASSERT_EQ(shell()->web_contents()->GetMainFrame(),
                result->container_frame_host);
    } else {
      ASSERT_NE(shell()->web_contents()->GetMainFrame(),
                result->container_frame_host);
    }
    ASSERT_EQ(result->container_frame_selector_index, expected_index);
    ASSERT_FALSE(result->object_id.empty());
  }

  void GetFieldsValue(const std::vector<std::vector<std::string>>& selectors,
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
                       bool exists,
                       const std::string& value) {
    ASSERT_EQ(expected_value, value);
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  bool SetFieldValue(const std::vector<std::string>& selectors,
                     const std::string& value,
                     bool simulate_key_presses) {
    base::RunLoop run_loop;
    bool result;
    web_controller_->SetFieldValue(
        selectors, value, simulate_key_presses,
        base::BindOnce(&WebControllerBrowserTest::OnSetFieldValue,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSetFieldValue(const base::Closure& done_callback,
                       bool* result_output,
                       bool result) {
    *result_output = result;
    std::move(done_callback).Run();
  }

 protected:
  std::unique_ptr<WebController> web_controller_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;

  DISALLOW_COPY_AND_ASSIGN(WebControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ConcurrentElementsVisible) {
  std::vector<std::vector<std::string>> selectors;
  std::vector<bool> results;

  std::vector<std::string> a_selector;
  a_selector.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame.
  a_selector.clear();
  a_selector.emplace_back("#iframe");
  a_selector.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  a_selector.clear();
  a_selector.emplace_back("#iframe");
  a_selector.emplace_back("[name=name]");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // Shadow DOM.
  a_selector.clear();
  a_selector.emplace_back("#iframe");
  a_selector.emplace_back("#shadowsection");
  a_selector.emplace_back("#shadowbutton");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame inside IFrame.
  a_selector.clear();
  a_selector.emplace_back("#iframe");
  a_selector.emplace_back("#iframe");
  a_selector.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // Hidden element.
  a_selector.clear();
  a_selector.emplace_back("#hidden");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  RunElementChecks(kVisibilityCheck, selectors, results);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementExists) {
  std::vector<std::vector<std::string>> selectors;
  std::vector<bool> results;

  std::vector<std::string> a_selector;

  // A visible element
  a_selector.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // A hidden element.
  a_selector.clear();
  a_selector.emplace_back("#hidden");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // A nonexistent element.
  a_selector.clear();
  a_selector.emplace_back("#doesnotexist");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  RunElementChecks(kExistenceCheck, selectors, results);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#button");
  ClickElement(selectors);

  WaitForElementRemove(selectors);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ClickElementInIFrame) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#shadowsection");
  selectors.emplace_back("#shadowbutton");
  ClickElement(selectors);

  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  WaitForElementRemove(selectors);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#button");
  FindElement(selectors, 0, true);

  // IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  FindElement(selectors, 0, false);

  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("[name=name]");
  FindElement(selectors, 0, false);

  // IFrame inside IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  FindElement(selectors, 1, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#focus");

  // The element is not visible initially.
  const std::string checkNotVisibleScript = R"(
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      iframeRect.y + divRect.y > window.innerHeight;
  )";
  EXPECT_EQ(true, content::EvalJs(shell(), checkNotVisibleScript));
  FocusElement(selectors);

  // Verify that the scroll moved the div in the iframe into view.
  const std::string checkVisibleScript = R"(
    const scrollTimeoutMs = 500;
    var timer = null;

    function check() {
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      return iframeRect.y + divRect.y < window.innerHeight;
    }
    function onScrollDone() {
      window.removeEventListener("scroll", onScroll);
      domAutomationController.send(check());
    }
    function onScroll(e) {
      if (timer != null) {
        clearTimeout(timer);
      }
      timer = setTimeout(onScrollDone, scrollTimeoutMs);
    }
    if (check()) {
      // Scrolling finished before this script started. Just return the result.
      domAutomationController.send(true);
    } else {
      window.addEventListener("scroll", onScroll);
    }
  )";
  EXPECT_EQ(true, content::EvalJsWithManualReply(shell(), checkVisibleScript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOption) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#select");
  EXPECT_FALSE(SelectOption(selectors, "incorrect_label"));
  ASSERT_TRUE(SelectOption(selectors, "two"));

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("Two", content::EvalJs(shell(), javascript));

  ASSERT_TRUE(SelectOption(selectors, "one"));
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  selectors.clear();
  selectors.emplace_back("#incorrect_selector");
  EXPECT_FALSE(SelectOption(selectors, "two"));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIframe) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("select[name=state]");
  ASSERT_TRUE(SelectOption(selectors, "NY"));

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtml) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#testOuterHtml");
  std::string html;
  ASSERT_TRUE(GetOuterHtml(selectors, &html));
  EXPECT_EQ(
      R"(<div id="testOuterHtml"><span>Span</span><p>Paragraph</p></div>)",
      html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValue) {
  std::vector<std::vector<std::string>> selectors;
  std::vector<std::string> expected_values;

  std::vector<std::string> a_selector;
  a_selector.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");
  GetFieldsValue(selectors, expected_values);

  EXPECT_TRUE(
      SetFieldValue(a_selector, "foo", /* simulate_key_presses= */ false));
  expected_values.clear();
  expected_values.emplace_back("foo");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.clear();
  a_selector.emplace_back("#uppercase_input");
  selectors.emplace_back(a_selector);
  EXPECT_TRUE(
      SetFieldValue(a_selector, "baz", /* simulate_key_presses= */ true));
  expected_values.clear();
  expected_values.emplace_back("BAZ");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.clear();
  a_selector.emplace_back("#invalid_selector");
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  EXPECT_FALSE(
      SetFieldValue(a_selector, "foobar", /* simulate_key_presses= */ false));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ConcurrentGetFieldsValue) {
  std::vector<std::vector<std::string>> selectors;
  std::vector<std::string> expected_values;

  std::vector<std::string> a_selector;
  a_selector.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");

  a_selector.clear();
  a_selector.emplace_back("#input2");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld2");

  a_selector.clear();
  a_selector.emplace_back("#input3");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld3");

  a_selector.clear();
  a_selector.emplace_back("#input4");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld4");

  a_selector.clear();
  a_selector.emplace_back("#input5");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld5");

  GetFieldsValue(selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, NavigateToUrl) {
  EXPECT_EQ(kTargetWebsitePath, web_controller_->GetUrl().path());
  web_controller_->LoadURL(GURL(url::kAboutBlankURL));
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(url::kAboutBlankURL, web_controller_->GetUrl().spec());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, HighlightElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#select");

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.style.boxShadow;
  )";
  EXPECT_EQ("", content::EvalJs(shell(), javascript));
  ASSERT_TRUE(HighlightElement(selectors));
  // We only make sure that the element has a non-empty boxShadow style without
  // requiring an exact string match.
  EXPECT_NE("", content::EvalJs(shell(), javascript));
}

}  // namespace

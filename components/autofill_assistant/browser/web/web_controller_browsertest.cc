// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include <stddef.h>
#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/checked_range.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/action_strategy.pb.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/mock_script_executor_delegate.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_finder_result_type.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_autofill_assistant_agent.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::WithArgs;

}  // namespace

class WebControllerBrowserTest : public autofill_assistant::BaseBrowserTest,
                                 public content::WebContentsObserver {
 public:
  WebControllerBrowserTest() {}

  WebControllerBrowserTest(const WebControllerBrowserTest&) = delete;
  WebControllerBrowserTest& operator=(const WebControllerBrowserTest&) = delete;

  ~WebControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    MockAutofillAssistantAgent::RegisterForAllFrames(
        shell()->web_contents(), &autofill_assistant_agent_);

    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &user_data_, &log_info_,
        /* annotate_dom_model_service= */ nullptr,
        /*enable_full_stack_traces= */ true);

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
            FROM_HERE, heart_beat.QuitClosure(), base::Seconds(3));
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
      web_controller_->FindElement(
          selectors[i], strict,
          base::BindOnce(&WebControllerBrowserTest::ElementCheckCallback,
                         base::Unretained(this), run_loop.QuitClosure(),
                         selectors[i], &pending_number_of_checks, results[i]));
    }
    run_loop.Run();
  }

  void ElementCheckCallback(
      base::OnceClosure done_callback,
      const Selector& selector,
      size_t* pending_number_of_checks_output,
      bool expected_result,
      const ClientStatus& result,
      std::unique_ptr<ElementFinderResult> ignored_element) {
    EXPECT_EQ(expected_result, result.ok())
        << "selector: " << selector << " status: " << result;
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  void WaitForElementRemove(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selector, /* strict= */ false,
        base::BindOnce(&WebControllerBrowserTest::OnWaitForElementRemove,
                       base::Unretained(this), run_loop.QuitClosure(),
                       selector));
    run_loop.Run();
  }

  void OnWaitForElementRemove(
      base::OnceClosure done_callback,
      const Selector& selector,
      const ClientStatus& result,
      std::unique_ptr<ElementFinderResult> ignored_element) {
    std::move(done_callback).Run();
    if (result.ok()) {
      WaitForElementRemove(selector);
    }
  }

  void OnClientStatus(base::OnceClosure done_callback,
                      ClientStatus* status_output,
                      const ClientStatus& status) {
    *status_output = status;
    std::move(done_callback).Run();
  }

  void OnScriptFinished(base::OnceClosure done_callback,
                        const ScriptExecutor::Result& result) {
    std::move(done_callback).Run();
  }

  void OnClientStatusAndReadyState(base::OnceClosure done_callback,
                                   ClientStatus* result_output,
                                   DocumentReadyState* ready_state_out,
                                   const ClientStatus& status,
                                   DocumentReadyState ready_state,
                                   base::TimeDelta) {
    *result_output = status;
    *ready_state_out = ready_state;
    std::move(done_callback).Run();
  }

  ClientStatus FindElementAndPerformAll(
      const Selector& selector,
      std::unique_ptr<element_action_util::ElementActionVector>
          perform_actions) {
    base::RunLoop run_loop;
    ClientStatus status;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(&element_action_util::TakeElementAndPerform,
                       base::BindOnce(&element_action_util::PerformAll,
                                      std::move(perform_actions)),
                       base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                                      base::Unretained(this),
                                      run_loop.QuitClosure(), &status)));

    run_loop.Run();
    return status;
  }

  ClientStatus FindElementAndGetString(
      const Selector& selector,
      element_action_util::ElementActionGetCallback<const std::string&>
          perform_and_get,
      std::string* get_output) {
    base::RunLoop run_loop;
    ClientStatus status;

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &element_action_util::TakeElementAndGetProperty<const std::string&>,
            std::move(perform_and_get), std::string(),
            base::BindOnce(&WebControllerBrowserTest::OnPerformAndGetString,
                           base::Unretained(this), run_loop.QuitClosure(),
                           &status, get_output)));

    run_loop.Run();
    return status;
  }

  void OnPerformAndGetString(base::OnceClosure done_callback,
                             ClientStatus* status_output,
                             std::string* get_output,
                             const ClientStatus& status,
                             const std::string& get) {
    *status_output = status;
    *get_output = get;
    std::move(done_callback).Run();
  }

  void ClickOrTapElement(const Selector& selector, ClickType click_type) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    if (click_type == ClickType::JAVASCRIPT) {
      actions->emplace_back(base::BindOnce(&WebController::JsClickElement,
                                           web_controller_->GetWeakPtr()));
    } else {
      actions->emplace_back(base::BindOnce(
          &WebController::ScrollIntoView, web_controller_->GetWeakPtr(),
          /* animation= */ std::string(), /* vertical_alignment= */ "center",
          /* horizontal_alignment= */ "center"));
      actions->emplace_back(base::BindOnce(&WebController::ClickOrTapElement,
                                           web_controller_->GetWeakPtr(),
                                           click_type));
    }

    const ClientStatus& status =
        FindElementAndPerformAll(selector, std::move(actions));
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
  }

  void ScrollToElementPosition(
      const Selector& selector,
      const TopPadding& top_padding,
      const Selector& container_selector = Selector()) {
    base::RunLoop run_loop;
    ClientStatus result;

    if (container_selector.empty()) {
      web_controller_->FindElement(
          selector, /* strict_mode= */ true,
          base::BindOnce(
              &element_action_util::TakeElementAndPerform,
              base::BindOnce(&WebController::ScrollToElementPosition,
                             web_controller_->GetWeakPtr(),
                             /* container= */ nullptr, top_padding),
              base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                             base::Unretained(this), run_loop.QuitClosure(),
                             &result)));
    } else {
      web_controller_->FindElement(
          container_selector, /* strict_mode= */ true,
          base::BindOnce(&WebControllerBrowserTest::FindContainerCallback,
                         base::Unretained(this), selector, top_padding,
                         run_loop.QuitClosure(), &result));
    }

    run_loop.Run();
    EXPECT_EQ(ACTION_APPLIED, result.proto_status());
  }

  void FindContainerCallback(
      const Selector& selector,
      const TopPadding& top_padding,
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      const ClientStatus& status,
      std::unique_ptr<ElementFinderResult> container_result) {
    if (!status.ok()) {
      *result_output = status;
      std::move(done_callback).Run();
      return;
    }

    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(
            &element_action_util::TakeElementAndPerform,
            base::BindOnce(&WebController::ScrollToElementPosition,
                           web_controller_->GetWeakPtr(),
                           std::move(container_result), top_padding),
            base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                           base::Unretained(this), std::move(done_callback),
                           result_output)));
  }

  ClientStatus SelectOption(
      const Selector& selector,
      const std::string& re2,
      bool case_sensitive,
      SelectOptionProto::OptionComparisonAttribute option_comparison_attribute,
      bool strict) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    actions->emplace_back(base::BindOnce(
        &WebController::SelectOption, web_controller_->GetWeakPtr(), re2,
        case_sensitive, option_comparison_attribute, strict));
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus SelectOptionElement(const Selector& selector,
                                   const ElementFinderResult& option) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    actions->emplace_back(base::BindOnce(&WebController::SelectOptionElement,
                                         web_controller_->GetWeakPtr(),
                                         option));
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus CheckSelectedOptionElement(const ElementFinderResult& select,
                                          const ElementFinderResult& option) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->CheckSelectedOptionElement(
        option, select,
        base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));

    run_loop.Run();
    return result;
  }

  ClientStatus GetOuterHtml(const Selector& selector,
                            bool include_all_inner_text,
                            std::string* html_output) {
    const ClientStatus& result = FindElementAndGetString(
        selector,
        base::BindOnce(&WebController::GetOuterHtml,
                       web_controller_->GetWeakPtr(), include_all_inner_text),
        html_output);
    EXPECT_EQ(ACTION_APPLIED, result.proto_status());
    return result;
  }

  ClientStatus GetOuterHtmls(const Selector& selector,
                             bool include_all_inner_text,
                             std::vector<std::string>* htmls_output) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindAllElements(
        selector,
        base::BindOnce(
            &WebControllerBrowserTest::OnFindAllElementsForGetOuterHtmls,
            base::Unretained(this), run_loop.QuitClosure(), &result,
            htmls_output, include_all_inner_text));

    run_loop.Run();
    return result;
  }

  void OnFindAllElementsForGetOuterHtmls(
      base::OnceClosure done_callback,
      ClientStatus* client_status_output,
      std::vector<std::string>* htmls_output,
      bool include_all_inner_text,
      const ClientStatus& client_status,
      std::unique_ptr<ElementFinderResult> elements) {
    EXPECT_EQ(ACTION_APPLIED, client_status.proto_status());
    ASSERT_TRUE(elements);

    const ElementFinderResult* elements_ptr = elements.get();
    web_controller_->GetOuterHtmls(
        include_all_inner_text, *elements_ptr,
        base::BindOnce(&WebControllerBrowserTest::OnGetOuterHtmls,
                       base::Unretained(this), std::move(elements),
                       std::move(done_callback), client_status_output,
                       htmls_output));
  }

  void OnGetOuterHtmls(std::unique_ptr<ElementFinderResult> elements,
                       base::OnceClosure done_callback,
                       ClientStatus* client_status_output,
                       std::vector<std::string>* htmls_output,
                       const ClientStatus& client_status,
                       const std::vector<std::string>& htmls) {
    *client_status_output = client_status;
    *htmls_output = htmls;
    std::move(done_callback).Run();
  }

  ClientStatus GetElementTag(const Selector& selector,
                             std::string* element_tag_output) {
    const ClientStatus& result =
        FindElementAndGetString(selector,
                                base::BindOnce(&WebController::GetElementTag,
                                               web_controller_->GetWeakPtr()),
                                element_tag_output);
    EXPECT_EQ(ACTION_APPLIED, result.proto_status());
    return result;
  }

  ClientStatus SendChangeEvent(const Selector& selector) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    actions->emplace_back(base::BindOnce(&WebController::SendChangeEvent,
                                         web_controller_->GetWeakPtr()));
    const ClientStatus& result =
        FindElementAndPerformAll(selector, std::move(actions));
    EXPECT_EQ(ACTION_APPLIED, result.proto_status());
    return result;
  }

  ClientStatus GetStringAttribute(const Selector& selector,
                                  const std::vector<std::string>& attributes,
                                  std::string* value) {
    return FindElementAndGetString(
        selector,
        base::BindOnce(&WebController::GetStringAttribute,
                       web_controller_->GetWeakPtr(), attributes),
        value);
  }

  ClientStatus CheckOnTop(const ElementFinderResult& element) {
    ClientStatus captured_status;
    base::RunLoop run_loop;
    web_controller_->CheckOnTop(
        element, base::BindLambdaForTesting(
                     [&captured_status, &run_loop](const ClientStatus& status) {
                       captured_status = status;
                       run_loop.Quit();
                     }));
    run_loop.Run();
    return captured_status;
  }

  ClientStatus WaitUntilElementIsStable(const ElementFinderResult& element,
                                        int max_rounds,
                                        base::TimeDelta check_interval) {
    ClientStatus captured_status;
    base::RunLoop run_loop;
    web_controller_->WaitUntilElementIsStable(
        max_rounds, check_interval, element,
        base::BindLambdaForTesting(
            [&captured_status, &run_loop](const ClientStatus& status,
                                          base::TimeDelta) {
              captured_status = status;
              run_loop.Quit();
            }));
    run_loop.Run();
    return captured_status;
  }

  void FindElement(const Selector& selector,
                   ClientStatus* status_out,
                   ElementFinderResult* result_out) {
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
                     ElementFinderResult* result_out,
                     const ClientStatus& status,
                     std::unique_ptr<ElementFinderResult> result) {
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
    ElementFinderResult result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    CheckFindElementResult(result, is_main_frame);
  }

  void FindElementExpectEmptyResult(const Selector& selector) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    ElementFinderResult result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
    EXPECT_THAT(result.object_id(), IsEmpty());
  }

  void CheckFindElementResult(const ElementFinderResult& result,
                              bool is_main_frame) {
    if (is_main_frame) {
      EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame(),
                result.render_frame_host());
      EXPECT_EQ(result.frame_stack().size(), 0u);
    } else {
      EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame(),
                result.render_frame_host());
      EXPECT_GE(result.frame_stack().size(), 1u);
    }
    EXPECT_FALSE(result.object_id().empty());
  }

  void GetFieldsValue(const std::vector<Selector>& selectors,
                      const std::vector<std::string>& expected_values) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), expected_values.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->FindElement(
          selectors[i], /* strict= */ true,
          base::BindOnce(
              &WebControllerBrowserTest::GetFieldValueElementCallback,
              base::Unretained(this), run_loop.QuitClosure(),
              &pending_number_of_checks, expected_values[i]));
    }
    run_loop.Run();
  }

  void GetFieldValueElementCallback(
      base::OnceClosure done_callback,
      size_t* pending_number_of_checks_output,
      const std::string& expected_value,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinderResult> element_result) {
    if (!element_status.ok()) {
      OnGetFieldValue(nullptr, std::move(done_callback),
                      pending_number_of_checks_output, expected_value,
                      element_status, std::string());
      return;
    }
    const ElementFinderResult* element_result_ptr = element_result.get();
    web_controller_->GetFieldValue(
        *element_result_ptr,
        base::BindOnce(&WebControllerBrowserTest::OnGetFieldValue,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback),
                       pending_number_of_checks_output, expected_value));
  }

  void OnGetFieldValue(std::unique_ptr<ElementFinderResult> element,
                       base::OnceClosure done_callback,
                       size_t* pending_number_of_checks_output,
                       const std::string& expected_value,
                       const ClientStatus& status,
                       const std::string& value) {
    // Don't use ASSERT: If the check fails, this would result in an endless
    // loop without meaningful test results.
    EXPECT_EQ(expected_value, value);
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  ClientStatus SetFieldValue(const Selector& selector,
                             const std::string& value,
                             KeyboardValueFillStrategy fill_strategy) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    if (value.empty()) {
      actions->emplace_back(base::BindOnce(&WebController::SetValueAttribute,
                                           web_controller_->GetWeakPtr(),
                                           std::string()));
    } else {
      switch (fill_strategy) {
        case SET_VALUE:
          actions->emplace_back(
              base::BindOnce(&WebController::SetValueAttribute,
                             web_controller_->GetWeakPtr(), value));
          break;
        case SIMULATE_KEY_PRESSES:
          actions->emplace_back(
              base::BindOnce(&WebController::SetValueAttribute,
                             web_controller_->GetWeakPtr(), std::string()));
          actions->emplace_back(base::BindOnce(
              &WebController::ScrollIntoView, web_controller_->GetWeakPtr(),
              /* animation= */ std::string(),
              /* vertical_alignment= */ "center",
              /* horizontal_alignment= */ "center"));
          actions->emplace_back(
              base::BindOnce(&WebController::ClickOrTapElement,
                             web_controller_->GetWeakPtr(), ClickType::CLICK));
          actions->emplace_back(base::BindOnce(
              &WebController::SendKeyboardInput, web_controller_->GetWeakPtr(),
              UTF8ToUnicode(value), /* delay_in_milli= */ 0));
          break;
        case SIMULATE_KEY_PRESSES_SELECT_VALUE:
          actions->emplace_back(base::BindOnce(&WebController::SelectFieldValue,
                                               web_controller_->GetWeakPtr()));
          actions->emplace_back(base::BindOnce(
              &WebController::SendKeyboardInput, web_controller_->GetWeakPtr(),
              UTF8ToUnicode(value), /* delay_in_milli= */ 0));
          break;
        case SIMULATE_KEY_PRESSES_FOCUS:
          actions->emplace_back(
              base::BindOnce(&WebController::SetValueAttribute,
                             web_controller_->GetWeakPtr(), std::string()));
          actions->emplace_back(base::BindOnce(&WebController::FocusField,
                                               web_controller_->GetWeakPtr()));
          actions->emplace_back(base::BindOnce(
              &WebController::SendKeyboardInput, web_controller_->GetWeakPtr(),
              UTF8ToUnicode(value), /* delay_in_milli= */ 0));
          break;
        case UNSPECIFIED_KEYBAORD_STRATEGY:
          return ClientStatus(INVALID_ACTION);
      }
    }
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints,
                                 int delay_in_milli,
                                 bool use_js_focus) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    if (use_js_focus) {
      actions->emplace_back(base::BindOnce(&WebController::FocusField,
                                           web_controller_->GetWeakPtr()));
    } else {
      actions->emplace_back(base::BindOnce(
          &WebController::ScrollIntoView, web_controller_->GetWeakPtr(),
          /* animation= */ std::string(), /* vertical_alignment= */ "center",
          /* horizontal_alignment= */ "center"));
      actions->emplace_back(base::BindOnce(&WebController::ClickOrTapElement,
                                           web_controller_->GetWeakPtr(),
                                           ClickType::CLICK));
    }
    actions->emplace_back(base::BindOnce(&WebController::SendKeyboardInput,
                                         web_controller_->GetWeakPtr(),
                                         codepoints, delay_in_milli));
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints) {
    return SendKeyboardInput(selector, codepoints, -1, false);
  }

  ClientStatus SendKeyEvent(const Selector& selector,
                            const KeyEvent& key_event) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    actions->emplace_back(base::BindOnce(&WebController::FocusField,
                                         web_controller_->GetWeakPtr()));
    actions->emplace_back(base::BindOnce(&WebController::SendKeyEvent,
                                         web_controller_->GetWeakPtr(),
                                         key_event));
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attributes,
                            const std::string& value) {
    auto actions = std::make_unique<element_action_util::ElementActionVector>();
    actions->emplace_back(base::BindOnce(&WebController::SetAttribute,
                                         web_controller_->GetWeakPtr(),
                                         attributes, value));
    return FindElementAndPerformAll(selector, std::move(actions));
  }

  ClientStatus GetElementRect(const Selector& selector, RectF* rect_output) {
    base::RunLoop run_loop;
    ClientStatus result;

    web_controller_->FindElement(
        selector, /* strict= */ true,
        base::BindOnce(&WebControllerBrowserTest::GetElementRectElementCallback,
                       base::Unretained(this), run_loop.QuitClosure(), &result,
                       rect_output));

    run_loop.Run();
    return result;
  }

  void GetElementRectElementCallback(
      base::OnceClosure done_callback,
      ClientStatus* result_output,
      RectF* rect_output,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinderResult> element_result) {
    if (!element_status.ok()) {
      *result_output = element_status;
      std::move(done_callback).Run();
      return;
    }

    ASSERT_TRUE(element_result != nullptr);
    const ElementFinderResult* element_result_ptr = element_result.get();
    web_controller_->GetElementRect(
        *element_result_ptr,
        base::BindOnce(&WebControllerBrowserTest::OnGetElementRect,
                       base::Unretained(this), std::move(element_result),
                       std::move(done_callback), result_output, rect_output));
  }

  void OnGetElementRect(std::unique_ptr<ElementFinderResult> element,
                        base::OnceClosure done_callback,
                        ClientStatus* result_output,
                        RectF* rect_output,
                        const ClientStatus& rect_status,
                        const RectF& rect) {
    if (rect_status.ok()) {
      *rect_output = rect;
    }
    *result_output = rect_status;
    std::move(done_callback).Run();
  }

  // Show the overlay in the main page, which covers everything.
  void ShowOverlay() {
    EXPECT_TRUE(ExecJs(shell(),
                       R"(
document.getElementById("overlay").style.visibility='visible';
)"));
  }

  // Show the overlay in the first iframe, which covers the content
  // of that frame.
  void ShowOverlayInFrame() {
    EXPECT_TRUE(ExecJs(ChildFrameAt(shell()->web_contents(), 0),
                       R"(
document.getElementById("overlay_in_frame").style.visibility='visible';
)"));
  }

  // Hide the overlay in the main page.
  void HideOverlay() {
    EXPECT_TRUE(ExecJs(shell(),
                       R"(
document.getElementById("overlay").style.visibility='hidden';
)"));
  }

  // Hide the overlay in the first iframe.
  void HideOverlayInFrame() {
    EXPECT_TRUE(ExecJs(ChildFrameAt(shell()->web_contents(), 0),
                       R"(
document.getElementById("overlay_in_frame").style.visibility='hidden';
)"));
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
    ScrollToElementPosition(selector, top_padding);
    base::Value eval_result = content::EvalJs(shell(), R"(
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
    EXPECT_NEAR(top, window_height * 0.25, 1);

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

  ClientStatus RunWaitForDom(
      const ActionProto& wait_for_dom_action,
      bool use_observers,
      base::OnceCallback<void(ScriptExecutor*)> run_expectations) {
    MockScriptExecutorDelegate mock_script_executor_delegate;
    ON_CALL(mock_script_executor_delegate, GetWebController)
        .WillByDefault(Return(web_controller_.get()));
    TriggerContext trigger_context;
    if (use_observers) {
      trigger_context.SetScriptParameters(std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{
              {"ENABLE_OBSERVER_WAIT_FOR_DOM", "true"}}));
    }

    MockService mock_service;
    ActionsResponseProto actions_response;
    *actions_response.add_actions() = wait_for_dom_action;
    std::string serialized_actions_response;
    actions_response.SerializeToString(&serialized_actions_response);
    EXPECT_CALL(mock_service, GetActions)
        .WillOnce(RunOnceCallback<5>(200, serialized_actions_response,
                                     ServiceRequestSender::ResponseInfo{}));

    std::vector<ProcessedActionProto> captured_processed_actions;
    EXPECT_CALL(mock_service, GetNextActions)
        .WillOnce(WithArgs<3, 6>(
            [&captured_processed_actions](
                const std::vector<ProcessedActionProto>& processed_actions,
                ServiceRequestSender::ResponseCallback callback) {
              captured_processed_actions = processed_actions;

              // Send empty response to stop the script executor.
              std::move(callback).Run(200, std::string(),
                                      ServiceRequestSender::ResponseInfo{});
            }));
    ON_CALL(mock_script_executor_delegate, GetTriggerContext())
        .WillByDefault(Return(&trigger_context));
    ON_CALL(mock_script_executor_delegate, GetService())
        .WillByDefault(Return(&mock_service));
    GURL test_script_url("https://example.com");
    ON_CALL(mock_script_executor_delegate, GetScriptURL())
        .WillByDefault(testing::ReturnRef(test_script_url));
    std::vector<std::unique_ptr<Script>> ordered_interrupts;
    FakeScriptExecutorUiDelegate fake_script_executor_ui_delegate;
    UserData fake_user_data;
    ScriptExecutor script_executor(
        /* script_path= */ std::string(),
        /* additional_context= */ std::make_unique<TriggerContext>(),
        /* global_payload= */ std::string(),
        /* script_payload= */ std::string(),
        /* listener= */ nullptr, &ordered_interrupts,
        &mock_script_executor_delegate, &fake_script_executor_ui_delegate,
        /* is_interrupt_executor= */ false);
    base::RunLoop run_loop;
    script_executor.Run(
        &fake_user_data,
        base::BindOnce(&WebControllerBrowserTest::OnScriptFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    std::move(run_expectations).Run(&script_executor);

    CHECK_EQ(captured_processed_actions.size(), 1u);
    return ClientStatus(captured_processed_actions[0].status());
  }

  ClientStatus GetBackendNodeId(const ElementFinderResult& element,
                                int* backend_node_id) {
    ClientStatus result_status;

    base::RunLoop run_loop;
    web_controller_->GetBackendNodeId(
        element,
        base::BindLambdaForTesting([&](const ClientStatus& status, int id) {
          result_status = status;
          *backend_node_id = id;
          run_loop.Quit();
        }));
    run_loop.Run();

    return result_status;
  }

  ClientStatus RunPromptAfterShowCastAction(
      const ActionProto& show_cast_action,
      const ActionProto& prompt_action,
      MockScriptExecutorDelegate& mock_script_executor_delegate) {
    ON_CALL(mock_script_executor_delegate, GetWebController)
        .WillByDefault(Return(web_controller_.get()));

    MockService mock_service;
    ActionsResponseProto actions_response;
    *actions_response.add_actions() = show_cast_action;
    *actions_response.add_actions() = prompt_action;
    std::string serialized_actions_response;
    actions_response.SerializeToString(&serialized_actions_response);
    EXPECT_CALL(mock_service, GetActions)
        .WillOnce(RunOnceCallback<5>(200, serialized_actions_response,
                                     ServiceRequestSender::ResponseInfo{}));

    std::vector<ProcessedActionProto> captured_processed_actions;
    EXPECT_CALL(mock_service, GetNextActions)
        .WillOnce(WithArgs<3, 6>(
            [&captured_processed_actions](
                const std::vector<ProcessedActionProto>& processed_actions,
                ServiceRequestSender::ResponseCallback callback) {
              captured_processed_actions = processed_actions;
              // Send empty response to stop the script executor.
              std::move(callback).Run(200, std::string(),
                                      ServiceRequestSender::ResponseInfo{});
            }));

    TriggerContext trigger_context;
    ON_CALL(mock_script_executor_delegate, GetTriggerContext())
        .WillByDefault(Return(&trigger_context));
    ON_CALL(mock_script_executor_delegate, GetService())
        .WillByDefault(Return(&mock_service));
    GURL test_script_url("https://example.com");
    ON_CALL(mock_script_executor_delegate, GetScriptURL())
        .WillByDefault(testing::ReturnRef(test_script_url));

    EXPECT_CALL(mock_script_executor_delegate,
                EnterState(AutofillAssistantState::PROMPT))
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_script_executor_delegate,
                EnterState(AutofillAssistantState::RUNNING))
        .WillOnce(Return(true));

    std::vector<std::unique_ptr<Script>> ordered_interrupts;
    FakeScriptExecutorUiDelegate fake_script_executor_ui_delegate;
    UserData fake_user_data;
    ScriptExecutor script_executor(
        /* script_path= */ std::string(),
        /* additional_context= */ std::make_unique<TriggerContext>(),
        /* global_payload= */ std::string(),
        /* script_payload= */ std::string(),
        /* listener= */ nullptr, &ordered_interrupts,
        &mock_script_executor_delegate, &fake_script_executor_ui_delegate,
        /* is_interrupt_executor= */ false);
    base::RunLoop run_loop;
    script_executor.Run(
        &fake_user_data,
        base::BindOnce(&WebControllerBrowserTest::OnScriptFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    CHECK_EQ(captured_processed_actions.size(), 2u);
    return ClientStatus(captured_processed_actions[0].status());
  }

 protected:
  std::unique_ptr<WebController> web_controller_;
  UserData user_data_;
  UserModel user_model_;
  ProcessedActionStatusDetailsProto log_info_;
  MockAutofillAssistantAgent autofill_assistant_agent_;
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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, RequireNonemptyBoundingBox) {
  Selector button = Selector({"#button"});
  button.proto.add_filters()->mutable_bounding_box()->set_require_nonempty(
      true);
  RunLaxElementCheck(button, true);

  Selector hidden = Selector({"#hidden"});
  RunLaxElementCheck(hidden, true);
  hidden.proto.add_filters()->mutable_bounding_box()->set_require_nonempty(
      true);
  RunLaxElementCheck(hidden, false);

  Selector emptydiv = Selector({"#emptydiv"});
  RunLaxElementCheck(emptydiv, true);
  auto* emptydiv_box = emptydiv.proto.add_filters()->mutable_bounding_box();
  emptydiv_box->set_require_nonempty(true);
  RunLaxElementCheck(emptydiv, false);
  emptydiv_box->set_require_nonempty(false);
  RunLaxElementCheck(emptydiv, true);

  EXPECT_TRUE(content::ExecJs(shell(), R"(
  document.getElementById("emptydiv").style.height = '100px';
)"));
  RunLaxElementCheck(emptydiv, true);
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
  selector.proto.add_filters()->mutable_nth_match()->set_index(0);

  RunStrictElementCheck(selector, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, PseudoTypeThenCss) {
  Selector selector({"span"});
  selector.SetPseudoType(PseudoType::BEFORE);
  selector.proto.add_filters()->set_css_selector("div");

  // This makes no sense, but shouldn't return an unexpected error.
  ClientStatus status;
  ElementFinderResult result;
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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       PseudoElementContentWithCssStyle) {
  Selector selector({"#with_inner_text span"});
  auto* style = selector.proto.add_filters()->mutable_css_style();
  style->set_property("content");
  style->set_pseudo_element("before");
  style->mutable_value()->set_re2("\"before\"");
  RunLaxElementCheck(selector, true);

  style->mutable_value()->set_re2("\"nomatch\"");
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, CssVisibility) {
  Selector selector({"#button"});
  auto* style = selector.proto.add_filters()->mutable_css_style();
  style->set_property("visibility");
  style->mutable_value()->set_re2("visible");

  EXPECT_TRUE(content::ExecJs(shell(), R"(
  document.getElementById("button").style.visibility = 'hidden';
)"));
  RunLaxElementCheck(selector, false);
  style->set_should_match(false);
  RunLaxElementCheck(selector, true);

  EXPECT_TRUE(content::ExecJs(shell(), R"(
  document.getElementById("button").style.visibility = 'visible';
)"));
  style->set_should_match(true);
  RunLaxElementCheck(selector, true);
  style->set_should_match(false);
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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, MatchCssSelectorFilter) {
  Selector selector({"label"});
  selector.MatchingInnerText("terms and conditions");
  selector.proto.add_filters()->mutable_labelled();

  RunStrictElementCheck(selector, true);

  auto* last_filter = selector.proto.add_filters();

  last_filter->set_match_css_selector("input[type='checkbox']");
  RunStrictElementCheck(selector, true);

  last_filter->set_match_css_selector("input[type='text']");
  RunStrictElementCheck(selector, false);

  last_filter->set_match_css_selector(":checked");
  RunStrictElementCheck(selector, false);

  last_filter->set_match_css_selector(":not(:checked)");
  RunStrictElementCheck(selector, true);
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
           const item = document.querySelector("#scroll_item_3");
           item.addEventListener("click", function() {
             scrollItem3WasClicked = true;
           });)"));

  Selector selector({"#scroll_item_3"});
  ClickOrTapElement(selector, ClickType::CLICK);

  EXPECT_TRUE(content::EvalJs(shell(), "scrollItem3WasClicked").ExtractBool());

  // TODO(b/135909926): Find a reliable way of verifying that the button was
  // moved roughly to the center.
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElement) {
  Selector area_five({"#touch_area_five"});
  ClickOrTapElement(area_five, ClickType::TAP);
  WaitForElementRemove(area_five);

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
  WaitTillPageIsIdle(base::Hours(1));

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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ScrollToElementPosition) {
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
  ScrollToElementPosition(selector, top_padding);
  EXPECT_EQ(true, content::EvalJs(shell(), checkVisibleScript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ScrollToElementPositionWithContainer) {
  Selector selector({"#scroll_item_2"});
  Selector container_selector({"#scroll_container"});

  TopPadding top_padding{0, TopPadding::Unit::PIXELS};

  // std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  ScrollToElementPosition(selector, top_padding, container_selector);
  base::Value eval_result = content::EvalJs(shell(), R"(
      let item = document.querySelector("#scroll_item_2");
      let itemRect = item.getBoundingClientRect();
      let container = document.querySelector("#scroll_container");
      let containerRect = container.getBoundingClientRect();
      [itemRect.top, containerRect.top])")
                                .ExtractList();
  double element_top = eval_result.GetList()[0].GetDouble();
  double container_top = eval_result.GetList()[1].GetDouble();

  // Element is at the desired position.
  EXPECT_NEAR(element_top, container_top, 1);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ScrollToElementPosition_WithScrollIntoViewNeeded) {
  TestScrollIntoView(/* initial_window_scroll_y= */ 0,
                     /* initial_container_scroll_y=*/0);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ScrollToElementPosition_WithScrollIntoViewNotNeeded) {
  TestScrollIntoView(/* initial_window_scroll_y= */ 0,
                     /* initial_container_scroll_y=*/200);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ScrollToElementPosition_WithPaddingInPixels) {
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
  ScrollToElementPosition(selector, top_padding);

  double eval_result = content::EvalJs(shell(), R"(
      let scrollTarget = document.querySelector("#scroll-me");
      let scrollTargetRect = scrollTarget.getBoundingClientRect();
      scrollTargetRect.top;
  )")
                           .ExtractDouble();

  EXPECT_NEAR(360, eval_result, 1);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ScrollToElementPosition_WithPaddingInRatio) {
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
  ScrollToElementPosition(selector, top_padding);

  base::Value eval_result = content::EvalJs(shell(), R"(
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

  // Selecting on a non-<select> element.
  EXPECT_EQ(INVALID_TARGET,
            SelectOption(Selector({"#input1"}), std::string(),
                         /* case_sensitive= */ false, SelectOptionProto::LABEL,
                         /* strict= */ true)
                .proto_status());

  // Selecting with an invalid regular expression.
  EXPECT_EQ(INVALID_ACTION,
            SelectOption(selector, "*",
                         /* case_sensitive= */ false, SelectOptionProto::LABEL,
                         /* strict= */ true)
                .proto_status());

  // Fails if no comparison attribute is set.
  EXPECT_EQ(INVALID_ACTION,
            SelectOption(selector, "one", /* case_sensitive= */ false,
                         SelectOptionProto::NOT_SET, /* strict= */ true)
                .proto_status());

  // Select value not matching anything.
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "incorrect label",
                         /* case_sensitive= */ false, SelectOptionProto::LABEL,
                         /* strict= */ true)
                .proto_status());

  // Select value matching everything.
  EXPECT_EQ(TOO_MANY_OPTION_VALUES_FOUND,
            SelectOption(selector, ".*", /* case_sensitive= */ false,
                         SelectOptionProto::LABEL, /* strict= */ true)
                .proto_status());
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, ".*", /* case_sensitive= */ false,
                         SelectOptionProto::LABEL, /* strict= */ false)
                .proto_status());
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  // Select value matching the option's label.
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "^ZÜRICH", /* case_sensitive= */ false,
                         SelectOptionProto::LABEL, /* strict= */ true)
                .proto_status());
  EXPECT_EQ("Zürich Hauptbahnhof", content::EvalJs(shell(), javascript));

  // Select value matching the option's value.
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "^Aü万𠜎$", /* case_sensitive= */ false,
                         SelectOptionProto::VALUE, /* strict= */ true)
                .proto_status());
  EXPECT_EQ("Character Test Entry", content::EvalJs(shell(), javascript));

  // With a regular expression matching the option's value.
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "^O.E$", /* case_sensitive= */ false,
                         SelectOptionProto::VALUE, /* strict= */ true)
                .proto_status());
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  // With a regular expression matching the option's value case sensitive.
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "^O.E$", /* case_sensitive= */ true,
                         SelectOptionProto::VALUE, /* strict= */ true)
                .proto_status());
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  // Ignore disabled options.
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "five", /* case_sensitive= */ true,
                         SelectOptionProto::VALUE, /* strict= */ true)
                .proto_status());
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(selector, "^\\w{4}$", /* case_sensitive= */ false,
                         SelectOptionProto::VALUE, /* strict= */ true)
                .proto_status());
  EXPECT_EQ("Four", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIFrame) {
  // IFrame.
  Selector select_selector({"#iframe", "select[name=state]"});
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(select_selector, "^NY", /* case_sensitive= */ false,
                         SelectOptionProto::LABEL, /* strict= */ true)
                .proto_status());

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));

  // OOPIF.
  // Checking elements through EvalJs in OOPIF is blocked by cross-site.
  select_selector = Selector({"#iframeExternal", "select[name=pet]"});
  EXPECT_EQ(ACTION_APPLIED,
            SelectOption(select_selector, "^Cat", /* case_sensitive= */ false,
                         SelectOptionProto::LABEL, /* strict= */ true)
                .proto_status());

  Selector result_selector({"#iframeExternal", "#myPet"});
  GetFieldsValue({result_selector}, {"Cat"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtml) {
  std::string html;

  // Div.
  Selector div_selector({"#testOuterHtml"});
  ASSERT_EQ(ACTION_APPLIED,
            GetOuterHtml(div_selector,
                         /* include_all_inner_text*/ true, &html)
                .proto_status());
  EXPECT_EQ(
      R"(<div id="testOuterHtml"><span>Span</span><p>Paragraph</p></div>)",
      html);

  // IFrame.
  Selector iframe_selector({"#iframe", "#input"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtml(iframe_selector, /* include_all_inner_text*/ true, &html)
          .proto_status());
  EXPECT_EQ(R"(<input id="input" type="text">)", html);

  // OOPIF.
  Selector oopif_selector({"#iframeExternal", "#divToRemove"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtml(oopif_selector, /* include_all_inner_text*/ true, &html)
          .proto_status());
  EXPECT_EQ(R"(<div id="divToRemove">Text</div>)", html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtmls) {
  std::vector<std::string> htmls;

  Selector div_selector({".label"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtmls(div_selector, /* include_all_inner_text*/ true, &htmls)
          .proto_status());

  EXPECT_THAT(htmls,
              testing::ElementsAre(R"(<div class="label">Label 1</div>)",
                                   R"(<div class="label">Label 2</div>)",
                                   R"(<div class="label">Label 3</div>)"));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       GetOuterHtmlWithRedactedInnerText) {
  std::string html;

  // Div.
  Selector div_selector({"#testOuterHtml"});
  ASSERT_EQ(ACTION_APPLIED,
            GetOuterHtml(div_selector, /* include_all_inner_text*/ false, &html)
                .proto_status());
  EXPECT_EQ(R"(<div id="testOuterHtml"><span></span><p></p></div>)", html);

  // IFrame.
  Selector iframe_selector({"#iframe", "#input"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtml(iframe_selector, /* include_all_inner_text*/ false, &html)
          .proto_status());
  EXPECT_EQ(R"(<input id="input" type="text">)", html);

  // OOPIF.
  Selector oopif_selector({"#iframeExternal", "#divToRemove"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtml(oopif_selector, /* include_all_inner_text*/ false, &html)
          .proto_status());
  EXPECT_EQ(R"(<div id="divToRemove"></div>)", html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       GetOuterHtmlsWithRedactedInnerText) {
  std::vector<std::string> htmls;

  Selector div_selector({".label"});
  ASSERT_EQ(
      ACTION_APPLIED,
      GetOuterHtmls(div_selector, /* include_all_inner_text*/ false, &htmls)
          .proto_status());

  EXPECT_THAT(htmls, testing::ElementsAre(R"(<div class="label"></div>)",
                                          R"(<div class="label"></div>)",
                                          R"(<div class="label"></div>)"));
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
  EXPECT_EQ(
      ACTION_APPLIED,
      SetFieldValue(a_selector, /* value= */ "", SET_VALUE).proto_status());
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input3"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld3");
  GetFieldsValue(selectors, expected_values);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "new value", SIMULATE_KEY_PRESSES)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("new value");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input4"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld4");
  GetFieldsValue(selectors, expected_values);
  EXPECT_EQ(ACTION_APPLIED, SetFieldValue(a_selector, "new value",
                                          SIMULATE_KEY_PRESSES_SELECT_VALUE)
                                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("new value");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector = Selector({"#input5"});
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("helloworld5");
  GetFieldsValue(selectors, expected_values);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "new value", SIMULATE_KEY_PRESSES_FOCUS)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("new value");
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
  Selector b_selector({"#input7"});
  selectors.emplace_back(b_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(b_selector, input, /* delay_in_milli= */ -1,
                              /* use_js_focus= */ true)
                .proto_status());
  GetFieldsValue(selectors, {expected_output, expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputAndCheckPasswordField) {
  auto input = UTF8ToUnicode("password");
  std::string output;

  Selector selector({"#input_password"});
  EXPECT_EQ(ACTION_APPLIED, SendKeyboardInput(selector, input).proto_status());
  EXPECT_EQ(ACTION_APPLIED,
            GetStringAttribute(selector, {"value"}, &output).proto_status());
  EXPECT_EQ("password", output);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendKeyboardInputWithDelay) {
  Selector selector({"#input6"});
  const ClientStatus& status = SendKeyboardInput(selector, UTF8ToUnicode("abc"),
                                                 /* delay_in_milli= */ 1,
                                                 /* use_js_focus= */ true);
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());

  GetFieldsValue({selector}, {"abc"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, BackspaceKeyEvent) {
  EXPECT_TRUE(ExecJs(shell(), R"(
    document.getElementById('input4').addEventListener('keyup', (e) => {
      document.getElementById('input6').value = "triggered";
    });
    document.getElementById('input5').addEventListener('keyup', (e) => {
      document.getElementById('input7').value = "triggered";
    });
  )"));

  std::vector<Selector> selectors;
  Selector a_selector({"#input4"});
  selectors.emplace_back(a_selector);
  // The initial value of #input4 is "helloworld4", sending a backspace should
  // remove the last character.
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, UTF8ToUnicode("\b")).proto_status());
  Selector b_selector({"#input5"});
  selectors.emplace_back(b_selector);
  // The initial value of #input5 is "helloworld5", selecting the value and
  // sending a backspace should remove the entire text.
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(
                b_selector, "\b",
                KeyboardValueFillStrategy::SIMULATE_KEY_PRESSES_SELECT_VALUE)
                .proto_status());
  GetFieldsValue(selectors, {"helloworld", std::string()});

  selectors.clear();
  Selector a_trigger_selector({"#input6"});
  selectors.emplace_back(a_trigger_selector);
  Selector b_trigger_selector({"#input7"});
  selectors.emplace_back(b_trigger_selector);
  GetFieldsValue(selectors, {"triggered", "triggered"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendKeyCommands) {
  std::vector<Selector> selectors;
  // The initial value of #input2 is "helloworld2", sending a DeleteBackward
  // command should remove the last character.
  Selector a_selector({"#input2"});
  selectors.emplace_back(a_selector);
  KeyEvent a_key_event;
  a_key_event.add_command("MoveToEndOfLine");
  a_key_event.add_command("DeleteBackward");
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyEvent(a_selector, a_key_event).proto_status());
  // The initial value of #input3 is "helloworld3", sending SelectAll +
  // DeleteBackward commands should clear the text.
  Selector b_selector({"#input3"});
  selectors.emplace_back(b_selector);
  KeyEvent b_key_event;
  b_key_event.add_command("SelectAll");
  b_key_event.add_command("DeleteBackward");
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyEvent(b_selector, b_key_event).proto_status());
  GetFieldsValue(selectors, {"helloworld", std::string()});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputDevtoolsFailure) {
  // This makes devtools action fail and is used as a way of testing that the
  // case where SendKeyboardInput ends prematurely with 0 delay doesn't cause
  // issues.
  ElementFinderResult bad_element;
  bad_element.SetNodeFrameId("doesnotexist");

  ClientStatus status;
  base::RunLoop run_loop;
  web_controller_->SendKeyboardInput(
      UTF8ToUnicode("never sent"),
      /* key_press_delay_in_millisecond= */ 0, bad_element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(), &status));
  run_loop.Run();
  EXPECT_EQ(OTHER_ACTION_STATUS, status.proto_status());
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
            SendKeyboardInput(a_selector, input, /* delay_in_milli= */ 100,
                              /* use_js_focus= */ false)
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
      ElementFinderResult(), DOCUMENT_INTERACTIVE,
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
      ElementFinderResult(), DOCUMENT_COMPLETE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_EQ(DOCUMENT_COMPLETE, end_state);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitFrameDocumentReadyStateComplete) {
  ClientStatus status;

  ElementFinderResult iframe_element;
  FindElement(Selector({"#iframe"}), &status, &iframe_element);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      iframe_element, DOCUMENT_COMPLETE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_THAT(end_state, DOCUMENT_COMPLETE);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitExternalFrameDocumentReadyStateComplete) {
  ClientStatus status;

  ElementFinderResult iframe_element;
  FindElement(Selector({"#iframeExternal"}), &status, &iframe_element);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      iframe_element, DOCUMENT_COMPLETE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, status.proto_status()) << "Status: " << status;
  EXPECT_THAT(end_state, DOCUMENT_COMPLETE);
}

// Regression test for b/226551550
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, WaitDocumentReadyFails) {
  ClientStatus status;

  // This makes the devtools action fail.
  ElementFinderResult bad_element;
  bad_element.SetNodeFrameId("doesnotexist");

  DocumentReadyState end_state;
  base::RunLoop run_loop;
  web_controller_->WaitForDocumentReadyState(
      bad_element, DOCUMENT_COMPLETE,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatusAndReadyState,
                     base::Unretained(this), run_loop.QuitClosure(), &status,
                     &end_state));
  run_loop.Run();

  EXPECT_EQ(UNEXPECTED_JS_ERROR, status.proto_status()) << "Status: " << status;
  EXPECT_THAT(end_state, DOCUMENT_UNKNOWN_READY_STATE);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetElementRect) {
  RectF document_element_rect;
  Selector document_element({"#full_height_section"});
  EXPECT_EQ(
      ACTION_APPLIED,
      GetElementRect(document_element, &document_element_rect).proto_status());

  // The iFrame must be after the #full_height_section element to check that
  // the resulting rect is global.
  RectF iframe_element_rect;
  Selector iframe_element({"#iframe", "#touch_area_1"});
  EXPECT_EQ(
      ACTION_APPLIED,
      GetElementRect(iframe_element, &iframe_element_rect).proto_status());

  EXPECT_GT(iframe_element_rect.top, document_element_rect.bottom);

  // Make sure the element is within the iframe.
  RectF iframe_rect;
  Selector iframe({"#iframe"});
  EXPECT_EQ(ACTION_APPLIED,
            GetElementRect(iframe, &iframe_rect).proto_status());

  EXPECT_GT(iframe_element_rect.left, iframe_rect.left);
  EXPECT_LT(iframe_element_rect.right, iframe_rect.right);
  EXPECT_GT(iframe_element_rect.top, iframe_rect.top);
  EXPECT_LT(iframe_element_rect.bottom, iframe_rect.bottom);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetStringAttribute) {
  std::string value;

  std::vector<std::string> inner_text_attribute = {"innerText"};
  ASSERT_EQ(ACTION_APPLIED, GetStringAttribute(Selector({"#testOuterHtml p"}),
                                               inner_text_attribute, &value)
                                .proto_status());
  EXPECT_EQ("Paragraph", value);

  std::vector<std::string> option_label_attribute = {"options", "2", "label"};
  ASSERT_EQ(ACTION_APPLIED, GetStringAttribute(Selector({"#select"}),
                                               option_label_attribute, &value)
                                .proto_status());
  EXPECT_EQ("Three", value);

  std::vector<std::string> bad_access = {"none", "none"};
  ASSERT_EQ(UNEXPECTED_JS_ERROR,
            GetStringAttribute(Selector({"#button"}), bad_access, &value)
                .proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTop) {
  Selector button({"#button"});
  RunLaxElementCheck(button, true);

  button.proto.add_filters()->mutable_on_top();
  RunLaxElementCheck(button, true);

  ShowOverlay();
  RunLaxElementCheck(button, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTopNeedsScrolling) {
  // Scroll button out of the viewport.
  double target_bottom = content::EvalJs(shell(),
                                         R"(
const target = document.getElementById("touch_area_one");
const box = target.getBoundingClientRect();
window.scrollBy(0, box.bottom + 10);
target.getBoundingClientRect().bottom
)")
                             .ExtractDouble();

  // Before running the test, verify that the target is outside of the viewport,
  // as we wanted. full_height_section guarantees that this is never a problem.
  ASSERT_LE(target_bottom, 0);

  Selector target({"#touch_area_one"});
  RunLaxElementCheck(target, true);

  auto* on_top = target.proto.add_filters()->mutable_on_top();

  // Apply on_top without scrolling.
  on_top->set_scroll_into_view_if_needed(false);
  RunLaxElementCheck(target, false);
  on_top->set_accept_element_if_not_in_view(true);
  RunLaxElementCheck(target, true);

  // Allow on_top to scroll.
  on_top->set_scroll_into_view_if_needed(true);
  on_top->set_accept_element_if_not_in_view(false);
  RunLaxElementCheck(target, true);

  ASSERT_GE(content::EvalJs(shell(),
                            R"(
document.getElementById("touch_area_one").getBoundingClientRect().bottom
)")
                .ExtractDouble(),
            0);

  ShowOverlay();
  RunLaxElementCheck(target, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTopVeryTallElement) {
  Selector target({"#triple_height_section"});
  RunLaxElementCheck(target, true);
  auto* on_top = target.proto.add_filters()->mutable_on_top();

  // Apply on_top without scrolling.
  on_top->set_scroll_into_view_if_needed(false);
  RunLaxElementCheck(target, false);

  // Allow on_top to scroll.
  on_top->set_scroll_into_view_if_needed(true);
  RunLaxElementCheck(target, true);

  // Scroll until #triple_height_section is partially inside the viewport.
  EXPECT_TRUE(ExecJs(shell(), R"(
    const el = document.getElementById("triple_height_section");
    const pos = el.getBoundingClientRect().top - window.innerHeight + 100;
    window.scrollBy(0, pos);
  )"));
  RunLaxElementCheck(target, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ALabelIsNotAnOverlay) {
  Selector input({"#input1"});
  RunLaxElementCheck(input, true);

  input.proto.add_filters()->mutable_on_top();
  RunLaxElementCheck(input, true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTopFindsOverlayInFrame) {
  Selector button;
  button.proto.add_filters()->set_css_selector("#iframe");
  button.proto.add_filters()->mutable_enter_frame();
  button.proto.add_filters()->set_css_selector("button");
  button.proto.add_filters()->mutable_on_top();
  RunLaxElementCheck(button, true);

  ShowOverlayInFrame();
  RunLaxElementCheck(button, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTopFindsOverlayOverFrame) {
  Selector button;
  button.proto.add_filters()->set_css_selector("#iframe");
  button.proto.add_filters()->mutable_on_top();
  button.proto.add_filters()->mutable_enter_frame();
  button.proto.add_filters()->set_css_selector("button");
  RunLaxElementCheck(button, true);

  ShowOverlay();
  RunLaxElementCheck(button, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, OnTopFindsElementInShadow) {
  Selector button;
  button.proto.add_filters()->set_css_selector("#iframe");
  button.proto.add_filters()->mutable_enter_frame();
  button.proto.add_filters()->set_css_selector("#shadowsection");
  button.proto.add_filters()->mutable_enter_frame();
  button.proto.add_filters()->set_css_selector("#shadowbutton");
  RunLaxElementCheck(button, true);
  button.proto.add_filters()->mutable_on_top();
  RunLaxElementCheck(button, true);

  ShowOverlayInFrame();
  RunLaxElementCheck(button, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, CheckOnTop) {
  ClientStatus status;
  ElementFinderResult element;
  FindElement(Selector({"#button"}), &status, &element);
  ASSERT_TRUE(status.ok());

  // Make sure the button is visible.
  EXPECT_TRUE(ExecJs(
      shell(), "document.getElementById('button').scrollIntoViewIfNeeded();"));

  // The button is the topmost element.
  status = CheckOnTop(element);
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());
  EXPECT_EQ(WebControllerErrorInfoProto::UNSPECIFIED_WEB_ACTION,
            status.details().web_controller_error_info().failed_web_action());

  // The button is not the topmost element.
  ShowOverlay();
  status = CheckOnTop(element);
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, status.proto_status());
  EXPECT_EQ(WebControllerErrorInfoProto::ON_TOP,
            status.details().web_controller_error_info().failed_web_action());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, CheckOnTopInFrame) {
  ClientStatus status;
  ElementFinderResult element;
  FindElement(Selector({"#iframe", "#button"}), &status, &element);
  ASSERT_TRUE(status.ok());

  // Make sure the button is visible.
  EXPECT_TRUE(
      ExecJs(ChildFrameAt(shell()->web_contents(), 0),
             "document.getElementById('button').scrollIntoViewIfNeeded();"));

  // The button is covered by an overlay in the main frame
  ShowOverlay();
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, CheckOnTop(element).proto_status());

  // The button is covered by an overlay in the iframe
  HideOverlay();
  ShowOverlayInFrame();
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, CheckOnTop(element).proto_status());

  // The button is not covered by any overlay
  HideOverlayInFrame();
  EXPECT_EQ(ACTION_APPLIED, CheckOnTop(element).proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, NthMatch) {
  Selector selector;
  selector.proto.add_filters()->set_css_selector(".nth_match_parent");
  selector.proto.add_filters()->mutable_nth_match()->set_index(1);
  selector.proto.add_filters()->set_css_selector(".nth_match_child");

  auto* pick_at_filter = selector.proto.add_filters();
  std::string element_tag;

  pick_at_filter->mutable_nth_match()->set_index(0);
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(selector, &element_tag).proto_status());
  EXPECT_EQ("P", element_tag);

  pick_at_filter->mutable_nth_match()->set_index(1);
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(selector, &element_tag).proto_status());
  EXPECT_EQ("UL", element_tag);

  pick_at_filter->mutable_nth_match()->set_index(2);
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(selector, &element_tag).proto_status());
  EXPECT_EQ("LI", element_tag);

  pick_at_filter->mutable_nth_match()->set_index(3);
  ASSERT_EQ(ACTION_APPLIED,
            GetElementTag(selector, &element_tag).proto_status());
  EXPECT_EQ("STRONG", element_tag);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendChangeEvent) {
  Selector selector({"#input_with_onchange"});

  GetFieldsValue({selector}, {"0"});
  EXPECT_EQ(ACTION_APPLIED, SendChangeEvent(selector).proto_status());
  GetFieldsValue({selector}, {"1"});
  EXPECT_EQ(ACTION_APPLIED, SendChangeEvent(selector).proto_status());
  GetFieldsValue({selector}, {"2"});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendDuplexwebEvent) {
  Selector selector({"#duplexweb"});
  GetFieldsValue({selector}, {"empty"});
  web_controller_->DispatchJsEvent(base::DoNothing());
  GetFieldsValue({selector}, {"received"});
}

// Extremely flaky: https://crbug.com/1372516
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_WaitForElementToBecomeStable) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#touch_area_one"}), &element_status, &element);
  ASSERT_TRUE(element_status.ok());

  // Move the element indefinitely.
  EXPECT_TRUE(ExecJs(shell(), R"(
    (function() {
        let i = 0;
        document.getElementById('touch_area_one').style.position = 'absolute';
        document.browserTestInterval = setInterval(function() {
          document.getElementById('touch_area_one').style.left =
            `${10 * i++}px`;
        }, 100);
      })())"));
  EXPECT_EQ(ELEMENT_UNSTABLE,
            WaitUntilElementIsStable(element, 10, base::Milliseconds(100))
                .proto_status());

  // Stop moving the element.
  EXPECT_TRUE(ExecJs(shell(), "clearInterval(document.browserTestInterval);"));
  EXPECT_TRUE(
      WaitUntilElementIsStable(element, 10, base::Milliseconds(100)).ok());
}

// Extremely flaky: https://crbug.com/1372516
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_WaitForElementToBecomeStableForEmptyBoxModel) {
  ClientStatus element_status;

  // The element has an empty box model.
  ElementFinderResult empty_element;
  FindElement(Selector({"#emptydiv"}), &element_status, &empty_element);
  ASSERT_TRUE(element_status.ok());
  EXPECT_EQ(ELEMENT_POSITION_NOT_FOUND,
            WaitUntilElementIsStable(empty_element, 10, base::Milliseconds(10))
                .proto_status());

  // The element is always hidden and has no box model.
  ElementFinderResult hidden_element;
  FindElement(Selector({"#hidden"}), &element_status, &hidden_element);
  ASSERT_TRUE(element_status.ok());
  EXPECT_EQ(ELEMENT_POSITION_NOT_FOUND,
            WaitUntilElementIsStable(hidden_element, 10, base::Milliseconds(10))
                .proto_status());
}

// Extremely flaky: https://crbug.com/1372516
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       DISABLED_WaitForElementToBecomeStableDevtoolsFailure) {
  // This makes the devtools action fail.
  ElementFinderResult element;
  element.SetNodeFrameId("doesnotexist");
  element.SetRenderFrameHostForTest(web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(ELEMENT_POSITION_NOT_FOUND,
            WaitUntilElementIsStable(element, 10, base::Milliseconds(100))
                .proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionElement) {
  ClientStatus option_status;
  ElementFinderResult option;
  FindElement(Selector({"#select option:nth-child(2)"}), &option_status,
              &option);
  ASSERT_EQ(ACTION_APPLIED, option_status.proto_status());

  Selector selector({"#select"});

  GetFieldsValue({selector}, {"one"});
  EXPECT_EQ(ACTION_APPLIED,
            SelectOptionElement(selector, option).proto_status());
  GetFieldsValue({selector}, {"two"});

  // Using on a non-<select> element.
  EXPECT_EQ(INVALID_TARGET,
            SelectOptionElement(Selector({"#input1"}), option).proto_status());

  // Random element that is certainly not an option in the <select>.
  FindElement(Selector({"#input1"}), &option_status, &option);
  ASSERT_EQ(ACTION_APPLIED, option_status.proto_status());
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOptionElement(selector, option).proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, CheckSelectedOptionElement) {
  ClientStatus status;

  ElementFinderResult input;
  FindElement(Selector({"#input1"}), &status, &input);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  ElementFinderResult select;
  FindElement(Selector({"#select"}), &status, &select);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  ElementFinderResult selected_option;
  FindElement(Selector({"#select option:nth-child(1)"}), &status,
              &selected_option);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  ElementFinderResult not_selected_option;
  FindElement(Selector({"#select option:nth-child(2)"}), &status,
              &not_selected_option);
  ASSERT_EQ(ACTION_APPLIED, status.proto_status());

  EXPECT_EQ(ACTION_APPLIED,
            CheckSelectedOptionElement(select, selected_option).proto_status());
  EXPECT_EQ(
      ELEMENT_MISMATCH,
      CheckSelectedOptionElement(select, not_selected_option).proto_status());

  // Using on a non-<select> element.
  EXPECT_EQ(INVALID_TARGET,
            CheckSelectedOptionElement(input, selected_option).proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindUserDataElement) {
  autofill::AutofillProfile shipping;
  shipping.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_CITY, u"Zürich");
  user_model_.SetSelectedAutofillProfile(
      "SHIPPING", std::make_unique<autofill::AutofillProfile>(shipping),
      &user_data_);

  SelectorProto selector_proto;
  selector_proto.add_filters()->set_css_selector("#select option");
  auto* property = selector_proto.add_filters()->mutable_property();
  property->set_property("innerText");
  auto* inner_text = property->mutable_autofill_value_regexp();
  inner_text->mutable_profile()->set_identifier("SHIPPING");
  inner_text->mutable_value_expression_re2()->set_case_sensitive(false);
  inner_text->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ADDRESS_HOME_CITY));
  ClientStatus option_status;
  ElementFinderResult option;
  FindElement(Selector(selector_proto), &option_status, &option);
  ASSERT_EQ(ACTION_APPLIED, option_status.proto_status());

  Selector selector({"#select"});

  GetFieldsValue({selector}, {"one"});
  EXPECT_EQ(ACTION_APPLIED,
            SelectOptionElement(selector, option).proto_status());
  GetFieldsValue({selector}, {"two"});

  // Unknown profile.
  SelectorProto failing_selector_proto;
  failing_selector_proto.add_filters()->set_css_selector("#select option");
  auto* failing_property =
      failing_selector_proto.add_filters()->mutable_property();
  failing_property->set_property("innerText");
  auto* failing_inner_text = failing_property->mutable_autofill_value_regexp();
  failing_inner_text->mutable_profile()->set_identifier("BILLING");
  failing_inner_text->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ADDRESS_HOME_CITY));
  ClientStatus failing_option_status;
  ElementFinderResult failing_option;
  FindElement(Selector(failing_selector_proto), &failing_option_status,
              &failing_option);
  ASSERT_EQ(PRECONDITION_FAILED, failing_option_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ScrollIntoViewIfNeeded) {
  EXPECT_EQ(content::EvalJs(shell(), "window.scrollY").ExtractInt(), 0);

  // Make sure the target element is on top, such that no scrolling is
  // necessary.
  ClientStatus no_scroll_element_status;
  ElementFinderResult no_scroll_element;
  FindElement(Selector({"#trigger-keyboard"}), &no_scroll_element_status,
              &no_scroll_element);
  EXPECT_EQ(ACTION_APPLIED, no_scroll_element_status.proto_status());

  ClientStatus no_scroll_status;
  base::RunLoop no_scroll_run_loop;
  web_controller_->ScrollIntoViewIfNeeded(
      true, no_scroll_element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), no_scroll_run_loop.QuitClosure(),
                     &no_scroll_status));
  no_scroll_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, no_scroll_status.proto_status());
  EXPECT_EQ(content::EvalJs(shell(), "window.scrollY").ExtractInt(), 0);

  // Make sure the target element is after the full height view.
  ClientStatus scroll_element_status;
  ElementFinderResult scroll_element;
  FindElement(Selector({"#touch_area_five"}), &scroll_element_status,
              &scroll_element);
  EXPECT_EQ(ACTION_APPLIED, scroll_element_status.proto_status());

  ClientStatus scroll_status;
  base::RunLoop scroll_run_loop;
  web_controller_->ScrollIntoViewIfNeeded(
      true, scroll_element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), scroll_run_loop.QuitClosure(),
                     &scroll_status));
  scroll_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, scroll_status.proto_status());
  EXPECT_GT(content::EvalJs(shell(), "window.scrollY").ExtractDouble(), 0);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ScrollWindow) {
  EXPECT_EQ(content::EvalJs(shell(), "window.scrollY").ExtractInt(), 0);

  ScrollDistance scroll_distance;
  scroll_distance.set_pixels(20);

  ClientStatus status;
  base::RunLoop run_loop;
  web_controller_->ScrollWindow(
      scroll_distance, /* animation= */ std::string(),
      /* optional_frame= */ ElementFinderResult(),
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(), &status));
  run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());
  EXPECT_NEAR(content::EvalJs(shell(), "window.scrollY").ExtractDouble(), 20.0,
              0.5);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ScrollWindowOfIFrame) {
  EXPECT_EQ(content::EvalJs(shell(), "window.scrollY").ExtractInt(), 0);

  ScrollDistance scroll_distance;
  scroll_distance.set_pixels(20);

  // This needs to target an OOPIF, otherwise it will have the same `window`
  // and the test will fail.
  ClientStatus frame_status;
  ElementFinderResult frame;
  FindElement(Selector({"#iframeExternal", "body"}), &frame_status, &frame);
  EXPECT_EQ(ACTION_APPLIED, frame_status.proto_status());

  ClientStatus status;
  base::RunLoop run_loop;
  web_controller_->ScrollWindow(
      scroll_distance, /* animation= */ std::string(), frame,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(), &status));
  run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());
  EXPECT_EQ(content::EvalJs(shell(), "window.scrollY").ExtractInt(), 0);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ScrollContainer) {
  EXPECT_EQ(
      content::EvalJs(shell(),
                      "document.getElementById('scroll_container').scrollTop")
          .ExtractInt(),
      0);

  ScrollDistance scroll_distance;
  scroll_distance.set_pixels(20);

  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#scroll_container"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus status;
  base::RunLoop run_loop;
  web_controller_->ScrollContainer(
      scroll_distance, /* animation= */ std::string(), element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(), &status));
  run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, status.proto_status());
  EXPECT_NEAR(
      content::EvalJs(shell(),
                      "document.getElementById('scroll_container').scrollTop")
          .ExtractDouble(),
      20.0, 0.5);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, KeyMappings) {
  EXPECT_TRUE(ExecJs(shell(), R"(
    document.getElementById('input1').addEventListener('keydown', (e) => {
      lastKeydownEvent = `${e.key} ${e.keyCode} ${e.which}`;
    });
    document.getElementById('input1').addEventListener('keypress', (e) => {
      lastKeypressEvent = `${e.key} ${e.keyCode} ${e.which}`;
    });
    document.getElementById('input1').addEventListener('keyup', (e) => {
      lastKeyupEvent = `${e.key} ${e.keyCode} ${e.which}`;
    });
  )"));

  Selector selector({"#input1"});

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode("a")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            "a 65 65");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeypressEvent").ExtractString(),
            "a 97 97");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            "a 65 65");

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode("A")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            "A 65 65");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeypressEvent").ExtractString(),
            "A 65 65");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            "A 65 65");

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode("\b")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            "Backspace 8 8");
  // No keypress for backspace.
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            "Backspace 8 8");

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode("\r")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            "Enter 13 13");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeypressEvent").ExtractString(),
            "Enter 13 13");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            "Enter 13 13");

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode(",")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            ", 188 188");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeypressEvent").ExtractString(),
            ", 44 44");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            ", 188 188");

  EXPECT_EQ(SendKeyboardInput(selector, UTF8ToUnicode("<")).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(content::EvalJs(shell(), "lastKeydownEvent").ExtractString(),
            "< 188 188");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeypressEvent").ExtractString(),
            "< 60 60");
  EXPECT_EQ(content::EvalJs(shell(), "lastKeyupEvent").ExtractString(),
            "< 188 188");
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusAndBlur) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus focus_status;
  base::RunLoop focus_run_loop;
  web_controller_->FocusField(
      element, base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                              base::Unretained(this),
                              focus_run_loop.QuitClosure(), &focus_status));
  focus_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, focus_status.proto_status());
  EXPECT_TRUE(
      content::EvalJs(
          shell(),
          R"(document.activeElement === document.getElementById('input1'))")
          .ExtractBool());

  ClientStatus blur_status;
  base::RunLoop blur_run_loop;
  web_controller_->BlurField(
      element, base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                              base::Unretained(this),
                              blur_run_loop.QuitClosure(), &blur_status));
  blur_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, blur_status.proto_status());
  EXPECT_TRUE(
      content::EvalJs(shell(), R"(document.activeElement === document.body)")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ShowCastActionSetsTouchableAreaForMovingElement) {
  ActionProto show_cast_action;
  show_cast_action.mutable_show_cast()
      ->mutable_element_to_present()
      ->add_filters()
      ->set_css_selector("#touch_area_two");
  show_cast_action.mutable_show_cast()->set_wait_for_stable_element(
      OptionalStep::REQUIRE_STEP_SUCCESS);
  show_cast_action.mutable_show_cast()
      ->mutable_touchable_element_area()
      ->add_touchable()
      ->add_elements()
      ->add_filters()
      ->set_css_selector("#touch_area_two");

  // Show a prompt with a single option, with auto-select.
  // Used to test side effects of ShowCastAction.
  ActionProto prompt_action;
  prompt_action.mutable_prompt()->set_browse_mode(false);
  prompt_action.mutable_prompt()->set_message("test prompt message");
  auto* ok_proto = prompt_action.mutable_prompt()->add_choices();
  *ok_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("#touch_area_two");
  auto* chip = ok_proto->mutable_chip();
  chip->set_text("Ok");
  chip->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->set_tag("oktag");

  MockScriptExecutorDelegate mock_script_executor_delegate;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_script_executor_delegate, SetTouchableElementArea(_))
        .Times(2)
        .WillRepeatedly([&](const ElementAreaProto& area) {
          ASSERT_EQ(1, area.touchable().size());
          ASSERT_EQ(1, area.touchable(0).elements().size());
          ASSERT_EQ(1, area.touchable(0).elements(0).filters().size());
          EXPECT_EQ("#touch_area_two",
                    area.touchable(0).elements(0).filters(0).css_selector());
        });
    EXPECT_CALL(mock_script_executor_delegate, SetTouchableElementArea(_))
        .Times(1);
  }

  ClientStatus status = RunPromptAfterShowCastAction(
      show_cast_action, prompt_action, mock_script_executor_delegate);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);

  const std::string itemInViewportScript = R"(
      const item = document.querySelector("#touch_area_two");
      const itemRect = item.getBoundingClientRect();
      (itemRect.top >= 0) && (itemRect.bottom <= window.innerHeight)
    )";
  EXPECT_TRUE(content::EvalJs(shell(), itemInViewportScript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ShowCastActionSetsTouchableArea) {
  ActionProto show_cast_action;
  show_cast_action.mutable_show_cast()
      ->mutable_element_to_present()
      ->add_filters()
      ->set_css_selector("#input1");
  show_cast_action.mutable_show_cast()
      ->mutable_touchable_element_area()
      ->add_touchable()
      ->add_elements()
      ->add_filters()
      ->set_css_selector("#input1");

  // Show a prompt with a single option, with auto-select.
  // Used to test side effects of ShowCastAction.
  ActionProto prompt_action;
  prompt_action.mutable_prompt()->set_browse_mode(false);
  prompt_action.mutable_prompt()->set_message("test prompt message");
  auto* ok_proto = prompt_action.mutable_prompt()->add_choices();
  *ok_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("#input1");
  auto* chip = ok_proto->mutable_chip();
  chip->set_text("Ok");
  chip->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->set_tag("oktag");

  MockScriptExecutorDelegate mock_script_executor_delegate;
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_script_executor_delegate, SetTouchableElementArea(_))
        .Times(2)
        .WillRepeatedly([&](const ElementAreaProto& area) {
          ASSERT_EQ(1, area.touchable().size());
          ASSERT_EQ(1, area.touchable(0).elements().size());
          ASSERT_EQ(1, area.touchable(0).elements(0).filters().size());
          EXPECT_EQ("#input1",
                    area.touchable(0).elements(0).filters(0).css_selector());
        });
    EXPECT_CALL(mock_script_executor_delegate, SetTouchableElementArea(_))
        .Times(1);
  }

  ClientStatus status = RunPromptAfterShowCastAction(
      show_cast_action, prompt_action, mock_script_executor_delegate);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);

  const std::string itemInViewportScript = R"(
      const item = document.querySelector("#input1");
      const itemRect = item.getBoundingClientRect();
      (itemRect.top >= 0) && (itemRect.bottom <= window.innerHeight)
    )";
  const bool itemInViewport =
      content::EvalJs(shell(), itemInViewportScript).ExtractBool();
  EXPECT_TRUE(itemInViewport);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, WaitForDomForUniqueElement) {
  ActionProto action_proto;
  auto* wait_for_dom = action_proto.mutable_wait_for_dom();
  auto* condition = wait_for_dom->mutable_wait_condition();
  condition->mutable_client_id()->set_identifier("e");
  condition->set_require_unique_element(true);
  // This element is unique.
  *condition->mutable_match() = ToSelectorProto("#select");

  base::MockCallback<base::OnceCallback<void(ScriptExecutor*)>>
      run_expectations;
  EXPECT_CALL(run_expectations, Run(_))
      .WillOnce([](ScriptExecutor* script_executor) {
        EXPECT_TRUE(script_executor->GetElementStore()->HasElement("e"));
      });
  ClientStatus status = RunWaitForDom(action_proto, /* use_observers= */ false,
                                      run_expectations.Get());
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       WaitForDomForNonUniqueElement) {
  ActionProto action_proto;
  auto* wait_for_dom = action_proto.mutable_wait_for_dom();
  auto* condition = wait_for_dom->mutable_wait_condition();
  condition->mutable_client_id()->set_identifier("e");
  condition->set_require_unique_element(true);
  // This element is not unique.
  *condition->mutable_match() = ToSelectorProto("div");

  base::MockCallback<base::OnceCallback<void(ScriptExecutor*)>>
      run_expectations;
  EXPECT_CALL(run_expectations, Run(_))
      .WillOnce([](ScriptExecutor* script_executor) {
        EXPECT_FALSE(script_executor->GetElementStore()->HasElement("e"));
      });
  ClientStatus status = RunWaitForDom(action_proto, /* use_observers= */ false,
                                      run_expectations.Get());
  EXPECT_EQ(status.proto_status(), ELEMENT_RESOLUTION_FAILED);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ObserverWaitForDomForUniqueElement) {
  ActionProto action_proto;
  auto* wait_for_dom = action_proto.mutable_wait_for_dom();
  auto* condition = wait_for_dom->mutable_wait_condition();
  condition->mutable_client_id()->set_identifier("e");
  condition->set_require_unique_element(true);
  // This element is unique.
  *condition->mutable_match() = ToSelectorProto("#select");
  base::MockCallback<base::OnceCallback<void(ScriptExecutor*)>>
      run_expectations;
  EXPECT_CALL(run_expectations, Run(_))
      .WillOnce([](ScriptExecutor* script_executor) {
        EXPECT_TRUE(script_executor->GetElementStore()->HasElement("e"));
      });
  ClientStatus status = RunWaitForDom(action_proto, /* use_observers= */ true,
                                      run_expectations.Get());
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElementError) {
  ClientStatus element_status;
  ElementFinderResult element;
  Selector selector;
  selector.proto.set_tracking_id(1);
  selector.proto.add_filters()->set_css_selector("#select");
  selector.proto.add_filters()->mutable_bounding_box()->set_require_nonempty(
      true);
  selector.proto.add_filters()->set_css_selector("option:nth-child(100)");
  FindElement(selector, &element_status, &element);
  EXPECT_EQ(element_status.proto_status(), ELEMENT_RESOLUTION_FAILED);
  ASSERT_EQ(log_info_.element_finder_info().size(), 1);
  EXPECT_EQ(log_info_.element_finder_info(0).tracking_id(), 1);
  EXPECT_EQ(log_info_.element_finder_info(0).failed_filter_index_range_start(),
            0);
  EXPECT_EQ(log_info_.element_finder_info(0).failed_filter_index_range_end(),
            3);
  EXPECT_EQ(log_info_.element_finder_info(0).status(),
            ELEMENT_RESOLUTION_FAILED);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       RunElementFinderFromFrameElement) {
  ClientStatus frame_status;
  ElementFinderResult frame_element;
  FindElement(Selector({"#iframe", "body"}), &frame_status, &frame_element);
  ASSERT_EQ(ACTION_APPLIED, frame_status.proto_status());

  ClientStatus button_status;
  ElementFinderResult button_element;
  base::RunLoop button_run_loop;
  web_controller_->RunElementFinder(
      frame_element, Selector({"#shadowsection", "#shadowbutton"}),
      ElementFinderResultType::kExactlyOneMatch,
      base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                     base::Unretained(this), button_run_loop.QuitClosure(),
                     &button_status, &button_element));
  button_run_loop.Run();
  ASSERT_EQ(ACTION_APPLIED, button_status.proto_status());

  ClientStatus js_click_status;
  base::RunLoop js_click_run_loop;
  web_controller_->JsClickElement(
      button_element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), js_click_run_loop.QuitClosure(),
                     &js_click_status));
  js_click_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, js_click_status.proto_status());

  WaitForElementRemove(Selector({"#iframe", "#button"}));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, RunElementFinderFromOOPIF) {
  ClientStatus frame_status;
  ElementFinderResult frame_element;
  FindElement(Selector({"#iframeExternal", "body"}), &frame_status,
              &frame_element);
  ASSERT_EQ(ACTION_APPLIED, frame_status.proto_status());

  // Create fake element without object id and frame information only.
  ElementFinderResult fake_frame_element;
  fake_frame_element.SetRenderFrameHostForTest(
      frame_element.render_frame_host());
  fake_frame_element.SetNodeFrameId(
      frame_element.render_frame_host()->GetDevToolsFrameToken().ToString());

  ClientStatus button_status;
  ElementFinderResult button_element;
  base::RunLoop button_run_loop;
  web_controller_->RunElementFinder(
      fake_frame_element, Selector({"#button"}),
      ElementFinderResultType::kExactlyOneMatch,
      base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                     base::Unretained(this), button_run_loop.QuitClosure(),
                     &button_status, &button_element));
  button_run_loop.Run();
  ASSERT_EQ(ACTION_APPLIED, button_status.proto_status());

  ClientStatus js_click_status;
  base::RunLoop js_click_run_loop;
  web_controller_->JsClickElement(
      button_element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), js_click_run_loop.QuitClosure(),
                     &js_click_status));
  js_click_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, js_click_status.proto_status());

  WaitForElementRemove(Selector({"#iframeExternal", "#div"}));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ExecuteJSForFocusAndBlur) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus focus_status;
  base::RunLoop focus_run_loop;
  web_controller_->ExecuteJS(
      "this.focus();", element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), focus_run_loop.QuitClosure(),
                     &focus_status));
  focus_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, focus_status.proto_status());
  EXPECT_TRUE(
      content::EvalJs(
          shell(),
          R"(document.activeElement === document.getElementById('input1'))")
          .ExtractBool());

  ClientStatus blur_status;
  base::RunLoop blur_run_loop;
  web_controller_->ExecuteJS(
      "this.blur();", element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), blur_run_loop.QuitClosure(),
                     &blur_status));
  blur_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, blur_status.proto_status());
  EXPECT_TRUE(
      content::EvalJs(shell(), R"(document.activeElement === document.body)")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ExecuteJSWithClientStatus) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus valid_result_status;
  base::RunLoop valid_run_loop;
  web_controller_->ExecuteJS(
      "return 27; // ELEMENT_NOT_ON_TOP", element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), valid_run_loop.QuitClosure(),
                     &valid_result_status));
  valid_run_loop.Run();
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, valid_result_status.proto_status());

  ClientStatus invalid_result_status;
  base::RunLoop invalid_run_loop;
  web_controller_->ExecuteJS(
      "return -1;", element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), invalid_run_loop.QuitClosure(),
                     &invalid_result_status));
  invalid_run_loop.Run();
  EXPECT_EQ(INVALID_ACTION, invalid_result_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ExecuteJSWithBadReturnValue) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus result_status;
  base::RunLoop run_loop;
  web_controller_->ExecuteJS(
      "return 'text';", element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(),
                     &result_status));
  run_loop.Run();
  EXPECT_EQ(INVALID_ACTION, result_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ExecuteJSWithPromise) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  EXPECT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus success_status;
  base::RunLoop success_run_loop;
  web_controller_->ExecuteJS(
      R"(
        return new Promise((fulfill, reject) => {
          fulfill();
        });
      )",
      element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), success_run_loop.QuitClosure(),
                     &success_status));
  success_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, success_status.proto_status());

  ClientStatus error_status;
  base::RunLoop error_run_loop;
  web_controller_->ExecuteJS(
      R"(
        return new Promise((fulfill, reject) => {
          fulfill(27); // ELEMENT_NOT_ON_TOP
        });
      )",
      element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), error_run_loop.QuitClosure(),
                     &error_status));
  error_run_loop.Run();
  EXPECT_EQ(ELEMENT_NOT_ON_TOP, error_status.proto_status());

  ClientStatus reject_status;
  base::RunLoop reject_run_loop;
  web_controller_->ExecuteJS(
      R"(
        return new Promise((fulfill, reject) => {
          reject();
        });
      )",
      element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), reject_run_loop.QuitClosure(),
                     &reject_status));
  reject_run_loop.Run();
  EXPECT_EQ(UNEXPECTED_JS_ERROR, reject_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ExecuteJSWithException) {
  ClientStatus element_status;
  ElementFinderResult element;
  FindElement(Selector({"#input1"}), &element_status, &element);
  ASSERT_EQ(ACTION_APPLIED, element_status.proto_status());

  ClientStatus result_status;
  base::RunLoop run_loop;
  web_controller_->ExecuteJS(
      R"( // <-- this is line 0.
          function inner() {
            throw new Error('Test');
          }
          function outer() {
            inner();
          }
          outer();
          )",
      element,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(),
                     &result_status));
  run_loop.Run();
  EXPECT_EQ(UNEXPECTED_JS_ERROR, result_status.proto_status());
  EXPECT_THAT(result_status.details()
                  .unexpected_error_info()
                  .js_exception_line_numbers(),
              testing::ElementsAre(2, 5, 7));
  EXPECT_THAT(result_status.details()
                  .unexpected_error_info()
                  .js_exception_column_numbers(),
              testing::ElementsAre(18, 12, 10));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ParentFilter) {
  SelectorProto proto;
  proto.add_filters()->set_css_selector("#select option:checked");
  proto.add_filters()->mutable_parent();

  std::string element_tag;
  EXPECT_EQ(ACTION_APPLIED,
            GetElementTag(Selector(proto), &element_tag).proto_status());
  EXPECT_EQ("SELECT", element_tag);

  SelectorProto failing_proto;
  failing_proto.add_filters()->set_css_selector("body");
  failing_proto.add_filters()->mutable_parent();  // document
  failing_proto.add_filters()->mutable_parent();  // Nothing

  ClientStatus failing_status;
  ElementFinderResult ignored_element;
  FindElement(Selector(failing_proto), &failing_status, &ignored_element);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, failing_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, WebpageZoom) {
  double initial_width =
      content::EvalJs(
          shell(),
          R"(document.querySelector("#select").getBoundingClientRect().width)")
          .ExtractDouble();
  EXPECT_GT(initial_width, 0);

  ClientStatus body_status;
  ElementFinderResult body;
  FindElement(Selector({"body"}), &body_status, &body);
  EXPECT_EQ(ACTION_APPLIED, body_status.proto_status());

  ClientStatus zoom_status;
  base::RunLoop zoom_run_loop;
  web_controller_->ExecuteJS(
      "this.style.webkitTransform = 'scale(2)'", body,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), zoom_run_loop.QuitClosure(),
                     &zoom_status));
  zoom_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, zoom_status.proto_status());

  double after_zoom_width =
      content::EvalJs(
          shell(),
          R"(document.querySelector("#select").getBoundingClientRect().width)")
          .ExtractDouble();
  EXPECT_NEAR(after_zoom_width, initial_width * 2, 1);

  ClientStatus reset_status;
  base::RunLoop reset_run_loop;
  web_controller_->ExecuteJS(
      "this.style.webkitTransform = 'scale(1)'", body,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), reset_run_loop.QuitClosure(),
                     &reset_status));
  reset_run_loop.Run();
  EXPECT_EQ(ACTION_APPLIED, reset_status.proto_status());

  double after_reset_width =
      content::EvalJs(
          shell(),
          R"(document.querySelector("#select").getBoundingClientRect().width)")
          .ExtractDouble();
  EXPECT_NEAR(after_reset_width, initial_width, 1);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SetFieldValueThroughNative) {
  ClientStatus element_status;
  ElementFinderResult input;
  FindElement(Selector({"#input1"}), &element_status, &input);
  ASSERT_EQ(ACTION_APPLIED, element_status.proto_status());

  int backend_node_id;
  ASSERT_EQ(ACTION_APPLIED,
            GetBackendNodeId(input, &backend_node_id).proto_status());
  std::u16string expected_value = u"native";
  EXPECT_CALL(autofill_assistant_agent_,
              SetElementValue(backend_node_id, expected_value,
                              /* send_events= */ true, _))
      .WillOnce(RunOnceCallback<3>(true));

  ClientStatus fill_status;
  base::RunLoop run_loop;
  web_controller_->SetNativeValue(
      "native", input,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(),
                     &fill_status));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, fill_status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SetElementCheckedThroughNative) {
  ClientStatus element_status;
  ElementFinderResult input;
  FindElement(Selector({"#option1"}), &element_status, &input);
  ASSERT_EQ(ACTION_APPLIED, element_status.proto_status());

  int backend_node_id;
  ASSERT_EQ(ACTION_APPLIED,
            GetBackendNodeId(input, &backend_node_id).proto_status());

  EXPECT_CALL(autofill_assistant_agent_,
              SetElementChecked(backend_node_id, true,
                                /* send_events= */ true, _))
      .WillOnce(RunOnceCallback<3>(true));

  ClientStatus fill_status;
  base::RunLoop run_loop;
  web_controller_->SetNativeChecked(
      true, input,
      base::BindOnce(&WebControllerBrowserTest::OnClientStatus,
                     base::Unretained(this), run_loop.QuitClosure(),
                     &fill_status));
  run_loop.Run();

  EXPECT_EQ(ACTION_APPLIED, fill_status.proto_status());
}

}  // namespace autofill_assistant

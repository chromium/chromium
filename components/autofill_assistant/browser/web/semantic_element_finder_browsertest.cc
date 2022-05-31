// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/mock_script_executor_delegate.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_finder_result_type.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_autofill_assistant_agent.h"
#include "components/autofill_assistant/content/common/autofill_assistant_types.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::WithArgs;

}  // namespace

class SemanticElementFinderBrowserTest
    : public autofill_assistant::BaseBrowserTest,
      public content::WebContentsObserver {
 public:
  SemanticElementFinderBrowserTest() {}

  SemanticElementFinderBrowserTest(const SemanticElementFinderBrowserTest&) =
      delete;
  SemanticElementFinderBrowserTest& operator=(
      const SemanticElementFinderBrowserTest&) = delete;

  ~SemanticElementFinderBrowserTest() override {}

  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    MockAutofillAssistantAgent::RegisterForAllFrames(
        shell()->web_contents(), &autofill_assistant_agent_);

    annotate_dom_model_service_ = std::make_unique<AnnotateDomModelService>(
        /* opt_guide= */ nullptr, /* background_task_runner= */ nullptr);
    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &user_data_, &log_info_,
        annotate_dom_model_service_.get(),
        /* enable_full_stack_traces= */ true);

    Observe(shell()->web_contents());
  }

  void FindElement(const Selector& selector,
                   ClientStatus* status_out,
                   ElementFinderResult* result_out) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(&SemanticElementFinderBrowserTest::OnFindElement,
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

  void RunStrictElementCheck(const Selector& selector, bool expected_result) {
    ClientStatus status;
    ElementFinderResult ignored_element;
    FindElement(selector, &status, &ignored_element);
    EXPECT_EQ(expected_result, status.ok())
        << "selector: " << selector << " status: " << expected_result;
  }

  void FindElementExpectEmptyResult(const Selector& selector) {
    ClientStatus status;
    ElementFinderResult element;
    FindElement(selector, &status, &element);
    EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
    EXPECT_THAT(element.object_id(), IsEmpty());
  }

  void OnScriptFinished(base::OnceClosure done_callback,
                        const ScriptExecutor::Result& result) {
    std::move(done_callback).Run();
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
        &mock_script_executor_delegate, &fake_script_executor_ui_delegate);
    base::RunLoop run_loop;
    script_executor.Run(
        &fake_user_data,
        base::BindOnce(&SemanticElementFinderBrowserTest::OnScriptFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    std::move(run_expectations).Run(&script_executor);

    CHECK_EQ(captured_processed_actions.size(), 1u);
    return ClientStatus(captured_processed_actions[0].status());
  }

  int GetBackendNodeId(Selector selector, ClientStatus* status_out) {
    std::unique_ptr<ElementFinderResult> element_result;
    int backend_node_id = -1;

    base::RunLoop run_loop_1;
    web_controller_->FindElement(
        selector, true,
        base::BindLambdaForTesting(
            [&](const ClientStatus& status,
                std::unique_ptr<ElementFinderResult> result) {
              element_result = std::move(result);
              *status_out = status;
              run_loop_1.Quit();
            }));
    run_loop_1.Run();
    if (!status_out->ok()) {
      return backend_node_id;
    }

    // Second part in sequence, lookup backend node id.
    base::RunLoop run_loop_2;
    web_controller_->GetBackendNodeId(
        *element_result,
        base::BindLambdaForTesting([&](const ClientStatus& status, int id) {
          *status_out = status;
          backend_node_id = id;
          run_loop_2.Quit();
        }));
    run_loop_2.Run();

    log_info_.Clear();
    return backend_node_id;
  }

 protected:
  std::unique_ptr<WebController> web_controller_;
  UserData user_data_;
  ProcessedActionStatusDetailsProto log_info_;
  MockAutofillAssistantAgent autofill_assistant_agent_;
  std::unique_ptr<AnnotateDomModelService> annotate_dom_model_service_;
};

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       WaitForDomForSemanticElement) {
  // This element is unique.
  SelectorProto baseline_selector = ToSelectorProto("#select");

  ClientStatus element_status;
  int backend_node_id =
      GetBackendNodeId(Selector(baseline_selector), &element_status);
  EXPECT_TRUE(element_status.ok());

  NodeData node_data;
  node_data.backend_node_id = backend_node_id;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      // Capture any other frames.
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  ActionProto action_proto;
  auto* wait_for_dom = action_proto.mutable_wait_for_dom();
  auto* condition = wait_for_dom->mutable_wait_condition();
  condition->mutable_client_id()->set_identifier("e");
  condition->set_require_unique_element(true);
  auto* semantic_information =
      condition->mutable_match()->mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);

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

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       ElementExistenceCheckWithSemanticModel) {
  ClientStatus status;
  int backend_node_id = GetBackendNodeId(Selector({"#button"}), &status);
  EXPECT_TRUE(status.ok());

  NodeData node_data;
  node_data.backend_node_id = backend_node_id;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      // Capture any other frames.
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  // We pretend that the button is the correct element.
  SelectorProto proto;
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);
  RunStrictElementCheck(Selector(proto), true);

  ASSERT_EQ(log_info_.element_finder_info().size(), 1);
  const auto& result =
      log_info_.element_finder_info(0).semantic_inference_result();
  ASSERT_EQ(1, result.predicted_elements().size());
  EXPECT_EQ(backend_node_id, result.predicted_elements(0).backend_node_id());
  EXPECT_THAT(
      1, result.predicted_elements(0).semantic_information().semantic_role());
  EXPECT_THAT(2,
              result.predicted_elements(0).semantic_information().objective());
}

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       ElementExistenceCheckWithSemanticModelOOPIF) {
  ClientStatus status;
  int backend_node_id =
      GetBackendNodeId(Selector({"#iframeExternal", "#button"}), &status);
  EXPECT_TRUE(status.ok());

  NodeData node_data;
  node_data.backend_node_id = backend_node_id;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      // Capture any other frames.
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  // We pretend that the button is the correct element.
  SelectorProto proto;
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);
  RunStrictElementCheck(Selector(proto), true);

  ASSERT_EQ(log_info_.element_finder_info().size(), 1);
  const auto& result =
      log_info_.element_finder_info(0).semantic_inference_result();
  ASSERT_EQ(1, result.predicted_elements().size());
  EXPECT_EQ(backend_node_id, result.predicted_elements(0).backend_node_id());
  EXPECT_THAT(
      1, result.predicted_elements(0).semantic_information().semantic_role());
  EXPECT_THAT(2,
              result.predicted_elements(0).semantic_information().objective());
}

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       ElementExistenceCheckWithSemanticModelNotFound) {
  // All frames return an empty list as a result.
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillRepeatedly(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                         std::vector<NodeData>{}));

  SelectorProto proto;
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);
  FindElementExpectEmptyResult(Selector(proto));
}

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       ElementExistenceCheckWithSemanticMultipleFound) {
  SelectorProto proto;
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);

  NodeData node_data;
  node_data.backend_node_id = 5;
  NodeData node_data_other;
  node_data_other.backend_node_id = 13;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data_other}))
      // Capture any other frames.
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  // Two elements are found in different frames.
  ClientStatus status;
  FindElement(Selector(proto), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(
    SemanticElementFinderBrowserTest,
    ElementExistenceCheckWithSemanticModelUsesIgnoreObjective) {
  NodeData node_data;
  node_data.backend_node_id = 5;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, true, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  SelectorProto proto;
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);
  // All we want is this to be propagated to the GetSemanticNodes call as
  // configured in the previous expectation.
  semantic_information->set_ignore_objective(true);

  ClientStatus ignore_status;
  FindElement(Selector(proto), &ignore_status, nullptr);

  // TODO(b/217160707): For now we expect the originally passed in semantic info
  // to be logged instead of the objective inferred by the model.
  ASSERT_EQ(log_info_.element_finder_info().size(), 1);
  const auto& result =
      log_info_.element_finder_info(0).semantic_inference_result();
  ASSERT_EQ(1, result.predicted_elements().size());
  EXPECT_EQ(5, result.predicted_elements(0).backend_node_id());
  EXPECT_THAT(
      1, result.predicted_elements(0).semantic_information().semantic_role());
  EXPECT_THAT(2,
              result.predicted_elements(0).semantic_information().objective());
}

IN_PROC_BROWSER_TEST_F(SemanticElementFinderBrowserTest,
                       SemanticAndCssComparison) {
  ClientStatus status;
  int backend_node_id = GetBackendNodeId(Selector({"#button"}), &status);
  EXPECT_TRUE(status.ok());

  NodeData node_data;
  node_data.backend_node_id = backend_node_id;
  EXPECT_CALL(autofill_assistant_agent_,
              GetSemanticNodes(1, 2, false, base::Milliseconds(5000), _))
      .WillOnce(RunOnceCallback<4>(mojom::NodeDataStatus::kSuccess,
                                   std::vector<NodeData>{node_data}))
      // Capture any other frames.
      .WillRepeatedly(RunOnceCallback<4>(
          mojom::NodeDataStatus::kUnexpectedError, std::vector<NodeData>()));

  // We pretend that the button is the correct element.
  SelectorProto proto = ToSelectorProto("#button");
  auto* semantic_information = proto.mutable_semantic_information();
  semantic_information->set_semantic_role(1);
  semantic_information->set_objective(2);
  semantic_information->set_check_matches_css_element(true);
  RunStrictElementCheck(Selector(proto), true);

  ASSERT_EQ(log_info_.element_finder_info().size(), 1);
  const auto& result =
      log_info_.element_finder_info(0).semantic_inference_result();
  ASSERT_EQ(1, result.predicted_elements().size());
  EXPECT_EQ(backend_node_id, result.predicted_elements(0).backend_node_id());
  EXPECT_THAT(
      1, result.predicted_elements(0).semantic_information().semantic_role());
  EXPECT_THAT(2,
              result.predicted_elements(0).semantic_information().objective());
  EXPECT_TRUE(result.predicted_elements(0).matches_css_element());
}

}  // namespace autofill_assistant

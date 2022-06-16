// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "components/autofill_assistant/content/common/autofill_assistant_driver.mojom.h"
#include "components/autofill_assistant/content/common/proto/semantic_feature_overrides.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::SizeIs;

constexpr int kDummySemanticRole = 9999;
constexpr int kDummyObjective = 1111;

class MockAutofillAssistantDriver : public mojom::AutofillAssistantDriver {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>(
                  std::move(handle)));
  }

  MOCK_METHOD(
      void,
      GetAnnotateDomModel,
      (base::TimeDelta timeout,
       base::OnceCallback<
           void(mojom::ModelStatus, base::File, const std::string&)> callback),
      (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillAssistantDriver> receivers_;
};

class AutofillAssistantAgentBrowserTest : public content::RenderViewTest {
 public:
  AutofillAssistantAgentBrowserTest() = default;

  void SetUp() override {
    RenderViewTest::SetUp();

    GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::AutofillAssistantDriver::Name_,
            base::BindRepeating(
                &MockAutofillAssistantDriver::BindPendingReceiver,
                base::Unretained(&autofill_assistant_driver_)));
    autofill_assistant_agent_ = std::make_unique<AutofillAssistantAgent>(
        GetMainRenderFrame(), &associated_interfaces_);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("autofill_assistant")
                                         .AppendASCII("model")
                                         .AppendASCII("model.tflite");
    model_file_ = base::File(model_file_path,
                             (base::File::FLAG_OPEN | base::File::FLAG_READ));
  }

  void TearDown() override {
    autofill_assistant_agent_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  MockAutofillAssistantDriver autofill_assistant_driver_;
  std::unique_ptr<AutofillAssistantAgent> autofill_assistant_agent_;
  base::File model_file_;

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

TEST_F(AutofillAssistantAgentBrowserTest, GetSemanticNodes) {
  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<1>(mojom::ModelStatus::kSuccess,
                                   model_file_.Duplicate(), std::string()));

  base::MockCallback<base::OnceCallback<void(mojom::NodeDataStatus,
                                             const std::vector<NodeData>&)>>
      callback;
  EXPECT_CALL(callback, Run(mojom::NodeDataStatus::kSuccess, SizeIs(1)));

  LoadHTML(R"(
    <div>
      <h1>Shipping address</h1>
      <label for="street">Street Address</label><input id="street">
    </div>)");

  autofill_assistant_agent_->GetSemanticNodes(
      /* role= */ 47 /* ADDRESS_LINE1 */,
      /* objective= */ 7 /* FILL_DELIVERY_ADDRESS */,
      /* ignore_objective= */ false,
      /* model_timeout= */ base::Milliseconds(1000), callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, GetSemanticNodesModelTimeout) {
  // Do not reply to the model call.
  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<1>(mojom::ModelStatus::kTimeout, base::File(),
                                   std::string()));

  base::MockCallback<base::OnceCallback<void(mojom::NodeDataStatus,
                                             const std::vector<NodeData>&)>>
      callback;
  EXPECT_CALL(callback, Run(mojom::NodeDataStatus::kModelLoadTimeout, _));

  LoadHTML(R"(
    <div>
      <h1>Shipping address</h1>
      <label for="street">Street Address</label><input id="street">
    </div>)");

  autofill_assistant_agent_->GetSemanticNodes(
      /* role= */ 47 /* ADDRESS_LINE1 */,
      /* objective= */ 7 /* FILL_DELIVERY_ADDRESS */,
      /* ignore_objective= */ false,
      /* model_timeout= */ base::Milliseconds(0), callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, GetSemanticNodesModelError) {
  // Do not reply to the model call.
  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<1>(mojom::ModelStatus::kUnexpectedError,
                                   base::File(), std::string()));

  base::MockCallback<base::OnceCallback<void(mojom::NodeDataStatus,
                                             const std::vector<NodeData>&)>>
      callback;
  EXPECT_CALL(callback, Run(mojom::NodeDataStatus::kModelLoadError, _));

  LoadHTML(R"(
    <div>
      <h1>Shipping address</h1>
      <label for="street">Street Address</label><input id="street">
    </div>)");

  autofill_assistant_agent_->GetSemanticNodes(
      /* role= */ 47 /* ADDRESS_LINE1 */,
      /* objective= */ 7 /* FILL_DELIVERY_ADDRESS */,
      /* ignore_objective= */ false,
      /* model_timeout= */ base::Milliseconds(1000), callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, GetSemanticNodesIgnoreObjective) {
  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<1>(mojom::ModelStatus::kSuccess,
                                   model_file_.Duplicate(), std::string()));

  LoadHTML(R"(
    <div>
      <h1>Shipping address</h1>
      <label for="street">Street Address</label><input id="street">
    </div>)");

  base::MockCallback<base::OnceCallback<void(mojom::NodeDataStatus,
                                             const std::vector<NodeData>&)>>
      callback;
  EXPECT_CALL(callback, Run(mojom::NodeDataStatus::kSuccess, SizeIs(1)));

  autofill_assistant_agent_->GetSemanticNodes(
      /* role= */ 47 /* ADDRESS_LINE1 */,
      /* objective= */ 6 /* FILL_BILLING_ADDRESS */,
      /* ignore_objective= */ true,
      /* model_timeout= */ base::Milliseconds(1000), callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, Overrides) {
  SemanticSelectorPolicy policy_proto;
  auto* single_override =
      policy_proto.mutable_bag_of_words()->add_data_point_map();

  auto* coordinate = single_override->add_key_coordinate();
  coordinate->set_feature_concatenation_index(0);
  // Vocabulary entry "input"
  coordinate->set_vocabulary_index(1);
  coordinate->set_number_of_occurrences(1);

  auto* coordinate2 = single_override->add_key_coordinate();
  coordinate2->set_feature_concatenation_index(3);
  // Vocabulary entry "street"
  coordinate2->set_vocabulary_index(862);
  coordinate2->set_number_of_occurrences(1);

  auto* value = single_override->mutable_value();
  value->set_objective(kDummyObjective);
  value->set_semantic_role(kDummySemanticRole);

  std::string policy;
  ASSERT_TRUE(policy_proto.SerializeToString(&policy));

  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<1>(mojom::ModelStatus::kSuccess,
                                   model_file_.Duplicate(), policy));

  LoadHTML(R"(
    <div>
      <label for="street">street</label><input id="street">
    </div>)");

  base::MockCallback<base::OnceCallback<void(mojom::NodeDataStatus,
                                             const std::vector<NodeData>&)>>
      callback;
  EXPECT_CALL(callback, Run(mojom::NodeDataStatus::kSuccess, SizeIs(1)));

  autofill_assistant_agent_->GetSemanticNodes(
      kDummySemanticRole, kDummyObjective,
      /* ignore_objective= */ false,
      /* model_timeout= */ base::Milliseconds(1000), callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, SetElementCheckedForCheckbox) {
  LoadHTML(R"(<input type="checkbox" id="checkbox">)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  const auto web_element =
      GetMainRenderFrame()
          ->GetWebFrame()
          ->GetDocument()
          .GetElementById(blink::WebString::FromUTF8("checkbox"))
          .To<blink::WebInputElement>();
  autofill_assistant_agent_->SetElementChecked(
      web_element.GetDevToolsNodeIdForTest(), true,
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(web_element.IsChecked(), true);
}

TEST_F(AutofillAssistantAgentBrowserTest, SetElementCheckedForRadioButton) {
  LoadHTML(R"(<input type="radio" id="radio_button">)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  const auto web_element =
      GetMainRenderFrame()
          ->GetWebFrame()
          ->GetDocument()
          .GetElementById(blink::WebString::FromUTF8("radio_button"))
          .To<blink::WebInputElement>();
  autofill_assistant_agent_->SetElementChecked(
      web_element.GetDevToolsNodeIdForTest(), true,
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(web_element.IsChecked(), true);
}

TEST_F(AutofillAssistantAgentBrowserTest, SetCheckedFailsForNonCheckableInput) {
  LoadHTML(R"(<input type="text" id="text_input">)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));

  const auto web_element =
      GetMainRenderFrame()->GetWebFrame()->GetDocument().GetElementById(
          blink::WebString::FromUTF8("text_input"));

  autofill_assistant_agent_->SetElementChecked(
      web_element.GetDevToolsNodeIdForTest(), true,
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillAssistantAgentBrowserTest, SetElementValueForInput) {
  LoadHTML(R"(<input id="id">)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  const auto web_element = GetMainRenderFrame()
                               ->GetWebFrame()
                               ->GetDocument()
                               .GetElementById(blink::WebString::FromUTF8("id"))
                               .To<blink::WebFormControlElement>();

  autofill_assistant_agent_->SetElementValue(
      web_element.GetDevToolsNodeIdForTest(), u"value",
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(web_element.Value(), "value");
}

TEST_F(AutofillAssistantAgentBrowserTest, SetElementValueForSelect) {
  LoadHTML(R"(
    <select id="id">
      <option value="dog">Dog</option>
      <option value="cat">Cat</option>
    </select>)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  const auto web_element = GetMainRenderFrame()
                               ->GetWebFrame()
                               ->GetDocument()
                               .GetElementById(blink::WebString::FromUTF8("id"))
                               .To<blink::WebFormControlElement>();

  EXPECT_EQ(web_element.Value(), "dog");

  autofill_assistant_agent_->SetElementValue(
      web_element.GetDevToolsNodeIdForTest(), u"cat",
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(web_element.Value(), "cat");
}

TEST_F(AutofillAssistantAgentBrowserTest,
       SetElementValueFailsForNonFormControl) {
  LoadHTML(R"(
    <div id="id"></div>)");

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));

  const auto web_element =
      GetMainRenderFrame()->GetWebFrame()->GetDocument().GetElementById(
          blink::WebString::FromUTF8("id"));

  autofill_assistant_agent_->SetElementValue(
      web_element.GetDevToolsNodeIdForTest(), u"value",
      /* send_events= */ true, callback.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace autofill_assistant

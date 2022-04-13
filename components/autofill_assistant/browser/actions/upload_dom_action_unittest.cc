// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class UploadDomActionTest : public testing::Test {
 public:
  UploadDomActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_upload_dom() = proto_;
    UploadDomAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  UploadDomProto::Result UploadDomResult(
      const std::vector<std::string>& outer_htmls) {
    UploadDomProto::Result result;
    for (const auto& outer_html : outer_htmls) {
      result.add_outer_htmls(outer_html);
    }
    return result;
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UploadDomProto proto_;
};

TEST_F(UploadDomActionTest, EmptySelectorFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(UploadDomActionTest, ActionFailsForNonExistentElement) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT), base::Seconds(0)));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, TIMED_OUT),
                        Property(&ProcessedActionProto::upload_dom_result,
                                 Eq(UploadDomResult({})))))));
  Run();
}

TEST_F(UploadDomActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;
  proto_.set_include_all_inner_text(true);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetOuterHtml(/*include_all_inner_text*/ true,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), "<html></html>"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::upload_dom_result,
                                 Eq(UploadDomResult({"<html></html>"})))))));
  Run();
}

TEST_F(UploadDomActionTest, ReturnsEmptyStringForNotFoundElement) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  // Unlikely scenario, if ShortWaitForElement succeeds, the FindElement will
  // too. Failing FindElement is however the more interesting test than failing
  // GetOuterHtml.
  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(mock_action_delegate_, FindElement(selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, GetOuterHtml).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_RESOLUTION_FAILED),
          Property(&ProcessedActionProto::upload_dom_result,
                   Eq(UploadDomResult({})))))));
  Run();
}

TEST_F(UploadDomActionTest, RedactedText) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetOuterHtml(/*include_all_inner_text*/ false,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), "<html></html>"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::upload_dom_result,
                                 Eq(UploadDomResult({"<html></html>"})))))));
  Run();
}

TEST_F(UploadDomActionTest, MultipleDomUpload) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;
  proto_.set_can_match_multiple_elements(true);
  proto_.set_include_all_inner_text(true);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));

  EXPECT_CALL(mock_action_delegate_, FindAllElements(selector, _))
      .WillOnce(testing::WithArgs<1>([](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId("fake_object_id");
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinderResult expected_result;
  expected_result.SetObjectId("fake_object_id");

  std::vector<std::string> fake_htmls{"<div></div>", "<span></span>"};
  EXPECT_CALL(mock_web_controller_,
              GetOuterHtmls(/*include_all_inner_text*/ true,
                            EqualsElement(expected_result), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), fake_htmls));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::upload_dom_result,
                   Eq(UploadDomResult({"<div></div>", "<span></span>"})))))));
  Run();
}

TEST_F(UploadDomActionTest, MultipleDomUploadRedactText) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;
  proto_.set_can_match_multiple_elements(true);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));

  EXPECT_CALL(mock_action_delegate_, FindAllElements(selector, _))
      .WillOnce(testing::WithArgs<1>([](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId("fake_object_id");
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinderResult expected_result;
  expected_result.SetObjectId("fake_object_id");

  std::vector<std::string> fake_htmls{"<div></div>", "<span></span>"};

  EXPECT_CALL(mock_web_controller_,
              GetOuterHtmls(/*include_all_inner_text*/ false,
                            EqualsElement(expected_result), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), fake_htmls));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::upload_dom_result,
                   Eq(UploadDomResult({"<div></div>", "<span></span>"})))))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant

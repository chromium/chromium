// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ReturnRef;
using ::testing::WithArgs;

class WaitForDomActionTest : public testing::Test {
 public:
  WaitForDomActionTest() {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, FindElement(_, _, _))
        .WillByDefault(WithArgs<2>([](auto&& callback) {
          std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                  std::make_unique<ElementFinderResult>());
        }));

    EXPECT_CALL(mock_action_delegate_, WaitForDomWithSlowWarning(_, _, _, _, _))
        .WillRepeatedly(Invoke(this, &WaitForDomActionTest::FakeWaitForDom));
  }

 protected:
  // Fakes ActionDelegate::WaitForDom.
  void FakeWaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
    checker_ = std::make_unique<BatchElementChecker>();
    has_check_elements_result_ = false;
    check_elements.Run(
        checker_.get(),
        base::BindOnce(&WaitForDomActionTest::OnCheckElementsDone,
                       base::Unretained(this)));
    checker_->AddAllDoneCallback(
        base::BindOnce(&WaitForDomActionTest::OnWaitForDomDone,
                       base::Unretained(this), std::move(callback)));
    checker_->Run(&mock_web_controller_);
  }

  // Called from the check_elements callback passed to FakeWaitForDom.
  void OnCheckElementsDone(const ClientStatus& result) {
    ASSERT_FALSE(has_check_elements_result_);  // Duplicate calls
    has_check_elements_result_ = true;
    check_elements_result_ = result;
  }

  // Called by |checker_| once it's done. This ends the call to
  // FakeWaitForDom().
  void OnWaitForDomDone(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
    ASSERT_TRUE(
        has_check_elements_result_);  // OnCheckElementsDone() not called
    std::move(callback).Run(check_elements_result_,
                            base::Milliseconds(fake_wait_time_));
  }

  // Runs the action defined in |proto_| and reports the result to |callback_|.
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_wait_for_dom() = proto_;
    WaitForDomAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  WaitForDomProto proto_;
  std::unique_ptr<BatchElementChecker> checker_;
  bool has_check_elements_result_ = false;
  ClientStatus check_elements_result_;
  int fake_wait_time_ = 0;
};

TEST_F(WaitForDomActionTest, NoSelectors) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(WaitForDomActionTest, ConditionMet) {
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));

  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, TimingStatsConditionMet) {
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));

  fake_wait_time_ = 500;
  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 500);
}

TEST_F(WaitForDomActionTest, ConditionNotMet) {
  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, ReportMatchesToServer) {
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element1"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element2"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element3"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"#element4"}), /* strict= */ false, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));

  auto* any_of = proto_.mutable_wait_condition()->mutable_any_of();
  auto* condition1 = any_of->add_conditions();
  *condition1->mutable_match() = ToSelectorProto("#element1");
  condition1->set_payload("1");
  condition1->set_tag("1tag");

  auto* condition2 = any_of->add_conditions();
  *condition2->mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("#element2");
  condition2->set_payload("2");
  condition2->set_tag("2tag");

  auto* condition3 = any_of->add_conditions();
  *condition3->mutable_match() = ToSelectorProto("#element3");
  condition3->set_payload("3");
  condition3->set_tag("3tag");

  auto* condition4 = any_of->add_conditions();
  *condition4->mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("#element4");
  condition4->set_payload("4");
  condition4->set_tag("4tag");

  // Condition 1 and 2 are met, conditions 3 and 4 are not.

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_THAT(capture.wait_for_dom_result().matching_condition_payloads(),
              ElementsAre("1", "2"));
  EXPECT_THAT(capture.wait_for_dom_result().matching_condition_tags(),
              ElementsAre("1tag", "2tag"));
}

TEST_F(WaitForDomActionTest, StoreMatchForLater) {
  Selector expected_selector({"#element"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector, /* strict= */ false, _))
      .WillOnce(WithArgs<2>([&expected_selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId(
            expected_selector.proto.filters(0).css_selector());
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  auto* condition = proto_.mutable_wait_condition();
  *condition->mutable_match() = ToSelectorProto("#element");
  condition->set_payload("1");
  condition->mutable_client_id()->set_identifier("element");

  EXPECT_CALL(callback_, Run(_));
  Run();

  EXPECT_TRUE(mock_action_delegate_.GetElementStore()->HasElement("element"));
}

TEST_F(WaitForDomActionTest, StoreStrictMatchForLater) {
  Selector expected_selector({"#element"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector, /* strict= */ true, _))
      .WillOnce(WithArgs<2>([&expected_selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId(
            expected_selector.proto.filters(0).css_selector());
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  auto* condition = proto_.mutable_wait_condition();
  *condition->mutable_match() = ToSelectorProto("#element");
  condition->set_payload("1");
  condition->mutable_client_id()->set_identifier("element");
  condition->set_require_unique_element(true);

  EXPECT_CALL(callback_, Run(_));
  Run();

  EXPECT_TRUE(mock_action_delegate_.GetElementStore()->HasElement("element"));
}

TEST_F(WaitForDomActionTest, StrictMatchFailsForMultipleElements) {
  Selector expected_selector({"#element"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector, /* strict= */ true, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(TOO_MANY_ELEMENTS),
                                std::make_unique<ElementFinderResult>());
      }));

  auto* condition = proto_.mutable_wait_condition();
  *condition->mutable_match() = ToSelectorProto("#element");
  condition->set_payload("1");
  condition->mutable_client_id()->set_identifier("element");
  condition->set_require_unique_element(true);

  EXPECT_CALL(callback_, Run(_));
  Run();

  EXPECT_FALSE(mock_action_delegate_.GetElementStore()->HasElement("element"));
}

TEST_F(WaitForDomActionTest, RemoveElementsNoLongerFound) {
  Selector expected_found_selector({"#element-found"});
  Selector expected_not_found_selector({"#element-not-found"});
  test_util::MockFindElement(mock_web_controller_, expected_found_selector);
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_not_found_selector, _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));

  // A previous run found this element.
  mock_action_delegate_.GetElementStore()->AddElement("element2",
                                                      DomObjectFrameStack());

  auto* any_of = proto_.mutable_wait_condition()->mutable_any_of();
  auto* condition1 = any_of->add_conditions();
  *condition1->mutable_match() = ToSelectorProto("#element-found");
  condition1->mutable_client_id()->set_identifier("element1");
  auto* condition2 = any_of->add_conditions();
  *condition2->mutable_match() = ToSelectorProto("#element-not-found");
  condition2->mutable_client_id()->set_identifier("element2");

  EXPECT_CALL(callback_, Run(_));
  Run();

  EXPECT_TRUE(mock_action_delegate_.GetElementStore()->HasElement("element1"));
  EXPECT_FALSE(mock_action_delegate_.GetElementStore()->HasElement("element2"));
}

TEST_F(WaitForDomActionTest, ReturnsRolledUpErrorInformation) {
  ProcessedActionStatusDetailsProto log_info;

  Selector selector_1({"#element-1"});
  ElementFinderInfoProto* info_1 = log_info.add_element_finder_info();
  info_1->set_tracking_id(1);
  info_1->set_failed_filter_index_range_start(0);
  info_1->set_failed_filter_index_range_end(2);
  info_1->set_status(INVALID_SELECTOR);
  EXPECT_CALL(mock_web_controller_, FindElement(selector_1, _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  Selector selector_2({"#element-2"});
  ElementFinderInfoProto* info_2 = log_info.add_element_finder_info();
  info_2->set_tracking_id(2);
  info_2->set_failed_filter_index_range_start(1);
  info_2->set_failed_filter_index_range_end(3);
  info_2->set_get_document_failed(true);
  info_2->set_status(ELEMENT_RESOLUTION_FAILED);
  EXPECT_CALL(mock_web_controller_, FindElement(selector_2, _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  Selector selector_3({"#element-3"});
  ElementFinderInfoProto* info_3 = log_info.add_element_finder_info();
  info_3->set_tracking_id(3);
  info_3->set_status(ACTION_APPLIED);
  EXPECT_CALL(mock_web_controller_, FindElement(selector_3, _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ACTION_APPLIED),
                                std::make_unique<ElementFinderResult>());
      }));
  ProcessedActionStatusDetailsProto clear_log_info;
  EXPECT_CALL(mock_action_delegate_, GetLogInfo)
      .WillOnce(ReturnRef(clear_log_info))  // Once to clear at the start.
      .WillOnce(ReturnRef(log_info));       // Once to fill at the end.

  auto* condition = proto_.mutable_wait_condition()->mutable_all_of();
  auto* condition_1 = condition->add_conditions();
  *condition_1->mutable_match() = ToSelectorProto("#element-1");
  condition_1->mutable_match()->set_tracking_id(1);
  condition_1->mutable_client_id()->set_identifier("element-1");
  auto* condition_2 = condition->add_conditions();
  *condition_2->mutable_match() = ToSelectorProto("#element-2");
  condition_2->mutable_client_id()->set_identifier("element-2");
  condition_2->mutable_match()->set_tracking_id(2);
  auto* condition_3 = condition->add_conditions();
  *condition_3->mutable_match() = ToSelectorProto("#element-3");
  condition_3->mutable_client_id()->set_identifier("element-3");
  condition_3->mutable_match()->set_tracking_id(3);

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ELEMENT_RESOLUTION_FAILED);
  ASSERT_EQ(capture.status_details().element_finder_info_size(), 3);
  EXPECT_EQ(capture.status_details().element_finder_info(0).status(),
            INVALID_SELECTOR);
  EXPECT_EQ(capture.status_details().element_finder_info(0).tracking_id(), 1);
  EXPECT_EQ(capture.status_details()
                .element_finder_info(0)
                .failed_filter_index_range_start(),
            0);
  EXPECT_EQ(capture.status_details()
                .element_finder_info(0)
                .failed_filter_index_range_end(),
            2);
  EXPECT_EQ(capture.status_details().element_finder_info(1).status(),
            ELEMENT_RESOLUTION_FAILED);
  EXPECT_EQ(capture.status_details().element_finder_info(1).tracking_id(), 2);
  EXPECT_EQ(capture.status_details()
                .element_finder_info(1)
                .failed_filter_index_range_start(),
            1);
  EXPECT_EQ(capture.status_details()
                .element_finder_info(1)
                .failed_filter_index_range_end(),
            3);
  EXPECT_TRUE(
      capture.status_details().element_finder_info(1).get_document_failed());
  EXPECT_EQ(capture.status_details().element_finder_info(2).status(),
            ACTION_APPLIED);
  EXPECT_EQ(capture.status_details().element_finder_info(2).tracking_id(), 3);
}

}  // namespace
}  // namespace autofill_assistant

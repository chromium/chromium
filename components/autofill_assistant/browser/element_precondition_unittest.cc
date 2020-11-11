// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::WithArgs;

class ElementPreconditionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_, OnFindElement(Selector({"exists"}), _))
        .WillByDefault(WithArgs<1>([](auto&& callback) {
          std::move(callback).Run(OkClientStatus(),
                                  std::make_unique<ElementFinder::Result>());
        }));
    ON_CALL(mock_web_controller_, OnFindElement(Selector({"exists_too"}), _))
        .WillByDefault(WithArgs<1>([](auto&& callback) {
          std::move(callback).Run(OkClientStatus(),
                                  std::make_unique<ElementFinder::Result>());
        }));
    ON_CALL(mock_web_controller_,
            OnFindElement(Selector({"does_not_exist"}), _))
        .WillByDefault(RunOnceCallback<1>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
    ON_CALL(mock_web_controller_,
            OnFindElement(Selector({"does_not_exist_either"}), _))
        .WillByDefault(RunOnceCallback<1>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  }

 protected:
  // Runs a precondition given |exists_| and |value_match_|.
  void Check(
      base::OnceCallback<void(const ClientStatus&,
                              const std::vector<std::string>&)> callback) {
    ElementPrecondition precondition(condition_);
    BatchElementChecker batch_checks;
    precondition.Check(&batch_checks, std::move(callback));
    batch_checks.Run(&mock_web_controller_);
  }

  MockWebController mock_web_controller_;
  base::MockCallback<base::OnceCallback<void(const ClientStatus&,
                                             const std::vector<std::string>&)>>
      mock_callback_;
  ElementConditionProto condition_;
};

TEST_F(ElementPreconditionTest, Empty) {
  EXPECT_TRUE(ElementPrecondition(condition_).empty());
}

TEST_F(ElementPreconditionTest, NonEmpty) {
  *condition_.mutable_match() = ToSelectorProto("exists");
  EXPECT_FALSE(ElementPrecondition(condition_).empty());
}

TEST_F(ElementPreconditionTest, NoConditions) {
  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, EmptySelector) {
  condition_.mutable_match();

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, ElementExists) {
  *condition_.mutable_match() = ToSelectorProto("exists");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, ElementDoes_Not_Exist) {
  *condition_.mutable_match() = ToSelectorProto("does_not_exist");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AnyOf_Empty) {
  condition_.mutable_any_of();

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AnyOf_NoneMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AnyOf_SomeMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AnyOf_AllMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AllOf_Empty) {
  condition_.mutable_all_of();

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AllOf_NoneMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AllOf_SomeMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AllOf_AllMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, NoneOf_Empty) {
  condition_.mutable_none_of();

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, NoneOf_NoneMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, NoneOf_SomeMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, NoneOf_AllMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ELEMENT_RESOLUTION_FAILED), _));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, Payload_ConditionMet) {
  auto* exists = condition_.mutable_any_of()->add_conditions();
  *exists->mutable_match() = ToSelectorProto("exists");
  exists->set_payload("exists");

  auto* exists_too = condition_.mutable_any_of()->add_conditions();
  *exists_too->mutable_match() = ToSelectorProto("exists_too");
  exists_too->set_payload("exists_too");

  condition_.set_payload("any_of");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED),
                  ElementsAre("exists", "exists_too", "any_of")));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, Payload_ConditionNotMet) {
  auto* exists = condition_.mutable_none_of()->add_conditions();
  *exists->mutable_match() = ToSelectorProto("exists");
  exists->set_payload("exists");

  auto* exists_too = condition_.mutable_none_of()->add_conditions();
  *exists_too->mutable_match() = ToSelectorProto("exists_too");
  exists_too->set_payload("exists_too");

  condition_.set_payload("none_of");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  ElementsAre("exists", "exists_too")));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, Complex) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");
  auto* none_of = condition_.mutable_all_of()->add_conditions();
  none_of->set_payload("none_of");
  auto* does_not_exist_in_none_of =
      none_of->mutable_none_of()->add_conditions();
  *does_not_exist_in_none_of->mutable_match() =
      ToSelectorProto("does_not_exist");
  does_not_exist_in_none_of->set_payload("does_not_exist in none_of");
  *none_of->mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  auto* any_of = condition_.mutable_all_of()->add_conditions();
  any_of->set_payload("any_of");
  auto* exists_in_any_of = any_of->mutable_any_of()->add_conditions();
  *exists_in_any_of->mutable_match() = ToSelectorProto("exists");
  exists_in_any_of->set_payload("exists in any_of");

  *any_of->mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED),
                  ElementsAre("none_of", "exists in any_of", "any_of")));
  Check(mock_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant

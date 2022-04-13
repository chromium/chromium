// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

namespace autofill_assistant {

namespace {

class BatchElementCheckerTest : public testing::Test {
 protected:
  BatchElementCheckerTest() : checks_() {}

  void SetUp() override {
    // Any other selector apart from does_not_exist and does_not_exist_either
    // will exist.
    test_util::MockFindAnyElement(mock_web_controller_);

    ON_CALL(mock_web_controller_,
            FindElement(Selector({"does_not_exist"}), _, _))
        .WillByDefault(RunOnceCallback<2>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
    ON_CALL(mock_web_controller_,
            FindElement(Selector({"does_not_exist_either"}), _, _))
        .WillByDefault(RunOnceCallback<2>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  }

  void OnElementExistenceCheck(
      const std::string& name,
      const ClientStatus& result,
      const std::vector<std::string>& ignored_payloads,
      const std::vector<std::string>& ignored_tags,
      const base::flat_map<std::string, DomObjectFrameStack>&
          ignored_elements) {
    element_exists_results_[name] = result.ok();
  }

  BatchElementChecker::ElementConditionCheckCallback ElementExistenceCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnElementExistenceCheck,
                          base::Unretained(this), name);
  }

  void OnFieldValueCheck(const std::string& name,
                         const ClientStatus& result,
                         const std::string& value) {
    get_field_value_results_[name] = value;
  }

  BatchElementChecker::GetFieldValueCallback FieldValueCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnFieldValueCheck,
                          base::Unretained(this), name);
  }

  void OnDone(const std::string& name) { all_done_.insert(name); }

  base::OnceCallback<void()> DoneCallback(const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnDone,
                          base::Unretained(this), name);
  }

  // Runs a precondition given |exists_| and |value_match_|.
  void CheckElementCondition() {
    BatchElementChecker batch_checks;

    batch_checks.AddElementConditionCheck(condition_, mock_callback_.Get());
    batch_checks.Run(&mock_web_controller_);
  }

  void Run(const std::string& callback_name) {
    checks_.AddAllDoneCallback(DoneCallback(callback_name));
    checks_.Run(&mock_web_controller_);
  }

  ElementConditionProto Match(const Selector& selector, bool strict = false) {
    ElementConditionProto condition;
    *condition.mutable_match() = selector.proto;
    condition.set_require_unique_element(strict);
    return condition;
  }

  ElementConditionProto StrictMatch(const Selector& selector) {
    return Match(selector, /* strict= */ true);
  }

  MockWebController mock_web_controller_;
  BatchElementChecker checks_;
  base::flat_map<std::string, bool> element_exists_results_;
  base::flat_map<std::string, std::string> get_field_value_results_;
  base::flat_set<std::string> all_done_;
  base::MockCallback<base::OnceCallback<void(
      const ClientStatus&,
      const std::vector<std::string>&,
      const std::vector<std::string>&,
      const base::flat_map<std::string, DomObjectFrameStack>&)>>
      mock_callback_;
  ElementConditionProto condition_;
};

TEST_F(BatchElementCheckerTest, Empty) {
  EXPECT_TRUE(checks_.empty());
  checks_.AddElementConditionCheck(Match(Selector({"exists"})),
                                   ElementExistenceCallback("exists"));
  EXPECT_FALSE(checks_.empty());
  Run("all_done");
  EXPECT_THAT(all_done_, Contains("all_done"));
}

TEST_F(BatchElementCheckerTest, OneElementFound) {
  Selector expected_selector({"exists"});
  test_util::MockFindElement(mock_web_controller_, expected_selector);
  checks_.AddElementConditionCheck(Match(expected_selector),
                                   ElementExistenceCallback("exists"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("exists", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneElementNotFound) {
  Selector expected_notexists_selector({"does_not_exist"});
  checks_.AddElementConditionCheck(Match(expected_notexists_selector),
                                   ElementExistenceCallback("does_not_exist"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("does_not_exist", false)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, TooManyElementsForStrict) {
  Selector expected_multiple_selector({"multiple"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_multiple_selector, /* strict= */ true, _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(TOO_MANY_ELEMENTS), nullptr));
  checks_.AddElementConditionCheck(StrictMatch(expected_multiple_selector),
                                   ElementExistenceCallback("multiple"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("multiple", false)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueFound) {
  Selector expected_selector({"field"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "some value"));
  checks_.AddFieldValueCheck(expected_selector, FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "some value")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueNotFound) {
  Selector expected_selector({"field"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector, /* strict= */ true, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  checks_.AddFieldValueCheck(expected_selector, FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueEmpty) {
  Selector expected_selector({"field"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));
  checks_.AddFieldValueCheck(expected_selector, FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", std::string())));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, MultipleElements) {
  Selector expected_selector_1({"1"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_1);
  Selector expected_selector_2({"2"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_2);
  Selector expected_selector_3({"3"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector_3, /* strict= */ false, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  Selector expected_selector_4({"4"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector_4, /* strict= */ true, _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(TOO_MANY_ELEMENTS), nullptr));
  Selector expected_selector_5({"5"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector_5)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  Selector expected_selector_6({"6"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector_6)),
                            _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                   std::string()));

  checks_.AddElementConditionCheck(Match(expected_selector_1),
                                   ElementExistenceCallback("1"));
  checks_.AddElementConditionCheck(Match(expected_selector_2),
                                   ElementExistenceCallback("2"));
  checks_.AddElementConditionCheck(Match(expected_selector_3),
                                   ElementExistenceCallback("3"));
  checks_.AddElementConditionCheck(StrictMatch(expected_selector_4),
                                   ElementExistenceCallback("4"));
  checks_.AddFieldValueCheck(expected_selector_5, FieldValueCallback("5"));
  checks_.AddFieldValueCheck(expected_selector_6, FieldValueCallback("6"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("3", false)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("4", false)));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("5", "value")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("6", std::string())));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, DeduplicateElementExists) {
  Selector expected_selector_1({"1"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_1);
  Selector expected_selector_2({"2"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_2);
  Selector expected_selector_3({"3"});
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector_3, /* strict= */ true, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(expected_selector_3, /* strict= */ false, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  checks_.AddElementConditionCheck(Match(expected_selector_1),
                                   ElementExistenceCallback("first 1"));
  checks_.AddElementConditionCheck(Match(expected_selector_1),
                                   ElementExistenceCallback("second 1"));
  checks_.AddElementConditionCheck(Match(expected_selector_2),
                                   ElementExistenceCallback("2"));
  checks_.AddElementConditionCheck(StrictMatch(expected_selector_3),
                                   ElementExistenceCallback("first 3"));
  checks_.AddElementConditionCheck(Match(expected_selector_3),
                                   ElementExistenceCallback("second 3"));

  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("first 3", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("second 3", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, CallMultipleAllDoneCallbacks) {
  Selector expected_selector({"exists"});
  test_util::MockFindElement(mock_web_controller_, expected_selector);
  checks_.AddElementConditionCheck(Match(expected_selector),
                                   ElementExistenceCallback("exists"));
  checks_.AddAllDoneCallback(DoneCallback("1"));
  checks_.AddAllDoneCallback(DoneCallback("2"));
  checks_.AddAllDoneCallback(DoneCallback("3"));
  checks_.Run(&mock_web_controller_);
  EXPECT_THAT(all_done_, ElementsAre("1", "2", "3"));
}

TEST_F(BatchElementCheckerTest, IsElementConditionEmpty) {
  EXPECT_TRUE(BatchElementChecker::IsElementConditionEmpty(condition_));
}

TEST_F(BatchElementCheckerTest, NonEmpty) {
  *condition_.mutable_match() = ToSelectorProto("exists");
  EXPECT_FALSE(BatchElementChecker::IsElementConditionEmpty(condition_));
}

TEST_F(BatchElementCheckerTest, NoConditions) {
  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, EmptySelector) {
  condition_.mutable_match();

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, ElementExists) {
  *condition_.mutable_match() = ToSelectorProto("exists");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, ElementDoesNotExist) {
  *condition_.mutable_match() = ToSelectorProto("does_not_exist");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AnyOfEmpty) {
  condition_.mutable_any_of();

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AnyOfNoneMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AnyOfSomeMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AnyOfAllMatch) {
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_any_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AllOfEmpty) {
  condition_.mutable_all_of();

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AllOfNoneMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AllOfSomeMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, AllOfAllMatch) {
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_all_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, NoneOfEmpty) {
  condition_.mutable_none_of();

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, NoneOfNoneMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist_either");

  EXPECT_CALL(
      mock_callback_,
      Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, NoneOfSomeMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("does_not_exist");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, NoneOfAllMatch) {
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists");
  *condition_.mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("exists_too");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  _, _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, PayloadConditionMet) {
  auto* exists = condition_.mutable_any_of()->add_conditions();
  *exists->mutable_match() = ToSelectorProto("exists");
  exists->set_payload("exists");

  auto* exists_too = condition_.mutable_any_of()->add_conditions();
  *exists_too->mutable_match() = ToSelectorProto("exists_too");
  exists_too->set_payload("exists_too");

  condition_.set_payload("any_of");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED),
                  ElementsAre("exists", "exists_too", "any_of"), _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, PayloadConditionNotMet) {
  auto* exists = condition_.mutable_none_of()->add_conditions();
  *exists->mutable_match() = ToSelectorProto("exists");
  exists->set_payload("exists");

  auto* exists_too = condition_.mutable_none_of()->add_conditions();
  *exists_too->mutable_match() = ToSelectorProto("exists_too");
  exists_too->set_payload("exists_too");

  condition_.set_payload("none_of");

  EXPECT_CALL(mock_callback_, Run(Property(&ClientStatus::proto_status,
                                           ELEMENT_RESOLUTION_FAILED),
                                  ElementsAre("exists", "exists_too"), _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, Complex) {
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
                  ElementsAre("none_of", "exists in any_of", "any_of"), _, _));
  CheckElementCondition();
}

TEST_F(BatchElementCheckerTest, ReturnsFoundElements) {
  auto* exists = condition_.mutable_all_of()->add_conditions();
  *exists->mutable_match() = ToSelectorProto("exists");
  exists->set_payload("exists");
  exists->set_tag("exists_tag");
  exists->mutable_client_id()->set_identifier("exists");

  auto* exists_too = condition_.mutable_all_of()->add_conditions();
  *exists_too->mutable_match() = ToSelectorProto("exists_too");
  exists_too->set_payload("exists_too");
  exists_too->set_tag("exists_too_tag");
  exists_too->mutable_client_id()->set_identifier("exists_too");

  EXPECT_CALL(mock_callback_,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED),
                  ElementsAre("exists", "exists_too"),
                  ElementsAre("exists_tag", "exists_too_tag"),
                  UnorderedElementsAre(Key("exists"), Key("exists_too"))));
  CheckElementCondition();
}

}  // namespace
}  // namespace autofill_assistant

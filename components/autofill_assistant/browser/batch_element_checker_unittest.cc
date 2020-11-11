// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
using ::testing::InSequence;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

namespace autofill_assistant {

namespace {

class BatchElementCheckerTest : public testing::Test {
 protected:
  BatchElementCheckerTest() : checks_() {}

  void SetUp() override { test_util::MockFindAnyElement(mock_web_controller_); }

  void OnElementExistenceCheck(const std::string& name,
                               const ClientStatus& result,
                               const ElementFinder::Result& ignored_element) {
    element_exists_results_[name] = result.ok();
  }

  BatchElementChecker::ElementCheckCallback ElementExistenceCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnElementExistenceCheck,
                          base::Unretained(this), name);
  }

  void OnVisibilityRequirementCheck(
      const std::string& name,
      const ClientStatus& result,
      const ElementFinder::Result& ignored_element) {
    element_visible_results_[name] = result.ok();
  }

  BatchElementChecker::ElementCheckCallback VisibilityRequirementCallback(
      const std::string& name) {
    return base::BindOnce(
        &BatchElementCheckerTest::OnVisibilityRequirementCheck,
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

  void Run(const std::string& callback_name) {
    checks_.AddAllDoneCallback(DoneCallback(callback_name));
    checks_.Run(&mock_web_controller_);
  }

  MockWebController mock_web_controller_;
  BatchElementChecker checks_;
  std::map<std::string, bool> element_exists_results_;
  std::map<std::string, bool> element_visible_results_;
  std::map<std::string, std::string> get_field_value_results_;
  std::set<std::string> all_done_;
};

TEST_F(BatchElementCheckerTest, Empty) {
  EXPECT_TRUE(checks_.empty());
  checks_.AddElementCheck(Selector({"exists"}),
                          ElementExistenceCallback("exists"));
  EXPECT_FALSE(checks_.empty());
  Run("all_done");
  EXPECT_THAT(all_done_, Contains("all_done"));
}

TEST_F(BatchElementCheckerTest, OneElementFound) {
  Selector expected_selector({"exists"});
  test_util::MockFindElement(mock_web_controller_, expected_selector);
  checks_.AddElementCheck(expected_selector,
                          ElementExistenceCallback("exists"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("exists", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneElementNotFound) {
  Selector expected_notexists_selector({"does_not_exist"});
  EXPECT_CALL(mock_web_controller_,
              OnFindElement(expected_notexists_selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  checks_.AddElementCheck(expected_notexists_selector,
                          ElementExistenceCallback("does_not_exist"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("does_not_exist", false)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueFound) {
  Selector expected_selector({"field"});
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(EqualsElement(test_util::MockFindElement(
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
  EXPECT_CALL(mock_web_controller_, OnFindElement(expected_selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _)).Times(0);
  checks_.AddFieldValueCheck(expected_selector, FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueEmpty) {
  Selector expected_selector({"field"});
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(EqualsElement(test_util::MockFindElement(
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
  EXPECT_CALL(mock_web_controller_, OnFindElement(expected_selector_3, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  Selector expected_selector_4({"4"});
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(EqualsElement(test_util::MockFindElement(
                                  mock_web_controller_, expected_selector_4)),
                              _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  Selector expected_selector_5({"5"});
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(EqualsElement(test_util::MockFindElement(
                                  mock_web_controller_, expected_selector_5)),
                              _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                   std::string()));

  checks_.AddElementCheck(expected_selector_1, ElementExistenceCallback("1"));
  checks_.AddElementCheck(expected_selector_2, ElementExistenceCallback("2"));
  checks_.AddElementCheck(expected_selector_3, ElementExistenceCallback("3"));
  checks_.AddFieldValueCheck(expected_selector_4, FieldValueCallback("4"));
  checks_.AddFieldValueCheck(expected_selector_5, FieldValueCallback("5"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("3", false)));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("4", "value")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("5", std::string())));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, DeduplicateElementExists) {
  Selector expected_selector_1({"1"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_1);
  Selector expected_selector_2({"2"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_2);

  checks_.AddElementCheck(expected_selector_1,
                          ElementExistenceCallback("first 1"));
  checks_.AddElementCheck(expected_selector_1,
                          ElementExistenceCallback("second 1"));
  checks_.AddElementCheck(expected_selector_2, ElementExistenceCallback("2"));

  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, DeduplicateElementVisible) {
  Selector expected_selector_1({"1"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_1);
  Selector expected_selector_2({"2"});
  test_util::MockFindElement(mock_web_controller_, expected_selector_2);

  checks_.AddElementCheck(expected_selector_1,
                          VisibilityRequirementCallback("first 1"));
  checks_.AddElementCheck(expected_selector_1,
                          VisibilityRequirementCallback("second 1"));
  checks_.AddElementCheck(expected_selector_2,
                          VisibilityRequirementCallback("2"));

  Run("was_run");

  EXPECT_THAT(element_visible_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_visible_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_visible_results_, Contains(Pair("2", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, CallMultipleAllDoneCallbacks) {
  Selector expected_selector({"exists"});
  test_util::MockFindElement(mock_web_controller_, expected_selector);
  checks_.AddElementCheck(expected_selector,
                          ElementExistenceCallback("exists"));
  checks_.AddAllDoneCallback(DoneCallback("1"));
  checks_.AddAllDoneCallback(DoneCallback("2"));
  checks_.AddAllDoneCallback(DoneCallback("3"));
  checks_.Run(&mock_web_controller_);
  EXPECT_THAT(all_done_, ElementsAre("1", "2", "3"));
}

// Deduplicate get field

}  // namespace
}  // namespace autofill_assistant

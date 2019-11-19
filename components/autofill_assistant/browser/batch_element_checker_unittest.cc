// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
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

  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));
    ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus(), ""));
  }

  void OnElementExistenceCheck(const std::string& name,
                               const ClientStatus& result) {
    element_exists_results_[name] = result.ok();
  }

  BatchElementChecker::ElementCheckCallback ElementExistenceCallback(
      const std::string& name) {
    return base::BindOnce(&BatchElementCheckerTest::OnElementExistenceCheck,
                          base::Unretained(this), name);
  }

  void OnVisibilityRequirementCheck(const std::string& name,
                                    const ClientStatus& result) {
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
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"exists"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  checks_.AddElementCheck(Selector({"exists"}),
                          ElementExistenceCallback("exists"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("exists", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneElementNotFound) {
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"does_not_exist"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  checks_.AddElementCheck(Selector({"does_not_exist"}),
                          ElementExistenceCallback("does_not_exist"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("does_not_exist", false)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueFound) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"field"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "some value"));
  checks_.AddFieldValueCheck(Selector({"field"}), FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "some value")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueNotFound) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"field"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(), ""));
  checks_.AddFieldValueCheck(Selector({"field"}), FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, OneFieldValueEmpty) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"field"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  checks_.AddFieldValueCheck(Selector({"field"}), FieldValueCallback("field"));
  Run("was_run");

  EXPECT_THAT(get_field_value_results_, Contains(Pair("field", "")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, MultipleElements) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"1"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"2"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"3"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"4"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"5"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(), ""));

  checks_.AddElementCheck(Selector({"1"}), ElementExistenceCallback("1"));
  checks_.AddElementCheck(Selector({"2"}), ElementExistenceCallback("2"));
  checks_.AddElementCheck(Selector({"3"}), ElementExistenceCallback("3"));
  checks_.AddFieldValueCheck(Selector({"4"}), FieldValueCallback("4"));
  checks_.AddFieldValueCheck(Selector({"5"}), FieldValueCallback("5"));
  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("3", false)));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("4", "value")));
  EXPECT_THAT(get_field_value_results_, Contains(Pair("5", "")));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, DeduplicateElementExists) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"1"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"2"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  checks_.AddElementCheck(Selector({"1"}), ElementExistenceCallback("first 1"));
  checks_.AddElementCheck(Selector({"1"}),
                          ElementExistenceCallback("second 1"));
  checks_.AddElementCheck(Selector({"2"}), ElementExistenceCallback("2"));

  Run("was_run");

  EXPECT_THAT(element_exists_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_exists_results_, Contains(Pair("2", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, DeduplicateElementVisible) {
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"2"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  checks_.AddElementCheck(Selector({"1"}).MustBeVisible(),
                          VisibilityRequirementCallback("first 1"));
  checks_.AddElementCheck(Selector({"1"}).MustBeVisible(),
                          VisibilityRequirementCallback("second 1"));
  checks_.AddElementCheck(Selector({"2"}).MustBeVisible(),
                          VisibilityRequirementCallback("2"));

  Run("was_run");

  EXPECT_THAT(element_visible_results_, Contains(Pair("first 1", true)));
  EXPECT_THAT(element_visible_results_, Contains(Pair("second 1", true)));
  EXPECT_THAT(element_visible_results_, Contains(Pair("2", true)));
  EXPECT_THAT(all_done_, Contains("was_run"));
}

TEST_F(BatchElementCheckerTest, CallMultipleAllDoneCallbacks) {
  checks_.AddElementCheck(Selector({"exists"}),
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

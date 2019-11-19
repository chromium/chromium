// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
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
using ::testing::Eq;

class ElementPreconditionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"exists"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"empty"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_,
            OnElementCheck(Eq(Selector({"does_not_exist"})), _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));

    ON_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"exists"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "foo"));
    ON_CALL(mock_web_controller_,
            OnGetFieldValue(Eq(Selector({"does_not_exist"})), _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus(), ""));
    ON_CALL(mock_web_controller_, OnGetFieldValue(Eq(Selector({"empty"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
  }

 protected:
  // Runs a precondition given |exists_| and |value_match_|.
  void Check(base::OnceCallback<void(bool)> callback) {
    ElementPrecondition precondition(exist_, value_match_);
    BatchElementChecker batch_checks;
    precondition.Check(&batch_checks, std::move(callback));
    batch_checks.Run(&mock_web_controller_);
  }

  MockWebController mock_web_controller_;
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback_;
  google::protobuf::RepeatedPtrField<ElementReferenceProto> exist_;
  google::protobuf::RepeatedPtrField<FormValueMatchProto> value_match_;
};

TEST_F(ElementPreconditionTest, Empty) {
  EXPECT_TRUE(ElementPrecondition(exist_, value_match_).empty());
}

TEST_F(ElementPreconditionTest, NonEmpty) {
  exist_.Add()->add_selectors("exists");
  EXPECT_FALSE(ElementPrecondition(exist_, value_match_).empty());
}

TEST_F(ElementPreconditionTest, NoConditions) {
  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, EmptySelector) {
  exist_.Add();

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, ElementExists) {
  exist_.Add()->add_selectors("exists");

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, ElementDoesNotExist) {
  exist_.Add()->add_selectors("does_not_exist");

  EXPECT_CALL(mock_callback_, Run(false));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueMatchDoesNotExist) {
  value_match_.Add()->mutable_element()->add_selectors("does_not_exist");

  EXPECT_CALL(mock_callback_, Run(false));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueMatchNonEmpty) {
  value_match_.Add()->mutable_element()->add_selectors("exists");

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueShouldNotMatchEmpty) {
  value_match_.Add()->mutable_element()->add_selectors("empty");

  EXPECT_CALL(mock_callback_, Run(false));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueShouldMatchEmpty) {
  auto* match = value_match_.Add();
  match->mutable_element()->add_selectors("empty");
  match->set_value("");

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueMatchWrongValue) {
  auto* match = value_match_.Add();
  match->mutable_element()->add_selectors("exists");
  match->set_value("wrong");

  EXPECT_CALL(mock_callback_, Run(false));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, FormValueMatchCorrectValue) {
  auto* match = value_match_.Add();
  match->mutable_element()->add_selectors("exists");
  match->set_value("foo");

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, SomeMatch) {
  exist_.Add()->add_selectors("exists");
  exist_.Add()->add_selectors("does_not_exist");

  value_match_.Add()->mutable_element()->add_selectors("empty");
  auto* match = value_match_.Add();
  match->mutable_element()->add_selectors("exists");
  match->set_value("wrong");

  EXPECT_CALL(mock_callback_, Run(false));
  Check(mock_callback_.Get());
}

TEST_F(ElementPreconditionTest, AllMatch) {
  exist_.Add()->add_selectors("exists");
  exist_.Add()->add_selectors("empty");

  auto* match_exists = value_match_.Add();
  match_exists->mutable_element()->add_selectors("exists");
  match_exists->set_value("foo");

  auto* match_empty = value_match_.Add();
  match_empty->mutable_element()->add_selectors("empty");
  match_empty->set_value("");

  EXPECT_CALL(mock_callback_, Run(true));
  Check(mock_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant

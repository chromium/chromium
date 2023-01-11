// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status_macros.h"

#include <stdio.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

Status StatusTestFunction(bool fail) {
  if (fail) {
    return Status(error::INVALID_ARGUMENT, "Fail was true.");
  }
  return Status::StatusOK();
}

Status ReturnIfErrorWrapperFunction(bool fail) {
  RETURN_IF_ERROR(StatusTestFunction(fail));

  // Return error here to make sure that we aren't just returning the OK from
  // StatusTestFunction.
  return Status(error::INTERNAL, "Returning Internal Error");
}

// RETURN_IF_ERROR macro actually returns on a non-OK status.
TEST(StatusMacros, ReturnsOnError) {
  Status test_status = ReturnIfErrorWrapperFunction(/*fail=*/true);
  EXPECT_FALSE(test_status.ok());
  EXPECT_EQ(test_status.code(), error::INVALID_ARGUMENT);
}

// RETURN_IF_ERROR macro continues on an OK status.
TEST(StatusMacros, ReturnIfErrorContinuesOnOk) {
  Status test_status = ReturnIfErrorWrapperFunction(/*fail=*/false);
  EXPECT_FALSE(test_status.ok());
  EXPECT_EQ(test_status.code(), error::INTERNAL);
}

// Function to test StatusOr macros.
template <typename T>
StatusOr<T> StatusOrTestFunction(bool fail, T return_value) {
  if (fail) {
    return Status(error::INVALID_ARGUMENT, "Test failure requested.");
  }
  return std::forward<T>(return_value);
}

// Function for testing ASSIGN_OR_RETURN.
template <typename T>
StatusOr<T> AssignOrReturnWrapperFunction(bool fail, T return_value) {
  ASSIGN_OR_RETURN(T value,
                   StatusOrTestFunction(fail, std::forward<T>(return_value)));
  return std::forward<T>(value);
}

// ASSIGN_OR_RETURN actually assigns the value if the status is OK.
TEST(StatusMacros, AssignOnOk) {
  constexpr int kReturnValue = 42;

  StatusOr<int> status_or_result =
      AssignOrReturnWrapperFunction(/*fail=*/false, kReturnValue);

  ASSERT_TRUE(status_or_result.ok());
  EXPECT_EQ(status_or_result.ValueOrDie(), kReturnValue);
}

// ASSIGN_OR_RETURN actually returns on a non-OK status.
TEST(StatusMacros, ReturnOnError) {
  StatusOr<int> status_or_result =
      AssignOrReturnWrapperFunction(/*fail=*/true, /*return_value=*/0);
  EXPECT_FALSE(status_or_result.ok());
}

StatusOr<int> MultipleAssignOrReturnWrapperFunction(int return_value) {
  bool fail = false;
  int value;
  ASSIGN_OR_RETURN(value, StatusOrTestFunction(fail, return_value));
  ASSIGN_OR_RETURN(value, StatusOrTestFunction(fail, return_value));
  ASSIGN_OR_RETURN(value, StatusOrTestFunction(fail, return_value));
  ASSIGN_OR_RETURN(value, StatusOrTestFunction(fail, return_value));
  return value;
}

// ASSIGN_OR_RETURN compiles when used multiple times.
TEST(StatusMacros, MultipleAssignsSucceed) {
  constexpr int kReturnValue = 42;
  StatusOr<int> status_or_result =
      MultipleAssignOrReturnWrapperFunction(kReturnValue);
  ASSERT_TRUE(status_or_result.ok());
  EXPECT_EQ(status_or_result.ValueOrDie(), kReturnValue);
}

// ASSIGN_OR_RETURN actually moves the value if the status is OK.
TEST(StatusMacros, AssignOnOkMoveable) {
  constexpr int kReturnValue = 42;

  StatusOr<std::unique_ptr<int>> status_or_result =
      AssignOrReturnWrapperFunction(/*fail=*/false,
                                    std::make_unique<int>(kReturnValue));

  ASSERT_TRUE(status_or_result.ok());
  EXPECT_EQ(*status_or_result.ValueOrDie(), kReturnValue);
}

// ASSIGN_OR_RETURN actually returns on a non-OK status.
TEST(StatusMacros, ReturnOnErrorMoveable) {
  StatusOr<std::unique_ptr<int>> status_or_result =
      AssignOrReturnWrapperFunction(/*fail=*/true,
                                    std::make_unique<int>(/*return_value=*/0));
  EXPECT_FALSE(status_or_result.ok());
}

// ASSIGN_OR_ONCE_CALLBACK_AND_RETURN testing
void AssignOrOnceCallbackWrapperFunction(
    bool fail,
    base::OnceCallback<void(Status)> callback) {
  constexpr int kReturnValue = 42;
  int value;
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(value, callback,
                                     StatusOrTestFunction(fail, kReturnValue));
  ASSERT_EQ(value, kReturnValue);
}

class CallbackTestClass {
 public:
  explicit CallbackTestClass(Status test_status) : test_status_(test_status) {}

  void AssignInCallback(Status status) {
    num_callback_invocations_++;
    test_status_ = status;
  }

  int num_callback_invocations() { return num_callback_invocations_; }
  Status status() { return test_status_; }

 private:
  Status test_status_;
  int num_callback_invocations_ = 0;
};

// ASSIGN_OR_ONCE_CALLBACK_AND_RETURN assigns on OK error.
TEST(StatusMacros, OnceCallbackAssignOnOk) {
  CallbackTestClass callback_test_class(Status::StatusOK());

  base::OnceCallback<void(Status)> callback =
      base::BindOnce(&CallbackTestClass::AssignInCallback,
                     base::Unretained(&callback_test_class));

  AssignOrOnceCallbackWrapperFunction(/*fail=*/false, std::move(callback));

  constexpr int kExpectedNumberOfCallbackInvocations = 0;
  EXPECT_EQ(callback_test_class.num_callback_invocations(),
            kExpectedNumberOfCallbackInvocations);
  EXPECT_EQ(callback_test_class.status(), Status::StatusOK());
}

// ASSIGN_OR_ONCE_CALLBACK_AND_RETURN calls the callback and returns on non-OK
// status.
TEST(StatusMacros, OnceCallbackAndReturnOnError) {
  CallbackTestClass callback_test_class(Status::StatusOK());

  base::OnceCallback<void(Status)> callback =
      base::BindOnce(&CallbackTestClass::AssignInCallback,
                     base::Unretained(&callback_test_class));

  AssignOrOnceCallbackWrapperFunction(/*fail=*/true, std::move(callback));

  constexpr int kExpectedNumberOfCallbackInvocations = 1;
  EXPECT_EQ(callback_test_class.num_callback_invocations(),
            kExpectedNumberOfCallbackInvocations);
  EXPECT_EQ(callback_test_class.status().code(), error::INVALID_ARGUMENT);
}

void MultipleAssignOrOnceCallbackWrapperFunction(
    base::OnceCallback<void(Status)> callback) {
  constexpr int kReturnValue = 42;
  constexpr bool kFail = false;

  int value;
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(value, callback,
                                     StatusOrTestFunction(kFail, kReturnValue));
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(value, callback,
                                     StatusOrTestFunction(kFail, kReturnValue));
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(value, callback,
                                     StatusOrTestFunction(kFail, kReturnValue));
  ASSERT_EQ(value, kReturnValue);
}

// ASSIGN_OR_ONCE_CALLBACK_AND_RETURN can be used multiple times in a function.
TEST(StatusMacros, MultipleAssignOrOnceCallbackCompletes) {
  CallbackTestClass callback_test_class(Status::StatusOK());

  base::OnceCallback<void(Status)> callback =
      base::BindOnce(&CallbackTestClass::AssignInCallback,
                     base::Unretained(&callback_test_class));

  MultipleAssignOrOnceCallbackWrapperFunction(std::move(callback));

  constexpr int kExpectedNumberOfCallbackInvocations = 0;
  EXPECT_EQ(callback_test_class.num_callback_invocations(),
            kExpectedNumberOfCallbackInvocations);
  EXPECT_EQ(callback_test_class.status(), Status::StatusOK());
}

}  // namespace
}  // namespace reporting

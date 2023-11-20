// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status_macros.h"

#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

Status StatusTestFunction(bool fail) {
  if (fail) {
    return Status(error::INVALID_ARGUMENT, "Fail was true.");
  }
  return Status::StatusOK();
}

Status ReturnIfErrorStatusWrapperFunction(bool fail) {
  RETURN_IF_ERROR_STATUS(StatusTestFunction(fail));

  // Return error here to make sure that we aren't just returning the OK from
  // StatusTestFunction.
  return Status(error::INTERNAL, "Returning Internal Error");
}

// RETURN_IF_ERROR_STATUS macro actually returns on a non-OK status.
TEST(StatusMacros, ReturnsOnErrorStatus) {
  Status test_status = ReturnIfErrorStatusWrapperFunction(/*fail=*/true);
  EXPECT_FALSE(test_status.ok());
  EXPECT_EQ(test_status.code(), error::INVALID_ARGUMENT);
}

// RETURN_IF_ERROR_STATUS macro continues on an OK status.
TEST(StatusMacros, ReturnIfErrorStatusContinuesOnOk) {
  Status test_status = ReturnIfErrorStatusWrapperFunction(/*fail=*/false);
  EXPECT_FALSE(test_status.ok());
  EXPECT_EQ(test_status.code(), error::INTERNAL);
}

base::unexpected<Status> UnexpectedStatusTestFunction(bool fail) {
  if (fail) {
    return base::unexpected(Status(error::INVALID_ARGUMENT, "Fail was true."));
  }
  return base::unexpected(Status::StatusOK());
}

StatusOr<int> ReturnIfErrorUnexpectedStatusWrapperFunction(bool fail) {
  RETURN_IF_ERROR_STATUS(UnexpectedStatusTestFunction(fail));

  // Return error here to make sure that we aren't just returning the OK from
  // UnexpectedStatusTestFunction.
  return base::unexpected(Status(error::INTERNAL, "Returning Internal Error"));
}

// RETURN_IF_ERROR_STATUS macro actually returns on a non-OK status.
TEST(StatusMacros, ReturnsOnErrorUnexpectedStatus) {
  StatusOr<int> test_status =
      ReturnIfErrorUnexpectedStatusWrapperFunction(/*fail=*/true);
  EXPECT_FALSE(test_status.has_value());
  EXPECT_EQ(test_status.error().code(), error::INVALID_ARGUMENT);
}

// RETURN_IF_ERROR_STATUS macro continues on an OK status.
TEST(StatusMacros, ReturnIfErrorUnexpectedStatusContinuesOnOk) {
  StatusOr<int> test_status =
      ReturnIfErrorUnexpectedStatusWrapperFunction(/*fail=*/false);
  EXPECT_FALSE(test_status.has_value());
  EXPECT_EQ(test_status.error().code(), error::INTERNAL);
}

TEST(StatusMacros, CheckOKOnStatus) {
  const auto ok_status = Status::StatusOK();
  CHECK_OK(ok_status);
  CHECK_OK(ok_status) << "error message";
  // rvalue
  CHECK_OK(Status::StatusOK());
  // Can't check on error status here because CHECK does not use gtest
  // utilities.
}

TEST(StatusMacros, DCheckOKOnStatus) {
  const auto ok_status = Status::StatusOK();
  DCHECK_OK(ok_status);
  DCHECK_OK(ok_status) << "error message";
  // rvalue
  DCHECK_OK(Status::StatusOK());
  // Can't check on error status here because DCHECK does not use gtest
  // utilities.
}

void AssertOKErrorStatus() {
  ASSERT_OK(Status(error::INTERNAL, ""));
}

TEST(StatusMacros, AssertOKOnStatus) {
  const auto ok_status = Status::StatusOK();
  ASSERT_OK(ok_status);
  ASSERT_OK(ok_status) << "error message";
  // rvalue
  ASSERT_OK(Status::StatusOK());
  EXPECT_FATAL_FAILURE(AssertOKErrorStatus(), "error::INTERNAL");
}

void ExpectOKErrorStatus() {
  EXPECT_OK(Status(error::INTERNAL, ""));
}

TEST(StatusMacros, ExpectOKOnStatus) {
  EXPECT_OK(Status::StatusOK());
  EXPECT_OK(Status::StatusOK()) << "error message";
  EXPECT_NONFATAL_FAILURE(ExpectOKErrorStatus(), "error::INTERNAL");
}

TEST(StatusMacros, CheckOKOnStatusOr) {
  StatusOr<int> status_or(2);
  CHECK_OK(status_or);
  CHECK_OK(status_or) << "error message";
  // rvalue
  CHECK_OK(StatusOr<int>(2));
  // Can't check on error status here because CHECK does not use gtest
  // utilities.
}

TEST(StatusMacros, DCheckOKOnStatusOr) {
  StatusOr<int> status_or(2);
  DCHECK_OK(status_or);
  DCHECK_OK(status_or) << "error message";
  // rvalue
  DCHECK_OK(StatusOr<int>(2));
  // Can't check on error status here because DCHECK does not use gtest
  // utilities.
}

void AssertOKErrorStatusOr() {
  ASSERT_OK(StatusOr<int>(base::unexpected(Status(error::INTERNAL, ""))));
}

TEST(StatusMacros, AssertOKOnStatusOr) {
  StatusOr<int> status_or(2);
  ASSERT_OK(status_or);
  ASSERT_OK(status_or) << "error message";
  // rvalue
  ASSERT_OK(StatusOr<int>(2));
  EXPECT_FATAL_FAILURE(AssertOKErrorStatusOr(), "error::INTERNAL");
}

void ExpectOKErrorStatusOr() {
  EXPECT_OK(StatusOr<int>(base::unexpected(Status(error::INTERNAL, ""))));
}

TEST(StatusMacros, ExpectOKOnStatusOr) {
  StatusOr<int> status_or(2);
  EXPECT_OK(status_or);
  EXPECT_OK(status_or) << "error message";
  // rvalue
  EXPECT_OK(StatusOr<int>(2));
  EXPECT_NONFATAL_FAILURE(ExpectOKErrorStatusOr(), "error::INTERNAL");
}
}  // namespace
}  // namespace reporting

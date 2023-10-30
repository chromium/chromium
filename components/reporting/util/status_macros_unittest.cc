// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status_macros.h"

#include "base/types/expected.h"
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

}  // namespace
}  // namespace reporting

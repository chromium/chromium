// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(SyncErrorTest, ModelError) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  SyncError error(location, SyncError::MODEL_ERROR, msg);
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("model error was encountered: ", error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
}

TEST(SyncErrorTest, PreconditionError) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  SyncError error(location, SyncError::PRECONDITION_ERROR_WITH_CLEAR_DATA, msg);
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("failed precondition was encountered with clear data: ",
            error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
}

}  // namespace

}  // namespace syncer

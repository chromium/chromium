// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using std::string;
using ::testing::HasSubstr;

using SyncErrorTest = ::testing::Test;

TEST_F(SyncErrorTest, Unset) {
  SyncError error;
  EXPECT_FALSE(error.IsSet());
}

TEST_F(SyncErrorTest, Default) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  DataType type = PREFERENCES;
  SyncError error(location, SyncError::MODEL_ERROR, msg, type);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("model error was encountered: ", error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.data_type());
}

TEST_F(SyncErrorTest, PolicyError) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  DataType type = PREFERENCES;
  SyncError error(location, SyncError::DATATYPE_POLICY_ERROR, msg, type);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("disabled due to configuration constraints: ",
            error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.data_type());
}

TEST_F(SyncErrorTest, ToString) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  DataType type = PREFERENCES;
  std::string expected = std::string(DataTypeToDebugString(type)) +
                         " model error was encountered: " + msg;
  LOG(INFO) << "Expect " << expected;
  SyncError error(location, SyncError::MODEL_ERROR, msg, type);
  EXPECT_TRUE(error.IsSet());
  EXPECT_THAT(error.ToString(), HasSubstr(expected));

  SyncError error2;
  EXPECT_FALSE(error2.IsSet());
  EXPECT_EQ(std::string(), error2.ToString());

  error2 = error;
  EXPECT_TRUE(error2.IsSet());
  EXPECT_THAT(error.ToString(), HasSubstr(expected));
}

}  // namespace

}  // namespace syncer

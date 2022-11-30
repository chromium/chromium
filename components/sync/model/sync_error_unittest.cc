// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_error.h"

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
  ModelType type = PREFERENCES;
  SyncError error(location, SyncError::DATATYPE_ERROR, msg, type);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("datatype error was encountered: ", error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.model_type());
  EXPECT_EQ(SyncError::SYNC_ERROR_SEVERITY_ERROR, error.GetSeverity());
}

TEST_F(SyncErrorTest, LowSeverity) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  ModelType type = PREFERENCES;
  SyncError error(location, SyncError::DATATYPE_POLICY_ERROR, msg, type);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("disabled due to configuration constraints: ",
            error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.model_type());
  EXPECT_EQ(SyncError::SYNC_ERROR_SEVERITY_INFO, error.GetSeverity());
}

TEST_F(SyncErrorTest, Reset) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  ModelType type = PREFERENCES;

  SyncError error;
  EXPECT_FALSE(error.IsSet());

  error.Reset(location, msg, type);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.model_type());

  base::Location location2 = FROM_HERE;
  std::string msg2 = "test";
  ModelType type2 = PREFERENCES;
  error.Reset(location2, msg2, type2);
  ASSERT_TRUE(error.IsSet());
  EXPECT_EQ(location2.line_number(), error.location().line_number());
  EXPECT_EQ(msg2, error.message());
  EXPECT_EQ(type2, error.model_type());
}

TEST_F(SyncErrorTest, Copy) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  ModelType type = PREFERENCES;

  SyncError error1;
  EXPECT_FALSE(error1.IsSet());
  SyncError error2(error1);
  EXPECT_FALSE(error2.IsSet());

  error1.Reset(location, msg, type);
  ASSERT_TRUE(error1.IsSet());
  EXPECT_EQ(location.line_number(), error1.location().line_number());
  EXPECT_EQ(msg, error1.message());
  EXPECT_EQ(type, error1.model_type());

  SyncError error3(error1);
  ASSERT_TRUE(error3.IsSet());
  EXPECT_EQ(error1.location().line_number(), error3.location().line_number());
  EXPECT_EQ(error1.message(), error3.message());
  EXPECT_EQ(error1.model_type(), error3.model_type());

  SyncError error4;
  EXPECT_FALSE(error4.IsSet());
  SyncError error5(error4);
  EXPECT_FALSE(error5.IsSet());
}

TEST_F(SyncErrorTest, Assign) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  ModelType type = PREFERENCES;

  SyncError error1;
  EXPECT_FALSE(error1.IsSet());
  SyncError error2;
  error2 = error1;
  EXPECT_FALSE(error2.IsSet());

  error1.Reset(location, msg, type);
  ASSERT_TRUE(error1.IsSet());
  EXPECT_EQ(location.line_number(), error1.location().line_number());
  EXPECT_EQ(msg, error1.message());
  EXPECT_EQ(type, error1.model_type());

  error2 = error1;
  ASSERT_TRUE(error2.IsSet());
  EXPECT_EQ(error1.location().line_number(), error2.location().line_number());
  EXPECT_EQ(error1.message(), error2.message());
  EXPECT_EQ(error1.model_type(), error2.model_type());

  error2 = SyncError();
  EXPECT_FALSE(error2.IsSet());
}

TEST_F(SyncErrorTest, ToString) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  ModelType type = PREFERENCES;
  std::string expected = std::string(ModelTypeToDebugString(type)) +
                         " datatype error was encountered: " + msg;
  LOG(INFO) << "Expect " << expected;
  SyncError error(location, SyncError::DATATYPE_ERROR, msg, type);
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

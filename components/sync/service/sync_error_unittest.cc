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
  DataType type = PREFERENCES;
  SyncError error(location, SyncError::MODEL_ERROR, msg, type);
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("model error was encountered: ", error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.data_type());
}

TEST(SyncErrorTest, PolicyError) {
  base::Location location = FROM_HERE;
  std::string msg = "test";
  DataType type = PREFERENCES;
  SyncError error(location, SyncError::DATATYPE_POLICY_ERROR, msg, type);
  EXPECT_EQ(location.line_number(), error.location().line_number());
  EXPECT_EQ("disabled due to configuration constraints: ",
            error.GetMessagePrefix());
  EXPECT_EQ(msg, error.message());
  EXPECT_EQ(type, error.data_type());
}

}  // namespace

}  // namespace syncer

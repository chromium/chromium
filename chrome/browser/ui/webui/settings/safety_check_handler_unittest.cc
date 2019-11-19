// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "testing/gtest/include/gtest/gtest.h"

class SafetyCheckHandlerTest : public ::testing::Test {
 public:
  void Callback(VersionUpdater::Status status,
                int progress,
                bool rollback,
                const std::string& version,
                int64_t update_size,
                const base::string16& message) {
    callback_invoked_ = true;
    result_ = status;
  }

 protected:
  bool callback_invoked_ = false;
  VersionUpdater::Status result_;
  TestVersionUpdater version_updater_;
  SafetyCheckHandler safety_check_;
};

TEST_F(SafetyCheckHandlerTest, CheckUpdatesUpdated) {
  version_updater_.SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_.CheckUpdates(
      &version_updater_,
      base::Bind(&SafetyCheckHandlerTest::Callback, base::Unretained(this)));
  ASSERT_TRUE(callback_invoked_);
  EXPECT_EQ(VersionUpdater::Status::UPDATED, result_);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdatesNotUpdated) {
  version_updater_.SetReturnedStatus(VersionUpdater::Status::DISABLED);
  safety_check_.CheckUpdates(
      &version_updater_,
      base::Bind(&SafetyCheckHandlerTest::Callback, base::Unretained(this)));
  ASSERT_TRUE(callback_invoked_);
  EXPECT_EQ(VersionUpdater::Status::DISABLED, result_);
}

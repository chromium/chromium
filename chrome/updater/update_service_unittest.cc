// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service.h"

#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests value parity between UpdateService::Result and UpdateService::Result.
TEST(UpdateServiceTest, ResultEnumTest) {
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kSuccess),
            static_cast<int>(update_client::Error::NONE));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kUpdateInProgress),
            static_cast<int>(update_client::Error::UPDATE_IN_PROGRESS));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kUpdateCanceled),
            static_cast<int>(update_client::Error::UPDATE_CANCELED));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kRetryLater),
            static_cast<int>(update_client::Error::RETRY_LATER));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kServiceFailed),
            static_cast<int>(update_client::Error::SERVICE_ERROR));
  EXPECT_EQ(
      static_cast<int>(updater::UpdateService::Result::kUpdateCheckFailed),
      static_cast<int>(update_client::Error::UPDATE_CHECK_ERROR));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kAppNotFound),
            static_cast<int>(update_client::Error::CRX_NOT_FOUND));
  EXPECT_EQ(static_cast<int>(updater::UpdateService::Result::kInvalidArgument),
            static_cast<int>(update_client::Error::INVALID_ARGUMENT));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl.h"

#include <string>

#include "chrome/updater/update_service.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::internal {

TEST(UpdateServiceImplTest, TestToResult) {
  EXPECT_EQ(ToResult(update_client::Error::NONE),
            UpdateService::Result::kSuccess);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_IN_PROGRESS),
            UpdateService::Result::kUpdateInProgress);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CANCELED),
            UpdateService::Result::kUpdateCanceled);
  EXPECT_EQ(ToResult(update_client::Error::RETRY_LATER),
            UpdateService::Result::kRetryLater);
  EXPECT_EQ(ToResult(update_client::Error::SERVICE_ERROR),
            UpdateService::Result::kServiceFailed);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CHECK_ERROR),
            UpdateService::Result::kUpdateCheckFailed);
  EXPECT_EQ(ToResult(update_client::Error::CRX_NOT_FOUND),
            UpdateService::Result::kAppNotFound);
  EXPECT_EQ(ToResult(update_client::Error::INVALID_ARGUMENT),
            UpdateService::Result::kInvalidArgument);
  EXPECT_EQ(ToResult(update_client::Error::BAD_CRX_DATA_CALLBACK),
            UpdateService::Result::kInvalidArgument);
}

}  // namespace updater::internal

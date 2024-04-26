// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/install_result_code.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

TEST(InstallResultCodeTest, IsSuccess) {
  // TODO(crbug.com/40821686): Test the rest of the constants.
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessNewInstall));
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessAlreadyInstalled));

  EXPECT_FALSE(IsSuccess(InstallResultCode::kExpectedAppIdCheckFailed));
}

}  // namespace webapps

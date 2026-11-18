// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_util.h"

#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(CloudPolicyUtilTest, GetNetworkNameEmptyOnWin11_24H2OrGreater) {
  if (base::win::GetVersion() < base::win::Version::WIN11_24H2) {
    GTEST_SKIP() << "Only applies to Windows 11 24H2+.";
  }
  EXPECT_TRUE(GetNetworkName().empty());
}

}  // namespace policy

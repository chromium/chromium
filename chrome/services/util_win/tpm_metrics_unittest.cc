// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/tpm_metrics.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

using TpmIdentifier = metrics::SystemProfileProto_TpmIdentifier;

TEST(TpmIdentifierTest, TpmTest) {
  // Ensure that each value has been populated
  std::optional<TpmIdentifier> identifier = GetTpmIdentifier();
  EXPECT_TRUE(identifier->tpm_specific_version() != "");
  EXPECT_TRUE(identifier->manufacturer_version() != "");
  EXPECT_TRUE(identifier->manufacturer_version_info() != "");
  EXPECT_TRUE(identifier->manufacturer_id() != 0u);
}

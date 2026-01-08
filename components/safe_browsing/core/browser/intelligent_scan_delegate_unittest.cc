// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(IntelligentScanDelegateTest, IsIntelligentScanAvailable) {
  EXPECT_FALSE(IntelligentScanDelegate::IsIntelligentScanAvailable(
      IntelligentScanDelegate::ModelType::kNotSupportedOnDevice));
  EXPECT_FALSE(IntelligentScanDelegate::IsIntelligentScanAvailable(
      IntelligentScanDelegate::ModelType::kNotSupportedServerSide));
  EXPECT_TRUE(IntelligentScanDelegate::IsIntelligentScanAvailable(
      IntelligentScanDelegate::ModelType::kOnDevice));
  EXPECT_TRUE(IntelligentScanDelegate::IsIntelligentScanAvailable(
      IntelligentScanDelegate::ModelType::kServerSide));
}

}  // namespace safe_browsing

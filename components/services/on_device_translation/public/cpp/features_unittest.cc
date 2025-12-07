// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/public/cpp/features.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

TEST(IsValidTranslateKitVersionTest, InvalidEmptyVersion) {
  EXPECT_FALSE(IsValidTranslateKitVersion(""));
}

TEST(IsValidTranslateKitVersionTest, InvalidZeroVersion) {
  EXPECT_FALSE(IsValidTranslateKitVersion("0.0.0.0"));
}

TEST(IsValidTranslateKitVersionTest, InvalidWildcardVersion) {
  EXPECT_FALSE(IsValidTranslateKitVersion("*"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.*"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.*"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.10.*"));
}

TEST(IsValidTranslateKitVersionTest, InvalidTruncatedVersion) {
  EXPECT_FALSE(IsValidTranslateKitVersion("0.0.0"));

  EXPECT_FALSE(IsValidTranslateKitVersion("2024.1.1"));

  EXPECT_FALSE(IsValidTranslateKitVersion("2"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2024"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025"));

  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.1"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.10"));

  EXPECT_FALSE(IsValidTranslateKitVersion("2026"));
}

TEST(IsValidTranslateKitVersionTest, InvalidLowerThanMinimumVersion) {
  EXPECT_FALSE(IsValidTranslateKitVersion("2024.1.1.1"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2024.9.9.9"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2024.10.10.10"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2024.11.11.11"));

  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.1.1"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.9.9"));
  EXPECT_FALSE(IsValidTranslateKitVersion("2025.1.9.10"));
}

TEST(IsValidTranslateKitVersionTest, MinimumVersion) {
  EXPECT_TRUE(IsValidTranslateKitVersion("2025.1.10.0"));
}

TEST(IsValidTranslateKitVersionTest, GreaterThanMinimumVersion) {
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.1.0"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.1.1"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.1.10"));

  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.10.0"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.10.1"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.1.10.10"));

  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.1.0"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.1.1"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.1.10"));

  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.10.0"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.10.1"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.10.10.10"));

  EXPECT_TRUE(IsValidTranslateKitVersion("3025.12.1.0"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.12.1.1"));
  EXPECT_TRUE(IsValidTranslateKitVersion("3025.12.1.10"));
}

}  // namespace
}  // namespace on_device_translation

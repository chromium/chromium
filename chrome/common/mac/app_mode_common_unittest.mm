// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/app_mode_common.h"

#include <optional>

#include "base/files/file_path.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_mode {

TEST(AppModeCommonTest, EncodeAndDecodeValid) {
  ChromeConnectionConfig original =
      ChromeConnectionConfig::GenerateForCurrentProcess();
  base::FilePath encoded = original.EncodeAsPath();

  std::optional<ChromeConnectionConfig> decoded =
      ChromeConnectionConfig::DecodeFromPath(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(original.framework_version, decoded->framework_version);
  EXPECT_EQ(version_info::GetVersionNumber(), decoded->framework_version);
  EXPECT_EQ(original.is_mojo_ipcz_enabled, decoded->is_mojo_ipcz_enabled);
  EXPECT_TRUE(decoded->is_mojo_ipcz_enabled);

  // Valid version with MojoIpcz enabled.
  decoded = ChromeConnectionConfig::DecodeFromPath(
      base::FilePath(FILE_PATH_LITERAL("1.2.3.4:1")));
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ("1.2.3.4", decoded->framework_version);
  EXPECT_TRUE(decoded->is_mojo_ipcz_enabled);

  // Valid version with MojoIpcz disabled.
  decoded = ChromeConnectionConfig::DecodeFromPath(
      base::FilePath(FILE_PATH_LITERAL("1.2.3.4:0")));
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ("1.2.3.4", decoded->framework_version);
  EXPECT_FALSE(decoded->is_mojo_ipcz_enabled);

  // Legacy single part (no MojoIpcz bit).
  decoded = ChromeConnectionConfig::DecodeFromPath(
      base::FilePath(FILE_PATH_LITERAL("1.2.3.4")));
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ("1.2.3.4", decoded->framework_version);
  EXPECT_FALSE(decoded->is_mojo_ipcz_enabled);
}

TEST(AppModeCommonTest, DecodeInvalid) {
  // Empty path.
  EXPECT_FALSE(
      ChromeConnectionConfig::DecodeFromPath(base::FilePath()).has_value());

  // Path traversal attacks.
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("../../evil:1")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("../evil")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("/tmp/evil:1")))
                   .has_value());

  // Non-numeric or invalid versions.
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("invalid_version:1")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("1.2.3.a:1")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL(":1")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("1.2.3.4/foo:1")))
                   .has_value());

  // Invalid MojoIpcz flags.
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("1.2.3.4:2")))
                   .has_value());
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("1.2.3.4:true")))
                   .has_value());

  // Extra colon-delimited components.
  EXPECT_FALSE(ChromeConnectionConfig::DecodeFromPath(
                   base::FilePath(FILE_PATH_LITERAL("1.2.3.4:1:extra")))
                   .has_value());
}

}  // namespace app_mode

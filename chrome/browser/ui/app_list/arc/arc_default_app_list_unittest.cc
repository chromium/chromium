// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetBoardName(const std::string& content) {
  base::ScopedTempDir scoped_dir;
  CHECK(scoped_dir.CreateUniqueTempDir());
  base::FilePath path;
  CHECK(base::CreateTemporaryFileInDir(scoped_dir.GetPath(), &path));
  CHECK(base::WriteFile(path, content));
  return ArcDefaultAppList::GetBoardNameForTesting(path);
}

TEST(ArcDefaultAppListTest, GetBoardName) {
  // Verify it returns "" when the path doesn't exist.
  EXPECT_TRUE(ArcDefaultAppList::GetBoardNameForTesting(
                  (base::FilePath("/non/existent")))
                  .empty());

  // Verify it returns "" when "^ro.product.board=" doesn't exist in the file.
  EXPECT_TRUE(GetBoardName("").empty());
  EXPECT_TRUE(GetBoardName("ro.product.xxxxx=").empty());
  EXPECT_TRUE(GetBoardName("ro.product.board").empty());
  EXPECT_TRUE(GetBoardName("#ro.product.board=abc").empty());

  // Verify it return the value part when found.
  EXPECT_EQ("a", GetBoardName("ro.product.board=a"));
  EXPECT_EQ("abc", GetBoardName("ro.product.board=abc"));
  EXPECT_EQ("abc", GetBoardName("ro.product.board=abc\n"));
  EXPECT_EQ("abc", GetBoardName("\nro.product.board=abc\n"));

  // Test the multi-line cases.
  EXPECT_EQ("a", GetBoardName("ro.product.board=a\nro.product.xxxxx=x"));
  EXPECT_EQ("a", GetBoardName("ro.product.xxxxx=x\nro.product.board=a"));
  EXPECT_EQ("a",
            GetBoardName(
                "ro.product.xxxxx=x\nro.product.board=a\nro.product.yyyyy=y"));
  EXPECT_TRUE(GetBoardName(
                  "ro.product.xxxxx=x\n#ro.product.board=a\nro.product.yyyyy=y")
                  .empty());
}

}  // namespace

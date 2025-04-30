// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/auto_launch_util.h"

#include "base/logging.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutoLaunchUtilTest, KeyNameKnownAnswer) {
  const base::FilePath path(FILE_PATH_LITERAL("C:\\foobar"));
  base::ScopedPathOverride path_override(chrome::DIR_USER_DATA, path);
  EXPECT_EQ(auto_launch_util::GetAutoLaunchKeyName(),
            L"GoogleChromeAutoLaunch_77A34B8C72FB474DE57439E114D86EE5");
}

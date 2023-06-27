// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/unittest_util_win.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

class CleanupTaskTest : public testing::Test {};

#if BUILDFLAG(IS_WIN)
TEST_F(CleanupTaskTest, RunCleanupObsoleteFiles) {
  // Set up a mock `GoogleUpdate.exe`, and the following mock directories:
  // `Download`, `Install`, and a versioned `1.2.3.4` directory.
  const absl::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(GetTestScope());
  ASSERT_TRUE(google_update_exe.has_value());
  SetupMockUpdater(google_update_exe.value());

  auto cleanup_task = base::MakeRefCounted<CleanupTask>(GetTestScope());
  ASSERT_TRUE(cleanup_task->RunCleanupObsoleteFiles());

  // Expect only a single file `GoogleUpdate.exe` and nothing else under
  // `\Google\Update`.
  ExpectOnlyMockUpdater(google_update_exe.value());
}
#endif

}  // namespace updater

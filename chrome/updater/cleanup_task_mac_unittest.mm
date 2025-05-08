// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#import <CoreFoundation/CoreFoundation.h>
#include <unistd.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(CleanupTaskMacTest, CleansOldCache) {
  const UpdaterScope scope = GetUpdaterScopeForTesting();
  VLOG(2) << __func__ << scope;
  if (scope == UpdaterScope::kSystem) {
    GTEST_SKIP() << "Cannot create system prefs as user.";
  }
  base::test::TaskEnvironment task_environment;

  base::FilePath cache;
  ASSERT_TRUE(base::apple::GetLocalDirectory(NSCachesDirectory, &cache));
  cache = cache.Append(MAC_BUNDLE_IDENTIFIER_STRING);
  ASSERT_TRUE(base::DeletePathRecursively(cache));
  ASSERT_TRUE(base::CreateDirectory(cache));

  const base::FilePath crx_cache = cache.Append("crx_cache");
  const base::FilePath file = crx_cache.Append(
      "appid_1."
      "f522c227412f46118df438ca07ac4ec89ccd7cf37b10ed79064a56d58f718df0");
  EXPECT_TRUE(base::CreateDirectory(crx_cache));
  EXPECT_TRUE(base::WriteFile(file, "contents"));

  auto cleanup_task = base::MakeRefCounted<CleanupTask>(
      scope, base::MakeRefCounted<Configurator>(
                 CreateLocalPrefs(scope), CreateExternalConstants(), scope));
  base::RunLoop run_loop;
  cleanup_task->Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(crx_cache));
  EXPECT_FALSE(base::PathExists(cache));
  base::DeletePathRecursively(cache);
}

TEST(CleanupTaskMacTest, CleansOldCacheSymlinkSafe) {
  const UpdaterScope scope = GetUpdaterScopeForTesting();
  if (scope == UpdaterScope::kSystem) {
    GTEST_SKIP() << "Cannot create system prefs as user.";
  }
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath survivor = temp.GetPath().Append("surviving_file");
  ASSERT_TRUE(base::WriteFile(survivor, "contents"));

  base::FilePath cache;
  ASSERT_TRUE(base::apple::GetLocalDirectory(NSCachesDirectory, &cache));
  cache = cache.Append(MAC_BUNDLE_IDENTIFIER_STRING);
  ASSERT_TRUE(base::DeletePathRecursively(cache));
  ASSERT_FALSE(symlink(temp.GetPath().value().c_str(), cache.value().c_str()));

  auto cleanup_task = base::MakeRefCounted<CleanupTask>(
      scope, base::MakeRefCounted<Configurator>(
                 CreateLocalPrefs(scope), CreateExternalConstants(), scope));
  base::RunLoop run_loop;
  cleanup_task->Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(survivor));
  EXPECT_FALSE(base::PathExists(cache));
  base::DeletePathRecursively(cache);
}

}  // namespace updater

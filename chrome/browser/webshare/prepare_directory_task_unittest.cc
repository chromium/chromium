// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/prepare_directory_task.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace {

void CreateDummyFile(const base::FilePath& path, base::Time last_modified) {
  const std::string contents = "contents";
  ASSERT_TRUE(base::WriteFile(path, contents));
  ASSERT_TRUE(base::TouchFile(path, last_modified, last_modified));
}

}  // namespace

namespace webshare {

TEST(PrepareDirectoryTask, DeleteOldFiles) {
  content::BrowserTaskEnvironment task_environment;

  const base::Time ancient_file_time =
      base::Time::Now() - PrepareDirectoryTask::kSharedFileLifetime * 2;
  const base::Time recent_file_time =
      base::Time::Now() - PrepareDirectoryTask::kSharedFileLifetime / 2;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath first_file = temp_dir.GetPath().Append("first.txt");
  const base::FilePath second_file = temp_dir.GetPath().Append("second.txt");
  const base::FilePath third_file = temp_dir.GetPath().Append("third.txt");

  // adding files in a sub directory
  const base::FilePath first_subdir_path = temp_dir.GetPath().Append("share-1");
  const base::FilePath fourth_file = first_subdir_path.Append("fourth.txt");

  const base::FilePath second_subdir_path =
      temp_dir.GetPath().Append("share-2");
  const base::FilePath fifth_file = second_subdir_path.Append("fifth.txt");

  CreateDummyFile(first_file, ancient_file_time);
  CreateDummyFile(second_file, recent_file_time);
  CreateDummyFile(third_file, ancient_file_time);

  base::CreateDirectory(first_subdir_path);
  CreateDummyFile(fourth_file, ancient_file_time);
  EXPECT_TRUE(
      base::TouchFile(first_subdir_path, ancient_file_time, ancient_file_time));

  base::CreateDirectory(second_subdir_path);
  CreateDummyFile(fifth_file, recent_file_time);
  EXPECT_TRUE(
      base::TouchFile(second_subdir_path, recent_file_time, recent_file_time));

  base::RunLoop run_loop;

  PrepareDirectoryTask task(
      temp_dir.GetPath(),
      /*required_space=*/0L,
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        EXPECT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  task.Start();
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(first_file));
  EXPECT_TRUE(base::PathExists(second_file));
  EXPECT_FALSE(base::PathExists(third_file));
  EXPECT_FALSE(base::PathExists(fourth_file));
  EXPECT_TRUE(base::PathExists(fifth_file));
}

}  // namespace webshare

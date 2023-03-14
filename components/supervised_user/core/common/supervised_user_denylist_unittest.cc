// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_denylist.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

TEST(SupervisedUserDenylistTest, URINotEncrypted) {
  // URI's that are not hashed will not be read by the file reader.
  GURL test_url("http://www.example.com/");
  base::ScopedTempDir user_data_dir_override_;
  base::test::TaskEnvironment task_environment;
  ASSERT_TRUE(user_data_dir_override_.CreateUniqueTempDir());
  SupervisedUserDenylist denylist_;

  // Set up test directory and denylist file.
  base::ScopedPathOverride path_override(base::DIR_TEST_DATA,
                                         user_data_dir_override_.GetPath());
  base::FilePath denylist_dir;
  base::PathService::Get(base::DIR_TEST_DATA, &denylist_dir);
  base::FilePath denylist_path =
      denylist_dir.Append(supervised_user::kDenylistFilename);

  EXPECT_TRUE(base::WriteFile(denylist_path, test_url.possibly_invalid_spec()));
  base::RunLoop run_loop;
  denylist_.ReadFromFile(
      denylist_path,
      base::BindRepeating([](base::RunLoop* run_loop) { run_loop->Quit(); },
                          base::Unretained(&run_loop)));
  run_loop.Run();

  EXPECT_FALSE(denylist_.HasURL(test_url));
  EXPECT_EQ(denylist_.GetEntryCount(), (unsigned)0);
}

TEST(SupervisedUserDenylistTest, AppendUnencryptedURI) {
  // If any URI that is not encrypted is written to the denylist, it will not be
  // decrypted correctly. Therefore, no URIs will be in the denylist.
  GURL test_url("http://www.example.com/");
  GURL test_url1("http://www.example1.com/");
  base::ScopedTempDir user_data_dir_override_;
  base::test::TaskEnvironment task_environment;
  ASSERT_TRUE(user_data_dir_override_.CreateUniqueTempDir());
  SupervisedUserDenylist denylist_;

  // Set up test directory and denylist file.
  base::ScopedPathOverride path_override(base::DIR_TEST_DATA,
                                         user_data_dir_override_.GetPath());
  base::FilePath denylist_dir;
  base::PathService::Get(base::DIR_TEST_DATA, &denylist_dir);
  base::FilePath denylist_path =
      denylist_dir.Append(supervised_user::kDenylistFilename);

  SupervisedUserDenylist::Hash hash_test_url(test_url.host());
  EXPECT_TRUE(base::WriteFile(denylist_path, hash_test_url.data));
  EXPECT_TRUE(base::AppendToFile(denylist_path, test_url1.host()));

  base::RunLoop run_loop;
  denylist_.ReadFromFile(
      denylist_path,
      base::BindRepeating([](base::RunLoop* run_loop) { run_loop->Quit(); },
                          base::Unretained(&run_loop)));
  run_loop.Run();

  EXPECT_FALSE(denylist_.HasURL(test_url));
  EXPECT_FALSE(denylist_.HasURL(test_url1));
  EXPECT_EQ(denylist_.GetEntryCount(), (unsigned)0);
}

TEST(SupervisedUserDenylistTest, AppendEncryptedURI) {
  // Hashed URIs that are append to file will be contained in denylist_.
  GURL test_url("http://www.example.com/");
  GURL test_url1("http://www.example1.com/");
  base::ScopedTempDir user_data_dir_override_;
  base::test::TaskEnvironment task_environment;
  ASSERT_TRUE(user_data_dir_override_.CreateUniqueTempDir());
  SupervisedUserDenylist denylist_;

  // Set up test directory and denylist file.
  base::ScopedPathOverride path_override(base::DIR_TEST_DATA,
                                         user_data_dir_override_.GetPath());
  base::FilePath denylist_dir;
  base::PathService::Get(base::DIR_TEST_DATA, &denylist_dir);
  base::FilePath denylist_path =
      denylist_dir.Append(supervised_user::kDenylistFilename);

  SupervisedUserDenylist::Hash hash_test_url(test_url.host());
  EXPECT_TRUE(base::WriteFile(denylist_path, hash_test_url.data));
  SupervisedUserDenylist::Hash hash_test_url1(test_url1.host());
  EXPECT_TRUE(base::AppendToFile(denylist_path, hash_test_url1.data));

  base::RunLoop run_loop;
  denylist_.ReadFromFile(
      denylist_path,
      base::BindRepeating([](base::RunLoop* run_loop) { run_loop->Quit(); },
                          base::Unretained(&run_loop)));
  run_loop.Run();

  EXPECT_TRUE(denylist_.HasURL(test_url));
  EXPECT_TRUE(denylist_.HasURL(test_url1));
  EXPECT_EQ(denylist_.GetEntryCount(), (unsigned)2);
}

}  // namespace supervised_user

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/digest_verifier.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(DigestVerifier, KnownFile) {
  scoped_refptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  base::FilePath dll_path = GetSampleDLLPath();
  ASSERT_TRUE(base::PathExists(dll_path)) << dll_path.value();

  EXPECT_TRUE(digest_verifier->IsKnownFile(dll_path));
}

TEST(DigestVerifier, UnknownFile) {
  scoped_refptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  const base::FilePath self_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();

  EXPECT_FALSE(digest_verifier->IsKnownFile(self_path));
}

TEST(DigestVerifier, InexistentFile) {
  scoped_refptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  base::FilePath invalid_path(L"this_file_should_not_exist.dll");
  ASSERT_FALSE(base::PathExists(invalid_path));

  EXPECT_FALSE(digest_verifier->IsKnownFile(invalid_path));
}

TEST(DigestVerifer, CreateFromFile) {
  std::string file_content1 = "File content 1";
  std::string file_content2 = "File content 2";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath known_file = temp_dir.GetPath().Append(L"known_file.txt");
  chrome_cleaner::CreateFileWithContent(known_file, file_content1.data(),
                                        file_content1.size());

  scoped_refptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromFile(known_file);

  EXPECT_TRUE(digest_verifier->IsKnownFile(known_file));

  // Different file name, same content. Should not be recognized.
  base::FilePath unknown_file = temp_dir.GetPath().Append(L"unknown_file.txt");
  chrome_cleaner::CreateFileWithContent(unknown_file, file_content1.data(),
                                        file_content1.size());
  EXPECT_FALSE(digest_verifier->IsKnownFile(unknown_file));

  // Same file name, different content. Should not be recognized.
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  base::FilePath same_name_file = temp_dir.GetPath().Append(L"known_file.txt");
  chrome_cleaner::CreateFileWithContent(same_name_file, file_content2.data(),
                                        file_content2.size());
  EXPECT_FALSE(digest_verifier->IsKnownFile(same_name_file));

  // Same file name, same content. Should be recognized.
  base::ScopedTempDir temp_dir3;
  ASSERT_TRUE(temp_dir3.CreateUniqueTempDir());
  base::FilePath same_content_file =
      temp_dir.GetPath().Append(L"known_file.txt");
  chrome_cleaner::CreateFileWithContent(same_content_file, file_content1.data(),
                                        file_content1.size());
  EXPECT_TRUE(digest_verifier->IsKnownFile(same_content_file));
}

}  // namespace chrome_cleaner

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/matcher_util.h"

#include <shlobj.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_signature_matcher.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

constexpr base::char16 kFileName1[] = L"File1";
constexpr base::char16 kFileName2[] = L"File2";
constexpr base::char16 kFileName3[] = L"File3";
constexpr base::char16 kFileName4[] = L"File4";

constexpr char kFileContent1[] = "This is the file content.";
constexpr char kFileContent2[] = "Hi!";
constexpr char kFileContent3[] = "Hello World!";
constexpr char kFileContent4[] = "Ha!";

constexpr char kFileContent[] = "This is the file content.";

const char* const kKnownContentDigests[] = {
    "00D2BB3E285BA62224888A9AD874AC2787D4CF681F30A2FD8EE2873859ECE1DC",
    // Hash for content: |kFileContent|.
    "BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371",
};

constexpr base::char16 kKnownOriginalFilename[] = L"uws.exe";
constexpr base::char16 kUnknownOriginalFilename[] = L"knowngood.exe";
const base::char16* const kKnownOriginalFilenames[] = {
    L"dummy entry", L"uws.exe", L"zombie uws.exe",
};

constexpr base::char16 kKnownCompanyName[] = L"uws vendor inc";
constexpr base::char16 kUnknownCompanyName[] = L"paradise";
const base::char16* const kKnownCompanyNames[] = {
    L"dummy entry", L"uws vendor inc", L"ACME",
};

constexpr FileDigestInfo kFileContentDigestInfos[] = {
    {"02544E052F29BBA79C81243EC63B43B6CD85B185461928E65BFF501346C62A75", 33},
    {"04614470DDF4939091F5EC4A13C92A9EAAACF07CA5C3F713E792E2D21CD24075", 21},
    // Hash for content: |kFileContent2|.
    {"82E0B92772BC0DA59AAB0B9231AA006FB37B4F99EC3E853C5A62786A1C7215BD", 4},
    {"9000000000000000000000000000000000000000000000000000000000000009", 4},
    {"94F7BDF53CDFDE7AA5E5C90BCDA6793B7377CE39E2591ABC758EBAE8072A275C", 12},
    // Hash for content: |kFileContent1|.
    {"BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371", 26},
};

// Messages are logged to a vector for testing.
class LoggingTest : public testing::Test {
 public:
  LoggingOverride logger_;
};

}  // namespace

TEST(MatcherUtilTest, IsKnownFileByDigest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path1(temp_dir.GetPath().Append(kFileName1));
  base::FilePath file_path2(temp_dir.GetPath().Append(kFileName2));
  base::FilePath file_path3(temp_dir.GetPath().Append(kFileName3));

  CreateFileWithContent(file_path1, kFileContent, sizeof(kFileContent));
  CreateFileWithRepeatedContent(file_path2, kFileContent, sizeof(kFileContent),
                                2);

  std::unique_ptr<SignatureMatcher> signature_matcher =
      std::make_unique<SignatureMatcher>();
  ASSERT_TRUE(signature_matcher);

  // Hash: BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371.
  EXPECT_TRUE(IsKnownFileByDigest(file_path1, signature_matcher.get(),
                                  kKnownContentDigests,
                                  base::size(kKnownContentDigests)));
  // Hash: not present.
  EXPECT_FALSE(IsKnownFileByDigest(file_path2, signature_matcher.get(),
                                   kKnownContentDigests,
                                   base::size(kKnownContentDigests)));
  // The file doesn't exist.
  EXPECT_FALSE(IsKnownFileByDigest(file_path3, signature_matcher.get(),
                                   kKnownContentDigests,
                                   base::size(kKnownContentDigests)));
}

TEST(MatcherUtilTest, IsKnownFileByDigestInfo) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path1(temp_dir.GetPath().Append(kFileName1));
  base::FilePath file_path2(temp_dir.GetPath().Append(kFileName2));
  base::FilePath file_path3(temp_dir.GetPath().Append(kFileName3));
  base::FilePath file_path4(temp_dir.GetPath().Append(kFileName4));

  CreateFileWithContent(file_path1, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path2, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path3, kFileContent3, sizeof(kFileContent3));

  std::unique_ptr<SignatureMatcher> signature_matcher =
      std::make_unique<SignatureMatcher>();

  // Search BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371.
  EXPECT_TRUE(IsKnownFileByDigestInfo(file_path1, signature_matcher.get(),
                                      kFileContentDigestInfos,
                                      base::size(kFileContentDigestInfos)));
  // Search 82E0B92772BC0DA59AAB0B9231AA006FB37B4F99EC3E853C5A62786A1C7215BD.
  EXPECT_TRUE(IsKnownFileByDigestInfo(file_path2, signature_matcher.get(),
                                      kFileContentDigestInfos,
                                      base::size(kFileContentDigestInfos)));

  // Replace the content of file_path2 with a content of the same size, it
  // must no longer match.
  ASSERT_EQ(sizeof(kFileContent2), sizeof(kFileContent4));
  CreateFileWithContent(file_path2, kFileContent4, sizeof(kFileContent4));
  EXPECT_FALSE(IsKnownFileByDigestInfo(file_path2, signature_matcher.get(),
                                       kFileContentDigestInfos,
                                       base::size(kFileContentDigestInfos)));

  // The digest of |file_path3| is not in the array.
  EXPECT_FALSE(IsKnownFileByDigestInfo(file_path3, signature_matcher.get(),
                                       kFileContentDigestInfos,
                                       base::size(kFileContentDigestInfos)));
  // The |file_path4| doesn't exist.
  EXPECT_FALSE(IsKnownFileByDigestInfo(file_path4, signature_matcher.get(),
                                       kFileContentDigestInfos,
                                       base::size(kFileContentDigestInfos)));
}

TEST(MatcherUtilTest, IsKnownFileByOriginalFilename) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  TestSignatureMatcher signature_matcher;

  const base::FilePath normalized_temp_dir_path =
      NormalizePath(temp_dir.GetPath());

  // A non-existing file should not be recognized.
  base::FilePath nonexistent_file_path(
      normalized_temp_dir_path.Append(kFileName1));
  EXPECT_FALSE(IsKnownFileByOriginalFilename(
      nonexistent_file_path, &signature_matcher, kKnownOriginalFilenames,
      base::size(kKnownOriginalFilenames)));

  // An existing file without version information should not be recognized.
  base::FilePath file_path2(normalized_temp_dir_path.Append(kFileName2));
  CreateFileWithContent(file_path2, kFileContent, sizeof(kFileContent));
  EXPECT_FALSE(IsKnownFileByOriginalFilename(
      file_path2, &signature_matcher, kKnownOriginalFilenames,
      base::size(kKnownOriginalFilenames)));

  // A file with version information but not in the array should not be
  // recognized.
  base::FilePath file_path3(normalized_temp_dir_path.Append(kFileName3));

  VersionInformation goodware_information = {};
  goodware_information.original_filename = kUnknownOriginalFilename;
  signature_matcher.MatchVersionInformation(file_path3, goodware_information);

  CreateFileWithContent(file_path3, kFileContent, sizeof(kFileContent));
  EXPECT_FALSE(IsKnownFileByOriginalFilename(
      file_path3, &signature_matcher, kKnownOriginalFilenames,
      base::size(kKnownOriginalFilenames)));

  // A file with version information present in the array should be recognized.
  base::FilePath file_path4(normalized_temp_dir_path.Append(kFileName4));

  VersionInformation badware_information = {};
  badware_information.original_filename = kKnownOriginalFilename;
  signature_matcher.MatchVersionInformation(file_path4, badware_information);

  CreateFileWithContent(file_path4, kFileContent, sizeof(kFileContent));
  EXPECT_TRUE(IsKnownFileByOriginalFilename(
      file_path4, &signature_matcher, kKnownOriginalFilenames,
      base::size(kKnownOriginalFilenames)));
}

TEST(MatcherUtilTest, IsKnownFileByCompanyName) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath normalized_temp_dir_path =
      NormalizePath(temp_dir.GetPath());

  TestSignatureMatcher signature_matcher;

  // A non-existing file should not be recognized.
  base::FilePath nonexistent_file_path(
      normalized_temp_dir_path.Append(kFileName1));
  EXPECT_FALSE(IsKnownFileByCompanyName(nonexistent_file_path,
                                        &signature_matcher, kKnownCompanyNames,
                                        base::size(kKnownCompanyNames)));

  // An existing file without version information should not be recognized.
  base::FilePath file_path2(normalized_temp_dir_path.Append(kFileName2));
  CreateFileWithContent(file_path2, kFileContent, sizeof(kFileContent));
  EXPECT_FALSE(IsKnownFileByCompanyName(file_path2, &signature_matcher,
                                        kKnownCompanyNames,
                                        base::size(kKnownCompanyNames)));

  // A file with version information but not in the array should not be
  // recognized.
  base::FilePath file_path3(normalized_temp_dir_path.Append(kFileName3));

  VersionInformation goodware_information = {};
  goodware_information.company_name = kUnknownCompanyName;
  signature_matcher.MatchVersionInformation(file_path3, goodware_information);

  CreateFileWithContent(file_path3, kFileContent, sizeof(kFileContent));
  EXPECT_FALSE(IsKnownFileByCompanyName(file_path3, &signature_matcher,
                                        kKnownCompanyNames,
                                        base::size(kKnownCompanyNames)));

  // A file with version information present in the array should be recognized.
  base::FilePath file_path4(normalized_temp_dir_path.Append(kFileName4));

  VersionInformation badware_information = {};
  badware_information.company_name = kKnownCompanyName;
  signature_matcher.MatchVersionInformation(file_path4, badware_information);

  CreateFileWithContent(file_path4, kFileContent, sizeof(kFileContent));
  EXPECT_TRUE(IsKnownFileByCompanyName(file_path4, &signature_matcher,
                                       kKnownCompanyNames,
                                       base::size(kKnownCompanyNames)));
}

}  // namespace chrome_cleaner

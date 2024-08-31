// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_validator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/test_file_util.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace offline_pages {

namespace {

const char kTestData1[] = "This is a test. ";
const char kTestData2[] = "Hello World!";

const int kSmallFileSize = 2 * 1024;
const int kBigFileSize = 3 * 1024 * 1024;
#if BUILDFLAG(IS_ANDROID)
const int kSizeForTestContentUri = 173;
#endif  // BUILDFLAG(IS_ANDROID)

// Digest for kTestData1 + kTestData2.
const std::string kExpectedDigestForTestData(
    "\x9D\xBF\xED\xE4\x54\x16\xA6\xA3\x36\x2C\x88\xD8\xA8\x2A\x3A\xF3\x51\x1A"
    "\x6E\x34\x7E\xEF\xA4\xD5\x1D\xDE\x2A\xD0\xFE\x39\xE8\xA8",
    32);
// Digest for file with size 2K, filled with test data.
const std::string kExpectedDigestForSmallFile(
    "\x10\xFC\x3C\x51\xA1\x52\xE9\x0E\x5B\x90\x31\x9B\x60\x1D\x92\xCC\xF3\x72"
    "\x90\xEF\x53\xC3\x5F\xF9\x25\x07\x68\x7D\x8A\x91\x1A\x08",
    32);
// Digest for file with size 3M, filled with test data.
const std::string kExpectedDigestForBigFile(
    "\xF6\xDD\x7F\xEC\x85\x84\xAD\x00\x21\x9A\x44\x70\x71\xC1\xFA\x36\x8A\x1C"
    "\xAE\xE4\xD9\xC1\x46\x08\x3D\x23\x37\x13\xDD\xCC\xD2\xC0",
    32);
// Digest for content URI generated from net/data/file_stream_unittest/red.png.
const std::string kExpectedDigestForContentUri(
    "\xEB\x7E\xB8\xE7\x3E\xD1\xE5\x45\x55\xCF\xA1\x8B\x7D\xD6\x75\x26\x2F\x8C"
    "\x8C\xDE\x31\x2B\x94\x43\x46\xE2\xF7\x74\xC8\xF7\x3A\x18",
    32);

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++)
    result.append(1, static_cast<char>(i % 256));
  return result;
}

#if BUILDFLAG(IS_ANDROID)
base::FilePath GetContentUriPathForTest() {
  base::FilePath test_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_dir);
  test_dir = test_dir.AppendASCII("net");
  test_dir = test_dir.AppendASCII("data");
  test_dir = test_dir.AppendASCII("file_stream_unittest");
  EXPECT_TRUE(base::PathExists(test_dir));
  base::FilePath image_file = test_dir.Append(FILE_PATH_LITERAL("red.png"));

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  base::FilePath path = base::InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(base::PathExists(path));

  return path;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class ArchiveValidatorTest : public testing::Test {
 public:
  ArchiveValidatorTest() = default;
  ~ArchiveValidatorTest() override = default;

  base::FilePath CreateFileWithContent(const std::string& content);

 private:
  base::ScopedTempDir temp_dir_;
};

base::FilePath ArchiveValidatorTest::CreateFileWithContent(
    const std::string& content) {
  if (!temp_dir_.CreateUniqueTempDir()) {
    return base::FilePath();
  }
  base::FilePath temp_file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.txt"));
  base::WriteFile(temp_file_path, content);
  return temp_file_path;
}

TEST_F(ArchiveValidatorTest, ComputeDigestOnData) {
  ArchiveValidator archive_validator;
  archive_validator.Update(kTestData1, sizeof(kTestData1) - 1);
  archive_validator.Update(kTestData2, sizeof(kTestData2) - 1);
  std::string actual_digest = archive_validator.Finish();
  EXPECT_EQ(kExpectedDigestForTestData, actual_digest);
}

TEST_F(ArchiveValidatorTest, GetSizeAndComputeDigestOnTinyFile) {
  std::string expected_data(kTestData1);
  expected_data += kTestData2;
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  std::pair<int64_t, std::string> actual_size_and_digest =
      ArchiveValidator::GetSizeAndComputeDigest(temp_file_path);
  EXPECT_EQ(static_cast<int64_t>(expected_data.size()),
            actual_size_and_digest.first);
  EXPECT_EQ(kExpectedDigestForTestData, actual_size_and_digest.second);
}

TEST_F(ArchiveValidatorTest, GetSizeAndComputeDigestOnSmallFile) {
  std::string expected_data(MakeContentOfSize(kSmallFileSize));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  std::pair<int64_t, std::string> actual_size_and_digest =
      ArchiveValidator::GetSizeAndComputeDigest(temp_file_path);
  EXPECT_EQ(kSmallFileSize, actual_size_and_digest.first);
  EXPECT_EQ(kExpectedDigestForSmallFile, actual_size_and_digest.second);
}

TEST_F(ArchiveValidatorTest, GetSizeAndComputeDigestOnBigFile) {
  std::string expected_data(MakeContentOfSize(kBigFileSize));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  std::pair<int64_t, std::string> actual_size_and_digest =
      ArchiveValidator::GetSizeAndComputeDigest(temp_file_path);
  EXPECT_EQ(kBigFileSize, actual_size_and_digest.first);
  EXPECT_EQ(kExpectedDigestForBigFile, actual_size_and_digest.second);
}

#if BUILDFLAG(IS_ANDROID)
// Flaky. https://crbug.com/1022323
TEST_F(ArchiveValidatorTest, DISABLED_GetSizeAndComputeDigestOnContentUri) {
  base::FilePath content_uri_path = GetContentUriPathForTest();
  std::pair<int64_t, std::string> actual_size_and_digest =
      ArchiveValidator::GetSizeAndComputeDigest(content_uri_path);
  EXPECT_EQ(kSizeForTestContentUri, actual_size_and_digest.first);
  EXPECT_EQ(kExpectedDigestForContentUri, actual_size_and_digest.second);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(ArchiveValidatorTest, ValidateSmallFile) {
  std::string expected_data(MakeContentOfSize(kSmallFileSize));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  EXPECT_TRUE(ArchiveValidator::ValidateFile(temp_file_path, kSmallFileSize,
                                             kExpectedDigestForSmallFile));
}

TEST_F(ArchiveValidatorTest, ValidateBigFile) {
  std::string expected_data(MakeContentOfSize(kBigFileSize));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  EXPECT_TRUE(ArchiveValidator::ValidateFile(temp_file_path, kBigFileSize,
                                             kExpectedDigestForBigFile));
}

#if BUILDFLAG(IS_ANDROID)
// Flaky. https://crbug.com/1022322
TEST_F(ArchiveValidatorTest, DISABLED_ValidateContentUri) {
  base::FilePath content_uri_path = GetContentUriPathForTest();
  EXPECT_TRUE(ArchiveValidator::ValidateFile(
      content_uri_path, kSizeForTestContentUri, kExpectedDigestForContentUri));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace offline_pages

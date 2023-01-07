// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/test_zip_archiver_util.h"

#include <limits>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/contrib/minizip/unzip.h"

namespace chrome_cleaner {

namespace {

const char kTestContent[] = "Hello World";
const base::Time::Exploded kTestFileTime = {
    // Year
    2018,
    // Month
    1,
    // Day of week
    1,
    // Day of month
    1,
    // Hour
    2,
    // Minute
    3,
    // Second
    5,
    // Millisecond
    0,
};
// The zip should be encrypted and use UTF-8 encoding.
const uint16_t kExpectedZipFlag = 0x1 | (0x1 << 11);

const int64_t kReadBufferSize = 4096;

// The |day_of_week| in |base::Time::Exploded| won't be set correctly.
// TODO(veranika): This is copied from
// https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive_minizip.cc.
// It would be better to move it to //base.
base::Time::Exploded ExplodeDosTime(const uint32_t dos_time) {
  base::Time::Exploded time_part;
  uint32_t remain_part = dos_time;
  // 0-4bits, second divied by 2.
  time_part.second = (remain_part & 0x1F) * 2;
  remain_part >>= 5;
  // 5-10bits, minute (0–59).
  time_part.minute = remain_part & 0x3F;
  remain_part >>= 6;
  // 11-15bits, hour (0–23 on a 24-hour clock).
  time_part.hour = remain_part & 0x1F;
  remain_part >>= 5;
  // 16-20bits, day of the month (1-31).
  time_part.day_of_month = remain_part & 0x1F;
  remain_part >>= 5;
  // 21-24bits, month (1-23).
  time_part.month = remain_part & 0xF;
  remain_part >>= 4;
  // 25-31bits, year offset from 1980.
  time_part.year = (remain_part & 0x7F) + 1980;
  return time_part;
}

}  // namespace

ZipArchiverTestFile::ZipArchiverTestFile() : initialized_(false) {}

ZipArchiverTestFile::~ZipArchiverTestFile() = default;

void ZipArchiverTestFile::Initialize() {
  // Can not be reinitialized.
  CHECK(!initialized_);
  initialized_ = true;

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &src_file_path_));

  ASSERT_EQ(static_cast<size_t>(base::WriteFile(src_file_path_, kTestContent,
                                                strlen(kTestContent))),
            strlen(kTestContent));
  // Set a fixed timestamp, so the modified time will be identical in every
  // test.
  base::Time file_time;
  ASSERT_TRUE(base::Time::FromLocalExploded(kTestFileTime, &file_time));
  ASSERT_TRUE(base::TouchFile(src_file_path_, file_time, file_time));
}

const base::FilePath& ZipArchiverTestFile::GetSourceFilePath() const {
  return src_file_path_;
}

const base::FilePath& ZipArchiverTestFile::GetTempDirPath() const {
  return temp_dir_.GetPath();
}

void ZipArchiverTestFile::ExpectValidZipFile(
    const base::FilePath& zip_file_path,
    const std::string& filename_in_zip,
    const std::string& password) {
  unzFile unzip_object = unzOpen64(zip_file_path.AsUTF8Unsafe().c_str());
  ASSERT_NE(unzip_object, nullptr);
  EXPECT_EQ(unzLocateFile(unzip_object, filename_in_zip.c_str(),
                          /*iCaseSensitivity=*/1),
            UNZ_OK);

  unz_file_info64 file_info;
  const size_t filename_length = strlen(filename_in_zip.c_str());
  std::vector<char> filename(filename_length + 1);
  ASSERT_GT(std::numeric_limits<Cr_z_uLong>::max(), filename.size());
  EXPECT_EQ(unzGetCurrentFileInfo64(
                unzip_object, &file_info, filename.data(),
                static_cast<Cr_z_uLong>(filename.size()),
                /*extraField=*/nullptr, /*extraFieldBufferSize=*/0,
                /*szComment=*/nullptr, /*commentBufferSize=*/0),
            UNZ_OK);
  EXPECT_EQ(file_info.flag & kExpectedZipFlag, kExpectedZipFlag);
  // The compression method should be STORE(0).
  EXPECT_EQ(file_info.compression_method, 0UL);
  EXPECT_EQ(file_info.uncompressed_size, strlen(kTestContent));
  EXPECT_EQ(file_info.size_filename, filename_in_zip.size());
  EXPECT_STREQ(filename.data(), filename_in_zip.c_str());

  base::Time::Exploded file_time = ExplodeDosTime(file_info.dosDate);
  EXPECT_EQ(file_time.year, kTestFileTime.year);
  EXPECT_EQ(file_time.month, kTestFileTime.month);
  EXPECT_EQ(file_time.day_of_month, kTestFileTime.day_of_month);
  EXPECT_EQ(file_time.hour, kTestFileTime.hour);
  EXPECT_EQ(file_time.minute, kTestFileTime.minute);
  // Dos time has a resolution of 2 seconds.
  EXPECT_EQ(file_time.second, (kTestFileTime.second / 2) * 2);

  EXPECT_EQ(unzOpenCurrentFilePassword(unzip_object, password.c_str()), UNZ_OK);

  const size_t content_length = strlen(kTestContent);
  std::vector<char> content;
  uint8_t read_buffer[kReadBufferSize];
  while (true) {
    int read_size =
        unzReadCurrentFile(unzip_object, read_buffer, kReadBufferSize);
    EXPECT_GE(read_size, 0);
    if (read_size == 0) {
      break;
    }
    content.insert(content.end(), read_buffer, read_buffer + read_size);
    if (content.size() > content_length) {
      break;
    }
  }
  EXPECT_EQ(content.size(), content_length);
  EXPECT_EQ(std::string(content.begin(), content.end()), kTestContent);

  EXPECT_EQ(unzCloseCurrentFile(unzip_object), UNZ_OK);
  ASSERT_EQ(unzClose(unzip_object), UNZ_OK);
}

}  // namespace chrome_cleaner

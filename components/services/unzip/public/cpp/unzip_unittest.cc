// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unzip {
namespace {

base::FilePath GetArchivePath(const base::StringPiece archive_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("unzip_service")
      .AppendASCII(archive_name);
}

// Counts the number of files under |dir|. If |some_files_empty| is not null,
// sets it to true if at least one of the files is empty.
int CountFiles(const base::FilePath& dir, bool* some_files_empty = nullptr) {
  int file_count = 0;
  base::FileEnumerator file_enumerator(dir, /*recursive=*/true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    if (int64_t file_size; some_files_empty != nullptr &&
                           base::GetFileSize(path, &file_size) &&
                           file_size == 0) {
      *some_files_empty = true;
      some_files_empty = nullptr;  // So we don't check files again.
    }

    file_count++;
  }

  return file_count;
}

class UnzipTest : public testing::Test {
 public:
  UnzipTest() = default;
  ~UnzipTest() override = default;

  // Unzips |zip_file| into |output_dir| and returns true if the unzip was
  // successful. Only extract files for which |filter_callback| returns true, if
  // it is provided.
  bool DoUnzip(const base::FilePath& zip_file,
               const base::FilePath& output_dir,
               UnzipFilterCallback filter_callback = {}) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    bool result = false;

    UnzipCallback result_callback =
        base::BindLambdaForTesting([&](const bool success) {
          result = success;
          run_loop.QuitClosure().Run();
        });

    UnzipWithFilter(std::move(unzipper), zip_file, output_dir,
                    std::move(filter_callback), std::move(result_callback));

    run_loop.Run();
    return result;
  }

  Encoding DoDetectEncoding(const base::FilePath& zip_file) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    Encoding result = UNKNOWN_ENCODING;

    DetectEncodingCallback result_callback =
        base::BindLambdaForTesting([&](const Encoding encoding) {
          result = encoding;
          run_loop.QuitClosure().Run();
        });

    DetectEncoding(std::move(unzipper), zip_file, std::move(result_callback));
    run_loop.Run();
    return result;
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    unzip_dir_ = temp_dir_.GetPath();
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  base::FilePath unzip_dir_;

  unzip::UnzipperImpl unzipper_;
  mojo::ReceiverSet<mojom::Unzipper> receivers_;
};

TEST_F(UnzipTest, UnzipAbsentArchive) {
  EXPECT_FALSE(DoUnzip(GetArchivePath("absent_archive.zip"), unzip_dir_));

  // No files should have been extracted.
  EXPECT_EQ(0, CountFiles(unzip_dir_));
}

TEST_F(UnzipTest, UnzipBadArchive) {
  EXPECT_FALSE(DoUnzip(GetArchivePath("bad_archive.zip"), unzip_dir_));

  // No files should have been extracted.
  EXPECT_EQ(0, CountFiles(unzip_dir_));
}

TEST_F(UnzipTest, UnzipWrongCrc) {
  EXPECT_FALSE(DoUnzip(GetArchivePath("Wrong CRC.zip"), unzip_dir_));

  // No files should have been extracted.
  EXPECT_EQ(0, CountFiles(unzip_dir_));
}

TEST_F(UnzipTest, UnzipGoodArchive) {
  EXPECT_TRUE(DoUnzip(GetArchivePath("good_archive.zip"), unzip_dir_));

  // Sanity check that the right number of files have been extracted and that
  // they are not empty.
  bool some_files_empty = false;
  EXPECT_EQ(8, CountFiles(unzip_dir_, &some_files_empty));
  EXPECT_FALSE(some_files_empty);
}

TEST_F(UnzipTest, UnzipWithFilter) {
  EXPECT_TRUE(DoUnzip(GetArchivePath("good_archive.zip"), unzip_dir_,
                      base::BindRepeating([](const base::FilePath& path) {
                        return path.MatchesExtension(FILE_PATH_LITERAL(".txt"));
                      })));

  // It should only have kept the 2 text files from the archive.
  bool some_files_empty = false;
  EXPECT_EQ(2, CountFiles(unzip_dir_, &some_files_empty));
  EXPECT_FALSE(some_files_empty);
}

TEST_F(UnzipTest, DetectEncodingAbsentArchive) {
  EXPECT_EQ(UNKNOWN_ENCODING,
            DoDetectEncoding(GetArchivePath("absent_archive.zip")));
}

TEST_F(UnzipTest, DetectEncodingBadArchive) {
  EXPECT_EQ(UNKNOWN_ENCODING,
            DoDetectEncoding(GetArchivePath("bad_archive.zip")));
}

TEST_F(UnzipTest, DetectEncodingAscii) {
  EXPECT_EQ(Encoding::ASCII_7BIT,
            DoDetectEncoding(GetArchivePath("good_archive.zip")));
}

// See https://crbug.com/903664
TEST_F(UnzipTest, DetectEncodingUtf8) {
  EXPECT_EQ(Encoding::UTF8,
            DoDetectEncoding(GetArchivePath("UTF8 (Bug 903664).zip")));
}

// See https://crbug.com/1287893
TEST_F(UnzipTest, DetectEncodingSjis) {
  for (const base::StringPiece name : {
           "SJIS 00.zip",
           "SJIS 01.zip",
           "SJIS 02.zip",
           "SJIS 03.zip",
           "SJIS 04.zip",
           "SJIS 05.zip",
           "SJIS 06.zip",
           "SJIS 07.zip",
           "SJIS 08.zip",
           "SJIS 09.zip",
           "SJIS 10.zip",
           "SJIS 11.zip",
           "SJIS 12.zip",
           "SJIS 13.zip",
       }) {
    EXPECT_EQ(Encoding::JAPANESE_SHIFT_JIS,
              DoDetectEncoding(GetArchivePath(name)));
  }
}

}  // namespace
}  // namespace unzip

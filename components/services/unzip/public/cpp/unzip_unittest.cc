// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <cstdint>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unzip {
namespace {

base::FilePath GetArchivePath(std::string_view archive_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
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
               UnzipFilterCallback filter_callback = unzip::AllContents()) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    bool result = false;

    Unzip(std::move(unzipper), zip_file, output_dir,
          unzip::mojom::UnzipOptions::New(), std::move(filter_callback),
          base::DoNothing(), base::BindLambdaForTesting([&](bool success) {
            result = success;
            run_loop.QuitClosure().Run();
          }));

    run_loop.Run();
    return result;
  }

  // Unzips |zip_file| into |output_dir| and returns true if the unzip was
  // successful. |options| hosts parameters for the unpack.
  bool DoUnzipWithOptions(const base::FilePath& zip_file,
                          const base::FilePath& output_dir,
                          mojom::UnzipOptionsPtr options) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    bool result = false;

    Unzip(std::move(unzipper), zip_file, output_dir, std::move(options),
          AllContents(), base::BindLambdaForTesting([&](uint64_t) {}),
          base::BindLambdaForTesting([&](bool success) {
            result = success;
            run_loop.QuitClosure().Run();
          }));

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

  mojom::Info DoGetExtractedInfo(const base::FilePath& zip_file) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    mojom::Info result;

    GetExtractedInfoCallback result_callback =
        base::BindLambdaForTesting([&](mojom::InfoPtr info) {
          result = *info;
          run_loop.QuitClosure().Run();
        });

    GetExtractedInfo(std::move(unzipper), zip_file, std::move(result_callback));
    run_loop.Run();
    return result;
  }

  uint64_t DoGetProgressSize(const base::FilePath& zip_file,
                             const base::FilePath& output_dir) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    uint64_t bytes = 0;

    Unzip(std::move(unzipper), zip_file, output_dir,
          unzip::mojom::UnzipOptions::New("auto", ""), AllContents(),
          base::BindLambdaForTesting([&](uint64_t written_bytes) {
            bytes = written_bytes;
            run_loop.QuitClosure().Run();
          }),
          base::BindLambdaForTesting(
              [&](bool) { run_loop.QuitClosure().Run(); }));

    run_loop.Run();
    return bytes;
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

// Checks that the Unzipper service does not overwrite an existing file.
TEST_F(UnzipTest, DuplicatedNames) {
  EXPECT_FALSE(DoUnzip(GetArchivePath("Duplicate Filenames.zip"), unzip_dir_));

  // Check that the first file was correctly extracted.
  std::string content;
  EXPECT_TRUE(
      base::ReadFileToString(unzip_dir_.AppendASCII("Simple.txt"), &content));
  EXPECT_EQ("Simple 1\n", content);

  // Check that no other file was extracted.
  EXPECT_EQ(1, CountFiles(unzip_dir_));
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
  for (const std::string_view name : {
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

TEST_F(UnzipTest, GetExtractedSize) {
  mojom::Info result = DoGetExtractedInfo(GetArchivePath("good_archive.zip"));
  EXPECT_TRUE(result.size_is_valid);
  EXPECT_EQ(137, static_cast<int64_t>(result.size));
}

TEST_F(UnzipTest, GetExtractedSizeBrokenArchive) {
  mojom::Info result = DoGetExtractedInfo(GetArchivePath("bad_archive.zip"));
  EXPECT_FALSE(result.size_is_valid);
}

TEST_F(UnzipTest, UnzipWithOptions) {
  unzip::mojom::UnzipOptionsPtr options =
      unzip::mojom::UnzipOptions::New("auto", "");
  EXPECT_TRUE(DoUnzipWithOptions(GetArchivePath("good_archive.zip"), unzip_dir_,
                                 std::move(options)));

  // 8 files should have been extracted.
  bool some_files_empty = false;
  EXPECT_EQ(8, CountFiles(unzip_dir_, &some_files_empty));
}

TEST_F(UnzipTest, GetExtractedProgressSize) {
  uint64_t result =
      DoGetProgressSize(GetArchivePath("good_archive.zip"), unzip_dir_);
  // Check: first file extracted is 23 bytes long.
  EXPECT_EQ(23ul, result);
}

TEST_F(UnzipTest, ExtractEncrypted) {
  mojom::Info result =
      DoGetExtractedInfo(GetArchivePath("encrypted_archive.zip"));
  EXPECT_TRUE(result.is_encrypted);

  unzip::mojom::UnzipOptionsPtr options =
      unzip::mojom::UnzipOptions::New("auto", "fake_password");
  EXPECT_TRUE(DoUnzipWithOptions(GetArchivePath("encrypted_archive.zip"),
                                 unzip_dir_, std::move(options)));

  // Check: 5 files should have been extracted.
  bool some_files_empty = false;
  EXPECT_EQ(5, CountFiles(unzip_dir_, &some_files_empty));
}

TEST_F(UnzipTest, DetectAESArchive) {
  mojom::Info result =
      DoGetExtractedInfo(GetArchivePath("DifferentEncryptions.zip"));
  EXPECT_TRUE(result.uses_aes_encryption);
}

}  // namespace
}  // namespace unzip

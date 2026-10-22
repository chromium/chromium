// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

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
    std::optional<int64_t> file_size = base::GetFileSize(path);

    if (some_files_empty != nullptr && file_size.has_value() &&
        file_size.value() == 0) {
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

  bool DoDecodeXz(const base::FilePath& in_file,
                  const base::FilePath& out_file) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());
    bool result = false;
    base::RunLoop run_loop;
    DecodeXz(std::move(unzipper), in_file, out_file,
             base::BindLambdaForTesting([&](bool success) {
               result = success;
               run_loop.Quit();
             }));
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

TEST_F(UnzipTest, UnzipGoodArchiveWithExtraBytes) {
  EXPECT_TRUE(DoUnzip(GetArchivePath("good_archive_prefixed.zip"), unzip_dir_));
  bool some_files_empty = false;
  EXPECT_EQ(8, CountFiles(unzip_dir_, &some_files_empty));
  EXPECT_FALSE(some_files_empty);
}

TEST_F(UnzipTest, UnzipBadArchiveHang) {
  // Don't hang trying to open this bad archive.
  EXPECT_FALSE(DoUnzip(GetArchivePath("bad_archive_hang.zip"), unzip_dir_));
  EXPECT_EQ(0, CountFiles(unzip_dir_));
}

TEST_F(UnzipTest, UnzipZip64) {
  EXPECT_TRUE(DoUnzip(GetArchivePath("good_zip64.zip"), unzip_dir_));
  bool some_files_empty = false;
  EXPECT_EQ(1, CountFiles(unzip_dir_, &some_files_empty));
  EXPECT_FALSE(some_files_empty);
}

TEST_F(UnzipTest, UnzipZip64WithExtraBytes) {
  EXPECT_TRUE(DoUnzip(GetArchivePath("good_zip64_prefixed.zip"), unzip_dir_));
  bool some_files_empty = false;
  EXPECT_EQ(1, CountFiles(unzip_dir_, &some_files_empty));
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

TEST_F(UnzipTest, DecodeXz_Success) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  ASSERT_TRUE(DoDecodeXz(GetArchivePath("file1.xz"), out));
  EXPECT_EQ(ReadFileToBytes(out), ReadFileToBytes(GetArchivePath("file1")));
  ASSERT_TRUE(base::DeleteFile(out));
  ASSERT_TRUE(DoDecodeXz(GetArchivePath("bd646.xz"), out));
  EXPECT_EQ(ReadFileToBytes(out), ReadFileToBytes(GetArchivePath("bd646")));
}

TEST_F(UnzipTest, DecodeXz_DontReplaceExistingOutfile) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  ASSERT_TRUE(base::WriteFile(out, "data"));
  ASSERT_FALSE(DoDecodeXz(GetArchivePath("file1.xz"), out));
  std::string out_contents;
  ASSERT_TRUE(base::ReadFileToString(out, &out_contents));
  EXPECT_EQ(out_contents, "data");
}

TEST_F(UnzipTest, DecodeXz_MissingInput) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  EXPECT_FALSE(DoDecodeXz(GetArchivePath("doesnotexist"), out));
  EXPECT_FALSE(base::PathExists(out));
}

TEST_F(UnzipTest, DecodeXz_BadFormat) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  EXPECT_FALSE(DoDecodeXz(GetArchivePath("file1"), out));
  EXPECT_FALSE(base::PathExists(out));
}

TEST_F(UnzipTest, DecodeXz_Cancel) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  base::FilePath in = GetArchivePath("file1.xz");
  mojo::PendingRemote<mojom::Unzipper> unzipper;
  receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());
  base::RunLoop run_loop;
  DecodeXz(std::move(unzipper), in, out,
           base::BindLambdaForTesting([&](bool success) {
             EXPECT_FALSE(success);
             run_loop.Quit();
           }))
      .Run();
  run_loop.Run();
  EXPECT_FALSE(base::PathExists(out));
}

TEST_F(UnzipTest, DecodeXz_CancelAfterReturn) {
  base::FilePath out = unzip_dir_.AppendASCII("out");
  base::FilePath in = GetArchivePath("file1.xz");
  base::OnceClosure cancellation;
  int call_count = 0;
  {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    cancellation = DecodeXz(std::move(unzipper), in, out,
                            base::BindLambdaForTesting([&](bool success) {
                              EXPECT_TRUE(success);
                              ++call_count;
                              run_loop.Quit();
                            }));
    run_loop.Run();
    EXPECT_EQ(call_count, 1);
  }
  std::move(cancellation).Run();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(call_count, 1);
}

TEST_F(UnzipTest, UnzipZip64InZip64) {
  // Unzip outer ZIP.
  EXPECT_TRUE(DoUnzip(GetArchivePath("zip64_in_zip64.zip"), unzip_dir_));
  base::FilePath extracted_file = unzip_dir_.Append(FILE_PATH_LITERAL("-"));

  // Confirm the extracted file is a ZIP. If the inner Zip64 is mistakenly
  // unzipped, this will fail.
  std::optional<std::vector<uint8_t>> contents =
      base::ReadFileToBytes(extracted_file);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->size() >= 4);
  ASSERT_EQ((*contents)[0], 0x50);
  ASSERT_EQ((*contents)[1], 0x4b);
  ASSERT_EQ((*contents)[2], 0x03);
  ASSERT_EQ((*contents)[3], 0x04);

  // Extract the inner ZIP.
  base::FilePath inner_unzip_dir =
      unzip_dir_.Append(FILE_PATH_LITERAL("inner_out"));
  ASSERT_TRUE(base::CreateDirectory(inner_unzip_dir));
  EXPECT_TRUE(DoUnzip(extracted_file, inner_unzip_dir));

  // Confirm the inner file contents.
  std::string inner_contents;
  ASSERT_TRUE(base::ReadFileToString(
      inner_unzip_dir.Append(FILE_PATH_LITERAL("-")), &inner_contents));
  EXPECT_EQ(inner_contents, "innermost\n");
}

TEST_F(UnzipTest, UnzipZip64InZip) {
  // Unzip outer ZIP.
  EXPECT_TRUE(DoUnzip(GetArchivePath("zip64_in_zip.zip"), unzip_dir_));
  base::FilePath extracted_file =
      unzip_dir_.Append(FILE_PATH_LITERAL("zip64.zip"));

  // Confirm the extracted file.
  std::optional<std::vector<uint8_t>> contents =
      base::ReadFileToBytes(extracted_file);
  // If "-" exists but not "zip64.zip", the inner zip64 was unzipped.
  ASSERT_TRUE(contents) << "Failed to read `zip64.zip`; does `-` exist? "
                        << base::PathExists(
                               unzip_dir_.Append(FILE_PATH_LITERAL("-")));
  ASSERT_TRUE(contents->size() >= 4);

  // Note: If this is instead 'i', the inner ZIP was unzipped.
  ASSERT_EQ((*contents)[0], 0x50);
  ASSERT_EQ((*contents)[1], 0x4b);
  ASSERT_EQ((*contents)[2], 0x03);
  ASSERT_EQ((*contents)[3], 0x04);

  // Extract the inner ZIP.
  base::FilePath inner_unzip_dir =
      unzip_dir_.Append(FILE_PATH_LITERAL("inner_out"));
  ASSERT_TRUE(base::CreateDirectory(inner_unzip_dir));
  EXPECT_TRUE(DoUnzip(extracted_file, inner_unzip_dir));

  // Confirm the inner file contents.
  std::string inner_contents;
  ASSERT_TRUE(base::ReadFileToString(
      inner_unzip_dir.Append(FILE_PATH_LITERAL("-")), &inner_contents));
  EXPECT_EQ(inner_contents, "innermost\n");
}

// zip_with_two_interesting_zip64s.zip is a non-zip64 zip that contains two
// zip64s. The second zip64 has the interesting property that the "relative
// offset" in its zip64 EoCDL, if interpreted as a relative offset in the
// outer zip, happens to point at the (well-formed) zip64 EoCDR of the first
// zip64. A parser that simply scans for zip64 EoCDLs is likely to get
// confused by this.
TEST_F(UnzipTest, UnzipZipWithInterestingZip64s) {
  // Unzip outer ZIP.
  EXPECT_TRUE(DoUnzip(GetArchivePath("zip_with_two_interesting_zip64s.zip"),
                      unzip_dir_));
  // Two inner ZIPs. If there's only one, it's probably the contents of the
  // inner ZIP.
  EXPECT_EQ(2, CountFiles(unzip_dir_));
}

class FakeUnzipper : public mojom::Unzipper {
 public:
  using UnzipHandler = base::RepeatingCallback<void(
      mojo::PendingRemote<storage::mojom::Directory>,
      mojo::PendingRemote<mojom::UnzipFilter>,
      UnzipCallback)>;

  explicit FakeUnzipper(UnzipHandler handler) : handler_(std::move(handler)) {}
  ~FakeUnzipper() override = default;

  void Bind(mojo::PendingReceiver<mojom::Unzipper> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::Unzipper:
  void Unzip(base::File zip_file,
             mojo::PendingRemote<storage::mojom::Directory> output_dir,
             mojom::UnzipOptionsPtr options,
             mojo::PendingRemote<mojom::UnzipFilter> filter,
             mojo::PendingRemote<mojom::UnzipListener> listener,
             UnzipCallback callback) override {
    handler_.Run(std::move(output_dir), std::move(filter), std::move(callback));
  }

  void DetectEncoding(base::File zip_file,
                      DetectEncodingCallback callback) override {
    std::move(callback).Run(UNKNOWN_ENCODING);
  }

  void GetExtractedInfo(base::File zip_file,
                        GetExtractedInfoCallback callback) override {
    std::move(callback).Run(mojom::Info::New());
  }

  void DecodeXz(base::File in_file,
                base::File out_file,
                DecodeXzCallback callback) override {
    std::move(callback).Run(false);
  }

 private:
  UnzipHandler handler_;
  mojo::ReceiverSet<mojom::Unzipper> receivers_;
};

bool DoFakeUnzip(const base::FilePath& output_dir,
                 FakeUnzipper::UnzipHandler handler,
                 UnzipFilterCallback filter_callback = AllContents()) {
  FakeUnzipper fake_unzipper(std::move(handler));
  mojo::PendingRemote<mojom::Unzipper> unzipper_remote;
  fake_unzipper.Bind(unzipper_remote.InitWithNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  bool result = false;
  Unzip(std::move(unzipper_remote), GetArchivePath("good_archive.zip"),
        output_dir, mojom::UnzipOptions::New(), std::move(filter_callback),
        base::DoNothing(), base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.Quit();
        }));
  run_loop.Run();
  return result;
}

// Verifies that the Directory capability is revoked before the Unzip
// completion callback runs (both on normal completion and on cancellation),
// preventing a compromised utility process from modifying the output directory
// after Unzip completes.
TEST_F(UnzipTest, DirectoryRevokedBeforeCompletionCallback) {
  for (bool cancel : {false, true}) {
    SCOPED_TRACE(cancel);
    mojo::Remote<storage::mojom::Directory> retained_dir;
    mojom::Unzipper::UnzipCallback held_callback;
    base::RunLoop unzip_started_loop;

    FakeUnzipper fake_unzipper(base::BindLambdaForTesting(
        [&](mojo::PendingRemote<storage::mojom::Directory> output_dir,
            mojo::PendingRemote<mojom::UnzipFilter> filter,
            mojom::Unzipper::UnzipCallback callback) {
          retained_dir.Bind(std::move(output_dir));
          retained_dir.FlushForTesting();
          if (cancel) {
            held_callback = std::move(callback);
            unzip_started_loop.Quit();
          } else {
            std::move(callback).Run(true);
          }
        }));

    mojo::PendingRemote<mojom::Unzipper> unzipper_remote;
    fake_unzipper.Bind(unzipper_remote.InitWithNewPipeAndPassReceiver());

    base::RunLoop completion_loop;
    base::OnceClosure cancel_closure =
        Unzip(std::move(unzipper_remote), GetArchivePath("good_archive.zip"),
              unzip_dir_, mojom::UnzipOptions::New(), AllContents(),
              base::DoNothing(), base::BindLambdaForTesting([&](bool success) {
                EXPECT_EQ(!cancel, success);
                retained_dir.FlushForTesting();
                EXPECT_FALSE(retained_dir.is_connected());
                completion_loop.Quit();
              }));

    if (cancel) {
      unzip_started_loop.Run();
      EXPECT_TRUE(retained_dir.is_connected());
      std::move(cancel_closure).Run();
    }
    completion_loop.Run();

    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::File created_file;
    EXPECT_FALSE(retained_dir->OpenFile(
        base::FilePath(FILE_PATH_LITERAL("post_unzip.txt")),
        storage::mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists,
        storage::mojom::FileReadAccess::kReadAllowed,
        storage::mojom::FileWriteAccess::kWriteAllowed, &error, &created_file));
    EXPECT_EQ(0, CountFiles(unzip_dir_));
  }
}

// Verifies that `UnzipFilter::ShouldUnzipFile` rejects unsafe or non-normalized
// paths without passing them to `filter_callback`.
TEST_F(UnzipTest, FilterNormalizesPaths) {
  EXPECT_TRUE(DoFakeUnzip(
      unzip_dir_,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<storage::mojom::Directory> output_dir_pending,
              mojo::PendingRemote<mojom::UnzipFilter> filter_pending,
              mojom::Unzipper::UnzipCallback callback) {
            mojo::Remote<mojom::UnzipFilter> filter(std::move(filter_pending));

            bool should_unzip = true;
            filter->ShouldUnzipFile(
                base::FilePath(FILE_PATH_LITERAL("../evil.txt")),
                &should_unzip);
            EXPECT_FALSE(should_unzip);

            should_unzip = true;
            filter->ShouldUnzipFile(unzip_dir_.AppendASCII("absolute.txt"),
                                    &should_unzip);
            EXPECT_FALSE(should_unzip);

            should_unzip = true;
            filter->ShouldUnzipFile(
                base::FilePath(FILE_PATH_LITERAL("./manifest.json")),
                &should_unzip);
            EXPECT_FALSE(should_unzip);

            should_unzip = false;
            filter->ShouldUnzipFile(
                base::FilePath(FILE_PATH_LITERAL("allowed.txt")),
                &should_unzip);
            EXPECT_TRUE(should_unzip);

            std::move(callback).Run(true);
          }),
      base::BindRepeating([](const base::FilePath& path) {
        EXPECT_FALSE(path.IsAbsolute());
        EXPECT_FALSE(path.ReferencesParent());
        return path != base::FilePath(FILE_PATH_LITERAL("manifest.json"));
      })));
}

// Verifies that the Directory capability enforces the filter callback and
// create-only open mode, and denies renaming or locking files.
TEST_F(UnzipTest, DirectoryEnforcesFilterAndOpenMode) {
  const base::FilePath existing_file = unzip_dir_.AppendASCII("manifest.json");
  ASSERT_TRUE(base::WriteFile(existing_file, "original"));

  EXPECT_TRUE(DoFakeUnzip(
      unzip_dir_,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<storage::mojom::Directory> output_dir_pending,
              mojo::PendingRemote<mojom::UnzipFilter> filter_pending,
              mojom::Unzipper::UnzipCallback callback) {
            mojo::Remote<storage::mojom::Directory> output_dir(
                std::move(output_dir_pending));
            auto open_file = [&](const base::FilePath::CharType* name,
                                 storage::mojom::FileOpenMode mode =
                                     storage::mojom::FileOpenMode::
                                         kCreateAndOpenOnlyIfNotExists) {
              base::File::Error error = base::File::FILE_ERROR_FAILED;
              base::File file;
              output_dir->OpenFile(
                  base::FilePath(name), mode,
                  storage::mojom::FileReadAccess::kReadAllowed,
                  storage::mojom::FileWriteAccess::kWriteAllowed, &error,
                  &file);
              return error;
            };

            // Attempting to open a file excluded by the filter (directly or via
            // a "." path component) must be denied even if ShouldUnzipFile() is
            // never called.
            EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
                      open_file(FILE_PATH_LITERAL("manifest.json")));
            EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
                      open_file(FILE_PATH_LITERAL("./manifest.json")));

#if BUILDFLAG(IS_WIN)
            // On Windows, trailing dots/spaces, NTFS alternate data streams,
            // root-relative paths, and drive-relative paths must not bypass the
            // filter.
            for (const auto* candidate : {
                     FILE_PATH_LITERAL("manifest.json."),
                     FILE_PATH_LITERAL("manifest.json "),
                     FILE_PATH_LITERAL("x.exe::$DATA"),
                     FILE_PATH_LITERAL("\\manifest.json"),
                     FILE_PATH_LITERAL("C:manifest.json"),
                 }) {
              EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
                        open_file(candidate))
                  << candidate;
            }
#endif

            // Creating an allowed file with kCreateAndOpenOnlyIfNotExists
            // succeeds, but overwriting it with kAlwaysCreate is denied.
            EXPECT_EQ(base::File::FILE_OK,
                      open_file(FILE_PATH_LITERAL("allowed.txt")));
            EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
                      open_file(FILE_PATH_LITERAL("allowed.txt"),
                                storage::mojom::FileOpenMode::kAlwaysCreate));

            // Attempting to call an unsupported Directory method (such as
            // RenameFile) must report a bad Mojo message and disconnect the
            // Directory pipe.
            mojo::test::BadMessageObserver bad_message_observer;
            base::File::Error error = base::File::FILE_OK;
            EXPECT_FALSE(output_dir->RenameFile(
                base::FilePath(FILE_PATH_LITERAL("allowed.txt")),
                base::FilePath(FILE_PATH_LITERAL("manifest.json")), &error));
            EXPECT_EQ("Unexpected Directory::RenameFile from unzipper",
                      bad_message_observer.WaitForBadMessage());
            output_dir.FlushForTesting();
            EXPECT_FALSE(output_dir.is_connected());

            std::move(callback).Run(true);
          }),
      base::BindRepeating([](const base::FilePath& path) {
        return path != base::FilePath(FILE_PATH_LITERAL("manifest.json")) &&
               !path.MatchesExtension(FILE_PATH_LITERAL(".exe"));
      })));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(existing_file, &content));
  EXPECT_EQ("original", content);
}

// Verifies that `DeleteFile` is restricted to files created during the current
// operation, erases the path from `created_files_` on deletion, and allows
// re-creating and deleting a previously deleted path.
TEST_F(UnzipTest, DirectoryAllowsSingleDeleteOfCreatedFilesOnly) {
  const base::FilePath existing_file = unzip_dir_.AppendASCII("manifest.json");
  ASSERT_TRUE(base::WriteFile(existing_file, "original"));
  const base::FilePath existing_allowed =
      unzip_dir_.AppendASCII("preexisting.txt");
  ASSERT_TRUE(base::WriteFile(existing_allowed, "keep"));

  EXPECT_TRUE(DoFakeUnzip(
      unzip_dir_,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<storage::mojom::Directory> output_dir_pending,
              mojo::PendingRemote<mojom::UnzipFilter> filter_pending,
              mojom::Unzipper::UnzipCallback callback) {
            mojo::Remote<storage::mojom::Directory> output_dir(
                std::move(output_dir_pending));
            auto create_allowed = [&]() {
              base::File::Error error = base::File::FILE_ERROR_FAILED;
              base::File file;
              output_dir->OpenFile(
                  base::FilePath(FILE_PATH_LITERAL("allowed.txt")),
                  storage::mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists,
                  storage::mojom::FileReadAccess::kReadAllowed,
                  storage::mojom::FileWriteAccess::kWriteAllowed, &error,
                  &file);
              EXPECT_EQ(base::File::FILE_OK, error);
              EXPECT_TRUE(file.IsValid());
            };
            auto delete_file = [&](const base::FilePath::CharType* name) {
              bool deleted = false;
              output_dir->DeleteFile(base::FilePath(name), &deleted);
              return deleted;
            };

            // Attempting to delete a file excluded by the filter (directly or
            // via "./") or a pre-existing file that was not created by this
            // Unzip operation must fail.
            EXPECT_FALSE(delete_file(FILE_PATH_LITERAL("manifest.json")));
            EXPECT_FALSE(delete_file(FILE_PATH_LITERAL("./manifest.json")));
            EXPECT_FALSE(delete_file(FILE_PATH_LITERAL("preexisting.txt")));

            // Create an allowed file, delete it once, and verify a second
            // delete without re-creating it fails.
            create_allowed();
            EXPECT_TRUE(delete_file(FILE_PATH_LITERAL("allowed.txt")));
            EXPECT_FALSE(delete_file(FILE_PATH_LITERAL("allowed.txt")));

            // Re-creating a deleted file via OpenFile and deleting it again
            // (e.g. if a duplicate zip entry also fails) is allowed.
            create_allowed();
            EXPECT_TRUE(delete_file(FILE_PATH_LITERAL("allowed.txt")));

            std::move(callback).Run(true);
          }),
      base::BindRepeating([](const base::FilePath& path) {
        return path != base::FilePath(FILE_PATH_LITERAL("manifest.json"));
      })));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(existing_file, &content));
  EXPECT_EQ("original", content);
  ASSERT_TRUE(base::ReadFileToString(existing_allowed, &content));
  EXPECT_EQ("keep", content);
  EXPECT_FALSE(base::PathExists(unzip_dir_.AppendASCII("allowed.txt")));
}

// Verifies that archive entries whose names end in a dot or a space are
// extracted rather than silently skipped by path normalization.
TEST_F(UnzipTest, UnzipArchiveWithTrailingDotsAndSpaces) {
  base::ScopedTempDir archive_dir;
  ASSERT_TRUE(archive_dir.CreateUniqueTempDir());
  const base::FilePath backing_file = archive_dir.GetPath().AppendASCII("src");
  ASSERT_TRUE(base::WriteFile(backing_file, "payload"));

  class TestFileAccessor : public zip::FileAccessor {
   public:
    explicit TestFileAccessor(const base::FilePath& backing_file)
        : backing_file_(backing_file) {}

    bool Open(zip::Paths paths, std::vector<base::File>* files) override {
      for (const auto& path : paths) {
        if (path == base::FilePath(FILE_PATH_LITERAL("dir.")) ||
            path == base::FilePath(FILE_PATH_LITERAL("dir "))) {
          files->emplace_back();
        } else {
          files->emplace_back(backing_file_,
                              base::File::FLAG_OPEN | base::File::FLAG_READ);
        }
      }
      return true;
    }

    bool List(const base::FilePath& path,
              std::vector<base::FilePath>* files,
              std::vector<base::FilePath>* subdirs) override {
      return true;
    }

    bool GetInfo(const base::FilePath& path, Info* info) override {
      info->is_directory = (path == base::FilePath(FILE_PATH_LITERAL("dir.")) ||
                            path == base::FilePath(FILE_PATH_LITERAL("dir ")));
      return true;
    }

   private:
    const base::FilePath backing_file_;
  };

  TestFileAccessor file_accessor(backing_file);
  const base::FilePath zip_path = archive_dir.GetPath().AppendASCII("test.zip");
  const base::FilePath src_files[] = {
      base::FilePath(FILE_PATH_LITERAL("trailing_dot.")),
      base::FilePath(FILE_PATH_LITERAL("trailing_space ")),
      base::FilePath(FILE_PATH_LITERAL("dir.")),
      base::FilePath(FILE_PATH_LITERAL("dir ")),
      base::FilePath(FILE_PATH_LITERAL("dir./nested_dot.")),
      base::FilePath(FILE_PATH_LITERAL("dir /nested_space ")),
  };
  zip::ZipParams params{
      .file_accessor = &file_accessor,
      .dest_file = zip_path,
      .src_files = src_files,
  };
  ASSERT_TRUE(zip::Zip(params));

  EXPECT_TRUE(DoUnzip(zip_path, unzip_dir_));
  bool some_files_empty = false;
  EXPECT_EQ(4, CountFiles(unzip_dir_, &some_files_empty));
  EXPECT_FALSE(some_files_empty);
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(base::PathExists(unzip_dir_.AppendASCII("trailing_dot")));
  EXPECT_TRUE(base::PathExists(unzip_dir_.AppendASCII("trailing_space")));
  EXPECT_TRUE(base::PathExists(
      unzip_dir_.AppendASCII("dir").AppendASCII("nested_dot")));
  EXPECT_TRUE(base::PathExists(
      unzip_dir_.AppendASCII("dir").AppendASCII("nested_space")));
#else
  EXPECT_TRUE(base::PathExists(unzip_dir_.AppendASCII("trailing_dot.")));
  EXPECT_TRUE(base::PathExists(unzip_dir_.AppendASCII("trailing_space ")));
  EXPECT_TRUE(base::PathExists(
      unzip_dir_.AppendASCII("dir.").AppendASCII("nested_dot.")));
  EXPECT_TRUE(base::PathExists(
      unzip_dir_.AppendASCII("dir ").AppendASCII("nested_space ")));
#endif
}

}  // namespace
}  // namespace unzip

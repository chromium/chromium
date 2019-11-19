// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unzip {

namespace {

// Note: this method has to return void for the ASSERTION_* to compile.
void GetArchivePath(const base::FilePath::StringPieceType& archive_name,
                    base::FilePath* path) {
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, path));
  *path = path->Append(FILE_PATH_LITERAL("components"))
              .Append(FILE_PATH_LITERAL("test"))
              .Append(FILE_PATH_LITERAL("data"))
              .Append(FILE_PATH_LITERAL("unzip_service"))
              .Append(archive_name);
  ASSERT_TRUE(base::PathExists(*path));
}

// Sets the number of files under |dir| in |file_count| and if
// |some_files_empty| is not null, sets it to true if at least one of the files
// is empty.
void CountFiles(const base::FilePath& dir,
                int64_t* file_count,
                bool* some_files_empty) {
  ASSERT_TRUE(file_count);
  *file_count = 0;
  base::FileEnumerator file_enumerator(dir, /*recursive=*/true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    if (some_files_empty) {
      int64_t file_size = 0;
      ASSERT_TRUE(base::GetFileSize(path, &file_size));
      if (file_size == 0) {
        *some_files_empty = true;
        some_files_empty = nullptr;  // So we don't check files again.
      }
    }
    (*file_count)++;
  }
}

class UnzipTest : public testing::Test {
 public:
  UnzipTest() = default;
  ~UnzipTest() override = default;

  // Unzips |zip_file| into |output_dir| and returns true if the unzip was
  // successful.
  bool DoUnzip(const base::FilePath& zip_file,
               const base::FilePath& output_dir) {
    return DoUnzipWithFilter(zip_file, output_dir, UnzipFilterCallback());
  }

  // Same as DoUnzip() but only extract files for which |filter_callback|
  // returns true.
  bool DoUnzipWithFilter(const base::FilePath& zip_file,
                         const base::FilePath& output_dir,
                         UnzipFilterCallback filter_callback) {
    mojo::PendingRemote<mojom::Unzipper> unzipper;
    receivers_.Add(&unzipper_, unzipper.InitWithNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    bool result = false;

    UnzipCallback result_callback = base::BindOnce(
        [](base::OnceClosure quit_closure, bool* out_result, bool result) {
          *out_result = result;
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &result);

    if (filter_callback) {
      UnzipWithFilter(std::move(unzipper), zip_file, output_dir,
                      std::move(filter_callback), std::move(result_callback));
    } else {
      Unzip(std::move(unzipper), zip_file, output_dir,
            std::move(result_callback));
    }
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

TEST_F(UnzipTest, UnzipBadArchive) {
  base::FilePath bad_archive;
  GetArchivePath(FILE_PATH_LITERAL("bad_archive.zip"), &bad_archive);
  EXPECT_FALSE(DoUnzip(bad_archive, unzip_dir_));

  // No files should have been extracted.
  int64_t file_count = -1;
  CountFiles(unzip_dir_, &file_count, /*some_files_empty=*/nullptr);
  EXPECT_EQ(0, file_count);
}

TEST_F(UnzipTest, UnzipGoodArchive) {
  base::FilePath archive;
  GetArchivePath(FILE_PATH_LITERAL("good_archive.zip"), &archive);
  EXPECT_TRUE(DoUnzip(archive, unzip_dir_));

  // Sanity check that the right number of files have been extracted and that
  // they are not empty.
  int64_t file_count = -1;
  bool some_files_empty = false;
  CountFiles(unzip_dir_, &file_count, /*some_files_empty=*/&some_files_empty);
  EXPECT_EQ(8, file_count);
  EXPECT_FALSE(some_files_empty);
}

TEST_F(UnzipTest, UnzipWithFilter) {
  base::FilePath archive;
  GetArchivePath(FILE_PATH_LITERAL("good_archive.zip"), &archive);
  EXPECT_TRUE(DoUnzipWithFilter(
      archive, unzip_dir_,
      base::BindRepeating([](const base::FilePath& path) -> bool {
        return path.MatchesExtension(FILE_PATH_LITERAL(".txt"));
      })));

  // It should only have kept the 2 text files from the archive.
  int64_t file_count = -1;
  bool some_files_empty = false;
  CountFiles(unzip_dir_, &file_count, /*some_files_empty=*/&some_files_empty);
  EXPECT_EQ(2, file_count);
  EXPECT_FALSE(some_files_empty);
}

}  // namespace
}  // namespace unzip

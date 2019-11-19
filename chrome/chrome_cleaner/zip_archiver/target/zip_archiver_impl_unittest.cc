// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/target/zip_archiver_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/zip_archiver/test_zip_archiver_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

const char kTestFilenameInZip[] = "a.txt";
const char kTestPassword[] = "1234";

class ZipArchiverImplTest : public testing::Test {
 public:
  void SetUp() override {
    test_file_.Initialize();
    base::File src_file(test_file_.GetSourceFilePath(),
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
    src_file_handle_ = base::win::ScopedHandle(src_file.TakePlatformFile());
    ASSERT_TRUE(src_file_handle_.IsValid());

    zip_file_path_ = test_file_.GetSourceFilePath().AddExtension(L".zip");
    base::File zip_file(zip_file_path_,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    zip_file_handle_ = base::win::ScopedHandle(zip_file.TakePlatformFile());
    ASSERT_TRUE(zip_file_handle_.IsValid());
  }

 protected:
  ZipArchiverTestFile test_file_;
  base::win::ScopedHandle src_file_handle_;
  base::win::ScopedHandle zip_file_handle_;
  base::FilePath zip_file_path_;

 private:
  base::test::TaskEnvironment task_environment_;
};

void RunArchiver(base::win::ScopedHandle src_file_handle,
                 base::win::ScopedHandle zip_file_handle,
                 const std::string& filename,
                 const std::string& password,
                 mojom::ZipArchiver::ArchiveCallback callback) {
  mojo::Remote<mojom::ZipArchiver> zip_archiver;
  ZipArchiverImpl zip_archiver_impl(
      zip_archiver.BindNewPipeAndPassReceiver(),
      /*connection_error_handler=*/base::DoNothing());
  zip_archiver_impl.Archive(mojo::WrapPlatformFile(src_file_handle.Take()),
                            mojo::WrapPlatformFile(zip_file_handle.Take()),
                            filename, password, std::move(callback));
}

void OnArchiveDone(ZipArchiverResultCode* test_result_code,
                   base::OnceClosure callback,
                   ZipArchiverResultCode result_code) {
  *test_result_code = result_code;
  std::move(callback).Run();
}

void BindArchiverThenRun(base::win::ScopedHandle src_file_handle,
                         base::win::ScopedHandle zip_file_handle,
                         const std::string& filename_in_zip,
                         const std::string& password,
                         mojom::ZipArchiver::ArchiveCallback callback) {
  scoped_refptr<MojoTaskRunner> task_runner = MojoTaskRunner::Create();

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(RunArchiver, std::move(src_file_handle),
                                std::move(zip_file_handle), filename_in_zip,
                                password, std::move(callback)));
}

}  // namespace

TEST_F(ZipArchiverImplTest, Archive) {
  ZipArchiverResultCode test_result_code;
  base::RunLoop loop;
  BindArchiverThenRun(
      std::move(src_file_handle_), std::move(zip_file_handle_),
      kTestFilenameInZip, kTestPassword,
      base::BindOnce(OnArchiveDone, &test_result_code, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(test_result_code, ZipArchiverResultCode::kSuccess);
  test_file_.ExpectValidZipFile(zip_file_path_, kTestFilenameInZip,
                                kTestPassword);
}

TEST_F(ZipArchiverImplTest, EmptyFilename) {
  ZipArchiverResultCode test_result_code;
  base::RunLoop loop;
  BindArchiverThenRun(
      std::move(src_file_handle_), std::move(zip_file_handle_), "",
      kTestPassword,
      base::BindOnce(OnArchiveDone, &test_result_code, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(test_result_code, ZipArchiverResultCode::kErrorInvalidParameter);
}

TEST_F(ZipArchiverImplTest, EmptyPassword) {
  ZipArchiverResultCode test_result_code;
  base::RunLoop loop;
  BindArchiverThenRun(
      std::move(src_file_handle_), std::move(zip_file_handle_),
      kTestFilenameInZip, "",
      base::BindOnce(OnArchiveDone, &test_result_code, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(test_result_code, ZipArchiverResultCode::kErrorInvalidParameter);
}

TEST_F(ZipArchiverImplTest, SourceIsTooBig) {
  base::File src_file(test_file_.GetSourceFilePath(),
                      base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(src_file.IsValid());
  // Increase the size of source file to one byte more than limit.
  ASSERT_TRUE(src_file.SetLength(kQuarantineSourceSizeLimit + 1));
  src_file.Close();

  ZipArchiverResultCode test_result_code;
  base::RunLoop loop;
  BindArchiverThenRun(
      std::move(src_file_handle_), std::move(zip_file_handle_),
      kTestFilenameInZip, kTestPassword,
      base::BindOnce(OnArchiveDone, &test_result_code, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(test_result_code, ZipArchiverResultCode::kErrorSourceFileTooBig);
}

}  // namespace chrome_cleaner

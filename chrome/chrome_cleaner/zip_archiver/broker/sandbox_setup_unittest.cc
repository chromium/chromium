// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/test_zip_archiver_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

const char kTestFilenameInZip[] = "a.txt";
const char kTestPassword[] = "1234";

class ZipArchiverSandboxSetupTest : public base::MultiProcessTest {
 public:
  ZipArchiverSandboxSetupTest()
      : mojo_task_runner_(MojoTaskRunner::Create()),
        zip_archiver_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
    ZipArchiverSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(), base::BindOnce([] {
          FAIL() << "ZipArchiver sandbox connection error";
        }));
    CHECK_EQ(
        RESULT_CODE_SUCCESS,
        StartSandboxTarget(MakeCmdLine("ZipArchiverSandboxSetupTargetMain"),
                           &setup_hooks, SandboxType::kTest));
    zip_archiver_ = setup_hooks.TakeZipArchiverRemote();
  }

 protected:
  void PostArchiveTask(base::win::ScopedHandle src_file_handle,
                       base::win::ScopedHandle zip_file_handle,
                       const std::string& filename_in_zip,
                       const std::string& password,
                       mojom::ZipArchiver::ArchiveCallback callback) {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(RunArchive, base::Unretained(zip_archiver_.get()),
                       mojo::WrapPlatformFile(src_file_handle.Take()),
                       mojo::WrapPlatformFile(zip_file_handle.Take()),
                       filename_in_zip, password, std::move(callback)));
  }

 private:
  static void RunArchive(mojo::Remote<mojom::ZipArchiver>* zip_archiver,
                         mojo::ScopedHandle mojo_src_handle,
                         mojo::ScopedHandle mojo_zip_handle,
                         const std::string& filename_in_zip,
                         const std::string& password,
                         mojom::ZipArchiver::ArchiveCallback callback) {
    DCHECK(zip_archiver);

    (*zip_archiver)
        ->Archive(std::move(mojo_src_handle), std::move(mojo_zip_handle),
                  filename_in_zip, password, std::move(callback));
  }

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteZipArchiverPtr zip_archiver_;
  base::test::TaskEnvironment task_environment_;
};

void OnArchiveDone(ZipArchiverResultCode* test_result_code,
                   base::OnceClosure callback,
                   ZipArchiverResultCode result_code) {
  *test_result_code = result_code;
  std::move(callback).Run();
}

}  // namespace

MULTIPROCESS_TEST_MAIN(ZipArchiverSandboxSetupTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  // |RunZipArchiverSandboxTarget| won't return. The mojo error handler will
  // abort this process when the connection is broken.
  RunZipArchiverSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                              sandbox_target_services);

  return 0;
}

TEST_F(ZipArchiverSandboxSetupTest, Archive) {
  ZipArchiverTestFile test_file;
  test_file.Initialize();

  base::File src_file(test_file.GetSourceFilePath(),
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::win::ScopedHandle src_file_handle(src_file.TakePlatformFile());
  ASSERT_TRUE(src_file_handle.IsValid());

  const base::FilePath zip_file_path =
      test_file.GetSourceFilePath().AddExtension(L".zip");
  base::File zip_file(zip_file_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::win::ScopedHandle zip_file_handle(zip_file.TakePlatformFile());
  ASSERT_TRUE(zip_file_handle.IsValid());

  ZipArchiverResultCode test_result_code;
  base::RunLoop loop;
  PostArchiveTask(
      std::move(src_file_handle), std::move(zip_file_handle),
      kTestFilenameInZip, kTestPassword,
      base::BindOnce(OnArchiveDone, &test_result_code, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(test_result_code, ZipArchiverResultCode::kSuccess);
  test_file.ExpectValidZipFile(zip_file_path, kTestFilenameInZip,
                               kTestPassword);
}

}  // namespace chrome_cleaner

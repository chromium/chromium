// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/sandboxed_zip_archiver.h"
#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/test_zip_archiver_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

constexpr char kTestPassword[] = "1234";
constexpr char kTestSymlink[] = "a.link";
constexpr uint32_t kProhibitedAccessPermissions[] = {
    DELETE,       READ_CONTROL,     WRITE_DAC,
    WRITE_OWNER,  FILE_APPEND_DATA, FILE_EXECUTE,
    FILE_READ_EA, FILE_WRITE_EA,    FILE_WRITE_ATTRIBUTES};

void OnArchiveDone(ZipArchiverResultCode* test_result_code,
                   base::OnceClosure done_callback,
                   ZipArchiverResultCode result_code) {
  *test_result_code = result_code;
  std::move(done_callback).Run();
}

class ZipArchiverSandboxedArchiverTest : public base::MultiProcessTest {
 public:
  void SetUp() override {
    scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
    ZipArchiverSandboxSetupHooks setup_hooks(
        mojo_task_runner.get(), base::BindOnce([] {
          FAIL() << "ZipArchiver sandbox connection error";
        }));
    ASSERT_EQ(RESULT_CODE_SUCCESS,
              StartSandboxTarget(MakeCmdLine("SandboxedZipArchiverTargetMain"),
                                 &setup_hooks, SandboxType::kTest));
    RemoteZipArchiverPtr zip_archiver = setup_hooks.TakeZipArchiverRemote();

    test_file_.Initialize();
    const base::FilePath& src_file_path = test_file_.GetSourceFilePath();

    std::string src_file_hash;
    ComputeSHA256DigestOfPath(src_file_path, &src_file_hash);

    const base::string16 zip_filename = internal::ConstructZipArchiveFileName(
        src_file_path.BaseName().value(), src_file_hash,
        /*max_filename_length=*/255);

    const base::FilePath& dst_archive_folder = test_file_.GetTempDirPath();
    expect_zip_file_path_ = dst_archive_folder.Append(zip_filename);

    zip_archiver_ = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner, std::move(zip_archiver), dst_archive_folder,
        kTestPassword);
  }

 protected:
  ZipArchiverResultCode Archive(const base::FilePath& path) {
    ZipArchiverResultCode result_code;
    base::RunLoop loop;
    // Unretained pointer is safe here since we will wait for the task.
    zip_archiver_->Archive(
        path, base::BindOnce(OnArchiveDone, base::Unretained(&result_code),
                             loop.QuitClosure()));
    loop.Run();
    return result_code;
  }

  ZipArchiverTestFile test_file_;
  base::FilePath expect_zip_file_path_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SandboxedZipArchiver> zip_archiver_;
};

// ArgumentVerifyingFakeArchiver runs and handles Archive requests in the
// sandbox child process. It checks if the parameters passed in the sandbox are
// configured correctly and if some checks are done in SandboxedZipArchiver
// before sending requests to the sandbox. It doesn't do real archiving.
class ArgumentVerifyingFakeArchiver : public mojom::ZipArchiver {
 public:
  explicit ArgumentVerifyingFakeArchiver(
      mojo::PendingReceiver<mojom::ZipArchiver> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        [] { FAIL() << "ZipArchiver sandbox connection error"; }));
  }

  ~ArgumentVerifyingFakeArchiver() override = default;

  void Archive(mojo::ScopedHandle src_file_handle,
               mojo::ScopedHandle zip_file_handle,
               const std::string& filename_in_zip,
               const std::string& password,
               ArchiveCallback callback) override {
    HANDLE raw_src_file_handle;
    if (mojo::UnwrapPlatformFile(std::move(src_file_handle),
                                 &raw_src_file_handle) != MOJO_RESULT_OK) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }
    base::File src_file(raw_src_file_handle);
    if (!src_file.IsValid()) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    // Test if the size of the source file has been checked.
    if (src_file.GetLength() > kQuarantineSourceSizeLimit) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    HANDLE raw_zip_file_handle;
    if (mojo::UnwrapPlatformFile(std::move(zip_file_handle),
                                 &raw_zip_file_handle) != MOJO_RESULT_OK) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }
    base::File zip_file(raw_zip_file_handle);
    if (!zip_file.IsValid()) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    // Test general prohibited file access permissions.
    for (uint32_t permission : kProhibitedAccessPermissions) {
      if (HasPermission(src_file, permission) ||
          HasPermission(zip_file, permission)) {
        std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
        return;
      }
    }

    // Check if |src_file| and |zip_file| have incorrect file access
    // permissions.
    if (HasPermission(src_file, FILE_WRITE_DATA) ||
        HasPermission(zip_file, FILE_READ_DATA)) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    std::move(callback).Run(ZipArchiverResultCode::kSuccess);
  }

 private:
  static bool HasPermission(const base::File& file, uint32_t permission) {
    HANDLE temp_handle;
    if (::DuplicateHandle(::GetCurrentProcess(), file.GetPlatformFile(),
                          ::GetCurrentProcess(), &temp_handle, permission,
                          false, 0)) {
      CloseHandle(temp_handle);
      return true;
    }
    return false;
  }

  mojo::Receiver<mojom::ZipArchiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentVerifyingFakeArchiver);
};

}  // namespace

MULTIPROCESS_TEST_MAIN(SandboxedZipArchiverTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  // |RunZipArchiverSandboxTarget| won't return. The mojo error handler will
  // abort this process when the connection is broken.
  RunZipArchiverSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                              sandbox_target_services);

  return 0;
}

TEST_F(ZipArchiverSandboxedArchiverTest, Archive) {
  EXPECT_EQ(ZipArchiverResultCode::kSuccess,
            Archive(test_file_.GetSourceFilePath()));

  test_file_.ExpectValidZipFile(
      expect_zip_file_path_,
      test_file_.GetSourceFilePath().BaseName().AsUTF8Unsafe(), kTestPassword);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceFileNotFound) {
  ASSERT_TRUE(base::DeleteFile(test_file_.GetSourceFilePath(), false));

  EXPECT_EQ(ZipArchiverResultCode::kErrorCannotOpenSourceFile,
            Archive(test_file_.GetSourceFilePath()));
}

TEST_F(ZipArchiverSandboxedArchiverTest, ZipFileExists) {
  base::File zip_file(expect_zip_file_path_, base::File::FLAG_CREATE);
  ASSERT_TRUE(zip_file.IsValid());

  EXPECT_EQ(ZipArchiverResultCode::kZipFileExists,
            Archive(test_file_.GetSourceFilePath()));
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsSymbolicLink) {
  base::FilePath symlink_path =
      test_file_.GetTempDirPath().AppendASCII(kTestSymlink);
  ASSERT_TRUE(::CreateSymbolicLink(
      symlink_path.AsUTF16Unsafe().c_str(),
      test_file_.GetSourceFilePath().AsUTF16Unsafe().c_str(), 0));

  EXPECT_EQ(ZipArchiverResultCode::kIgnoredSourceFile, Archive(symlink_path));
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsDirectory) {
  EXPECT_EQ(ZipArchiverResultCode::kIgnoredSourceFile,
            Archive(test_file_.GetTempDirPath()));
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsDefaultFileStream) {
  base::FilePath stream_path(base::StrCat(
      {test_file_.GetSourceFilePath().AsUTF16Unsafe(), L"::$data"}));

  EXPECT_EQ(ZipArchiverResultCode::kSuccess, Archive(stream_path));

  test_file_.ExpectValidZipFile(
      expect_zip_file_path_,
      test_file_.GetSourceFilePath().BaseName().AsUTF8Unsafe(), kTestPassword);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsNonDefaultFileStream) {
  base::FilePath stream_path(base::StrCat(
      {test_file_.GetSourceFilePath().AsUTF16Unsafe(), L":stream:$data"}));
  base::File stream_file(stream_path, base::File::FLAG_CREATE);
  ASSERT_TRUE(stream_file.IsValid());

  EXPECT_EQ(ZipArchiverResultCode::kIgnoredSourceFile, Archive(stream_path));
}

TEST_F(ZipArchiverSandboxedArchiverTest, ArchiveOpenedFileWithSharingAccess) {
  const base::FilePath path = test_file_.GetSourceFilePath();
  base::File opened_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE |
                                   base::File::FLAG_DELETE_ON_CLOSE);
  ASSERT_TRUE(opened_file.IsValid());

  EXPECT_EQ(ZipArchiverResultCode::kSuccess, Archive(path));

  test_file_.ExpectValidZipFile(expect_zip_file_path_,
                                path.BaseName().AsUTF8Unsafe(), kTestPassword);
}

namespace {

// ZipArchiverSandboxCheckTest uses ArgumentVerifyingFakeArchiver to check the
// sandbox configuration and if some checks are done in SandboxedZipArchiver
// before sending requests to the sandbox.
class ZipArchiverSandboxCheckTest : public base::MultiProcessTest {
 public:
  ZipArchiverSandboxCheckTest()
      : mojo_task_runner_(MojoTaskRunner::Create()),
        impl_ptr_(nullptr, base::OnTaskRunnerDeleter(mojo_task_runner_)) {
    RemoteZipArchiverPtr zip_archiver(
        new mojo::Remote<mojom::ZipArchiver>(),
        base::OnTaskRunnerDeleter(mojo_task_runner_));

    // Initialize the |impl_ptr_| in the mojo task and wait until it completed.
    base::RunLoop loop;
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ZipArchiverSandboxCheckTest::
                                      InitializeArgumentVerifyingFakeArchiver,
                                  base::Unretained(this), zip_archiver.get(),
                                  loop.QuitClosure()));
    loop.Run();

    test_file_.Initialize();

    zip_archiver_ = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner_, std::move(zip_archiver), test_file_.GetTempDirPath(),
        kTestPassword);
  }

 protected:
  ZipArchiverResultCode Archive(const base::FilePath& path) {
    ZipArchiverResultCode result_code;
    base::RunLoop loop;
    // Unretained pointer is safe here since we will wait for the task.
    zip_archiver_->Archive(
        path, base::BindOnce(OnArchiveDone, base::Unretained(&result_code),
                             loop.QuitClosure()));
    loop.Run();
    return result_code;
  }

  ZipArchiverTestFile test_file_;

 private:
  void InitializeArgumentVerifyingFakeArchiver(
      mojo::Remote<mojom::ZipArchiver>* zip_archiver,
      base::OnceClosure callback) {
    impl_ptr_.reset(new ArgumentVerifyingFakeArchiver(
        zip_archiver->BindNewPipeAndPassReceiver()));
    std::move(callback).Run();
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  std::unique_ptr<SandboxedZipArchiver> zip_archiver_;
  std::unique_ptr<ArgumentVerifyingFakeArchiver, base::OnTaskRunnerDeleter>
      impl_ptr_;
};

}  // namespace

TEST_F(ZipArchiverSandboxCheckTest, CheckPermission) {
  // Let ArgumentVerifyingFakeArchiver check if file handles are opened with
  // correct permissions.
  EXPECT_EQ(ZipArchiverResultCode::kSuccess,
            Archive(test_file_.GetSourceFilePath()));
}

TEST_F(ZipArchiverSandboxCheckTest, SourceIsTooBig) {
  const base::FilePath path = test_file_.GetSourceFilePath();
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(src_file.IsValid());
  // Increase the size of source file to one byte more than limit.
  ASSERT_TRUE(src_file.SetLength(kQuarantineSourceSizeLimit + 1));
  src_file.Close();

  // Expect SandboxedZipArchiver to check file size before sending a request to
  // ArgumentVerifyingFakeArchiver. ArgumentVerifyingFakeArchiver will return
  // kErrorInvalidParameter if the file size isn't checked first.
  EXPECT_EQ(ZipArchiverResultCode::kErrorSourceFileTooBig, Archive(path));
}

}  // namespace chrome_cleaner

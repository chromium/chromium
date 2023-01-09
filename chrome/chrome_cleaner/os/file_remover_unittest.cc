// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_remover.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/test/child_process_logger.h"
#include "chrome/chrome_cleaner/test/file_remover_test_util.h"
#include "chrome/chrome_cleaner/test/reboot_deletion_helper.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_layered_service_provider.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/sandboxed_zip_archiver.h"
#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::Eq;
using testing::Return;
using ValidationStatus = FileRemoverAPI::DeletionValidationStatus;

const wchar_t kRemoveFile1[] = L"remove_one.exe";
const wchar_t kRemoveFile2[] = L"remove_two.exe";
const wchar_t kRemoveFolder[] = L"remove";

class FileRemoverTest : public ::testing::Test {
 protected:
  FileRemoverTest()
      : default_file_remover_(
            /*digest_verifier=*/nullptr,
            /*archiver=*/nullptr,
            LayeredServiceProviderWrapper(),
            base::BindRepeating(&FileRemoverTest::RebootRequired,
                                base::Unretained(this))) {
    FileRemovalStatusUpdater::GetInstance()->Clear();
  }

  void RebootRequired() { reboot_required_ = true; }

  void TestBlacklistedRemoval(FileRemover* remover,
                              const base::FilePath& path) {
    DCHECK(remover);

    EXPECT_EQ(ValidationStatus::FORBIDDEN, remover->CanRemove(path));

    FileRemovalStatusUpdater* removal_status_updater =
        FileRemovalStatusUpdater::GetInstance();

    VerifyRemoveNowFailure(path, remover);
    EXPECT_EQ(removal_status_updater->GetRemovalStatus(path),
              REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);

    removal_status_updater->Clear();
    VerifyRegisterPostRebootRemovalFailure(path, remover);
    EXPECT_EQ(removal_status_updater->GetRemovalStatus(path),
              REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);

    EXPECT_TRUE(base::PathExists(path));
    EXPECT_FALSE(IsFileRegisteredForPostRebootRemoval(path));
  }

  FileRemover default_file_remover_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  bool reboot_required_ = false;
};

bool CreateNetworkedFile(const base::FilePath& path) {
  chrome_cleaner::CreateEmptyFile(path);

  // Fake an attribute that would be present on a networked file.
  // FILE_ATTRIBUTE_OFFLINE was chosen as FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
  // and FILE_ATTRIBUTE_RECALL_ON_OPEN do not seem to be user-settable.
  const DWORD attr = ::GetFileAttributes(path.value().c_str());
  return ::SetFileAttributesW(path.value().c_str(),
                              attr | FILE_ATTRIBUTE_OFFLINE);
}

}  // namespace

TEST_F(FileRemoverTest, RemoveNowValidFile) {
  // Create a temporary empty file.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  const base::FilePath file_path = temp.GetPath().Append(kRemoveFile1);
  EXPECT_TRUE(CreateEmptyFile(file_path));

  // Removing it must succeed.
  VerifyRemoveNowSuccess(file_path, &default_file_remover_);
  EXPECT_FALSE(base::PathExists(file_path));
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(file_path),
      REMOVAL_STATUS_REMOVED);
}

TEST_F(FileRemoverTest, RemoveNowAbsentFile) {
  // Create a non-existing file name.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  const base::FilePath file_path = temp.GetPath().Append(kRemoveFile1);
  EXPECT_FALSE(base::PathExists(file_path));

  // Removing it must not generate an error.
  VerifyRemoveNowSuccess(file_path, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_NOT_FOUND);

  // Ensure the non-existant files with non-existant parents don't generate an
  // error.
  base::FilePath file_path_deeper = file_path.Append(kRemoveFile2);
  VerifyRemoveNowSuccess(file_path_deeper, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path_deeper),
            REMOVAL_STATUS_NOT_FOUND);
}

TEST_F(FileRemoverTest, NoKnownFileRemoval) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FileRemover remover(
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST),
      /*archiver=*/nullptr, LayeredServiceProviderWrapper(), base::DoNothing());

  // Copy the sample DLL to the temp folder.
  base::FilePath dll_path = GetSampleDLLPath();
  ASSERT_TRUE(base::PathExists(dll_path)) << dll_path.value();

  base::FilePath target_dll_path(
      temp_dir.GetPath().Append(dll_path.BaseName()));
  ASSERT_TRUE(base::CopyFile(dll_path, target_dll_path));

  TestBlacklistedRemoval(&remover, target_dll_path);
}

TEST_F(FileRemoverTest, NoSelfRemoval) {
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  TestBlacklistedRemoval(&default_file_remover_, exe_path);
}

TEST_F(FileRemoverTest, NoWhitelistedFileRemoval) {
  base::FilePath program_files_dir =
      PreFetchedPaths::GetInstance()->GetProgramFilesFolder();
  TestBlacklistedRemoval(&default_file_remover_, program_files_dir);
}

TEST_F(FileRemoverTest, NoWhitelistFileTempRemoval) {
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  TestBlacklistedRemoval(&default_file_remover_, temp_dir);
}

TEST_F(FileRemoverTest, NoLSPRemoval) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath provider_path = temp.GetPath().Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(provider_path));

  TestLayeredServiceProvider lsp;
  lsp.AddProvider(kGUID1, provider_path);

  FileRemover remover(/*digest_verifier=*/nullptr, /*archiver=*/nullptr, lsp,
                      base::DoNothing());

  TestBlacklistedRemoval(&remover, provider_path);
}

TEST_F(FileRemoverTest, CanRemoveAbsolutePath) {
  EXPECT_EQ(ValidationStatus::ALLOWED,
            default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\bar")));
}

TEST_F(FileRemoverTest, NoRelativePathRemoval) {
  EXPECT_EQ(ValidationStatus::UNSAFE,
            default_file_remover_.CanRemove(base::FilePath(L"bar.txt")));
}

TEST_F(FileRemoverTest, NoDriveRemoval) {
  EXPECT_EQ(ValidationStatus::UNSAFE,
            default_file_remover_.CanRemove(base::FilePath(L"C:")));
  EXPECT_EQ(ValidationStatus::FORBIDDEN,
            default_file_remover_.CanRemove(base::FilePath(L"C:\\")));
}

TEST_F(FileRemoverTest, NoNetworkedRemoval) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath path = temp.GetPath().Append(kRemoveFile1);
  CreateNetworkedFile(path);

  EXPECT_EQ(ValidationStatus::FORBIDDEN, default_file_remover_.CanRemove(path));
}

TEST_F(FileRemoverTest, NoPathTraversal) {
  EXPECT_EQ(
      ValidationStatus::FORBIDDEN,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\..\\bar")));
  EXPECT_EQ(ValidationStatus::UNSAFE, default_file_remover_.CanRemove(
                                          base::FilePath(L"..\\foo\\bar.dll")));
}

TEST_F(FileRemoverTest, CorrectPathTraversalDetection) {
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\..bar.dll")));
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\bar..dll")));
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo..\\bar.dll")));
}

TEST_F(FileRemoverTest, RemoveNowDoesNotDeleteFolders) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create the folder and the files.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  base::CreateDirectory(subfolder_path);
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));

  // The folder should not be removed.
  VerifyRemoveNowFailure(subfolder_path, &default_file_remover_);
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(subfolder_path),
      REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);
  EXPECT_TRUE(base::PathExists(subfolder_path));
  EXPECT_TRUE(base::PathExists(file_path1));
}

TEST_F(FileRemoverTest, RemoveNowDeletesEmptyFolders) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create the folder and the files.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  base::CreateDirectory(subfolder_path);
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));
  base::FilePath subsubfolder_path = subfolder_path.Append(kRemoveFolder);
  base::CreateDirectory(subsubfolder_path);
  base::FilePath file_path2 = subsubfolder_path.Append(kRemoveFile2);
  ASSERT_TRUE(CreateEmptyFile(file_path2));

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Delete a file in a folder with other stuff, so the folder isn't deleted.
  VerifyRemoveNowSuccess(file_path1, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path1),
            REMOVAL_STATUS_REMOVED);
  EXPECT_TRUE(base::PathExists(subfolder_path));
  EXPECT_FALSE(base::PathExists(file_path1));
  EXPECT_TRUE(base::PathExists(subsubfolder_path));
  EXPECT_TRUE(base::PathExists(file_path2));

  // Delete the file and ensure the two parent folders are deleted since they
  // are now empty.
  VerifyRemoveNowSuccess(file_path2, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path2),
            REMOVAL_STATUS_REMOVED);
  EXPECT_FALSE(base::PathExists(subfolder_path));
  EXPECT_FALSE(base::PathExists(subsubfolder_path));
  EXPECT_FALSE(base::PathExists(file_path2));
}

TEST_F(FileRemoverTest, RemoveNowDeletesEmptyFoldersNotTemp) {
  base::ScopedPathOverride temp_override(base::DIR_TEMP);

  base::FilePath scoped_temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &scoped_temp_dir));

  // Create a file in temp.
  base::FilePath file_path = scoped_temp_dir.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path));

  // Delete the file and ensure Temp isn't deleted since it is whitelisted.
  VerifyRemoveNowSuccess(file_path, &default_file_remover_);
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(file_path),
      REMOVAL_STATUS_REMOVED);
  EXPECT_FALSE(base::PathExists(file_path));
  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  EXPECT_TRUE(base::PathExists(temp_dir));
}

TEST_F(FileRemoverTest, RegisterPostRebootRemoval) {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // When trying to delete a whitelisted file, we should fail to register the
  // file for removal, and no reboot should be required.
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  VerifyRegisterPostRebootRemovalFailure(exe_path, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(exe_path),
            REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);
  EXPECT_FALSE(reboot_required_);

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(kRemoveFile2);

  // When trying to delete an non-existant file, we should return success, but
  // not require a reboot.
  VerifyRegisterPostRebootRemovalSuccess(file_path, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_NOT_FOUND);
  EXPECT_FALSE(reboot_required_);

  // When trying to delete a real file, we should return success and require a
  // reboot.
  ASSERT_TRUE(CreateEmptyFile(file_path));
  VerifyRegisterPostRebootRemovalSuccess(file_path, &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  EXPECT_TRUE(reboot_required_);
  EXPECT_TRUE(IsFileRegisteredForPostRebootRemoval(file_path));
}

TEST_F(FileRemoverTest, RegisterPostRebootRemoval_Directories) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create an empty directory.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  ASSERT_TRUE(base::CreateDirectory(subfolder_path));

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Directories shouldn't be registered for deletion.
  VerifyRegisterPostRebootRemovalFailure(subfolder_path,
                                         &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(subfolder_path),
            REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);

  // Put a file into the directory and ensure the non-empty directory still
  // isn't registered for removal.
  removal_status_updater->Clear();
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));
  VerifyRegisterPostRebootRemovalFailure(subfolder_path,
                                         &default_file_remover_);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(subfolder_path),
            REMOVAL_STATUS_BLOCKLISTED_FOR_REMOVAL);
}

namespace {

constexpr char kTestPassword[] = "1234";
constexpr char kTestContent[] = "Hello World";
constexpr wchar_t kTestFileName[] = L"temp_file.exe";
constexpr wchar_t kTestExpectArchiveName[] =
    L"temp_file.exe_"
    L"A591A6D40BF420404A011733CFB7B190D62C65BF0BCDA32B57B277D9AD9F146E.zip";

class LoggedZipArchiverSandboxSetupHooks : public ZipArchiverSandboxSetupHooks {
 public:
  explicit LoggedZipArchiverSandboxSetupHooks(
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      base::OnceClosure connection_error_handler,
      chrome_cleaner::ChildProcessLogger* child_process_logger)
      : ZipArchiverSandboxSetupHooks(std::move(mojo_task_runner),
                                     std::move(connection_error_handler)),
        child_process_logger_(child_process_logger) {}

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override {
    child_process_logger_->UpdateSandboxPolicy(policy);
    return ZipArchiverSandboxSetupHooks::UpdateSandboxPolicy(policy,
                                                             command_line);
  }

 private:
  chrome_cleaner::ChildProcessLogger* child_process_logger_;
};

class FileRemoverQuarantineTest : public base::MultiProcessTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    use_reboot_removal_ = GetParam();

    ASSERT_TRUE(child_process_logger_.Initialize());

    scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
    LoggedZipArchiverSandboxSetupHooks setup_hooks(
        mojo_task_runner.get(), base::BindOnce([] {
          FAIL() << "ZipArchiver sandbox connection error";
        }),
        &child_process_logger_);
    ResultCode result_code =
        StartSandboxTarget(MakeCmdLine("FileRemoverQuarantineTargetMain"),
                           &setup_hooks, SandboxType::kTest);
    if (result_code != RESULT_CODE_SUCCESS)
      child_process_logger_.DumpLogs();
    ASSERT_EQ(RESULT_CODE_SUCCESS, result_code);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    auto zip_archiver = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner, setup_hooks.TakeZipArchiverRemote(),
        temp_dir_.GetPath(), kTestPassword);
    file_remover_ = std::make_unique<FileRemover>(
        /*digest_verifier=*/nullptr, std::move(zip_archiver),
        LayeredServiceProviderWrapper(), base::DoNothing());
  }

 protected:
  // Do removal corresponding to |use_reboot_removal_|. Also do corresponding
  // check for file existence and removal status.
  void DoAndExpectCorrespondingRemoval(const base::FilePath& path) {
    if (use_reboot_removal_) {
      VerifyRegisterPostRebootRemovalSuccess(path, file_remover_.get());
      EXPECT_EQ(
          REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL,
          FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
      EXPECT_TRUE(base::PathExists(path));
    } else {
      VerifyRemoveNowSuccess(path, file_remover_.get());
      EXPECT_EQ(
          REMOVAL_STATUS_REMOVED,
          FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
      EXPECT_FALSE(base::PathExists(path));
    }
    return;
  }

  bool use_reboot_removal_ = false;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileRemover> file_remover_;
  chrome_cleaner::ChildProcessLogger child_process_logger_;
};

}  // namespace

MULTIPROCESS_TEST_MAIN(FileRemoverQuarantineTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  DCHECK(sandbox_target_services);

  // |RunZipArchiverSandboxTarget| won't return. The mojo error handler will
  // abort this process when the connection is broken.
  RunZipArchiverSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                              sandbox_target_services);

  return 0;
}

TEST_P(FileRemoverQuarantineTest, QuarantineFile) {
  const base::FilePath path = temp_dir_.GetPath().Append(kTestFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  DoAndExpectCorrespondingRemoval(path);
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));

  const base::FilePath expected_archive_path =
      temp_dir_.GetPath().Append(kTestExpectArchiveName);
  EXPECT_TRUE(base::PathExists(expected_archive_path));
}

TEST_P(FileRemoverQuarantineTest, QuarantinesNotActiveFiles) {
  base::FilePath path = temp_dir_.GetPath().Append(L"temp_file.txt");
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  EXPECT_EQ(ValidationStatus::ALLOWED, file_remover_->CanRemove(path));

  DoAndExpectCorrespondingRemoval(path);
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));
}

TEST_P(FileRemoverQuarantineTest, FailToQuarantine) {
  const base::FilePath path = temp_dir_.GetPath().Append(kTestFileName);
  // Acquire exclusive read access to the file, so the archiver can't open the
  // file.
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                            base::File::FLAG_WIN_EXCLUSIVE_READ);
  ASSERT_TRUE(file.IsValid());

  (use_reboot_removal_)
      ? VerifyRegisterPostRebootRemovalFailure(path, file_remover_.get())
      : VerifyRemoveNowFailure(path, file_remover_.get());
  EXPECT_EQ(REMOVAL_STATUS_ERROR_IN_ARCHIVER,
            FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
  EXPECT_EQ(QUARANTINE_STATUS_ERROR,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_P(FileRemoverQuarantineTest, DuplicatedFile) {
  const base::FilePath path = temp_dir_.GetPath().Append(kTestFileName);
  const base::FilePath expected_archive_path =
      temp_dir_.GetPath().Append(kTestExpectArchiveName);

  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));
  DoAndExpectCorrespondingRemoval(path);
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));
  EXPECT_TRUE(base::PathExists(expected_archive_path));

  // The file should not be archived twice if there is already one with the same
  // name and content in the quarantine. So the modified time of the archive
  // should not be updated.
  base::File::Info old_info;
  ASSERT_TRUE(base::GetFileInfo(expected_archive_path, &old_info));

  // Recreate the source file and remove it again.
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));
  DoAndExpectCorrespondingRemoval(path);
  // Although the file won't be archived again, it still has a backup in the
  // quarantine. So the status should be |QUARANTINE_STATUS_QUARANTINED|.
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));
  EXPECT_TRUE(base::PathExists(expected_archive_path));

  base::File::Info new_info;
  ASSERT_TRUE(base::GetFileInfo(expected_archive_path, &new_info));
  EXPECT_EQ(old_info.last_modified, new_info.last_modified);
}

TEST_P(FileRemoverQuarantineTest, DoNotQuarantineSymbolicLink) {
  const base::FilePath path = temp_dir_.GetPath().Append(L"source_temp_file");
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  const base::FilePath sym_path = temp_dir_.GetPath().Append(kTestFileName);
  ASSERT_NE(0, ::CreateSymbolicLink(sym_path.value().c_str(),
                                    path.value().c_str(), 0));

  DoAndExpectCorrespondingRemoval(sym_path);
  EXPECT_EQ(
      QUARANTINE_STATUS_SKIPPED,
      FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(sym_path));

  const base::FilePath expected_archive_path =
      temp_dir_.GetPath().Append(kTestExpectArchiveName);
  EXPECT_FALSE(base::PathExists(expected_archive_path));
  // The original file should exist.
  EXPECT_TRUE(base::PathExists(path));
}

TEST_P(FileRemoverQuarantineTest, QuarantineDefaultFileStream) {
  const base::FilePath path = temp_dir_.GetPath().Append(kTestFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  base::FilePath stream_path(base::StrCat({path.value(), L"::$data"}));
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(stream_path, kTestContent, strlen(kTestContent)));

  DoAndExpectCorrespondingRemoval(stream_path);
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(
                stream_path));

  const base::FilePath expected_archive_path =
      temp_dir_.GetPath().Append(kTestExpectArchiveName);
  EXPECT_TRUE(base::PathExists(expected_archive_path));
}

TEST_P(FileRemoverQuarantineTest, DoNotQuarantineNonDefaultFileStream) {
  const base::FilePath path = temp_dir_.GetPath().Append(kTestFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  base::FilePath stream_path(base::StrCat({path.value(), L":stream:$data"}));
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(stream_path, kTestContent, strlen(kTestContent)));

  DoAndExpectCorrespondingRemoval(stream_path);
  EXPECT_EQ(QUARANTINE_STATUS_SKIPPED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(
                stream_path));
}

TEST_P(FileRemoverQuarantineTest, LongFileName) {
  // Craft a filename that is precisely MAX_PATH.
  static constexpr base::FilePath::StringPieceType kExtension(
      FILE_PATH_LITERAL(".exe"));
  size_t long_filename_length =
      MAX_PATH - temp_dir_.GetPath().value().size() - 1 - kExtension.size() - 1;
  base::FilePath::StringType long_filename(long_filename_length, 'a');
  long_filename.append(kExtension.data(), kExtension.size());

  const base::FilePath path = temp_dir_.GetPath().Append(long_filename);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithContent(path, kTestContent, strlen(kTestContent)));

  DoAndExpectCorrespondingRemoval(path);
  EXPECT_EQ(QUARANTINE_STATUS_QUARANTINED,
            FileRemovalStatusUpdater::GetInstance()->GetQuarantineStatus(path));
}

INSTANTIATE_TEST_SUITE_P(All,
                         FileRemoverQuarantineTest,
                         testing::Bool(),
                         GetParamNameForTest());

}  // namespace chrome_cleaner

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

// A temporary directory and some file names to create in it.
constexpr char kTempDirPathSwitch[] = "temp-dir-path";
constexpr wchar_t kTestFile1[] = L"test_file_1.txt";
constexpr wchar_t kTestFile2[] = L"test_file_2.txt";

const FindFileHandle kInvalidFindFileHandle =
    reinterpret_cast<FindFileHandle>(HandleToHandle64(INVALID_HANDLE_VALUE));

class TestChildProcess : public SandboxChildProcess {
 public:
  explicit TestChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : SandboxChildProcess(std::move(mojo_task_runner)) {}

  bool Initialize() {
    LowerToken();

    temp_dir_path_ = command_line().GetSwitchValuePath(kTempDirPathSwitch);
    if (temp_dir_path_.empty()) {
      LOG(ERROR) << "Initialize failed: Missing " << kTempDirPathSwitch
                 << " switch";
      return false;
    }

    return true;
  }

  base::FilePath temp_dir_path() const { return temp_dir_path_; }

  base::FilePath valid_utf8_path() const {
    return temp_dir_path_.Append(kValidUtf8Name);
  }

  base::FilePath invalid_utf8_path() const {
    return temp_dir_path_.Append(kInvalidUtf8Name);
  }

  base::FilePath test_file_1_path() const {
    return temp_dir_path_.Append(kTestFile1);
  }

  base::FilePath test_file_2_path() const {
    return temp_dir_path_.Append(kTestFile2);
  }

 private:
  ~TestChildProcess() override = default;

  base::FilePath temp_dir_path_;
};

scoped_refptr<TestChildProcess> SetupSandboxedChildProcess() {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  auto child_process = base::MakeRefCounted<TestChildProcess>(mojo_task_runner);
  if (!child_process->Initialize())
    return base::MakeRefCounted<TestChildProcess>(nullptr);
  return child_process;
}

MULTIPROCESS_TEST_MAIN(FindFirstFileSingle) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  // Test invalid inputs first.
  WIN32_FIND_DATAW data;
  EXPECT_EQ(
      SandboxErrorCode::NULL_FIND_HANDLE,
      proxy->FindFirstFile(child_process->valid_utf8_path(), &data, nullptr));

  FindFileHandle handle;
  EXPECT_EQ(
      SandboxErrorCode::NULL_DATA_HANDLE,
      proxy->FindFirstFile(child_process->valid_utf8_path(), nullptr, &handle));
  EXPECT_EQ(static_cast<uint32_t>(SandboxErrorCode::NULL_DATA_HANDLE),
            proxy->FindNextFile(handle, nullptr));

  uint32_t result =
      proxy->FindFirstFile(child_process->valid_utf8_path(), &data, &handle);
  if (kInvalidFindFileHandle == handle) {
    LOG(ERROR) << std::hex
               << "Didn't get a valid handle when trying to open a unicode "
                  "file path. Return code "
               << result;
    return 1;
  }
  if (!base::EqualsCaseInsensitiveASCII(kValidUtf8Name, data.cFileName)) {
    LOG(ERROR) << "Returned file name doesn't match, expected "
               << kValidUtf8Name << " and got " << data.cFileName;
    return 1;
  }

  result = proxy->FindNextFile(handle, &data);
  if (result != ERROR_NO_MORE_FILES) {
    LOG(ERROR) << std::hex
               << "Incorrectly returned additional files. Return code "
               << result;
    return 1;
  }

  result = proxy->FindClose(handle);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << std::hex
               << "Failed to close FindFirstFile handle. Return code "
               << result;
    return 1;
  }

  result =
      proxy->FindFirstFile(child_process->invalid_utf8_path(), &data, &handle);
  if (kInvalidFindFileHandle == handle) {
    LOG(ERROR) << std::hex
               << "Didn't get a valid handle when trying to open an invalid "
                  "utf8 path. Return code "
               << result;
    return 1;
  }

  if (!base::EqualsCaseInsensitiveASCII(kInvalidUtf8Name, data.cFileName)) {
    LOG(ERROR) << "Returned file name doesn't match, expected "
               << kInvalidUtf8Name << " and got " << data.cFileName;
    return 1;
  }

  result = proxy->FindNextFile(handle, &data);
  if (result != ERROR_NO_MORE_FILES) {
    LOG(ERROR) << std::hex
               << "Incorrectly returned additional files. Return code "
               << result;
    return 1;
  }

  result = proxy->FindClose(handle);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << std::hex
               << "Failed to close FindFirstFile handle. Return code "
               << result;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(FindFirstFileMultiple) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  base::FilePath find_path = child_process->temp_dir_path().Append(L"test_*");
  FindFileHandle handle;
  WIN32_FIND_DATAW data;
  uint32_t result = proxy->FindFirstFile(find_path, &data, &handle);
  if (kInvalidFindFileHandle == handle) {
    LOG(ERROR) << std::hex
               << "Didn't get a valid handle when trying to open a "
                  "file path. Return code "
               << result;
    return 1;
  }
  std::wstring first_found = data.cFileName;

  base::string16 file_name_1 =
      child_process->test_file_1_path().BaseName().value();
  base::string16 file_name_2 =
      child_process->test_file_2_path().BaseName().value();
  if (!base::EqualsCaseInsensitiveASCII(file_name_1, data.cFileName) &&
      !base::EqualsCaseInsensitiveASCII(file_name_2, data.cFileName)) {
    LOG(ERROR) << "Returned file name doesn't match, expected " << file_name_1
               << " or " << file_name_2 << " and got " << data.cFileName;
    return 1;
  }

  result = proxy->FindNextFile(handle, &data);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << std::hex << "First call to FindNextFile failed. Return code "
               << result;
    return 1;
  }
  if (!base::EqualsCaseInsensitiveASCII(file_name_1, data.cFileName) &&
      !base::EqualsCaseInsensitiveASCII(file_name_2, data.cFileName)) {
    LOG(ERROR) << "Returned file name doesn't match, expected " << file_name_1
               << " or " << file_name_2 << " and got " << data.cFileName;
    return 1;
  }
  if (first_found == data.cFileName) {
    LOG(ERROR) << "Same file name was returned twice. " << first_found;
    return 1;
  }

  result = proxy->FindNextFile(handle, &data);
  if (result != ERROR_NO_MORE_FILES) {
    LOG(ERROR) << std::hex
               << "Incorrectly returned additional files. Return code "
               << result;
    return 1;
  }

  result = proxy->FindClose(handle);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << std::hex
               << "Failed to close FindFirstFile handle. Return code "
               << result;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(FindFirstFileNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  WIN32_FIND_DATAW data;
  EXPECT_EQ(
      SandboxErrorCode::NULL_FIND_HANDLE,
      proxy->FindFirstFile(child_process->valid_utf8_path(), &data, nullptr));

  child_process->UnbindRequestsRemotes();
  FindFileHandle handle;
  EXPECT_EQ(SandboxErrorCode::INTERNAL_ERROR,
            proxy->FindFirstFile(base::FilePath(L"Name"), &data, &handle));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(FindNextFileNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  FindFileHandle handle = kInvalidFindFileHandle;
  WIN32_FIND_DATAW data;
  EXPECT_EQ(SandboxErrorCode::INTERNAL_ERROR,
            proxy->FindNextFile(handle, &data));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(FindCloseNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  EXPECT_EQ(SandboxErrorCode::INTERNAL_ERROR, proxy->FindClose(0));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(OpenReadOnlyFile) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  base::win::ScopedHandle result = proxy->OpenReadOnlyFile(
      base::FilePath(L"fake_name"), FILE_ATTRIBUTE_NORMAL);
  if (result.IsValid()) {
    LOG(ERROR) << "Didn't get an invalid handle when passing an invalid name. "
                  "Expected "
               << INVALID_HANDLE_VALUE << " but got " << result.Get();
    return 1;
  }

  result = proxy->OpenReadOnlyFile(
      PreFetchedPaths::GetInstance()->GetExecutablePath(),
      FILE_ATTRIBUTE_NORMAL);
  if (!result.IsValid()) {
    LOG(ERROR)
        << "Didn't get a valid handle when trying to open this executable";
    return 1;
  }

  result = proxy->OpenReadOnlyFile(child_process->valid_utf8_path(),
                                   FILE_ATTRIBUTE_NORMAL);
  if (!result.IsValid()) {
    LOG(ERROR)
        << "Didn't get a valid handle when trying to open a unicode file path";
    return 1;
  }
  result = proxy->OpenReadOnlyFile(child_process->invalid_utf8_path(),
                                   FILE_ATTRIBUTE_NORMAL);
  if (!result.IsValid()) {
    LOG(ERROR)
        << "Didn't get a valid handle when trying to open an invalid utf8 path";
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(OpenReadOnlyFileNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineFileRequestsProxy> proxy(
      child_process->GetFileRequestsProxy());

  const base::string16 too_long(std::numeric_limits<int16_t>::max() + 1, '0');
  EXPECT_FALSE(
      proxy->OpenReadOnlyFile(base::FilePath(too_long), FILE_ATTRIBUTE_NORMAL)
          .IsValid());

  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->OpenReadOnlyFile(base::FilePath(), FILE_ATTRIBUTE_NORMAL)
                   .IsValid());

  return ::testing::Test::HasNonfatalFailure();
}

using TestParentProcess = MaybeSandboxedParentProcess<SandboxedParentProcess>;

class EngineFileRequestsProxyTest
    : public ::testing::TestWithParam<const char*> {
 public:
  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();

    parent_process_ = base::MakeRefCounted<TestParentProcess>(
        mojo_task_runner_, TestParentProcess::CallbacksToSetup::kFileRequests);
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<TestParentProcess> parent_process_;
};

TEST_P(EngineFileRequestsProxyTest, TestRequest) {
  base::test::TaskEnvironment task_environment;

  // Create resources that tests running in the sandbox will not have access to
  // create for themselves, even before calling LowerToken.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(CreateEmptyFile(temp_dir.GetPath().Append(kValidUtf8Name)));
  ASSERT_TRUE(CreateEmptyFile(temp_dir.GetPath().Append(kInvalidUtf8Name)));
  ASSERT_TRUE(CreateEmptyFile(temp_dir.GetPath().Append(kTestFile1)));
  ASSERT_TRUE(CreateEmptyFile(temp_dir.GetPath().Append(kTestFile2)));
  parent_process_->AppendSwitchPath(kTempDirPathSwitch, temp_dir.GetPath());

  int32_t exit_code = -1;
  EXPECT_TRUE(
      parent_process_->LaunchConnectedChildProcess(GetParam(), &exit_code));
  EXPECT_EQ(0, exit_code);
}

INSTANTIATE_TEST_SUITE_P(ProxyTests,
                         EngineFileRequestsProxyTest,
                         testing::Values("FindFirstFileSingle",
                                         "FindFirstFileMultiple",
                                         "FindFirstFileNoHangs",
                                         "FindNextFileNoHangs",
                                         "FindCloseNoHangs",
                                         "OpenReadOnlyFile",
                                         "OpenReadOnlyFileNoHangs"),
                         GetParamNameForTest());

}  // namespace

}  // namespace chrome_cleaner

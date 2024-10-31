// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/test/test_util.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"

namespace on_device_translation {
namespace {

const char kMockLibraryName[] = "mock_translate_kit_lib";
const char kMockInvalidFunctionPointerLibraryName[] =
    "mock_invalid_function_pointer_lib";
const char kMockFailingLibraryName[] = "mock_failing_translate_kit_lib";

}  // namespace

base::FilePath GetMockLibraryPath() {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  return exe_path.AppendASCII(base::GetNativeLibraryName(kMockLibraryName));
}

base::FilePath GetMockInvalidFunctionPointerLibraryPath() {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  return exe_path.AppendASCII(
      base::GetNativeLibraryName(kMockInvalidFunctionPointerLibraryName));
}

base::FilePath GetMockFailingLibraryPath() {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  return exe_path.AppendASCII(
      base::GetNativeLibraryName(kMockFailingLibraryName));
}

// static
std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter>
FakeFileOperationProxy::Create(
    mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
    const std::vector<TestFile>& files) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  return std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter>(
      new FakeFileOperationProxy(std::move(proxy_receiver), task_runner, files,
                                 base::PassKey<FakeFileOperationProxy>()),
      base::OnTaskRunnerDeleter(task_runner));
}

FakeFileOperationProxy::FakeFileOperationProxy(
    mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::vector<TestFile>& files,
    base::PassKey<FakeFileOperationProxy>)
    : receiver_(this, std::move(proxy_receiver), task_runner),
      package_dir_(SetupDataDir(files)) {}
FakeFileOperationProxy::~FakeFileOperationProxy() = default;

void FakeFileOperationProxy::FileExists(uint32_t package_index,
                                        const base::FilePath& relative_path,
                                        FileExistsCallback callback) {
  const base::FilePath file_path = GetFilePath(package_index, relative_path);
  if (!base::PathExists(file_path)) {
    // File doesn't exist.
    std::move(callback).Run(/*exists=*/false, /*is_directory=*/false);
    return;
  }
  std::move(callback).Run(
      /*exists=*/true,
      /*is_directory=*/base::DirectoryExists(file_path));
}

void FakeFileOperationProxy::Open(uint32_t package_index,
                                  const base::FilePath& relative_path,
                                  OpenCallback callback) {
  const base::FilePath file_path = GetFilePath(package_index, relative_path);
  std::move(callback).Run(
      file_path.empty() ? base::File()
                        : base::File(file_path, base::File::FLAG_OPEN |
                                                    base::File::FLAG_READ));
}

base::ScopedTempDir SetupDataDir(const std::vector<TestFile>& files) {
  base::ScopedTempDir tmp_dir;
  CHECK(tmp_dir.CreateUniqueTempDir());
  for (const auto& file : files) {
    const auto path =
        tmp_dir.GetPath().Append(base::FilePath::FromASCII(file.relative_path));
    if (!base::DirectoryExists(path.DirName())) {
      CHECK(base::CreateDirectory(path.DirName()));
    }
    CHECK(base::File(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
              .WriteAndCheck(0, base::as_byte_span(file.content)));
  }
  return tmp_dir;
}

base::FilePath FakeFileOperationProxy::GetFilePath(
    uint32_t package_index,
    const base::FilePath& relative_path) {
  return package_dir_.GetPath()
      .AppendASCII(base::NumberToString(package_index))
      .Append(relative_path);
}

}  // namespace on_device_translation

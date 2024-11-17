// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_installer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TestInstaller::TestInstaller()
    : error_(0),
      install_count_(0),
      install_error_(InstallError::NONE),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

TestInstaller::~TestInstaller() {
  // The unpack path is deleted unconditionally by the component state code,
  // which is driving this installer. Therefore, the unpack path must not
  // exist when this object is destroyed.
  if (!unpack_path_.empty()) {
    EXPECT_FALSE(base::DirectoryExists(unpack_path_));
  }
}

void TestInstaller::OnUpdateError(int error) {
  error_ = error;
}

void TestInstaller::Install(const base::FilePath& unpack_path,
                            const std::string& /*public_key*/,
                            std::unique_ptr<InstallParams> install_params,
                            ProgressCallback progress_callback,
                            Callback callback) {
  ++install_count_;
  unpack_path_ = unpack_path;
  install_params_ = std::move(install_params);

  InstallComplete(std::move(callback), progress_callback,
                  Result(install_error_));
}

void TestInstaller::InstallComplete(Callback callback,
                                    ProgressCallback progress_callback,
                                    const Result& result) {
  for (const auto& sample : installer_progress_samples_) {
    progress_callback.Run(sample);
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), result));
}

std::optional<base::FilePath> TestInstaller::GetInstalledFile(
    const std::string& file) {
  return std::nullopt;
}

bool TestInstaller::Uninstall() {
  return false;
}

ReadOnlyTestInstaller::ReadOnlyTestInstaller(const base::FilePath& install_dir)
    : install_directory_(install_dir) {}

ReadOnlyTestInstaller::~ReadOnlyTestInstaller() = default;

std::optional<base::FilePath> ReadOnlyTestInstaller::GetInstalledFile(
    const std::string& file) {
  return install_directory_.AppendASCII(file);
}

VersionedTestInstaller::VersionedTestInstaller() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("TEST_"), &install_directory_);
}

VersionedTestInstaller::~VersionedTestInstaller() {
  base::DeletePathRecursively(install_directory_);
}

void VersionedTestInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& public_key,
    std::unique_ptr<InstallParams> /*install_params*/,
    ProgressCallback progress_callback,
    Callback callback) {
  std::optional<base::Value::Dict> manifest =
      update_client::ReadManifest(unpack_path);
  if (!manifest) {
    return;
  }
  const std::string* version_string = manifest->FindString("version");
  if (!version_string || !base::IsStringASCII(*version_string)) {
    return;
  }

  const base::Version version(*version_string);
  const base::FilePath path =
      install_directory_.AppendASCII(version.GetString());
  base::CreateDirectory(path.DirName());
  if (!base::Move(unpack_path, path)) {
    InstallComplete(std::move(callback), progress_callback,
                    Result(InstallError::GENERIC_ERROR));
    return;
  }
  current_version_ = version;
  ++install_count_;

  InstallComplete(std::move(callback), progress_callback,
                  Result(InstallError::NONE));
}

std::optional<base::FilePath> VersionedTestInstaller::GetInstalledFile(
    const std::string& file) {
  return install_directory_.AppendASCII(current_version_.GetString())
      .AppendASCII(file);
}

}  // namespace update_client

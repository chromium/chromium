// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_installer.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TestInstaller::TestInstaller() : error_(0), install_count_(0) {
}

TestInstaller::~TestInstaller() {
  // The unpack path is deleted unconditionally by the component state code,
  // which is driving this installer. Therefore, the unpack path must not
  // exist when this object is destroyed.
  if (!unpack_path_.empty())
    EXPECT_FALSE(base::DirectoryExists(unpack_path_));
}

void TestInstaller::OnUpdateError(int error) {
  error_ = error;
}

void TestInstaller::Install(const base::FilePath& unpack_path,
                            const std::string& /*public_key*/,
                            Callback callback) {
  ++install_count_;
  unpack_path_ = unpack_path;

  InstallComplete(std::move(callback), Result(InstallError::NONE));
}

void TestInstaller::InstallComplete(Callback callback,
                                    const Result& result) const {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(std::move(callback), result));
}

bool TestInstaller::GetInstalledFile(const std::string& file,
                                     base::FilePath* installed_file) {
  return false;
}

bool TestInstaller::Uninstall() {
  return false;
}

ReadOnlyTestInstaller::ReadOnlyTestInstaller(const base::FilePath& install_dir)
    : install_directory_(install_dir) {
}

ReadOnlyTestInstaller::~ReadOnlyTestInstaller() {
}

bool ReadOnlyTestInstaller::GetInstalledFile(const std::string& file,
                                             base::FilePath* installed_file) {
  *installed_file = install_directory_.AppendASCII(file);
  return true;
}

VersionedTestInstaller::VersionedTestInstaller() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("TEST_"), &install_directory_);
}

VersionedTestInstaller::~VersionedTestInstaller() {
  base::DeleteFileRecursively(install_directory_);
}

void VersionedTestInstaller::Install(const base::FilePath& unpack_path,
                                     const std::string& public_key,
                                     Callback callback) {
  const auto manifest = update_client::ReadManifest(unpack_path);
  std::string version_string;
  manifest->GetStringASCII("version", &version_string);
  const base::Version version(version_string);

  const base::FilePath path =
      install_directory_.AppendASCII(version.GetString());
  base::CreateDirectory(path.DirName());
  if (!base::Move(unpack_path, path)) {
    InstallComplete(std::move(callback), Result(InstallError::GENERIC_ERROR));
    return;
  }
  current_version_ = version;
  ++install_count_;

  InstallComplete(std::move(callback), Result(InstallError::NONE));
}

bool VersionedTestInstaller::GetInstalledFile(const std::string& file,
                                              base::FilePath* installed_file) {
  const base::FilePath path =
      install_directory_.AppendASCII(current_version_.GetString());
  *installed_file = path.Append(base::FilePath::FromUTF8Unsafe(file));
  return true;
}

}  // namespace update_client

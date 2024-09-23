// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_INSTALLER_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
}

namespace update_client {

// A TestInstaller is an installer that does nothing for installation except
// increment a counter.
class TestInstaller : public CrxInstaller {
 public:
  TestInstaller();

  void OnUpdateError(int error) override;

  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;

  std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) override;

  bool Uninstall() override;

  int error() const { return error_; }

  int install_count() const { return install_count_; }

  const InstallParams* install_params() const { return install_params_.get(); }

  void set_installer_progress_samples(
      std::vector<int> installer_progress_samples) {
    installer_progress_samples_.swap(installer_progress_samples);
  }

  void set_install_error(InstallError install_error) {
    install_error_ = install_error;
  }

 protected:
  ~TestInstaller() override;

  void InstallComplete(Callback callback,
                       ProgressCallback progress_callback,
                       const Result& result);

  int error_;
  int install_count_;

 private:
  // Contains the error code returned by the installer when it completes.
  InstallError install_error_;

  // Contains the |unpack_path| argument of the Install call.
  base::FilePath unpack_path_;

  // Contains the |install_params| argument of the Install call.
  std::unique_ptr<InstallParams> install_params_;

  // Constains values to be posted as install progress.
  std::vector<int> installer_progress_samples_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// A ReadOnlyTestInstaller is an installer that knows about files in an existing
// directory. It will not write to the directory.
class ReadOnlyTestInstaller : public TestInstaller {
 public:
  explicit ReadOnlyTestInstaller(const base::FilePath& installed_path);

  std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) override;

 private:
  ~ReadOnlyTestInstaller() override;

  base::FilePath install_directory_;
};

// A VersionedTestInstaller is an installer that installs files into versioned
// directories (e.g. somedir/25.23.89.141/<files>).
class VersionedTestInstaller : public TestInstaller {
 public:
  VersionedTestInstaller();

  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               std::unique_ptr<InstallParams> install_params,
               ProgressCallback progress_callback,
               Callback callback) override;

  std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) override;

 private:
  ~VersionedTestInstaller() override;

  base::FilePath install_directory_;
  base::Version current_version_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_INSTALLER_H_

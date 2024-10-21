// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/installer_paths.h"

namespace enterprise_companion {

class AppInstallIfNeeded : public App {
 public:
  AppInstallIfNeeded() = default;
  ~AppInstallIfNeeded() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    if (install_dir &&
        base::PathExists(install_dir->AppendASCII(kExecutableName))) {
      VLOG(1) << "Found an existing installation in " << *install_dir
              << ". The application will not be installed.";
      Shutdown(EnterpriseCompanionStatus::Success());
      return;
    }

#if BUILDFLAG(IS_WIN)
    std::optional<base::FilePath> alt_install_dir =
        GetInstallDirectoryForAlternateArch();
    if (alt_install_dir &&
        base::PathExists(alt_install_dir->AppendASCII(kExecutableName))) {
      VLOG(1) << "Found an existing installation in " << *alt_install_dir
              << ". The application will not be installed.";
      Shutdown(EnterpriseCompanionStatus::Success());
      return;
    }
#endif

    scoped_refptr<device_management_storage::DMStorage> dm_storage =
        device_management_storage::GetDefaultDMStorage();
    if (dm_storage->GetEnrollmentToken().empty() &&
        !dm_storage->IsValidDMToken() && !dm_storage->IsEnrollmentMandatory()) {
      VLOG(1) << "The device does not appear to be cloud-managed, the "
                 "application will not be installed.";
      Shutdown(EnterpriseCompanionStatus::Success());
      return;
    }

    Shutdown(CreateAppInstall()->Run());
  }

  SEQUENCE_CHECKER(sequence_checker_);
};

std::unique_ptr<App> CreateAppInstallIfNeeded() {
  return std::make_unique<AppInstallIfNeeded>();
}

}  // namespace enterprise_companion

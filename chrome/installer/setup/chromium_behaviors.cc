// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/brand_behaviors.h"

namespace installer {

void UpdateInstallStatus(installer::ArchiveType archive_type,
                         installer::InstallStatus install_status) {}

std::wstring GetDistributionData() {
  return std::wstring();
}

void DoPostUninstallOperations(const base::Version& version,
                               const base::FilePath& local_data_path,
                               const std::wstring& distribution_data) {}

}  // namespace installer

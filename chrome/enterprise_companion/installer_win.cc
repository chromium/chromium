// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer.h"

#include <memory>
#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace enterprise_companion {

const wchar_t kAppRegKey[] = L"Software\\" COMPANY_SHORTNAME_STRING
                             "\\Update\\Clients\\" ENTERPRISE_COMPANION_APPID;
const wchar_t kRegValuePV[] = L"pv";
const wchar_t kRegValueName[] = L"name";

bool Install() {
  base::FilePath source_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &source_exe_path)) {
    LOG(ERROR) << "Failed to retrieve the current executable's path.";
    return false;
  }

  const std::optional<base::FilePath> install_directory = GetInstallDirectory();
  if (!install_directory) {
    LOG(ERROR) << "Failed to get install directory";
    return false;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create temporary directory.";
    return false;
  }

  std::unique_ptr<WorkItemList> install_list =
      base::WrapUnique(WorkItemList::CreateWorkItemList());

  install_list->AddCopyTreeWorkItem(
      source_exe_path, install_directory->AppendASCII(kExecutableName),
      temp_dir.GetPath(), WorkItem::ALWAYS);
  install_list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, kAppRegKey,
                                        KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, kAppRegKey, KEY_WOW64_32KEY, kRegValuePV,
      base::ASCIIToWide(kEnterpriseCompanionVersion), /*overwrite=*/true);
  install_list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, kAppRegKey, KEY_WOW64_32KEY, kRegValueName,
      L"" PRODUCT_FULLNAME_STRING, /*overwrite=*/true);

  std::optional<base::FilePath> alternate_arch_install_dir =
      GetInstallDirectoryForAlternateArch();
  if (alternate_arch_install_dir &&
      base::PathExists(*alternate_arch_install_dir)) {
    VLOG(1) << "Found an existing installation for a different architecture at "
            << *alternate_arch_install_dir
            << ". It will be removed by this install.";
    install_list->AddDeleteTreeWorkItem(*alternate_arch_install_dir,
                                        temp_dir.GetPath());
  }

  if (!install_list->Do()) {
    LOG(ERROR) << "Install failed, rolling back...";
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete.";
    return false;
  }

  return true;
}

}  // namespace enterprise_companion

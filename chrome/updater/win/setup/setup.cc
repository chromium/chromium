// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/util.h"

namespace updater {

namespace {

const base::char16* kUpdaterFiles[] = {
    L"icudtl.dat",
    L"updater.exe",
    L"uninstall.cmd",
#if defined(COMPONENT_BUILD)
    // TODO(sorin): get the list of component dependencies from a build-time
    // file instead of hardcoding the names of the components here.
    L"base.dll",
    L"base_i18n.dll",
    L"boringssl.dll",
    L"crcrypto.dll",
    L"icui18n.dll",
    L"icuuc.dll",
    L"libc++.dll",
    L"prefs.dll",
    L"protobuf_lite.dll",
    L"url_lib.dll",
    L"zlib.dll",
#endif
};

}  // namespace

int Setup() {
  VLOG(1) << __func__;

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  if (!TaskScheduler::Initialize()) {
    LOG(ERROR) << "Failed to initialize the scheduler.";
    return -1;
  }
  base::ScopedClosureRunner task_scheduler_terminate_caller(
      base::BindOnce([]() { TaskScheduler::Terminate(); }));

  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    LOG(ERROR) << "GetTempDir failed.";
    return -1;
  }
  base::FilePath product_dir;
  if (!GetProductDirectory(&product_dir)) {
    LOG(ERROR) << "GetProductDirectory failed.";
    return -1;
  }
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "PathService failed.";
    return -1;
  }

  installer::SelfCleaningTempDir backup_dir;
  if (!backup_dir.Initialize(temp_dir, L"updater-backup")) {
    LOG(ERROR) << "Failed to initialize the backup dir.";
    return -1;
  }

  const base::FilePath source_dir = exe_path.DirName();

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  for (const auto* file : kUpdaterFiles) {
    const base::FilePath target_path = product_dir.Append(file);
    const base::FilePath source_path = source_dir.Append(file);
    install_list->AddCopyTreeWorkItem(source_path.value(), target_path.value(),
                                      temp_dir.value(), WorkItem::ALWAYS);
  }

  for (const auto& key_path :
       {GetRegistryKeyClientsUpdater(), GetRegistryKeyClientStateUpdater()}) {
    install_list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_path,
                                          WorkItem::kWow64Default);
    install_list->AddSetRegValueWorkItem(
        HKEY_CURRENT_USER, key_path, WorkItem::kWow64Default, kRegistryValuePV,
        base::ASCIIToUTF16(UPDATER_VERSION_STRING), true);
    install_list->AddSetRegValueWorkItem(
        HKEY_CURRENT_USER, key_path, WorkItem::kWow64Default,
        kRegistryValueName, base::ASCIIToUTF16(PRODUCT_FULLNAME_STRING), true);
  }

  base::CommandLine run_updater_ua_command(product_dir.Append(L"updater.exe"));
  run_updater_ua_command.AppendSwitch(kUpdateAppsSwitch);
#if !defined(NDEBUG)
  run_updater_ua_command.AppendSwitch(kEnableLoggingSwitch);
  run_updater_ua_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                           "*/chrome/updater/*=2");
#endif
  if (!install_list->Do() || !RegisterUpdateAppsTask(run_updater_ua_command)) {
    LOG(ERROR) << "Install failed, rolling back...";
    install_list->Rollback();
    UnregisterUpdateAppsTask();
    LOG(ERROR) << "Rollback complete.";
    return -1;
  }

  VLOG(1) << "Setup succeeded.";
  return 0;
}

}  // namespace updater

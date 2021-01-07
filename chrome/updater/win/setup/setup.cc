// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup.h"

#include <shlobj.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/util.h"

namespace updater {
namespace {

// Adds work items to register the COM Server with Windows.
void AddComServerWorkItems(HKEY root,
                           const base::FilePath& com_server_path,
                           WorkItemList* list) {
  DCHECK(list);
  if (com_server_path.empty()) {
    LOG(DFATAL) << "com_server_path is invalid.";
    return;
  }

  for (const auto& clsid :
       {__uuidof(UpdaterClass), __uuidof(UpdaterInternalClass),
        __uuidof(GoogleUpdate3WebUserClass)}) {
    const base::string16 clsid_reg_path = GetComServerClsidRegistryPath(clsid);

    // Delete any old registrations first.
    for (const auto& reg_path : {clsid_reg_path}) {
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
        list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
    }

    list->AddCreateRegKeyWorkItem(root, clsid_reg_path,
                                  WorkItem::kWow64Default);
    const base::string16 local_server32_reg_path =
        base::StrCat({clsid_reg_path, L"\\LocalServer32"});
    list->AddCreateRegKeyWorkItem(root, local_server32_reg_path,
                                  WorkItem::kWow64Default);

    base::CommandLine run_com_server_command(com_server_path);
    run_com_server_command.AppendSwitch(kServerSwitch);
#if !defined(NDEBUG)
    run_com_server_command.AppendSwitch(kEnableLoggingSwitch);
    run_com_server_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                             "*/chrome/updater/*=2");
#endif

    list->AddSetRegValueWorkItem(
        root, local_server32_reg_path, WorkItem::kWow64Default, L"",
        run_com_server_command.GetCommandLineString(), true);
  }
}

// Adds work items to register the COM Service with Windows.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            WorkItemList* list) {
  DCHECK(list);
  DCHECK(::IsUserAnAdmin());

  if (com_service_path.empty()) {
    LOG(DFATAL) << "com_service_path is invalid.";
    return;
  }

  list->AddWorkItem(new installer::InstallServiceWorkItem(
      kWindowsServiceName, kWindowsServiceName,
      base::CommandLine(com_service_path), base::ASCIIToUTF16(UPDATER_KEY),
      {__uuidof(UpdaterServiceClass)}, {}));
}

// Adds work items to register the COM Interfaces with Windows.
void AddComInterfacesWorkItems(HKEY root,
                               const base::FilePath& typelib_path,
                               WorkItemList* list) {
  DCHECK(list);
  if (typelib_path.empty()) {
    LOG(DFATAL) << "typelib_path is invalid.";
    return;
  }

  for (const auto& iid : GetInterfaces()) {
    const base::string16 iid_reg_path = GetComIidRegistryPath(iid);
    const base::string16 typelib_reg_path = GetComTypeLibRegistryPath(iid);

    // Delete any old registrations first.
    for (const auto& reg_path : {iid_reg_path, typelib_reg_path}) {
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
        list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
    }

    // Registering the Ole Automation marshaler with the CLSID
    // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the
    // interfaces.
    list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\ProxyStubClsid32",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(
        root, iid_reg_path + L"\\ProxyStubClsid32", WorkItem::kWow64Default,
        L"", L"{00020424-0000-0000-C000-000000000046}", true);
    list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\TypeLib",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, iid_reg_path + L"\\TypeLib",
                                 WorkItem::kWow64Default, L"",
                                 base::win::WStringFromGUID(iid), true);

    // The TypeLib registration for the Ole Automation marshaler.
    const base::FilePath qualified_typelib_path =
        typelib_path.Append(GetComTypeLibResourceIndex(iid));
    list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                                 WorkItem::kWow64Default, L"",
                                 qualified_typelib_path.value(), true);
    list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                                 WorkItem::kWow64Default, L"",
                                 qualified_typelib_path.value(), true);
  }
}

// Returns a list of base file names which the setup copies to the install
// directory. The source of these files is either the unpacked metainstaller
// archive, or the `out` directory of the build, if a command line argument is
// present. In the former case, which is the normal execution flow, the files
// are enumerated from the directory where the metainstaller unpacked its
// contents. In the latter case, the file containing the run time dependencies
// of the updater (which is generated by GN at build time) is parsed, and the
// relevant file names are extracted.
std::vector<base::FilePath> GetSetupFiles(const base::FilePath& source_dir) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kInstallFromOutDir)) {
    return ParseFilesFromDeps(source_dir.Append(FILE_PATH_LITERAL(
        "gen\\chrome\\updater\\win\\installer\\updater.runtime_deps")));
  }
  std::vector<base::FilePath> result;
  base::FileEnumerator it(
      source_dir, false, base::FileEnumerator::FileType::FILES,
      FILE_PATH_LITERAL("*"), base::FileEnumerator::FolderSearchPolicy::ALL,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (base::FilePath file = it.Next(); !file.empty(); file = it.Next()) {
    result.push_back(file.BaseName());
  }
  if (it.GetError() != base::File::Error::FILE_OK) {
    VLOG(2) << __func__ << " could not enumerate files : " << it.GetError();
    return {};
  }
  return result;
}

}  // namespace

// TODO(crbug.com/1069976): use specific return values for different code paths.
int Setup(bool is_machine) {
  VLOG(1) << __func__ << ", is_machine: " << is_machine;
  DCHECK(!is_machine || ::IsUserAnAdmin());
  HKEY key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    LOG(ERROR) << "GetTempDir failed.";
    return -1;
  }
  base::FilePath versioned_dir;
  if (!GetVersionedDirectory(&versioned_dir)) {
    LOG(ERROR) << "GetVersionedDirectory failed.";
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

  const auto source_dir = exe_path.DirName();
  const auto setup_files = GetSetupFiles(source_dir);
  if (setup_files.empty()) {
    LOG(ERROR) << "No files to set up.";
    return -1;
  }

  // All source files are installed in a flat directory structure inside the
  // versioned directory, hence the BaseName function call below.
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  for (const auto& file : setup_files) {
    const base::FilePath target_path = versioned_dir.Append(file.BaseName());
    const base::FilePath source_path = source_dir.Append(file);
    install_list->AddCopyTreeWorkItem(source_path, target_path, temp_dir,
                                      WorkItem::ALWAYS);
  }

  for (const auto& key_path :
       {GetRegistryKeyClientsUpdater(), GetRegistryKeyClientStateUpdater()}) {
    install_list->AddCreateRegKeyWorkItem(key, key_path,
                                          WorkItem::kWow64Default);
    install_list->AddSetRegValueWorkItem(
        key, key_path, WorkItem::kWow64Default, kRegistryValuePV,
        base::ASCIIToUTF16(UPDATER_VERSION_STRING), true);
    install_list->AddSetRegValueWorkItem(
        key, key_path, WorkItem::kWow64Default, kRegistryValueName,
        base::ASCIIToUTF16(PRODUCT_FULLNAME_STRING), true);
  }

  static constexpr base::FilePath::StringPieceType kUpdaterExe =
      FILE_PATH_LITERAL("updater.exe");
  AddComServerWorkItems(key, versioned_dir.Append(kUpdaterExe),
                        install_list.get());

  if (is_machine) {
    AddComServiceWorkItems(versioned_dir.Append(kUpdaterExe),
                           install_list.get());
  }

  AddComInterfacesWorkItems(key, versioned_dir.Append(kUpdaterExe),
                            install_list.get());

  base::CommandLine run_updater_wake_command(versioned_dir.Append(kUpdaterExe));
  run_updater_wake_command.AppendSwitch(kWakeSwitch);

#if !defined(NDEBUG)
  run_updater_wake_command.AppendSwitch(kEnableLoggingSwitch);
  run_updater_wake_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                             "*/chrome/updater/*=2");
#endif
  if (!install_list->Do() || !RegisterWakeTask(run_updater_wake_command)) {
    LOG(ERROR) << "Install failed, rolling back...";
    install_list->Rollback();
    UnregisterWakeTask();
    LOG(ERROR) << "Rollback complete.";
    return -1;
  }

  VLOG(1) << "Setup succeeded.";
  return 0;
}

}  // namespace updater

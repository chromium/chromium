// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <shlobj.h>
#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"

// Specialization for std::hash so that IID instances can be stored in an
// associative container. This implementation of the hash function adds
// together four 32-bit integers which make up an IID. The function does not
// have to be efficient or guarantee no collisions. It is used infrequently,
// for a small number of IIDs, and the container deals with collisions.
template <>
struct std::hash<IID> {
  size_t operator()(const IID& iid) const {
    return iid.Data1 + (iid.Data2 + (iid.Data3 << 16)) + [&iid]() {
      size_t val = 0;
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
          val += (iid.Data4[j + i * 4] << (j * 4));
        }
      }
      return val;
    }();
  }
};

namespace updater {
namespace {

constexpr wchar_t kTaskName[] = L"UpdateApps";
constexpr wchar_t kTaskDescription[] = L"Update all applications.";

}  // namespace

// crbug.com(1216670) - the name of the task must be scoped for user or system.
bool RegisterWakeTask(const base::CommandLine& run_command) {
  auto task_scheduler = TaskScheduler::CreateInstance();
  if (!task_scheduler->RegisterTask(
          kTaskName, kTaskDescription, run_command,
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, true)) {
    LOG(ERROR) << "RegisterWakeTask failed.";
    return false;
  }
  VLOG(1) << "RegisterWakeTask succeeded.";
  return true;
}

// crbug.com(1216670) - the name of the task must be scoped for user or system.
void UnregisterWakeTask() {
  auto task_scheduler = TaskScheduler::CreateInstance();
  task_scheduler->DeleteTask(kTaskName);
}

std::vector<IID> GetSideBySideInterfaces() {
  return {
      __uuidof(IUpdaterInternal),
      __uuidof(IUpdaterInternalCallback),
  };
}

std::vector<IID> GetActiveInterfaces() {
  return {__uuidof(IAppBundleWeb),
          __uuidof(IAppWeb),
          __uuidof(ICompleteStatus),
          __uuidof(ICurrentState),
          __uuidof(IGoogleUpdate3Web),
          __uuidof(IUpdateState),
          __uuidof(IUpdater),
          __uuidof(IUpdaterObserver),
          __uuidof(IUpdaterRegisterAppCallback),
          __uuidof(IUpdaterCallback)};
}

std::vector<CLSID> GetSideBySideServers(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return {__uuidof(UpdaterInternalUserClass)};
    case UpdaterScope::kSystem:
      return {__uuidof(UpdaterInternalSystemClass)};
  }
}

std::vector<CLSID> GetActiveServers(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return {__uuidof(UpdaterUserClass), __uuidof(GoogleUpdate3WebUserClass)};
    case UpdaterScope::kSystem:
      return {__uuidof(UpdaterSystemClass),
              __uuidof(GoogleUpdate3WebSystemClass)};
  }
}

void AddInstallComInterfaceWorkItems(HKEY root,
                                     const base::FilePath& typelib_path,
                                     GUID iid,
                                     WorkItemList* list) {
  const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
  const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);

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
  list->AddSetRegValueWorkItem(root, iid_reg_path + L"\\ProxyStubClsid32",
                               WorkItem::kWow64Default, L"",
                               L"{00020424-0000-0000-C000-000000000046}", true);
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

void AddInstallServerWorkItems(HKEY root,
                               CLSID clsid,
                               const base::FilePath& com_server_path,
                               bool internal_service,
                               WorkItemList* list) {
  const std::wstring clsid_reg_path = GetComServerClsidRegistryPath(clsid);

  // Delete any old registrations first.
  for (const auto& reg_path : {clsid_reg_path}) {
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
      list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
  }

  list->AddCreateRegKeyWorkItem(root, clsid_reg_path, WorkItem::kWow64Default);
  const std::wstring local_server32_reg_path =
      base::StrCat({clsid_reg_path, L"\\LocalServer32"});
  list->AddCreateRegKeyWorkItem(root, local_server32_reg_path,
                                WorkItem::kWow64Default);

  base::CommandLine run_com_server_command(com_server_path);
  run_com_server_command.AppendSwitch(kServerSwitch);
  run_com_server_command.AppendSwitchASCII(
      kServerServiceSwitch, internal_service
                                ? kServerUpdateServiceInternalSwitchValue
                                : kServerUpdateServiceSwitchValue);
  run_com_server_command.AppendSwitch(kEnableLoggingSwitch);
  run_com_server_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                           "*/chrome/updater/*=2");
  list->AddSetRegValueWorkItem(
      root, local_server32_reg_path, WorkItem::kWow64Default, L"",
      run_com_server_command.GetCommandLineString(), true);
}

// Adds work items to register the COM Service with Windows.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            bool internal_service,
                            WorkItemList* list) {
  DCHECK(::IsUserAnAdmin());

  if (com_service_path.empty()) {
    LOG(DFATAL) << "com_service_path is invalid.";
    return;
  }

  // This assumes the COM service runs elevated and in the system updater scope.
  base::CommandLine com_service_command(com_service_path);
  com_service_command.AppendSwitch(kSystemSwitch);
  com_service_command.AppendSwitch(kComServiceSwitch);
  com_service_command.AppendSwitchASCII(
      kServerServiceSwitch, internal_service
                                ? kServerUpdateServiceInternalSwitchValue
                                : kServerUpdateServiceSwitchValue);
  com_service_command.AppendSwitch(kEnableLoggingSwitch);
  com_service_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                        "*/chrome/updater/*=2");
  list->AddWorkItem(new installer::InstallServiceWorkItem(
      GetServiceName(internal_service).c_str(),
      GetServiceDisplayName(internal_service).c_str(), com_service_command,
      base::ASCIIToWide(UPDATER_KEY),
      internal_service ? GetSideBySideServers(UpdaterScope::kSystem)
                       : GetActiveServers(UpdaterScope::kSystem),
      {}));

  const std::vector<GUID> com_interfaces_to_install =
      internal_service ? GetSideBySideInterfaces() : GetActiveInterfaces();
  for (const auto& iid : com_interfaces_to_install) {
    AddInstallComInterfaceWorkItems(HKEY_LOCAL_MACHINE, com_service_path, iid,
                                    list);
  }
}

std::wstring GetComServerClsidRegistryPath(REFCLSID clsid) {
  return base::StrCat(
      {L"Software\\Classes\\CLSID\\", base::win::WStringFromGUID(clsid)});
}

std::wstring GetComServerAppidRegistryPath(REFGUID appid) {
  return base::StrCat(
      {L"Software\\Classes\\AppID\\", base::win::WStringFromGUID(appid)});
}

std::wstring GetComIidRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\Interface\\", base::win::WStringFromGUID(iid)});
}

std::wstring GetComTypeLibRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\TypeLib\\", base::win::WStringFromGUID(iid)});
}

std::wstring GetComTypeLibResourceIndex(REFIID iid) {
  // These values must be kept in sync with the numeric typelib resource
  // indexes in the resource file.
  constexpr wchar_t kUpdaterIndex[] = L"1";
  constexpr wchar_t kUpdaterInternalIndex[] = L"2";
  constexpr wchar_t kUpdaterLegacyIndex[] = L"3";

  static const std::unordered_map<IID, const wchar_t*> kTypeLibIndexes = {
      // Updater typelib.
      {__uuidof(ICompleteStatus), kUpdaterIndex},
      {__uuidof(IUpdater), kUpdaterIndex},
      {__uuidof(IUpdaterObserver), kUpdaterIndex},
      {__uuidof(IUpdaterRegisterAppCallback), kUpdaterIndex},
      {__uuidof(IUpdateState), kUpdaterIndex},
      {__uuidof(IUpdaterCallback), kUpdaterIndex},

      // Updater internal typelib.
      {__uuidof(IUpdaterInternal), kUpdaterInternalIndex},
      {__uuidof(IUpdaterInternalCallback), kUpdaterInternalIndex},

      // Updater legacy typelib.
      {__uuidof(IAppBundleWeb), kUpdaterLegacyIndex},
      {__uuidof(IAppWeb), kUpdaterLegacyIndex},
      {__uuidof(ICurrentState), kUpdaterLegacyIndex},
      {__uuidof(IGoogleUpdate3Web), kUpdaterLegacyIndex},
  };
  auto index = kTypeLibIndexes.find(iid);
  return index != kTypeLibIndexes.end() ? index->second : L"";
}

std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps) {
  constexpr size_t kDepsFileSizeMax = 0x4000;  // 16KB.
  std::string contents;
  if (!base::ReadFileToStringWithMaxSize(deps, &contents, kDepsFileSizeMax))
    return {};
  const base::flat_set<const wchar_t*, CaseInsensitiveASCIICompare>
      exclude_extensions = {L".pdb", L".js"};
  std::vector<base::FilePath> result;
  for (const auto& line :
       base::SplitString(contents, "\r\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    const auto filename =
        base::FilePath(base::ASCIIToWide(line)).NormalizePathSeparators();
    if (!base::Contains(exclude_extensions,
                        filename.FinalExtension().c_str())) {
      result.push_back(filename);
    }
  }
  return result;
}

}  // namespace updater

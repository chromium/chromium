// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <regstr.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <cstring>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

std::wstring CreateRandomTaskName(UpdaterScope scope) {
  GUID random_guid = {0};
  return SUCCEEDED(::CoCreateGuid(&random_guid))
             ? base::StrCat({GetTaskNamePrefix(scope),
                             base::win::WStringFromGUID(random_guid)})
             : std::wstring();
}

// Adds work items to `list` to install the progid corresponding to `clsid`.
void AddInstallComProgIdWorkItems(UpdaterScope scope,
                                  CLSID clsid,
                                  WorkItemList* list) {
  const std::wstring progid(GetProgIdForClsid(clsid));
  if (!progid.empty()) {
    const HKEY root = UpdaterScopeToHKeyRoot(scope);
    const std::wstring progid_reg_path(GetComProgIdRegistryPath(progid));

    // Delete any old registrations first.
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      list->AddDeleteRegKeyWorkItem(root, progid_reg_path, key_flag);
    }

    list->AddCreateRegKeyWorkItem(root, progid_reg_path + L"\\CLSID",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, progid_reg_path + L"\\CLSID",
                                 WorkItem::kWow64Default, L"",
                                 base::win::WStringFromGUID(clsid), true);
  }
}

}  // namespace

std::wstring GetTaskName(UpdaterScope scope) {
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope);
  return task_scheduler
             ? task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope))
             : std::wstring();
}

void UnregisterWakeTask(UpdaterScope scope) {
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope);
  if (!task_scheduler) {
    LOG(ERROR) << "Can't create a TaskScheduler instance.";
    return;
  }
  const std::wstring task_name = GetTaskName(scope);
  if (task_name.empty()) {
    LOG(ERROR) << "Empty task name during uninstall.";
    return;
  }
  if (task_scheduler->DeleteTask(task_name)) {
    VLOG(1) << "UnregisterWakeTask succeeded: " << task_name;
  } else {
    VLOG(1) << "UnregisterWakeTask failed: " << task_name;
  }
}

#define INTERFACE_PAIR(interface) \
  { __uuidof(interface), L#interface }

std::vector<std::pair<IID, std::wstring>> GetSideBySideInterfaces(
    UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return {
          INTERFACE_PAIR(IUpdaterInternalUser),
          INTERFACE_PAIR(IUpdaterInternalCallbackUser),
      };
    case UpdaterScope::kSystem:
      return {
          INTERFACE_PAIR(IUpdaterInternalSystem),
          INTERFACE_PAIR(IUpdaterInternalCallbackSystem),
      };
  }
}

std::vector<std::pair<IID, std::wstring>> GetActiveInterfaces(
    UpdaterScope scope) {
  return JoinVectors(
      [&scope]() -> std::vector<std::pair<IID, std::wstring>> {
        switch (scope) {
          case UpdaterScope::kUser:
            return {
                INTERFACE_PAIR(IUpdateStateUser),
                INTERFACE_PAIR(IUpdaterUser),
                INTERFACE_PAIR(ICompleteStatusUser),
                INTERFACE_PAIR(IUpdaterObserverUser),
                INTERFACE_PAIR(IUpdaterCallbackUser),
                INTERFACE_PAIR(IUpdaterAppStateUser),
                INTERFACE_PAIR(IUpdaterAppStatesCallbackUser),

                // legacy interfaces.
                INTERFACE_PAIR(IAppVersionWebUser),
                INTERFACE_PAIR(ICurrentStateUser),
                INTERFACE_PAIR(IGoogleUpdate3WebUser),
                INTERFACE_PAIR(IAppBundleWebUser),
                INTERFACE_PAIR(IAppWebUser),
                INTERFACE_PAIR(IAppCommandWebUser),
                INTERFACE_PAIR(IPolicyStatusUser),
                INTERFACE_PAIR(IPolicyStatus2User),
                INTERFACE_PAIR(IPolicyStatus3User),
                INTERFACE_PAIR(IPolicyStatusValueUser),
            };
          case UpdaterScope::kSystem:
            return {
                INTERFACE_PAIR(IUpdateStateSystem),
                INTERFACE_PAIR(IUpdaterSystem),
                INTERFACE_PAIR(ICompleteStatusSystem),
                INTERFACE_PAIR(IUpdaterObserverSystem),
                INTERFACE_PAIR(IUpdaterCallbackSystem),
                INTERFACE_PAIR(IUpdaterAppStateSystem),
                INTERFACE_PAIR(IUpdaterAppStatesCallbackSystem),

                // legacy interfaces.
                INTERFACE_PAIR(IAppVersionWebSystem),
                INTERFACE_PAIR(ICurrentStateSystem),
                INTERFACE_PAIR(IGoogleUpdate3WebSystem),
                INTERFACE_PAIR(IAppBundleWebSystem),
                INTERFACE_PAIR(IAppWebSystem),
                INTERFACE_PAIR(IAppCommandWebSystem),
                INTERFACE_PAIR(IPolicyStatusSystem),
                INTERFACE_PAIR(IPolicyStatus2System),
                INTERFACE_PAIR(IPolicyStatus3System),
                INTERFACE_PAIR(IPolicyStatusValueSystem),
                INTERFACE_PAIR(IProcessLauncher),
                INTERFACE_PAIR(IProcessLauncher2),
            };
        }
      }(),
      {
          // legacy interfaces.
          INTERFACE_PAIR(IAppBundleWeb),
          INTERFACE_PAIR(IAppWeb),
          INTERFACE_PAIR(IAppCommandWeb),
          INTERFACE_PAIR(IAppVersionWeb),
          INTERFACE_PAIR(ICurrentState),
          INTERFACE_PAIR(IGoogleUpdate3Web),
          INTERFACE_PAIR(IPolicyStatus),
          INTERFACE_PAIR(IPolicyStatus2),
          INTERFACE_PAIR(IPolicyStatus3),
          INTERFACE_PAIR(IPolicyStatusValue),
      });
}
#undef INTERFACE_PAIR

std::vector<std::pair<IID, std::wstring>> GetInterfaces(bool is_internal,
                                                        UpdaterScope scope) {
  return is_internal ? GetSideBySideInterfaces(scope)
                     : GetActiveInterfaces(scope);
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
      return {
          __uuidof(UpdaterUserClass),
          __uuidof(GoogleUpdate3WebUserClass),
          __uuidof(PolicyStatusUserClass),
      };
    case UpdaterScope::kSystem:
      return {
          __uuidof(UpdaterSystemClass),
          __uuidof(GoogleUpdate3WebSystemClass),
          __uuidof(GoogleUpdate3WebServiceClass),
          __uuidof(PolicyStatusSystemClass),
          __uuidof(ProcessLauncherClass),
      };
  }
}

std::vector<CLSID> GetServers(bool is_internal, UpdaterScope scope) {
  return is_internal ? GetSideBySideServers(scope) : GetActiveServers(scope);
}

bool InstallComInterfaces(UpdaterScope scope, bool is_internal) {
  VLOG(1) << __func__ << ": scope: " << scope
          << ": is_internal: " << is_internal;
  const std::optional<base::FilePath> versioned_directory =
      GetVersionedInstallDirectory(scope);
  if (!versioned_directory) {
    return false;
  }
  const base::FilePath updater_path =
      versioned_directory->Append(GetExecutableRelativePath());
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  for (const auto& [iid, interface_name] : GetInterfaces(is_internal, scope)) {
    AddInstallComInterfaceWorkItems(UpdaterScopeToHKeyRoot(scope), updater_path,
                                    iid, interface_name, list.get());
  }
  return list->Do();
}

bool AreComInterfacesPresent(UpdaterScope scope, bool is_internal) {
  VLOG(1) << __func__ << ": scope: " << scope
          << ": is_internal: " << is_internal;

  bool are_interfaces_present = true;
  for (const auto& [iid, interface_name] : GetInterfaces(is_internal, scope)) {
    const HKEY root = UpdaterScopeToHKeyRoot(scope);
    const std::wstring iid_path = GetComIidRegistryPath(iid);
    const std::wstring typelib_path = GetComTypeLibRegistryPath(iid);
    for (const auto& path :
         {iid_path, base::StrCat({iid_path, L"\\", L"ProxyStubClsid32"}),
          base::StrCat({iid_path, L"\\", L"TypeLib"}), typelib_path}) {
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        if (!base::win::RegKey(root, path.c_str(), KEY_QUERY_VALUE | key_flag)
                 .Valid()) {
          VLOG(2) << __func__ << ": interface entry missing: " << interface_name
                  << ": path: " << path << ": key_flag: " << key_flag;
          are_interfaces_present = false;
        }
      }
    }
  }
  VLOG_IF(2, are_interfaces_present) << __func__ << ": all interfaces present";
  return are_interfaces_present;
}

// Adds work items to `list` to install the interface `iid`.
void AddInstallComInterfaceWorkItems(HKEY root,
                                     const base::FilePath& typelib_path,
                                     GUID iid,
                                     const std::wstring& interface_name,
                                     WorkItemList* list) {
  const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
  const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);

  for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    // Registering the Ole Automation marshaler with the CLSID
    // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the
    // interfaces.
    list->AddCreateRegKeyWorkItem(root, iid_reg_path, key_flag);
    list->AddSetRegValueWorkItem(root, iid_reg_path, key_flag, L"",
                                 interface_name, true);
    {
      const std::wstring path = iid_reg_path + L"\\ProxyStubClsid32";
      list->AddCreateRegKeyWorkItem(root, path, key_flag);
      list->AddSetRegValueWorkItem(root, path, key_flag, L"",
                                   L"{00020424-0000-0000-C000-000000000046}",
                                   true);
    }
    {
      const std::wstring path = iid_reg_path + L"\\TypeLib";
      list->AddCreateRegKeyWorkItem(root, path, key_flag);
      list->AddSetRegValueWorkItem(root, path, key_flag, L"",
                                   base::win::WStringFromGUID(iid), true);
      list->AddSetRegValueWorkItem(root, path, key_flag, L"Version", L"1.0",
                                   true);
    }
  }

  // The TypeLib registration for the Ole Automation marshaler.
  const std::wstring typelib_resource_index = GetComTypeLibResourceIndex(iid);
  const base::FilePath qualified_typelib_path =
      typelib_path.Append(typelib_resource_index);
  for (const auto& path : {typelib_reg_path + L"\\1.0\\0\\win32",
                           typelib_reg_path + L"\\1.0\\0\\win64"}) {
    list->AddCreateRegKeyWorkItem(root, path, WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, path, WorkItem::kWow64Default, L"",
                                 qualified_typelib_path.value(), true);
  }
  list->AddSetRegValueWorkItem(
      root, typelib_reg_path + L"\\1.0", WorkItem::kWow64Default, L"",
      base::StrCat({PRODUCT_FULLNAME_STRING L" TypeLib for ", interface_name}),
      true);
}

// Adds work items to `list` to install the server `iid`.
void AddInstallServerWorkItems(HKEY root,
                               CLSID clsid,
                               const base::FilePath& com_server_path,
                               bool internal_service,
                               WorkItemList* list) {
  const std::wstring clsid_reg_path = GetComServerClsidRegistryPath(clsid);

  // Delete any old registrations first.
  for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    list->AddDeleteRegKeyWorkItem(root, clsid_reg_path, key_flag);
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
                                           kLoggingModuleSwitchValue);
  list->AddSetRegValueWorkItem(
      root, local_server32_reg_path, WorkItem::kWow64Default, L"",
      run_com_server_command.GetCommandLineString(), true);
}

void AddComServerWorkItems(const base::FilePath& com_server_path,
                           bool is_internal,
                           WorkItemList* list) {
  CHECK(list);

  if (com_server_path.empty()) {
    LOG(DFATAL) << "com_server_path is invalid.";
    return;
  }

  for (const auto& clsid : GetServers(is_internal, UpdaterScope::kUser)) {
    AddInstallServerWorkItems(HKEY_CURRENT_USER, clsid, com_server_path,
                              is_internal, list);
    AddInstallComProgIdWorkItems(UpdaterScope::kUser, clsid, list);
  }

  for (const auto& [iid, interface_name] :
       GetInterfaces(is_internal, UpdaterScope::kUser)) {
    AddInstallComInterfaceWorkItems(HKEY_CURRENT_USER, com_server_path, iid,
                                    interface_name, list);
  }
}

void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            bool internal_service,
                            WorkItemList* list) {
  CHECK(::IsUserAnAdmin());

  if (com_service_path.empty()) {
    LOG(DFATAL) << "com_service_path is invalid.";
    return;
  }

  // This assumes the COM service runs elevated and in the system updater scope.
  base::CommandLine com_service_command(com_service_path);
  com_service_command.AppendSwitch(kSystemSwitch);
  com_service_command.AppendSwitch(kWindowsServiceSwitch);
  com_service_command.AppendSwitchASCII(
      kServerServiceSwitch, internal_service
                                ? kServerUpdateServiceInternalSwitchValue
                                : kServerUpdateServiceSwitchValue);
  com_service_command.AppendSwitch(kEnableLoggingSwitch);
  com_service_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                        kLoggingModuleSwitchValue);

  base::CommandLine com_switch(base::CommandLine::NO_PROGRAM);
  com_switch.AppendSwitch(kComServiceSwitch);

  const std::vector<CLSID> clsids(
      GetServers(internal_service, UpdaterScope::kSystem));

  // Delete any old registrations in the 32-bit and 64-bit hives, because
  // `installer::InstallServiceWorkItem` does not do this. This is important for
  // scenarios where the machine may have a pre-existing 32-bit installation,
  // and the current install is 64-bit. If any 32-bit keys remain, they will
  // shadow the 64-bit keys.
  for (const auto& clsid : clsids) {
    const std::wstring clsid_reg_path = GetComServerClsidRegistryPath(clsid);
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, clsid_reg_path,
                                    key_flag);
    }
  }

  list->AddWorkItem(new installer::InstallServiceWorkItem(
      GetServiceName(internal_service).c_str(),
      GetServiceDisplayName(internal_service).c_str(), SERVICE_AUTO_START,
      com_service_command, com_switch, UPDATER_KEY, clsids, {}));

  for (const auto& clsid : clsids) {
    AddInstallComProgIdWorkItems(UpdaterScope::kSystem, clsid, list);
  }

  for (const auto& [iid, interface_name] :
       GetInterfaces(internal_service, UpdaterScope::kSystem)) {
    AddInstallComInterfaceWorkItems(HKEY_LOCAL_MACHINE, com_service_path, iid,
                                    interface_name, list);
  }
}

std::wstring GetProgIdForClsid(REFCLSID clsid) {
  auto clsid_comparator = [](REFCLSID a, REFCLSID b) {
    return std::memcmp(&a, &b, sizeof(a)) < 0;
  };

  const base::flat_map<CLSID, std::wstring, decltype(clsid_comparator)>
      kClsidToProgId = {
          {__uuidof(GoogleUpdate3WebSystemClass),
           kGoogleUpdate3WebSystemClassProgId},
          {__uuidof(GoogleUpdate3WebUserClass),
           kGoogleUpdate3WebUserClassProgId},
      };

  const auto progid = kClsidToProgId.find(clsid);
  return progid != kClsidToProgId.end() ? progid->second : L"";
}

std::wstring GetComProgIdRegistryPath(const std::wstring& progid) {
  return base::StrCat({L"Software\\Classes\\", progid});
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
  static constexpr wchar_t kUpdaterIndex[] = L"1";
  static constexpr wchar_t kUpdaterInternalIndex[] = L"2";
  static constexpr wchar_t kUpdaterLegacyIndex[] = L"3";

  static constexpr auto kTypeLibIndexes =
      base::MakeFixedFlatMap<IID, const wchar_t*>(
          {
              // Updater typelib.
              {__uuidof(ICompleteStatusUser), kUpdaterIndex},
              {__uuidof(ICompleteStatusSystem), kUpdaterIndex},
              {__uuidof(IUpdaterUser), kUpdaterIndex},
              {__uuidof(IUpdaterSystem), kUpdaterIndex},
              {__uuidof(IUpdaterObserverUser), kUpdaterIndex},
              {__uuidof(IUpdaterObserverSystem), kUpdaterIndex},
              {__uuidof(IUpdateStateUser), kUpdaterIndex},
              {__uuidof(IUpdateStateSystem), kUpdaterIndex},
              {__uuidof(IUpdaterCallbackUser), kUpdaterIndex},
              {__uuidof(IUpdaterCallbackSystem), kUpdaterIndex},
              {__uuidof(IUpdaterAppState), kUpdaterIndex},
              {__uuidof(IUpdaterAppStateUser), kUpdaterIndex},
              {__uuidof(IUpdaterAppStateSystem), kUpdaterIndex},
              {__uuidof(IUpdaterAppStatesCallbackUser), kUpdaterIndex},
              {__uuidof(IUpdaterAppStatesCallbackSystem), kUpdaterIndex},

              // Updater internal typelib.
              {__uuidof(IUpdaterInternalUser), kUpdaterInternalIndex},
              {__uuidof(IUpdaterInternalSystem), kUpdaterInternalIndex},
              {__uuidof(IUpdaterInternalCallbackUser), kUpdaterInternalIndex},
              {__uuidof(IUpdaterInternalCallbackSystem), kUpdaterInternalIndex},

              // Updater legacy typelib.
              {__uuidof(IAppVersionWeb), kUpdaterLegacyIndex},
              {__uuidof(IAppVersionWebUser), kUpdaterLegacyIndex},
              {__uuidof(IAppVersionWebSystem), kUpdaterLegacyIndex},
              {__uuidof(ICurrentState), kUpdaterLegacyIndex},
              {__uuidof(ICurrentStateUser), kUpdaterLegacyIndex},
              {__uuidof(ICurrentStateSystem), kUpdaterLegacyIndex},
              {__uuidof(IGoogleUpdate3Web), kUpdaterLegacyIndex},
              {__uuidof(IGoogleUpdate3WebUser), kUpdaterLegacyIndex},
              {__uuidof(IGoogleUpdate3WebSystem), kUpdaterLegacyIndex},
              {__uuidof(IAppBundleWeb), kUpdaterLegacyIndex},
              {__uuidof(IAppBundleWebUser), kUpdaterLegacyIndex},
              {__uuidof(IAppBundleWebSystem), kUpdaterLegacyIndex},
              {__uuidof(IAppWeb), kUpdaterLegacyIndex},
              {__uuidof(IAppWebUser), kUpdaterLegacyIndex},
              {__uuidof(IAppWebSystem), kUpdaterLegacyIndex},
              {__uuidof(IAppCommandWeb), kUpdaterLegacyIndex},
              {__uuidof(IAppCommandWebUser), kUpdaterLegacyIndex},
              {__uuidof(IAppCommandWebSystem), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatusUser), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatusSystem), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus2), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus2User), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus2System), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus3), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus3User), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatus3System), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatusValue), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatusValueUser), kUpdaterLegacyIndex},
              {__uuidof(IPolicyStatusValueSystem), kUpdaterLegacyIndex},
              {__uuidof(IProcessLauncher), kUpdaterLegacyIndex},
              {__uuidof(IProcessLauncher2), kUpdaterLegacyIndex},
          },
          IidComparator());
  auto* index = kTypeLibIndexes.find(iid);
  CHECK(index != kTypeLibIndexes.end());
  return index->second;
}

void RegisterUserRunAtStartup(const std::wstring& run_value_name,
                              const base::CommandLine& command,
                              WorkItemList* list) {
  CHECK(list);
  VLOG(1) << __func__;

  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, REGSTR_PATH_RUN, 0,
                               run_value_name, command.GetCommandLineString(),
                               true);
}

bool UnregisterUserRunAtStartup(const std::wstring& run_value_name) {
  VLOG(1) << __func__;

  return installer::DeleteRegistryValue(HKEY_CURRENT_USER, REGSTR_PATH_RUN, 0,
                                        run_value_name);
}

bool DeleteLegacyEntriesPerUser() {
  // The IProcessLauncher and IProcessLauncher2 interfaces are now only
  // registered for system since r1154562. So the code below removes these
  // interfaces from the user hive.
  bool success = true;
  for (const auto& iid :
       {__uuidof(IProcessLauncher), __uuidof(IProcessLauncher2)}) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      if (!installer::DeleteRegistryKey(HKEY_CURRENT_USER, reg_path,
                                        WorkItem::kWow64Default)) {
        success = false;
      }
    }
  }
  return success;
}

RegisterWakeTaskWorkItem::RegisterWakeTaskWorkItem(
    const base::CommandLine& run_command,
    UpdaterScope scope)
    : run_command_(run_command), scope_(scope) {}

RegisterWakeTaskWorkItem::~RegisterWakeTaskWorkItem() = default;

bool RegisterWakeTaskWorkItem::DoImpl() {
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope_);
  if (!task_scheduler) {
    LOG(ERROR) << "Can't create a TaskScheduler instance.";
    return false;
  }

  // Task already exists.
  if (!GetTaskName(scope_).empty()) {
    return true;
  }

  // Create a new task name and install.
  const std::wstring task_name = CreateRandomTaskName(scope_);
  if (task_name.empty()) {
    LOG(ERROR) << "Unexpected empty task name.";
    return false;
  }

  if (task_scheduler->IsTaskRegistered(task_name)) {
    LOG(ERROR) << "Unexpected task name found. " << task_name;
    return false;
  }

  if (!task_scheduler->RegisterTask(
          task_name, GetTaskDisplayName(scope_), run_command_,
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY |
              TaskScheduler::TriggerType::TRIGGER_TYPE_LOGON,
          true)) {
    return false;
  }

  task_name_ = task_name;
  return true;
}

void RegisterWakeTaskWorkItem::RollbackImpl() {
  if (task_name_.empty()) {
    return;
  }
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope_);
  if (!task_scheduler) {
    return;
  }
  std::ignore = task_scheduler->DeleteTask(task_name_);
}

}  // namespace updater

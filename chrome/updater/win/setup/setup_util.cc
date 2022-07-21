// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <regstr.h>
#include <shlobj.h>
#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/registry_util.h"
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

namespace updater {
namespace {

std::wstring GetTaskName(UpdaterScope scope) {
  return TaskScheduler::CreateInstance()->FindFirstTaskName(
      GetTaskNamePrefix(scope));
}

std::wstring CreateRandomTaskName(UpdaterScope scope) {
  GUID random_guid = {0};
  return SUCCEEDED(::CoCreateGuid(&random_guid))
             ? base::StrCat({GetTaskNamePrefix(scope),
                             base::win::WStringFromGUID(random_guid)})
             : std::wstring();
}

}  // namespace

bool RegisterWakeTask(const base::CommandLine& run_command,
                      UpdaterScope scope) {
  auto task_scheduler = TaskScheduler::CreateInstance();

  std::wstring task_name = GetTaskName(scope);
  if (!task_name.empty()) {
    // Update the currently installed scheduled task.
    if (task_scheduler->RegisterTask(
            scope, task_name.c_str(), GetTaskDisplayName(scope).c_str(),
            run_command, TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY,
            true)) {
      VLOG(1) << "RegisterWakeTask succeeded." << task_name;
      return true;
    } else {
      task_scheduler->DeleteTask(task_name.c_str());
    }
  }

  // Create a new task name and fall through to install that.
  task_name = CreateRandomTaskName(scope);
  if (task_name.empty()) {
    LOG(ERROR) << "Unexpected empty task name.";
    return false;
  }

  DCHECK(!task_scheduler->IsTaskRegistered(task_name.c_str()));

  if (task_scheduler->RegisterTask(
          scope, task_name.c_str(), GetTaskDisplayName(scope).c_str(),
          run_command, TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, true)) {
    VLOG(1) << "RegisterWakeTask succeeded: " << task_name;
    return true;
  }

  LOG(ERROR) << "RegisterWakeTask failed: " << task_name;
  return false;
}

void UnregisterWakeTask(UpdaterScope scope) {
  auto task_scheduler = TaskScheduler::CreateInstance();

  const std::wstring task_name = GetTaskName(scope);
  if (task_name.empty()) {
    LOG(ERROR) << "Empty task name during uninstall.";
    return;
  }

  task_scheduler->DeleteTask(task_name.c_str());
  VLOG(1) << "UnregisterWakeTask succeeded: " << task_name;
}

std::vector<IID> GetSideBySideInterfaces() {
  return {
      __uuidof(IUpdaterInternal),
      __uuidof(IUpdaterInternalCallback),
  };
}

std::vector<IID> GetActiveInterfaces() {
  return {
    __uuidof(IUpdateState), __uuidof(IUpdater), __uuidof(IUpdaterObserver),
        __uuidof(IUpdaterRegisterAppCallback), __uuidof(IUpdaterCallback),

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        __uuidof(IAppBundleWeb), __uuidof(IAppWeb), __uuidof(IAppCommandWeb),
        __uuidof(ICompleteStatus), __uuidof(ICurrentState),
        __uuidof(IGoogleUpdate3Web), __uuidof(IPolicyStatus),
        __uuidof(IPolicyStatus2), __uuidof(IPolicyStatus3),
        __uuidof(IPolicyStatusValue), __uuidof(IProcessLauncher),
        __uuidof(IProcessLauncher2),
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            __uuidof(GoogleUpdate3WebUserClass)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      };
    case UpdaterScope::kSystem:
      return {
        __uuidof(UpdaterSystemClass),

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            __uuidof(GoogleUpdate3WebSystemClass),
            __uuidof(ProcessLauncherClass)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      };
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
                                           kLoggingModuleSwitchValue);
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

  list->AddWorkItem(new installer::InstallServiceWorkItem(
      GetServiceName(internal_service).c_str(),
      GetServiceDisplayName(internal_service).c_str(), SERVICE_AUTO_START,
      com_service_command, com_switch, UPDATER_KEY,
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

  static const base::NoDestructor<std::unordered_map<IID, const wchar_t*>>
      kTypeLibIndexes{{
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
          {__uuidof(IAppCommandWeb), kUpdaterLegacyIndex},
          {__uuidof(ICurrentState), kUpdaterLegacyIndex},
          {__uuidof(IGoogleUpdate3Web), kUpdaterLegacyIndex},
          {__uuidof(IPolicyStatus), kUpdaterLegacyIndex},
          {__uuidof(IPolicyStatus2), kUpdaterLegacyIndex},
          {__uuidof(IPolicyStatus3), kUpdaterLegacyIndex},
          {__uuidof(IPolicyStatusValue), kUpdaterLegacyIndex},
          {__uuidof(IProcessLauncher), kUpdaterLegacyIndex},
          {__uuidof(IProcessLauncher2), kUpdaterLegacyIndex},
      }};
  auto index = kTypeLibIndexes->find(iid);
  return index != kTypeLibIndexes->end() ? index->second : L"";
}

void RegisterUserRunAtStartup(const std::wstring& run_value_name,
                              const base::CommandLine& command,
                              WorkItemList* list) {
  DCHECK(list);
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

}  // namespace updater

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/system_report_component.h"

#include <windows.h>

#include <psapi.h>
#include <stdint.h>
#include <winhttp.h>
#include <wrl/client.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/common_registry_names.h"
#include "chrome/chrome_cleaner/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/scoped_timed_task_logger.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/nt_internals.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "components/chrome_cleaner/public/constants/constants.h"

using base::WaitableEvent;

namespace chrome_cleaner {
namespace {

// The initial number of services when enumerating services.
const unsigned int kInitialNumberOfServices = 100;

const wchar_t kServicesKeyPath[] = L"system\\currentcontrolset\\services\\";

const wchar_t kAbsolutePrefix[] = L"\\??\\";
const wchar_t kSystemRootPrefix[] = L"\\systemroot\\";

const wchar_t kNameServerPath[] =
    L"system\\currentcontrolset\\services\\tcpip\\parameters\\interfaces";
const wchar_t kNameServer[] = L"nameserver";

const bool kHasFileInformation = true;
const bool kNoFileInformation = false;

struct RegistryKey {
  HKEY hkey;
  const wchar_t* key_path;
};

struct RegistryValue {
  HKEY hkey;
  const wchar_t* key_path;
  const wchar_t* value_name;
};

const RegistryKey startup_registry_keys[] = {
    {HKEY_LOCAL_MACHINE, L"software\\microsoft\\windows\\currentversion\\run"},
    {HKEY_CURRENT_USER, L"software\\microsoft\\windows\\currentversion\\run"},
    {HKEY_LOCAL_MACHINE,
     L"software\\microsoft\\windows\\currentversion\\runonce"},
    {HKEY_CURRENT_USER,
     L"software\\microsoft\\windows\\currentversion\\runonce"},
    {HKEY_LOCAL_MACHINE,
     L"software\\microsoft\\windows\\currentversion\\runonceex"},
    {HKEY_CURRENT_USER,
     L"software\\microsoft\\windows\\currentversion\\runonceex"},
    {HKEY_LOCAL_MACHINE,
     L"software\\microsoft\\windows\\currentversion\\runservices"},
    {HKEY_CURRENT_USER,
     L"software\\microsoft\\windows\\currentversion\\runservices"},
    {HKEY_LOCAL_MACHINE,
     L"software\\microsoft\\windows\\currentversion\\runservicesonce"},
    {HKEY_CURRENT_USER,
     L"software\\microsoft\\windows\\currentversion\\runservicesonce"}};

const RegistryValue system_path_values[] = {
    {HKEY_LOCAL_MACHINE, L"system\\currentcontrolset\\control\\session manager",
     L"bootexecute"},
    {HKEY_LOCAL_MACHINE, L"system\\currentcontrolset\\control\\session manager",
     L"setupexecute"},
    {HKEY_LOCAL_MACHINE, kAppInitDllsKeyPath, kAppInitDllsValueName},
    {HKEY_LOCAL_MACHINE, L"system\\currentcontrolset\\control\\session manager",
     L"appcertdlls"}};

const RegistryValue system_values[] = {
    {HKEY_LOCAL_MACHINE,
     L"software\\policies\\microsoft\\windows\\windowsupdate",
     L"disablewindowsupdateaccess"}};

const RegistryKey extension_policy_keys[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesWhitelistKeyPath},
    {HKEY_CURRENT_USER, kChromePoliciesWhitelistKeyPath},
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath},
    {HKEY_CURRENT_USER, kChromePoliciesForcelistKeyPath},
    {HKEY_LOCAL_MACHINE, kChromiumPoliciesWhitelistKeyPath},
    {HKEY_CURRENT_USER, kChromiumPoliciesWhitelistKeyPath},
    {HKEY_LOCAL_MACHINE, kChromiumPoliciesForcelistKeyPath},
    {HKEY_CURRENT_USER, kChromiumPoliciesForcelistKeyPath}};

const int64_t kParseAttemptTimeoutMilliseconds = 10000;

// Expand an executable path as if the launch process directory was the
// windows folder. This is used to resolve kernel module path.
bool ExpandToAbsolutePathFromWindowsFolder(const base::FilePath& path,
                                           base::FilePath* output) {
  DCHECK(output);
  if (path.IsAbsolute()) {
    *output = path;
    return true;
  }

  // Retrieve the system folder path.
  base::FilePath system_folder =
      PreFetchedPaths::GetInstance()->GetWindowsFolder();

  base::FilePath absolute_path = system_folder.Append(path);
  if (base::PathExists(absolute_path)) {
    *output = absolute_path;
    return true;
  }

  LOG(ERROR) << "Cannot determine absolute path: file does not exist.";
  return false;
}

void RetrieveDetailedFileInformationFromCommandLine(
    const base::string16& content,
    internal::FileInformation* file_information,
    bool* white_listed) {
  // Handle the case where |content| contains only an executable path.
  base::FilePath expanded_path(
      ExtractExecutablePathFromRegistryContent(content));
  if (base::PathExists(expanded_path)) {
    RetrieveDetailedFileInformation(expanded_path, file_information,
                                    white_listed);
    return;
  }

  // Fallback using the original path.
  RetrieveDetailedFileInformation(base::FilePath(content), file_information,
                                  white_listed);
}

void ReportRegistryValue(const RegKeyPath& key_path,
                         const wchar_t* name,
                         const base::string16& content,
                         bool has_file_information) {
  DCHECK(name);
  if (content.empty())
    return;

  internal::FileInformation file_information;
  if (has_file_information) {
    bool white_listed = false;
    RetrieveDetailedFileInformationFromCommandLine(content, &file_information,
                                                   &white_listed);
    if (white_listed)
      return;
  }

  internal::RegistryValue registry_value = {};
  std::vector<internal::FileInformation> file_informations;
  registry_value.key_path = key_path.FullPath();
  registry_value.value_name = name;
  registry_value.data = content;

  if (has_file_information)
    file_informations.push_back(file_information);

  LoggingServiceAPI::GetInstance()->AddRegistryValue(registry_value,
                                                     file_informations);
}

void ReportRegistryValues(const RegistryValue* report_data,
                          size_t report_data_length,
                          REGSAM access_mask,
                          bool has_file_information) {
  DCHECK(report_data);
  for (size_t offset = 0; offset < report_data_length; ++offset) {
    // Retrieve the content of the registry value.
    base::string16 content;
    const RegKeyPath key_path(report_data[offset].hkey,
                              report_data[offset].key_path, access_mask);
    RegistryError error;
    if (!ReadRegistryValue(key_path, report_data[offset].value_name, &content,
                           nullptr, &error)) {
      LOG_IF(WARNING, error != RegistryError::VALUE_NOT_FOUND)
          << "Failed to read string registry value: '" << key_path.FullPath()
          << "\\" << report_data[offset].value_name << "', error: '"
          << static_cast<int>(error) << "'.";
      continue;
    }
    ReportRegistryValue(key_path, report_data[offset].value_name, content,
                        has_file_information);
  }
}

void ReportRegistryKeys(const RegistryKey* report_data,
                        size_t length,
                        REGSAM access_mask,
                        bool has_file_information) {
  DCHECK(report_data);
  for (size_t offset = 0; offset < length; ++offset) {
    // Enumerate values from the current registry key.
    base::win::RegistryValueIterator values_it(
        report_data[offset].hkey, report_data[offset].key_path, access_mask);
    for (; values_it.Valid(); ++values_it) {
      base::string16 content;
      GetRegistryValueAsString(values_it.Value(), values_it.ValueSize(),
                               values_it.Type(), &content);
      RegKeyPath key_path(report_data[offset].hkey,
                          report_data[offset].key_path);
      ReportRegistryValue(
          key_path, values_it.Name(), content,
          // File information should only be text.
          has_file_information && (values_it.Type() == REG_SZ ||
                                   values_it.Type() == REG_EXPAND_SZ ||
                                   values_it.Type() == REG_MULTI_SZ));
    }
  }
}

// Reports nameservers i.e.
// system\currentcontrolset\services\tcpip\parameters\interfaces\*\nameserver
void ReportNameServer(REGSAM access_mask) {
  base::win::RegistryKeyIterator keys_it(HKEY_LOCAL_MACHINE, kNameServerPath,
                                         access_mask);
  // Check each key at this path.
  for (; keys_it.Valid(); ++keys_it) {
    const base::string16 full_path =
        base::StrCat({kNameServerPath, L"\\", keys_it.Name()});

    base::string16 content;
    uint32_t content_type = REG_NONE;
    const RegKeyPath nameserver_key_path(HKEY_LOCAL_MACHINE, full_path.c_str(),
                                         access_mask);
    // Check to see if this key has a nameserver value who's content is non
    // empty.
    if (!ReadRegistryValue(nameserver_key_path, kNameServer, &content,
                           &content_type, nullptr) ||
        content_type != REG_SZ || content.empty()) {
      continue;
    }

    ReportRegistryValue(nameserver_key_path, kNameServer, content,
                        kNoFileInformation);
  }
}

void ReportAppInitDllsTargets(REGSAM access_mask) {
  base::string16 content;
  uint32_t content_type = REG_NONE;
  const RegKeyPath appinit_dlls_key_path(HKEY_LOCAL_MACHINE,
                                         kAppInitDllsKeyPath, access_mask);
  if (!ReadRegistryValue(appinit_dlls_key_path, kAppInitDllsValueName, &content,
                         &content_type, nullptr) ||
      content_type != REG_SZ) {
    return;
  }

  base::string16 delimiters(PUPData::kCommonDelimiters,
                            PUPData::kCommonDelimitersLength);
  std::vector<base::string16> entries = base::SplitString(
      content, delimiters, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  bool white_listed = false;

  internal::RegistryValue registry_value = {};
  std::vector<internal::FileInformation> file_informations;
  registry_value.key_path = appinit_dlls_key_path.FullPath();
  registry_value.value_name = kAppInitDllsValueName;
  registry_value.data = content;

  for (const auto& entry : entries) {
    base::string16 long_path;
    ConvertToLongPath(entry, &long_path);
    base::FilePath expanded_path(
        ExpandEnvPathAndWow64Path(base::FilePath(long_path)));
    internal::FileInformation file_information;
    RetrieveDetailedFileInformation(expanded_path, &file_information,
                                    &white_listed);
    if (!white_listed)
      file_informations.push_back(file_information);
  }
  LoggingServiceAPI::GetInstance()->AddRegistryValue(registry_value,
                                                     file_informations);
}

void ReportRegistry(REGSAM access_mask) {
  // Report on startup keys/values.
  ReportRegistryKeys(startup_registry_keys, base::size(startup_registry_keys),
                     access_mask, kHasFileInformation);
  ReportRegistryValues(system_path_values, base::size(system_path_values),
                       access_mask, kHasFileInformation);
  ReportRegistryValues(system_values, base::size(system_values), access_mask,
                       kNoFileInformation);

  // Report on extension policy keys.
  ReportRegistryKeys(extension_policy_keys, base::size(extension_policy_keys),
                     access_mask, kNoFileInformation);
  ReportAppInitDllsTargets(access_mask);
  ReportNameServer(access_mask);
}

void ReportRegistry() {
  ReportRegistry(KEY_WOW64_32KEY);
  if (IsX64Architecture())
    ReportRegistry(KEY_WOW64_64KEY);
}

void ReportFoldersUnderPath(const base::FilePath& path, const wchar_t* prefix) {
  base::FileEnumerator folder_enum(path, false,
                                   base::FileEnumerator::DIRECTORIES);
  for (base::FilePath folder = folder_enum.Next(); !folder.empty();
       folder = folder_enum.Next()) {
    LoggingServiceAPI::GetInstance()->AddInstalledProgram(folder);
  }
}

void ReportInstalledPrograms() {
  static unsigned int install_paths[] = {
      base::DIR_PROGRAM_FILES,     base::DIR_PROGRAM_FILESX86,
      base::DIR_PROGRAM_FILES6432, base::DIR_APP_DATA,
      base::DIR_LOCAL_APP_DATA,    base::DIR_COMMON_APP_DATA,
  };
  std::set<base::FilePath> path_processed;
  for (size_t path = 0; path < base::size(install_paths); ++path) {
    base::FilePath install_path;
    bool success = base::PathService::Get(install_paths[path], &install_path);
    if (!success) {
      LOG(ERROR) << "Can't get path from PathService.";
      continue;
    }
    if (path_processed.insert(install_path).second)
      ReportFoldersUnderPath(install_path, L"Program:");
  }
}

void ReportRunningProcesses() {
  base::ProcessIterator iter(nullptr);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
    base::win::ScopedHandle handle(
        ::OpenProcess(PROCESS_QUERY_INFORMATION, false, entry->pid()));
    base::string16 exec_path;
    if (handle.IsValid() &&
        GetProcessExecutablePath(handle.Get(), &exec_path)) {
      internal::FileInformation file_information;
      bool white_listed = false;
      RetrieveDetailedFileInformation(base::FilePath(exec_path),
                                      &file_information, &white_listed);
      if (!white_listed) {
        LoggingServiceAPI::GetInstance()->AddProcess(entry->exe_file(),
                                                     file_information);
      }
    } else {
      LOG(WARNING) << "Unable to query process: '" << entry->exe_file() << "'";
    }
  }
}

bool RetrieveExecutablePathFromPid(DWORD pid, base::FilePath* path) {
  DCHECK(path);
  base::win::ScopedHandle process_handle(
      ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

  base::string16 exe_path;
  if (!GetProcessExecutablePath(process_handle.Get(), &exe_path))
    return false;

  *path = base::FilePath(exe_path);
  return true;
}

bool RetrieveExecutablePathFromServiceName(const wchar_t* service_name,
                                           base::FilePath* service_path) {
  DCHECK(service_name);
  DCHECK(service_path);
  base::string16 subkey_path(kServicesKeyPath);
  subkey_path += service_name;

  base::string16 content;
  if (!ReadRegistryValue(RegKeyPath(HKEY_LOCAL_MACHINE, subkey_path.c_str()),
                         L"imagepath", &content, nullptr, nullptr)) {
    return false;
  }

  base::FilePath file_path = base::FilePath(content);
  // Convert a NT native form ("\Device\HarddiskVolumeXX\...") to the
  // equivalent path that starts with a drive letter ("C:\...").
  base::FilePath dos_file_path;
  if (base::DevicePathToDriveLetterPath(file_path, &dos_file_path)) {
    file_path = dos_file_path;
  } else if (base::StartsWith(file_path.value(), kAbsolutePrefix,
                              base::CompareCase::INSENSITIVE_ASCII)) {
    // Remove the prefix "\??\" in front of the path.
    file_path = base::FilePath(
        file_path.value().substr(base::size(kAbsolutePrefix) - 1));
  } else if (base::StartsWith(file_path.value(), kSystemRootPrefix,
                              base::CompareCase::INSENSITIVE_ASCII)) {
    // Remove the prefix "\systemroot\" in front of the path. The path
    // will be resolved from the system folder.
    file_path = base::FilePath(
        file_path.value().substr(base::size(kSystemRootPrefix) - 1));
  }

  // For relative path, resolve them from %systemroot%.
  base::FilePath absolute_file_path;
  if (ExpandToAbsolutePathFromWindowsFolder(file_path, &absolute_file_path))
    file_path = absolute_file_path;

  *service_path = file_path;
  return true;
}

void ReportRunningServices(DWORD services_type) {
  ScopedScHandle service_manager;
  service_manager.Set(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT));
  if (!service_manager.IsValid()) {
    PLOG(ERROR) << "Cannot open service manager.";
    return;
  }

  std::vector<uint8_t> buffer(kInitialNumberOfServices *
                              sizeof(ENUM_SERVICE_STATUS_PROCESS));
  ULONG number_of_service = 0;
  while (true) {
    ULONG more_bytes_needed = 0;
    if (::EnumServicesStatusEx(service_manager.Get(), SC_ENUM_PROCESS_INFO,
                               services_type, SERVICE_ACTIVE, &buffer[0],
                               buffer.size(), &more_bytes_needed,
                               &number_of_service, nullptr, nullptr) != FALSE) {
      break;
    }

    if (::GetLastError() == ERROR_MORE_DATA) {
      buffer.resize(buffer.size() + more_bytes_needed);
    } else {
      PLOG(ERROR) << "Cannot enumerate running services.";
      return;
    }
  }

  LPENUM_SERVICE_STATUS_PROCESS services =
      reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESS>(&buffer[0]);
  for (size_t i = 0; i < number_of_service; i++) {
    base::FilePath service_path;
    bool service_found = false;
    if (services[i].ServiceStatusProcess.dwProcessId != 0) {
      service_found = RetrieveExecutablePathFromPid(
          services[i].ServiceStatusProcess.dwProcessId, &service_path);
    } else {
      service_found = RetrieveExecutablePathFromServiceName(
          services[i].lpServiceName, &service_path);
    }
    if (service_found) {
      internal::FileInformation file_information;
      bool white_listed = false;
      RetrieveDetailedFileInformation(service_path, &file_information,
                                      &white_listed);

      if (!white_listed) {
        LoggingServiceAPI::GetInstance()->AddService(services[i].lpDisplayName,
                                                     services[i].lpServiceName,
                                                     file_information);
      }
    }
  }
}

void ReportScheduledTasks() {
  std::unique_ptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance());
  std::vector<base::string16> task_names;
  if (!task_scheduler->GetTaskNameList(&task_names)) {
    LOG(ERROR) << "Failed to enumerate scheduled tasks.";
    return;
  }

  for (const auto& task_name : task_names) {
    TaskScheduler::TaskInfo task_info;
    if (!task_scheduler->GetTaskInfo(task_name.c_str(), &task_info)) {
      LOG(ERROR) << "Failed to get info for task '" << task_name << "'";
      continue;
    }
    std::vector<internal::FileInformation> actions;
    for (size_t action = 0; action < task_info.exec_actions.size(); ++action) {
      bool white_listed = false;
      internal::FileInformation file_information;
      RetrieveDetailedFileInformation(
          task_info.exec_actions[action].application_path, &file_information,
          &white_listed);
      if (!white_listed)
        actions.push_back(file_information);
    }
    LoggingServiceAPI::GetInstance()->AddScheduledTask(
        task_name, task_info.description, actions);
  }
}

// Report the file paths of all DLLs loaded into the process with |pid|. The
// |prefix| is prepended to the log line to qualify the running process.
// |paths_reported| contains already reported paths to avoid duplication.
// Reported paths are added to |paths_reported|.
void ReportLoadedModules(DWORD pid,
                         const wchar_t* prefix,
                         ModuleHost host,
                         std::set<base::FilePath>* paths_reported) {
  DCHECK(prefix);
  DCHECK(paths_reported);
  base::win::ScopedHandle snapshot(
      ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
  if (!snapshot.Get())
    return;

  MODULEENTRY32 module = {sizeof(module)};
  if (!::Module32First(snapshot.Get(), &module))
    return;

  do {
    base::FilePath file_path(module.szExePath);
    if (!paths_reported->insert(file_path).second) {
      // File details are already reported.
      continue;
    }

    internal::FileInformation file_information;
    bool white_listed = false;
    RetrieveDetailedFileInformation(file_path, &file_information,
                                    &white_listed);
    if (!white_listed) {
      LoggingServiceAPI::GetInstance()->AddLoadedModule(module.szModule, host,
                                                        file_information);
    }
  } while (::Module32Next(snapshot.Get(), &module));
}

void ReportLoadedModulesOfCurrentProcess() {
  std::set<base::FilePath> paths_reported;
  ReportLoadedModules(::GetCurrentProcessId(), L"CCT",
                      ModuleHost::CHROME_CLEANUP_TOOL, &paths_reported);
}

// Report the DLLs for every running process |executable|. DLLs are reported
// only once.
void ReportLoadedModulesOfRunningProcesses(const wchar_t* executable,
                                           ModuleHost host) {
  DCHECK(executable);

  // |paths_reported| is used to avoid duplicate report of the same path for
  // different running chrome processes.
  std::set<base::FilePath> paths_reported;
  base::NamedProcessIterator iter(executable, nullptr);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry())
    ReportLoadedModules(entry->pid(), executable, host, &paths_reported);
}

void ReportLayeredServiceProviders() {
  LSPPathToGUIDs providers;
  GetLayeredServiceProviders(LayeredServiceProviderWrapper(), &providers);
  for (LSPPathToGUIDs::const_iterator provider = providers.begin();
       provider != providers.end(); ++provider) {
    const base::FilePath& path = provider->first;

    internal::FileInformation file_information;
    bool white_listed = false;
    RetrieveDetailedFileInformationFromCommandLine(
        path.value(), &file_information, &white_listed);

    if (!white_listed) {
      std::vector<base::string16> logged_guids;
      const std::set<GUID, GUIDLess>& guids = provider->second;
      for (std::set<GUID, GUIDLess>::const_iterator guid = guids.begin();
           guid != guids.end(); ++guid) {
        base::string16 guid_str;
        GUIDToString(*guid, &guid_str);
        logged_guids.push_back(guid_str);
      }
      LoggingServiceAPI::GetInstance()->AddLayeredServiceProvider(
          logged_guids, file_information);
    }
  }
}

void ReportProxySettingsInformation() {
  // Retrieve the default Internet Explorer proxy configuration for the current
  // user.
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_proxy_info;
  if (::WinHttpGetIEProxyConfigForCurrentUser(&ie_proxy_info)) {
    // Report proxy information when it's not the default configuration.
    if (ie_proxy_info.lpszProxy || ie_proxy_info.lpszProxyBypass ||
        ie_proxy_info.lpszAutoConfigUrl || ie_proxy_info.fAutoDetect) {
      base::string16 config =
          (ie_proxy_info.lpszProxy ? ie_proxy_info.lpszProxy : L"");
      base::string16 bypass =
          (ie_proxy_info.lpszProxyBypass ? ie_proxy_info.lpszProxyBypass : L"");
      base::string16 autoconfig =
          (ie_proxy_info.lpszAutoConfigUrl ? ie_proxy_info.lpszAutoConfigUrl
                                           : L"");
      LoggingServiceAPI::GetInstance()->SetWinInetProxySettings(
          config, bypass, autoconfig, ie_proxy_info.fAutoDetect != FALSE);
    }

    if (ie_proxy_info.lpszProxy)
      ::GlobalFree(ie_proxy_info.lpszProxy);
    if (ie_proxy_info.lpszProxyBypass)
      ::GlobalFree(ie_proxy_info.lpszProxyBypass);
    if (ie_proxy_info.lpszAutoConfigUrl)
      ::GlobalFree(ie_proxy_info.lpszAutoConfigUrl);
  }

  // Retrieve the default WinHTTP proxy configuration from the registry.
  WINHTTP_PROXY_INFO proxy_info;
  if (::WinHttpGetDefaultProxyConfiguration(&proxy_info)) {
    const char* access_type = nullptr;
    switch (proxy_info.dwAccessType) {
      case WINHTTP_ACCESS_TYPE_NO_PROXY:
        access_type = "no proxy";
        break;
      case WINHTTP_ACCESS_TYPE_DEFAULT_PROXY:
        access_type = "default proxy";
        break;
      case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
        access_type = "named proxy";
        break;
      default:
        access_type = "unknown";
        break;
    }

    // Report proxy information when it's not the default configuration.
    if (proxy_info.dwAccessType != WINHTTP_ACCESS_TYPE_NO_PROXY ||
        proxy_info.lpszProxy || proxy_info.lpszProxyBypass) {
      base::string16 config =
          (proxy_info.lpszProxy ? proxy_info.lpszProxy : L"");
      base::string16 bypass =
          (proxy_info.lpszProxyBypass ? proxy_info.lpszProxyBypass : L"");
      LoggingServiceAPI::GetInstance()->SetWinHttpProxySettings(config, bypass);
    }

    if (proxy_info.lpszProxy)
      ::GlobalFree(proxy_info.lpszProxy);
    if (proxy_info.lpszProxyBypass)
      ::GlobalFree(proxy_info.lpszProxyBypass);
  }
}

void ReportForcelistExtensions() {
  std::vector<ExtensionPolicyRegistryEntry> policies;
  GetExtensionForcelistRegistryPolicies(&policies);

  for (const ExtensionPolicyRegistryEntry& policy : policies) {
    LoggingServiceAPI::GetInstance()->AddInstalledExtension(
        policy.extension_id,
        ExtensionInstallMethod::POLICY_EXTENSION_FORCELIST);
  }
}

void ReportInstalledExtensions(JsonParserAPI* json_parser) {
  DCHECK(json_parser);
  // TODO(proberge): Temporarily allowing syncing to avoid crashes in debug
  // mode. This isn't catastrophic since the cleanup tool doesn't have a UI and
  // the system report is collected at the end of the process. We also assert
  // blocking is allowed here since it will block the thread.
  base::AssertBlockingAllowedDeprecated();
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;

  ReportForcelistExtensions();

  std::vector<ExtensionPolicyRegistryEntry> extension_settings_policies;
  WaitableEvent extension_settings_done(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(
      json_parser, &extension_settings_policies, &extension_settings_done);

  std::vector<ExtensionPolicyFile> master_preferences_policies;
  WaitableEvent master_preferences_done(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(json_parser, &master_preferences_policies,
                                 &master_preferences_done);

  std::vector<ExtensionPolicyFile> default_extension_policies;
  WaitableEvent default_extensions_done(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(json_parser, &default_extension_policies,
                                     &default_extensions_done);

  // Wait for all asynchronous parsing to be done
  const base::TimeTicks end_time =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kParseAttemptTimeoutMilliseconds);
  extension_settings_done.TimedWaitUntil(end_time);
  master_preferences_done.TimedWaitUntil(end_time);
  default_extensions_done.TimedWaitUntil(end_time);

  // Log extensions that were found
  for (const ExtensionPolicyRegistryEntry& policy : extension_settings_policies)
    LoggingServiceAPI::GetInstance()->AddInstalledExtension(
        policy.extension_id, ExtensionInstallMethod::POLICY_EXTENSION_SETTINGS);

  for (const ExtensionPolicyFile& policy : master_preferences_policies)
    LoggingServiceAPI::GetInstance()->AddInstalledExtension(
        policy.extension_id, ExtensionInstallMethod::POLICY_MASTER_PREFERENCES);

  for (const ExtensionPolicyFile& policy : default_extension_policies)
    LoggingServiceAPI::GetInstance()->AddInstalledExtension(
        policy.extension_id, ExtensionInstallMethod::DEFAULT_APPS_EXTENSION);
}

}  // namespace

SystemReportComponent::SystemReportComponent(JsonParserAPI* json_parser)
    : created_report_(false), json_parser_(json_parser) {}

void SystemReportComponent::PreScan() {}

void SystemReportComponent::PostScan(const std::vector<UwSId>& found_pups) {
  // If no removable UwS was found, we should collect the detailed system report
  // now since there won't be a post-cleanup called.
  if (found_pups.size() == 0 ||
      !PUPData::HasFlaggedPUP(found_pups, &PUPData::HasRemovalFlag))
    CreateFullSystemReport();
}

void SystemReportComponent::PreCleanup() {}

void SystemReportComponent::PostCleanup(ResultCode result_code,
                                        RebooterAPI* rebooter) {
  // If the user cancels the cleanup, don't collect system information since the
  // old UI quits without giving the user a chance to opt out of uploading this.
  if (result_code == RESULT_CODE_CANCELED)
    return;

  CreateFullSystemReport();
}

void SystemReportComponent::PostValidation(ResultCode result_code) {}

void SystemReportComponent::OnClose(ResultCode result_code) {}

void SystemReportComponent::CreateFullSystemReport() {
  LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();

  // Don't collect a system report if we've already collected one
  if (created_report_)
    return;

  // Don't collect a system report if logs won't be uploaded.
  if (!logging_service->uploads_enabled()) {
    // TODO(proberge): Remove this by EOQ4 2018.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(kDumpRawLogsSwitch)) {
      ReportInstalledExtensions(json_parser_);
      created_report_ = true;
    }

    return;
  }

  logging_service->SetDetailedSystemReport(true);

  ScopedTimedTaskLogger scoped_timed_task_logger(
      "Logging detailed system report");
  // Make sure this process has the debug privilege to allow opening more
  // running processes. If we have to obtain it, be sure to drop it again to
  // leave the system in the state we found it. Some unit tests depend on not
  // having debug privileges.
  base::ScopedClosureRunner release_debug_rights;
  if (!HasDebugRightsPrivileges() && AcquireDebugRightsPrivileges()) {
    release_debug_rights.ReplaceClosure(
        base::BindOnce(base::IgnoreResult(&ReleaseDebugRightsPrivileges)));
  }

  ReportLoadedModulesOfCurrentProcess();
  ReportLoadedModulesOfRunningProcesses(L"chrome.exe", ModuleHost::CHROME);
  ReportRunningProcesses();
  ReportRunningServices(SERVICE_WIN32);
  {
    // The wow64 redirection must be disabled because path are reported as
    // viewed by the kernel.
    ScopedDisableWow64Redirection wow64_disabled;
    ReportRunningServices(SERVICE_DRIVER);
  }
  ReportRegistry();
  ReportScheduledTasks();
  ReportInstalledPrograms();
  ReportLayeredServiceProviders();
  ReportProxySettingsInformation();
  ReportInstalledExtensions(json_parser_);

  created_report_ = true;
}

}  // namespace chrome_cleaner

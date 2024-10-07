// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/setup/setup_util.h"

#include <objbase.h>

#include <windows.h>

#include <stddef.h>
#include <wtsapi32.h>

#include <initializer_list>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/user_hive_visitor.h"
#include "chrome/installer/util/app_command.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace installer {

namespace {

// Event log providers registry location.
constexpr wchar_t kEventLogProvidersRegPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

// Remove the registration of the browser's DelegateExecute verb handler class.
// This was once registered in support of "metro" mode on Windows 8.
void RemoveLegacyIExecuteCommandKey(const InstallerState& installer_state) {
  const std::wstring handler_class_uuid =
      install_static::GetLegacyCommandExecuteImplClsid();

  // No work to do if this mode of install never registered a DelegateExecute
  // verb handler.
  if (handler_class_uuid.empty())
    return;

  const HKEY root = installer_state.root_key();
  std::wstring delegate_execute_path(L"Software\\Classes\\CLSID\\");
  delegate_execute_path.append(handler_class_uuid);

  // Delete both 64 and 32 keys to handle 32->64 or 64->32 migration.
  for (REGSAM bitness : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
    installer::DeleteRegistryKey(root, delegate_execute_path, bitness);
}

// "The binaries" once referred to the on-disk footprint of Chrome and/or Chrome
// Frame when the products were configured to share such on-disk bits. Support
// for this mode of install was dropped from ToT in December 2016. Remove any
// stray bits in the registry leftover from such installs.
void RemoveBinariesVersionKey(const InstallerState& installer_state) {
#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::wstring path(install_static::GetClientsKeyPath(
      L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}"));
#else
  // Assume that non-Google is Chromium branding.
  std::wstring path(L"Software\\Chromium Binaries");
#endif
  installer::DeleteRegistryKey(installer_state.root_key(), path,
                               KEY_WOW64_32KEY);
#endif  // !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
}

void RemoveAppLauncherVersionKey(const InstallerState& installer_state) {
// The app launcher was only registered for Google Chrome.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr wchar_t kLauncherGuid[] =
      L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

  installer::DeleteRegistryKey(installer_state.root_key(),
                               install_static::GetClientsKeyPath(kLauncherGuid),
                               KEY_WOW64_32KEY);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void RemoveLegacyChromeAppCommands(const InstallerState& installer_state) {
// These app commands were only registered for Google Chrome.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  AppCommand(L"install-extension", {})
      .AddDeleteAppCommandWorkItems(installer_state.root_key(), list.get());
  list->Do();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

const char kUnPackStatusMetricsName[] = "Setup.Install.LzmaUnPackStatus";

base::Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path) {
  VLOG(1) << "Looking for Chrome version folder under " << chrome_path.value();
  base::FileEnumerator version_enum(chrome_path, false,
                                    base::FileEnumerator::DIRECTORIES);
  // TODO(tommi): The version directory really should match the version of
  // setup.exe.  To begin with, we should at least DCHECK that that's true.

  std::unique_ptr<base::Version> max_version(new base::Version("0.0.0.0"));
  bool version_found = false;

  while (!version_enum.Next().empty()) {
    base::FileEnumerator::FileInfo find_data = version_enum.GetInfo();
    VLOG(1) << "directory found: " << find_data.GetName().value();

    std::unique_ptr<base::Version> found_version(
        new base::Version(base::WideToASCII(find_data.GetName().value())));
    if (found_version->IsValid() &&
        found_version->CompareTo(*max_version.get()) > 0) {
      max_version = std::move(found_version);
      version_found = true;
    }
  }

  return (version_found ? max_version.release() : nullptr);
}

base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state,
                                  const base::Version& desired_version) {
  if (desired_version.IsValid()) {
    base::FilePath archive(
        installer_state.GetInstallerDirectory(desired_version)
            .Append(kChromeArchive));
    return base::PathExists(archive) ? archive : base::FilePath();
  }

  // Check based on the version number advertised to Google Update, since that
  // is the value used to select a specific differential update. If an archive
  // can't be found using that, fallback to using the newest version present.
  base::FilePath patch_source;
  const ProductState* product =
      original_state.GetProductState(installer_state.system_install());
  if (product) {
    patch_source = installer_state.GetInstallerDirectory(product->version())
                       .Append(installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  std::unique_ptr<base::Version> version(
      installer::GetMaxVersionFromArchiveDir(installer_state.target_path()));
  if (version) {
    patch_source = installer_state.GetInstallerDirectory(*version).Append(
        installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  return base::FilePath();
}

bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32_t delay_before_delete_ms) {
  static const wchar_t kRunDll32Path[] =
      L"%SystemRoot%\\System32\\rundll32.exe";
  wchar_t rundll32[MAX_PATH];
  DWORD size =
      ExpandEnvironmentStrings(kRunDll32Path, rundll32, std::size(rundll32));
  if (!size || size >= MAX_PATH)
    return false;

  STARTUPINFO startup = {sizeof(STARTUPINFO)};
  PROCESS_INFORMATION pi = {0};
  BOOL ok = ::CreateProcess(nullptr, rundll32, nullptr, nullptr, FALSE,
                            CREATE_SUSPENDED, nullptr, nullptr, &startup, &pi);
  if (ok) {
    // We use the main thread of the new process to run:
    //   Sleep(delay_before_delete_ms);
    //   DeleteFile(path);
    //   ExitProcess(0);
    // This runs before the main routine of the process runs, so it doesn't
    // matter much which executable we choose except that we don't want to
    // use e.g. a console app that causes a window to be created.
    size = static_cast<DWORD>((path.value().length() + 1) *
                              sizeof(path.value()[0]));
    void* mem = ::VirtualAllocEx(pi.hProcess, nullptr, size, MEM_COMMIT,
                                 PAGE_READWRITE);
    if (mem) {
      SIZE_T written = 0;
      ::WriteProcessMemory(pi.hProcess, mem, path.value().c_str(),
                           (path.value().size() + 1) * sizeof(path.value()[0]),
                           &written);
      HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
      PAPCFUNC sleep =
          reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "Sleep"));
      PAPCFUNC delete_file =
          reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "DeleteFileW"));
      PAPCFUNC exit_process =
          reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "ExitProcess"));
      if (!sleep || !delete_file || !exit_process) {
        NOTREACHED_IN_MIGRATION();
        ok = FALSE;
      } else {
        ::QueueUserAPC(sleep, pi.hThread, delay_before_delete_ms);
        ::QueueUserAPC(delete_file, pi.hThread,
                       reinterpret_cast<ULONG_PTR>(mem));
        ::QueueUserAPC(exit_process, pi.hThread, 0);
        ::ResumeThread(pi.hThread);
      }
    } else {
      PLOG(ERROR) << "VirtualAllocEx";
      ::TerminateProcess(pi.hProcess, ~static_cast<UINT>(0));
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  }

  return ok != FALSE;
}

bool AdjustThreadPriority() {
  const DWORD priority_class = ::GetPriorityClass(::GetCurrentProcess());
  if (priority_class == BELOW_NORMAL_PRIORITY_CLASS ||
      priority_class == IDLE_PRIORITY_CLASS) {
    // Don't use SetPriorityClass with PROCESS_MODE_BACKGROUND_BEGIN because it
    // will cap the process working set to 32 MiB. See
    // https://crbug.com/1475179.
    const BOOL result =
        ::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    PLOG_IF(WARNING, !result) << "Failed to enter background mode.";
    return !!result;
  }

  if (priority_class == 0)
    PLOG(WARNING) << "Failed to get the process's priority class.";

  return false;
}

bool IsUninstallSuccess(InstallStatus install_status) {
  // The following status values represent failed uninstalls:
  // 15: CHROME_NOT_INSTALLED
  // 20: UNINSTALL_FAILED
  // 21: UNINSTALL_CANCELLED
  return (install_status == UNINSTALL_SUCCESSFUL ||
          install_status == UNINSTALL_REQUIRES_REBOOT);
}

bool ContainsUnsupportedSwitch(const base::CommandLine& cmd_line) {
  static const char* const kLegacySwitches[] = {
      // Chrome Frame ready-mode.
      "ready-mode",
      "ready-mode-opt-in",
      "ready-mode-temp-opt-out",
      "ready-mode-end-temp-opt-out",
      // Chrome Frame quick-enable.
      "quick-enable-cf",
      // Installation of Chrome Frame.
      "chrome-frame",
      "migrate-chrome-frame",
      // Stand-alone App Launcher.
      "app-host",
      "app-launcher",
  };
  for (size_t i = 0; i < std::size(kLegacySwitches); ++i) {
    if (cmd_line.HasSwitch(kLegacySwitches[i]))
      return true;
  }
  return false;
}

bool IsProcessorSupported() {
#if defined(ARCH_CPU_X86_FAMILY)
  return base::CPU().has_sse3();
#elif defined(ARCH_CPU_ARM64)
  return true;
#else
#error Port
#endif
}

void DeleteRegistryKeyPartial(
    HKEY root,
    const std::wstring& path,
    const std::vector<std::wstring>& keys_to_preserve) {
  // Downcase the list of keys to preserve (all must be ASCII strings).
  std::set<std::wstring> lowered_keys_to_preserve;
  base::ranges::transform(
      keys_to_preserve,
      std::inserter(lowered_keys_to_preserve, lowered_keys_to_preserve.begin()),
      [](const std::wstring& str) {
        DCHECK(!str.empty());
        DCHECK(base::IsStringASCII(str));
        return base::ToLowerASCII(str);
      });
  base::win::RegKey key;
  LONG result =
      key.Open(root, path.c_str(),
               (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE));
  if (result != ERROR_SUCCESS) {
    LOG_IF(ERROR, result != ERROR_FILE_NOT_FOUND)
        << "Failed to open " << path << "; result = " << result;
    return;
  }

  // Repeatedly iterate over all subkeys deleting those that should not be
  // preserved until only those remain. Multiple passes are needed since
  // deleting one key may change the enumeration order of all remaining keys.

  // Subkeys or values to be skipped on subsequent passes.
  std::set<std::wstring> to_skip;
  DWORD index = 0;
  const size_t kMaxKeyNameLength = 256;  // MSDN says 255; +1 for terminator.
  std::wstring name(kMaxKeyNameLength, wchar_t());
  bool did_delete = false;  // True if at least one item was deleted.
  while (true) {
    DWORD name_length = base::saturated_cast<DWORD>(name.capacity());
    name.resize(name_length);
    result = ::RegEnumKeyEx(key.Handle(), index, &name[0], &name_length,
                            nullptr, nullptr, nullptr, nullptr);
    if (result == ERROR_MORE_DATA) {
      // Unexpected, but perhaps the max key name length was raised. MSDN
      // doesn't clearly say that name_length will contain the necessary
      // length in this case, so double the buffer and try again.
      name.reserve(name.capacity() * 2);
      continue;
    }
    if (result == ERROR_NO_MORE_ITEMS) {
      if (!did_delete)
        break;  // All subkeys were deleted. The job is done.
      // Otherwise, loop again.
      did_delete = false;
      index = 0;
      continue;
    }
    if (result != ERROR_SUCCESS)
      break;
    // Shrink the string to the actual length of the name.
    name.resize(name_length);

    // Skip over this key if it couldn't be deleted on a previous iteration.
    if (to_skip.count(name)) {
      ++index;
      continue;
    }

    // Skip over this key if it is one of the keys to preserve.
    if (base::IsStringASCII(name) &&
        lowered_keys_to_preserve.count(base::ToLowerASCII(name))) {
      // Add the true name of the key to the list of keys to skip for subsequent
      // iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }

    // Delete this key.
    result = key.DeleteKey(name.c_str());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete subkey " << name << " under path "
                 << path;
      // Skip over this key on subsequent iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }
    did_delete = true;
  }

  // Delete the key if it no longer has any subkeys.
  if (to_skip.empty()) {
    result = key.DeleteKey(L"");
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to delete key " << path;
    }
    return;
  }

  // Delete all values since subkeys are being preserved.
  to_skip.clear();
  did_delete = false;
  index = 0;
  while (true) {
    DWORD name_length = base::saturated_cast<int16_t>(name.capacity());
    name.resize(name_length);
    result = ::RegEnumValue(key.Handle(), index, &name[0], &name_length,
                            nullptr, nullptr, nullptr, nullptr);
    if (result == ERROR_MORE_DATA) {
      if (name_length <
          static_cast<DWORD>(std::numeric_limits<int16_t>::max())) {
        // Double the space to hold the value name and try again.
        name.reserve(name.capacity() * 2);
        continue;
      }
      // Otherwise, the max has been exceeded. Nothing more to be done.
      break;
    }
    if (result == ERROR_NO_MORE_ITEMS) {
      if (!did_delete)
        break;  // All values were deleted. The job is done.
      // Otherwise, loop again.
      did_delete = false;
      index = 0;
      continue;
    }
    if (result != ERROR_SUCCESS)
      break;
    // Shrink the string to the actual length of the name.
    name.resize(name_length);

    // Skip over this value if it couldn't be deleted on a previous iteration.
    if (to_skip.count(name)) {
      ++index;
      continue;
    }

    // Delete this value.
    result = key.DeleteValue(name.c_str());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete value " << name << " in key " << path;
      // Skip over this value on subsequent iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }
    did_delete = true;
  }
}

bool IsDowngradeAllowed(const InitialPreferences& prefs) {
  bool allow_downgrade = false;
  return prefs.GetBool(initial_preferences::kAllowDowngrade,
                       &allow_downgrade) &&
         allow_downgrade;
}

int GetInstallAge(const InstallerState& installer_state) {
  base::File::Info info;
  if (!base::GetFileInfo(installer_state.target_path(), &info))
    return -1;
  base::TimeDelta age = base::Time::Now() - info.creation_time;
  return age >= base::TimeDelta() ? age.InDays() : -1;
}

void RecordUnPackMetrics(UnPackStatus unpack_status, UnPackConsumer consumer) {
  std::string consumer_name;

  switch (consumer) {
    case UnPackConsumer::CHROME_ARCHIVE_PATCH:
      consumer_name = "ChromeArchivePatch";
      break;
    case UnPackConsumer::COMPRESSED_CHROME_ARCHIVE:
      consumer_name = "CompressedChromeArchive";
      break;
    case UnPackConsumer::SETUP_EXE_PATCH:
      consumer_name = "SetupExePatch";
      break;
    case UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE:
      consumer_name = "UncompressedChromeArchive";
      break;
  }

  base::UmaHistogramExactLinear(
      std::string(std::string(kUnPackStatusMetricsName) + "_" + consumer_name),
      unpack_status, UNPACK_STATUS_COUNT);
}

void RegisterEventLogProvider(const base::FilePath& install_directory,
                              const base::Version& version) {
  std::wstring reg_path(kEventLogProvidersRegPath);
  reg_path.append(install_static::InstallDetails::Get().install_full_name());
  VLOG(1) << "Registering Chrome's event log provider at " << reg_path;

  std::unique_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  work_item_list->set_log_message("Register event log provider");

  work_item_list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, reg_path,
                                          WorkItem::kWow64Default);
  // Speicifes the number of event categories defined in the dll.
  work_item_list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, reg_path, WorkItem::kWow64Default, L"CategoryCount",
      static_cast<DWORD>(1), true);
  // Specifies the event type emitted by this event source.
  work_item_list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, reg_path, WorkItem::kWow64Default, L"TypesSupported",
      static_cast<DWORD>(EVENTLOG_ERROR_TYPE | EVENTLOG_INFORMATION_TYPE |
                         EVENTLOG_WARNING_TYPE),
      true);

  const base::FilePath provider(
      install_directory.AppendASCII(version.GetString())
          .Append(FILE_PATH_LITERAL("eventlog_provider.dll")));

  static constexpr const wchar_t* kFileKeys[] = {
      L"CategoryMessageFile",
      L"EventMessageFile",
      L"ParameterMessageFile",
  };
  for (const wchar_t* file_key : kFileKeys) {
    work_item_list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, reg_path,
                                           WorkItem::kWow64Default, file_key,
                                           provider.value(), true);
  }

  // if the operation fails we log the error but still continue because none of
  // these are critical for the proper operation of the browser.
  if (!work_item_list->Do())
    work_item_list->Rollback();
}

void DeRegisterEventLogProvider() {
  std::wstring reg_path(kEventLogProvidersRegPath);
  reg_path.append(install_static::InstallDetails::Get().install_full_name());

  // TODO(http://crbug.com/668120): If the Event Viewer is open the provider dll
  // will fail to get deleted. This doesn't fail the uninstallation altogether
  // but leaves files behind.
  installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
                               WorkItem::kWow64Default);
}

void DoLegacyCleanups(const InstallerState& installer_state,
                      InstallStatus install_status) {
  // Do no harm if the install didn't succeed.
  if (InstallUtil::GetInstallReturnCode(install_status))
    return;

  // Cleanups that apply to any install mode.
  RemoveLegacyIExecuteCommandKey(installer_state);

  // The cleanups below only apply to normal Chrome, not side-by-side (canary).
  if (!install_static::InstallDetails::Get().is_primary_mode())
    return;

  RemoveBinariesVersionKey(installer_state);
  RemoveAppLauncherVersionKey(installer_state);
  RemoveLegacyChromeAppCommands(installer_state);
}

base::Time GetConsoleSessionStartTime() {
  constexpr DWORD kInvalidSessionId = 0xFFFFFFFF;
  DWORD console_session_id = ::WTSGetActiveConsoleSessionId();
  if (console_session_id == kInvalidSessionId)
    return base::Time();
  wchar_t* buffer = nullptr;
  DWORD buffer_size = 0;
  if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                    console_session_id, WTSSessionInfo, &buffer,
                                    &buffer_size)) {
    return base::Time();
  }
  absl::Cleanup wts_deleter = [buffer] { ::WTSFreeMemory(buffer); };

  WTSINFO* wts_info = nullptr;
  if (buffer_size < sizeof(*wts_info))
    return base::Time();

  wts_info = reinterpret_cast<WTSINFO*>(buffer);
  FILETIME filetime = {wts_info->LogonTime.u.LowPart,
                       static_cast<DWORD>(wts_info->LogonTime.u.HighPart)};
  return base::Time::FromFileTime(filetime);
}

std::optional<std::string> DecodeDMTokenSwitchValue(
    const std::wstring& encoded_token) {
  if (encoded_token.empty()) {
    LOG(ERROR) << "Empty DMToken specified on the command line";
    return std::nullopt;
  }

  // The token passed on the command line is base64-encoded, but since this is
  // on Windows, it is passed in as a wide string containing base64 values only.
  std::string token;
  if (!base::IsStringASCII(encoded_token) ||
      !base::Base64Decode(base::WideToASCII(encoded_token), &token)) {
    LOG(ERROR) << "DMToken passed on the command line is not correctly encoded";
    return std::nullopt;
  }

  return token;
}

std::optional<std::string> DecodeNonceSwitchValue(
    const std::string& encoded_nonce) {
  if (encoded_nonce.empty()) {
    // The nonce command line argument is optional.  If none is specified use
    // an empty string.
    return std::string();
  }

  // The nonce passed on the command line is base64-encoded.
  std::string nonce;
  if (!base::Base64Decode(encoded_nonce, &nonce)) {
    LOG(ERROR) << "Nonce passed on the command line is not correctly encoded";
    return std::nullopt;
  }

  return nonce;
}

bool StoreDMToken(const std::string& token) {
  DCHECK(install_static::IsSystemInstall());

  if (token.size() > kMaxDMTokenLength) {
    LOG(ERROR) << "DMToken length out of bounds";
    return false;
  }

  // Write the token both to the app-neutral and browser-specific locations.
  // Only the former is mandatory -- the latter is best-effort.
  base::win::RegKey key;
  std::wstring value_name;
  bool succeeded = false;
  for (const auto& is_browser_location : {InstallUtil::BrowserLocation(false),
                                          InstallUtil::BrowserLocation(true)}) {
    std::tie(key, value_name) = InstallUtil::GetCloudManagementDmTokenLocation(
        InstallUtil::ReadOnly(false), is_browser_location);
    // If the key couldn't be opened on the first iteration (the mandatory
    // location), return failure straight away. Otherwise, continue iterating.
    if (!key.Valid()) {
      if (succeeded)
        continue;
      // Logging already performed in GetCloudManagementDmTokenLocation.
      return false;
    }

    auto result =
        key.WriteValue(value_name.c_str(), token.data(),
                       base::saturated_cast<DWORD>(token.size()), REG_BINARY);
    if (result == ERROR_SUCCESS) {
      succeeded = true;
    } else if (!succeeded) {
      ::SetLastError(result);
      PLOG(ERROR) << "Unable to write specified DMToken to the registry";
      return false;
    }  // Else ignore the failure to write to the best-effort location.
  }

  VLOG(1) << "Successfully stored specified DMToken in the registry.";
  return true;
}

bool DeleteDMToken() {
  DCHECK(install_static::IsSystemInstall());

  // Delete the token from both the app-neutral and browser-specific locations.
  // Only the former is mandatory -- the latter is best-effort.
  for (const auto& is_browser_location : {InstallUtil::BrowserLocation(false),
                                          InstallUtil::BrowserLocation(true)}) {
    auto [key_path, value_name] =
        InstallUtil::GetCloudManagementDmTokenPath(is_browser_location);
    REGSAM wow_access = is_browser_location ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;

    base::win::RegKey key;
    auto result = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE | wow_access);
    if (result == ERROR_FILE_NOT_FOUND) {
      // The registry key which stores the DMToken value was not found, so
      // deletion is not necessary.
      continue;
    }
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to open registry key HKLM\\" << key_path
                  << " for deletion";
      // If the key couldn't be opened for the mandatory location, return
      // failure immediately. Otherwise, continue iterating.
      if (!is_browser_location)
        return false;
      continue;
    }

    if (!DeleteRegistryValue(key.Handle(), std::wstring(), wow_access,
                             value_name)) {
      if (!is_browser_location)
        return false;  // Logging already performed in `DeleteRegistryValue()`.
      continue;
    }  // Else ignore the failure to write to the best-effort location.

    // Delete the key if no other values or keys are present.
    if (key.GetValueCount().value_or(1) == 0) {
      key.DeleteKey(L"", base::win::RegKey::RecursiveDelete(false));
    }
  }

  VLOG(1) << "Successfully deleted DMToken from the registry.";
  return true;
}

base::FilePath GetNotificationHelperPath(const base::FilePath& target_path,
                                         const base::Version& version) {
  return target_path.AppendASCII(version.GetString())
      .Append(kNotificationHelperExe);
}

base::FilePath GetWerHelperPath(const base::FilePath& target_path,
                                const base::Version& version) {
  return target_path.AppendASCII(version.GetString()).Append(kWerDll);
}

std::wstring GetWerHelperRegistryPath() {
  return L"Software\\Microsoft\\Windows\\Windows Error Reporting"
         L"\\RuntimeExceptionHelperModules";
}

base::FilePath GetElevationServicePath(const base::FilePath& target_path,
                                       const base::Version& version) {
  return target_path.AppendASCII(version.GetString())
      .Append(kElevationServiceExe);
}

base::FilePath GetTracingServicePath(const base::FilePath& target_path,
                                     const base::Version& version) {
  return target_path.AppendASCII(version.GetString())
      .Append(kElevatedTracingServiceExe);
}

void AddUpdateDowngradeVersionItem(HKEY root,
                                   const base::Version& current_version,
                                   const base::Version& new_version,
                                   WorkItemList* list) {
  DCHECK(list);
  DCHECK(new_version.IsValid());
  const auto downgrade_version = InstallUtil::GetDowngradeVersion();
  const std::wstring client_state_key = install_static::GetClientStateKeyPath();
  if (current_version.IsValid() && new_version < current_version) {
    // This is a downgrade. Write the value if this is the first one (i.e., no
    // previous value exists). Otherwise, leave any existing value in place.
    if (!downgrade_version) {
      list->AddSetRegValueWorkItem(
          root, client_state_key, KEY_WOW64_32KEY, kRegDowngradeVersion,
          base::ASCIIToWide(current_version.GetString()), true);
    }
  } else if (!current_version.IsValid() || new_version >= downgrade_version) {
    // This is a new install or an upgrade to/past a previous DowngradeVersion.
    list->AddDeleteRegValueWorkItem(root, client_state_key, KEY_WOW64_32KEY,
                                    kRegDowngradeVersion);
  }
}

}  // namespace installer

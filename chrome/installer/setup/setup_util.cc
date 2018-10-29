// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include <windows.h>

#include <objbase.h>
#include <stddef.h>
#include <wtsapi32.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/user_hive_visitor.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "components/zucchini/zucchini.h"
#include "components/zucchini/zucchini_integration.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace installer {

namespace {

// Event log providers registry location.
constexpr wchar_t kEventLogProvidersRegPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

// Remove the registration of the browser's DelegateExecute verb handler class.
// This was once registered in support of "metro" mode on Windows 8.
void RemoveLegacyIExecuteCommandKey(const InstallerState& installer_state) {
  const base::string16 handler_class_uuid =
      install_static::GetLegacyCommandExecuteImplClsid();

  // No work to do if this mode of install never registered a DelegateExecute
  // verb handler.
  if (handler_class_uuid.empty())
    return;

  const HKEY root = installer_state.root_key();
  base::string16 delegate_execute_path(L"Software\\Classes\\CLSID\\");
  delegate_execute_path.append(handler_class_uuid);

  // Delete both 64 and 32 keys to handle 32->64 or 64->32 migration.
  for (REGSAM bitness : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    if (base::win::RegKey(root, delegate_execute_path.c_str(),
                          KEY_QUERY_VALUE | bitness)
            .Valid()) {
      const bool success =
          InstallUtil::DeleteRegistryKey(root, delegate_execute_path, bitness);
      UMA_HISTOGRAM_BOOLEAN("Setup.Install.DeleteIExecuteCommandClassKey",
                            success);
    }
  }
}

// "The binaries" once referred to the on-disk footprint of Chrome and/or Chrome
// Frame when the products were configured to share such on-disk bits. Support
// for this mode of install was dropped from ToT in December 2016. Remove any
// stray bits in the registry leftover from such installs.
void RemoveBinariesVersionKey(const InstallerState& installer_state) {
  base::string16 path(install_static::GetBinariesClientsKeyPath());
  if (base::win::RegKey(installer_state.root_key(), path.c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY)
          .Valid()) {
    const bool success = InstallUtil::DeleteRegistryKey(
        installer_state.root_key(), path, KEY_WOW64_32KEY);
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.DeleteBinariesClientsKey", success);
  }
}

// Remove leftover traces of multi-install Chrome Frame, if present. Once upon a
// time, Google Chrome Frame could be co-installed with Chrome such that they
// shared the same binaries on disk. Support for new installs of GCF was dropped
// from ToT in December 2013. Remove any stray bits in the registry leftover
// from an old multi-install GCF.
void RemoveMultiChromeFrame(const InstallerState& installer_state) {
// There never was a "Chromium Frame".
#if defined(GOOGLE_CHROME_BUILD)
  // To maximize cleanup, unconditionally delete GCF's Clients and ClientState
  // keys unless single-install GCF is present. This condition is satisfied if
  // both keys exist, Clients\pv contains a value, and
  // ClientState\UninstallString contains a path including "\Chrome Frame\".
  // Multi-install GCF would have had "\Chrome\", and anything else is garbage.

  static constexpr wchar_t kGcfGuid[] =
      L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
  base::string16 clients_key_path = install_static::GetClientsKeyPath(kGcfGuid);
  base::win::RegKey clients_key;
  base::string16 client_state_key_path =
      install_static::GetClientStateKeyPath(kGcfGuid);
  base::win::RegKey client_state_key;

  const bool has_clients_key =
      clients_key.Open(installer_state.root_key(), clients_key_path.c_str(),
                       KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS;
  const bool has_client_state_key =
      client_state_key.Open(installer_state.root_key(),
                            client_state_key_path.c_str(),
                            KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS;
  if (!has_clients_key && !has_client_state_key)
    return;  // Nothing to check or to clean.

  base::string16 value;
  if (has_clients_key && has_client_state_key &&
      clients_key.ReadValue(google_update::kRegVersionField, &value) ==
          ERROR_SUCCESS &&
      !value.empty() &&
      client_state_key.ReadValue(kUninstallStringField, &value) ==
          ERROR_SUCCESS &&
      value.find(L"\\Chrome Frame\\") != base::string16::npos) {
    return;  // Single-install Chrome Frame found.
  }
  client_state_key.Close();
  clients_key.Close();

  // Remnants of multi-install GCF or of a malformed GCF are present. Remove the
  // Clients and ClientState keys so that Google Update ceases to check for
  // updates, and the Programs and Features control panel entry to reduce user
  // confusion.
  constexpr int kOperations = 3;
  int success_count = 0;

  if (InstallUtil::DeleteRegistryKey(installer_state.root_key(),
                                     clients_key_path, KEY_WOW64_32KEY)) {
    ++success_count;
  }
  if (InstallUtil::DeleteRegistryKey(installer_state.root_key(),
                                     client_state_key_path, KEY_WOW64_32KEY)) {
    ++success_count;
  }
  if (InstallUtil::DeleteRegistryKey(
          installer_state.root_key(),
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
          L"Google Chrome Frame",
          KEY_WOW64_32KEY)) {
    ++success_count;
  }
  DCHECK_LE(success_count, kOperations);

  // Used for a histogram; do not reorder.
  enum MultiChromeFrameRemovalResult {
    ALL_FAILED = 0,
    PARTIAL_SUCCESS = 1,
    SUCCESS = 2,
    NUM_RESULTS
  };
  MultiChromeFrameRemovalResult result =
      (success_count == kOperations ? SUCCESS : (success_count ? PARTIAL_SUCCESS
                                                               : ALL_FAILED));
  UMA_HISTOGRAM_ENUMERATION("Setup.Install.MultiChromeFrameRemoved", result,
                            NUM_RESULTS);
#endif  // GOOGLE_CHROME_BUILD
}

void RemoveAppLauncherVersionKey(const InstallerState& installer_state) {
// The app launcher was only registered for Google Chrome.
#if defined(GOOGLE_CHROME_BUILD)
  static constexpr wchar_t kLauncherGuid[] =
      L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";

  base::string16 path = install_static::GetClientsKeyPath(kLauncherGuid);
  if (base::win::RegKey(installer_state.root_key(), path.c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY)
          .Valid()) {
    const bool succeeded = InstallUtil::DeleteRegistryKey(
        installer_state.root_key(), path, KEY_WOW64_32KEY);
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.DeleteAppLauncherClientsKey",
                          succeeded);
  }
#endif  // GOOGLE_CHROME_BUILD
}

void RemoveAppHostExe(const InstallerState& installer_state) {
// The app host was only installed for Google Chrome.
#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath app_host(
      installer_state.target_path().Append(FILE_PATH_LITERAL("app_host.exe")));

  if (base::PathExists(app_host)) {
    const bool succeeded = base::DeleteFile(app_host, false);
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.DeleteAppHost", succeeded);
  }
#endif  // GOOGLE_CHROME_BUILD
}

void RemoveLegacyChromeAppCommands(const InstallerState& installer_state) {
// These app commands were only registered for Google Chrome.
#if defined(GOOGLE_CHROME_BUILD)
  base::string16 path(GetCommandKey(L"install-extension"));

  if (base::win::RegKey(installer_state.root_key(), path.c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY)
          .Valid()) {
    const bool succeeded = InstallUtil::DeleteRegistryKey(
        installer_state.root_key(), path, KEY_WOW64_32KEY);
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.DeleteInstallExtensionCommand",
                          succeeded);
  }
#endif  // GOOGLE_CHROME_BUILD
}

}  // namespace

const char kUnPackStatusMetricsName[] = "Setup.Install.LzmaUnPackStatus";
const char kUnPackNTSTATUSMetricsName[] = "Setup.Install.LzmaUnPackNTSTATUS";

int CourgettePatchFiles(const base::FilePath& src,
                        const base::FilePath& patch,
                        const base::FilePath& dest) {
  VLOG(1) << "Applying Courgette patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const courgette::Status patch_status =
      courgette::ApplyEnsemblePatch(src.value().c_str(),
                                    patch.value().c_str(),
                                    dest.value().c_str());
  const int exit_code = (patch_status != courgette::C_OK) ?
      static_cast<int>(patch_status) + kCourgetteErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply Courgette patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

int BsdiffPatchFiles(const base::FilePath& src,
                     const base::FilePath& patch,
                     const base::FilePath& dest) {
  VLOG(1) << "Applying bsdiff patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const int patch_status = bsdiff::ApplyBinaryPatch(src, patch, dest);
  const int exit_code = patch_status != bsdiff::OK ?
                        patch_status + kBsdiffErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply bsdiff patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

int ZucchiniPatchFiles(const base::FilePath& src,
                       const base::FilePath& patch,
                       const base::FilePath& dest) {
  VLOG(1) << "Applying Zucchini patch " << patch.value() << " to file "
          << src.value() << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const zucchini::status::Code patch_status = zucchini::Apply(src, patch, dest);
  const int exit_code =
      (patch_status != zucchini::status::kStatusSuccess)
          ? static_cast<int>(patch_status) + kZucchiniErrorOffset
          : 0;

  LOG_IF(ERROR, exit_code) << "Failed to apply Zucchini patch " << patch.value()
                           << " to file " << src.value()
                           << " and generating file " << dest.value()
                           << ". err=" << exit_code;

  return exit_code;
}

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
        new base::Version(base::UTF16ToASCII(find_data.GetName().value())));
    if (found_version->IsValid() &&
        found_version->CompareTo(*max_version.get()) > 0) {
      max_version = std::move(found_version);
      version_found = true;
    }
  }

  return (version_found ? max_version.release() : NULL);
}

base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state,
                                  const base::Version& desired_version) {
  if (desired_version.IsValid()) {
    base::FilePath archive(installer_state.GetInstallerDirectory(
        desired_version).Append(kChromeArchive));
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
    patch_source = installer_state.GetInstallerDirectory(*version)
        .Append(installer::kChromeArchive);
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
      ExpandEnvironmentStrings(kRunDll32Path, rundll32, arraysize(rundll32));
  if (!size || size >= MAX_PATH)
    return false;

  STARTUPINFO startup = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION pi = {0};
  BOOL ok = ::CreateProcess(NULL, rundll32, NULL, NULL, FALSE, CREATE_SUSPENDED,
                            NULL, NULL, &startup, &pi);
  if (ok) {
    // We use the main thread of the new process to run:
    //   Sleep(delay_before_delete_ms);
    //   DeleteFile(path);
    //   ExitProcess(0);
    // This runs before the main routine of the process runs, so it doesn't
    // matter much which executable we choose except that we don't want to
    // use e.g. a console app that causes a window to be created.
    size = static_cast<DWORD>(
        (path.value().length() + 1) * sizeof(path.value()[0]));
    void* mem = ::VirtualAllocEx(pi.hProcess, NULL, size, MEM_COMMIT,
                                 PAGE_READWRITE);
    if (mem) {
      SIZE_T written = 0;
      ::WriteProcessMemory(
          pi.hProcess, mem, path.value().c_str(),
          (path.value().size() + 1) * sizeof(path.value()[0]), &written);
      HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
      PAPCFUNC sleep = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "Sleep"));
      PAPCFUNC delete_file = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "DeleteFileW"));
      PAPCFUNC exit_process = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "ExitProcess"));
      if (!sleep || !delete_file || !exit_process) {
        NOTREACHED();
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

bool AdjustProcessPriority() {
  DWORD priority_class = ::GetPriorityClass(::GetCurrentProcess());
  if (priority_class == BELOW_NORMAL_PRIORITY_CLASS ||
      priority_class == IDLE_PRIORITY_CLASS) {
    BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                     PROCESS_MODE_BACKGROUND_BEGIN);
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
  for (size_t i = 0; i < arraysize(kLegacySwitches); ++i) {
    if (cmd_line.HasSwitch(kLegacySwitches[i]))
      return true;
  }
  return false;
}

bool IsProcessorSupported() {
  return base::CPU().has_sse2();
}

base::string16 GetCommandKey(const wchar_t* name) {
  base::string16 cmd_key = install_static::GetClientsKeyPath();
  cmd_key.append(1, base::FilePath::kSeparators[0])
      .append(google_update::kRegCommandsKey)
      .append(1, base::FilePath::kSeparators[0])
      .append(name);
  return cmd_key;
}

void DeleteRegistryKeyPartial(
    HKEY root,
    const base::string16& path,
    const std::vector<base::string16>& keys_to_preserve) {
  // Downcase the list of keys to preserve (all must be ASCII strings).
  std::set<base::string16> lowered_keys_to_preserve;
  std::transform(
      keys_to_preserve.begin(), keys_to_preserve.end(),
      std::inserter(lowered_keys_to_preserve, lowered_keys_to_preserve.begin()),
      [](const base::string16& str) {
        DCHECK(!str.empty());
        DCHECK(base::IsStringASCII(str));
        return base::ToLowerASCII(str);
      });
  base::win::RegKey key;
  LONG result = key.Open(root, path.c_str(), (KEY_ENUMERATE_SUB_KEYS |
                                              KEY_QUERY_VALUE | KEY_SET_VALUE));
  if (result != ERROR_SUCCESS) {
    LOG_IF(ERROR, result != ERROR_FILE_NOT_FOUND) << "Failed to open " << path
                                                  << "; result = " << result;
    return;
  }

  // Repeatedly iterate over all subkeys deleting those that should not be
  // preserved until only those remain. Multiple passes are needed since
  // deleting one key may change the enumeration order of all remaining keys.

  // Subkeys or values to be skipped on subsequent passes.
  std::set<base::string16> to_skip;
  DWORD index = 0;
  const size_t kMaxKeyNameLength = 256;  // MSDN says 255; +1 for terminator.
  base::string16 name(kMaxKeyNameLength, base::char16());
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
    result = key.DeleteEmptyKey(L"");
    LOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed to delete empty key "
                                           << path << "; result: " << result;
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

bool IsDowngradeAllowed(const MasterPreferences& prefs) {
  bool allow_downgrade = false;
  return prefs.GetBool(master_preferences::kAllowDowngrade, &allow_downgrade) &&
         allow_downgrade;
}

int GetInstallAge(const InstallerState& installer_state) {
  base::File::Info info;
  if (!base::GetFileInfo(installer_state.target_path(), &info))
    return -1;
  base::TimeDelta age = base::Time::Now() - info.creation_time;
  return age >= base::TimeDelta() ? age.InDays() : -1;
}

void RecordUnPackMetrics(UnPackStatus unpack_status,
                         int32_t status,
                         UnPackConsumer consumer) {
  std::string consumer_name = "";

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

  base::LinearHistogram::FactoryGet(
      std::string(kUnPackStatusMetricsName) + "_" + consumer_name, 1,
      UNPACK_STATUS_COUNT, UNPACK_STATUS_COUNT + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(unpack_status);

  base::SparseHistogram::FactoryGet(
      std::string(kUnPackNTSTATUSMetricsName) + "_" + consumer_name,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(status);
}

void RegisterEventLogProvider(const base::FilePath& install_directory,
                              const base::Version& version) {
  base::string16 reg_path(kEventLogProvidersRegPath);
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
      L"CategoryMessageFile", L"EventMessageFile", L"ParameterMessageFile",
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
  base::string16 reg_path(kEventLogProvidersRegPath);
  reg_path.append(install_static::InstallDetails::Get().install_full_name());

  // TODO(http://crbug.com/668120): If the Event Viewer is open the provider dll
  // will fail to get deleted. This doesn't fail the uninstallation altogether
  // but leaves files behind.
  InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
                                 WorkItem::kWow64Default);
}

bool AreBinariesInstalled(const InstallerState& installer_state) {
  if (!install_static::InstallDetails::Get().supported_multi_install())
    return false;

  base::win::RegKey key;
  base::string16 pv;

  // True if the "pv" value exists and isn't empty.
  return key.Open(installer_state.root_key(),
                  install_static::GetBinariesClientsKeyPath().c_str(),
                  KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
         key.ReadValue(google_update::kRegVersionField, &pv) == ERROR_SUCCESS &&
         !pv.empty();
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
  RemoveMultiChromeFrame(installer_state);
  RemoveAppLauncherVersionKey(installer_state);
  RemoveAppHostExe(installer_state);
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
  base::ScopedClosureRunner wts_deleter(
      base::Bind(&::WTSFreeMemory, base::Unretained(buffer)));

  WTSINFO* wts_info = nullptr;
  if (buffer_size < sizeof(*wts_info))
    return base::Time();

  wts_info = reinterpret_cast<WTSINFO*>(buffer);
  FILETIME filetime = {wts_info->LogonTime.u.LowPart,
                       wts_info->LogonTime.u.HighPart};
  return base::Time::FromFileTime(filetime);
}

base::Optional<std::string> DecodeDMTokenSwitchValue(
    const base::string16& encoded_token) {
  if (encoded_token.empty()) {
    LOG(ERROR) << "Empty DMToken specified on the command line";
    return base::nullopt;
  }

  // The token passed on the command line is base64-encoded, but since this is
  // on Windows, it is passed in as a wide string containing base64 values only.
  std::string token;
  if (!base::IsStringASCII(encoded_token) ||
      !base::Base64Decode(base::UTF16ToASCII(encoded_token), &token)) {
    LOG(ERROR) << "DMToken passed on the command line is not correctly encoded";
    return base::nullopt;
  }

  return token;
}

bool StoreDMToken(const std::string& token) {
  DCHECK(install_static::IsSystemInstall());

  if (token.size() > kMaxDMTokenLength) {
    LOG(ERROR) << "DMToken length out of bounds";
    return false;
  }

  std::wstring path;
  std::wstring name;
  InstallUtil::GetMachineLevelUserCloudPolicyDMTokenRegistryPath(&path,
                                                                 &name);

  base::win::RegKey key;
  LONG result = key.Create(HKEY_LOCAL_MACHINE, path.c_str(),
                           KEY_WRITE | KEY_WOW64_64KEY);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to create/open registry key HKLM\\" << path
               << " for writing result=" << result;
    return false;
  }

  result =
      key.WriteValue(name.c_str(), token.data(),
                     base::saturated_cast<DWORD>(token.size()), REG_BINARY);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to write specified DMToken to the registry at HKLM\\"
               << path << "\\" << name << " result=" << result;
    return false;
  }

  VLOG(1) << "Successfully stored specified DMToken in the registry.";

  return true;
}

base::FilePath GetNotificationHelperPath(const base::FilePath& target_path,
                                         const base::Version& version) {
  return target_path.AppendASCII(version.GetString())
      .Append(kNotificationHelperExe);
}

base::FilePath GetElevationServicePath(const base::FilePath& target_path,
                                       const base::Version& version) {
  return target_path.AppendASCII(version.GetString())
      .Append(kElevationServiceExe);
}

base::string16 GetElevationServiceGuid(base::StringPiece16 prefix) {
  base::string16 result =
      InstallUtil::String16FromGUID(install_static::GetElevatorClsid());
  result.insert(0, prefix.data(), prefix.size());
  return result;
}

base::string16 GetElevationServiceClsidRegistryPath() {
  return GetElevationServiceGuid(L"Software\\Classes\\CLSID\\");
}

base::string16 GetElevationServiceAppidRegistryPath() {
  return GetElevationServiceGuid(L"Software\\Classes\\AppID\\");
}

}  // namespace installer

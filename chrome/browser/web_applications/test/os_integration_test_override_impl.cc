// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"

#include <codecvt>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_MAC)
#include <ImageIO/ImageIO.h>

#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#include "net/base/filename_util.h"
#import "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/shortcut.h"
#include "base/win/windows_types.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/icon_util.h"
#endif

namespace web_app {

namespace {

std::string GetAllFilesInDir(const base::FilePath& file_path) {
  std::vector<std::string> files_as_strs;
  base::FileEnumerator files(file_path, true, base::FileEnumerator::FILES);
  for (base::FilePath current = files.Next(); !current.empty();
       current = files.Next()) {
    files_as_strs.push_back(current.AsUTF8Unsafe());
  }
  return base::JoinString(base::make_span(files_as_strs), "\n  ");
}

#if BUILDFLAG(IS_WIN)
base::FilePath GetShortcutProfile(base::FilePath shortcut_path) {
  base::FilePath shortcut_profile;
  std::wstring cmd_line_string;
  if (base::win::ResolveShortcut(shortcut_path, nullptr, &cmd_line_string)) {
    base::CommandLine shortcut_cmd_line =
        base::CommandLine::FromString(L"program " + cmd_line_string);
    shortcut_profile =
        shortcut_cmd_line.GetSwitchValuePath(switches::kProfileDirectory);
  }
  return shortcut_profile;
}

std::vector<std::wstring> GetFileExtensionsForProgId(
    const std::wstring& file_handler_prog_id) {
  const std::wstring prog_id_path =
      base::StrCat({ShellUtil::kRegClasses, L"\\", file_handler_prog_id});

  // Get list of handled file extensions from value FileExtensions at
  // HKEY_CURRENT_USER\Software\Classes\<file_handler_prog_id>.
  base::win::RegKey file_extensions_key(HKEY_CURRENT_USER, prog_id_path.c_str(),
                                        KEY_QUERY_VALUE);
  std::wstring handled_file_extensions;
  LONG result = file_extensions_key.ReadValue(L"FileExtensions",
                                              &handled_file_extensions);
  CHECK_EQ(result, ERROR_SUCCESS);

  return base::SplitString(handled_file_extensions, std::wstring(L";"),
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Performs a blocking read of app icons from the disk.
SkColor IconManagerReadIconTopLeftColorForSize(WebAppIconManager& icon_manager,
                                               const AppId& app_id,
                                               SquareSizePx size_px) {
  SkColor result = SK_ColorTRANSPARENT;
  if (!icon_manager.HasIcons(app_id, IconPurpose::ANY, {size_px})) {
    return result;
  }
  base::RunLoop run_loop;
  icon_manager.ReadIcons(
      app_id, IconPurpose::ANY, {size_px},
      base::BindOnce(
          [](base::RunLoop* run_loop, SkColor* result, SquareSizePx size_px,
             std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            CHECK(base::Contains(icon_bitmaps, size_px));
            *result = icon_bitmaps.at(size_px).getColor(0, 0);
            run_loop->Quit();
          },
          &run_loop, &result, size_px));
  run_loop.Run();
  return result;
}
#endif

}  // namespace

OsIntegrationTestOverrideImpl::BlockingRegistration::BlockingRegistration() =
    default;
OsIntegrationTestOverrideImpl::BlockingRegistration::~BlockingRegistration() {
  base::ScopedAllowBlockingForTesting blocking;
  base::RunLoop wait_until_destruction_loop;
  // Lock the destrunction closure
  {
    base::AutoLock lock(test_override->destruction_closure_lock);
    CHECK(!test_override->on_destruction_)
        << "Cannot have multiple registrations at the same time.";
    // Set the destruction closure for the scoped override object.
    test_override->on_destruction_.ReplaceClosure(
        wait_until_destruction_loop.QuitClosure());

    // Unregister the override so new handles cannot be acquired.
    OsIntegrationTestOverride::SetForTesting(nullptr);
  }

  // Release the override & wait until all references are released.
  // Note: The `test_override` MUST be released before waiting on the run
  // loop, as then it will hang forever.
  test_override.reset();
  wait_until_destruction_loop.Run();
}

// static
scoped_refptr<OsIntegrationTestOverrideImpl>
OsIntegrationTestOverrideImpl::Get() {
  CHECK_IS_TEST();
  scoped_refptr<OsIntegrationTestOverride> current_override =
      OsIntegrationTestOverride::Get();
  CHECK(current_override);
  scoped_refptr<OsIntegrationTestOverrideImpl> test_override =
      base::WrapRefCounted<OsIntegrationTestOverrideImpl>(
          current_override->AsOsIntegrationTestOverrideImpl());
  CHECK(test_override);
  return test_override;
}

// static
std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
OsIntegrationTestOverrideImpl::OverrideForTesting(
    const base::FilePath& base_path) {
  auto test_override =
      base::WrapRefCounted(new OsIntegrationTestOverrideImpl(base_path));
  OsIntegrationTestOverride::SetForTesting(test_override);
  std::unique_ptr<BlockingRegistration> registration =
      std::make_unique<BlockingRegistration>();
  registration->test_override = std::move(test_override);
  return registration;
}

bool OsIntegrationTestOverrideImpl::SimulateDeleteShortcutsByUser(
    Profile* profile,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_WIN)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  CHECK(base::PathExists(desktop_shortcut_path));
  base::FilePath app_menu_shortcut_path =
      GetShortcutPath(profile, application_menu(), app_id, app_name);
  CHECK(base::PathExists(app_menu_shortcut_path));
  return base::DeleteFile(desktop_shortcut_path) &&
         base::DeleteFile(app_menu_shortcut_path);
#elif BUILDFLAG(IS_MAC)
  base::FilePath app_folder_shortcut_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  CHECK(base::PathExists(app_folder_shortcut_path));
  return base::DeletePathRecursively(app_folder_shortcut_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  LOG(INFO) << desktop_shortcut_path;
  CHECK(base::PathExists(desktop_shortcut_path));
  return base::DeleteFile(desktop_shortcut_path);
#else
  NOTREACHED() << "Not implemented on ChromeOS/Fuchsia ";
  return true;
#endif
}

#if BUILDFLAG(IS_MAC)
bool OsIntegrationTestOverrideImpl::DeleteChromeAppsDir() {
  if (chrome_apps_folder_.IsValid()) {
    return chrome_apps_folder_.Delete();
  } else {
    return false;
  }
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
bool OsIntegrationTestOverrideImpl::DeleteDesktopDirOnWin() {
  if (desktop_.IsValid()) {
    return desktop_.Delete();
  } else {
    return false;
  }
}

bool OsIntegrationTestOverrideImpl::DeleteApplicationMenuDirOnWin() {
  if (application_menu_.IsValid()) {
    return application_menu_.Delete();
  } else {
    return false;
  }
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
bool OsIntegrationTestOverrideImpl::DeleteDesktopDirOnLinux() {
  if (desktop_.IsValid()) {
    return desktop_.Delete();
  } else {
    return false;
  }
}
#endif  // BUILDFLAG(IS_LINUX)

bool OsIntegrationTestOverrideImpl::IsRunOnOsLoginEnabled(
    Profile* profile,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_LINUX)
  std::string shortcut_filename =
      "chrome-" + app_id + "-" + profile->GetBaseName().value() + ".desktop";
  return base::PathExists(startup().Append(shortcut_filename));
#elif BUILDFLAG(IS_WIN)
  base::FilePath startup_shortcut_path =
      GetShortcutPath(profile, startup(), app_id, app_name);
  return base::PathExists(startup_shortcut_path);
#elif BUILDFLAG(IS_MAC)
  std::string shortcut_filename = app_name + ".app";
  base::FilePath app_shortcut_path =
      chrome_apps_folder().Append(shortcut_filename);
  return startup_enabled_[app_shortcut_path];
#else
  NOTREACHED() << "Not implemented on ChromeOS/Fuchsia ";
  return true;
#endif
}

bool OsIntegrationTestOverrideImpl::IsFileExtensionHandled(
    Profile* profile,
    const AppId& app_id,
    std::string app_name,
    std::string file_extension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bool is_file_handled = false;
#if BUILDFLAG(IS_WIN)
  const std::wstring prog_id = GetProgIdForApp(profile->GetPath(), app_id);
  const std::vector<std::wstring> file_handler_prog_ids =
      ShellUtil::GetFileHandlerProgIdsForAppId(prog_id);
  const std::wstring extension = base::UTF8ToWide(file_extension);
  base::win::RegKey key;
  for (const auto& file_handler_prog_id : file_handler_prog_ids) {
    const std::vector<std::wstring> supported_file_extensions =
        GetFileExtensionsForProgId(file_handler_prog_id);
    if (base::Contains(supported_file_extensions, extension)) {
      const std::wstring reg_key = std::wstring(ShellUtil::kRegClasses) +
                                   base::FilePath::kSeparators[0] + extension +
                                   base::FilePath::kSeparators[0] +
                                   ShellUtil::kRegOpenWithProgids;
      LONG result = key.Open(HKEY_CURRENT_USER, reg_key.data(), KEY_READ);
      CHECK_EQ(ERROR_SUCCESS, result);
      return key.HasValue(file_handler_prog_id.data());
    }
  }
#elif BUILDFLAG(IS_MAC)
  const base::FilePath test_file_path =
      chrome_apps_folder().AppendASCII("test" + file_extension);
  const base::File test_file(
      test_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const GURL test_file_url = net::FilePathToFileURL(test_file_path);
  base::FilePath app_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  is_file_handled =
      shell_integration::CanApplicationHandleURL(app_path, test_file_url);
  base::DeleteFile(test_file_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath user_applications_dir =
      applications_dir().Append("applications");
  bool database_update_called = false;
  for (const LinuxFileRegistration& command : linux_file_registration_) {
    if (base::Contains(command.xdg_command, app_id) &&
        base::Contains(command.xdg_command,
                       profile->GetPath().BaseName().value())) {
      if (base::StartsWith(command.xdg_command, "xdg-mime install")) {
        is_file_handled = base::Contains(command.file_contents,
                                         "\"*" + file_extension + "\"");
      } else {
        CHECK(base::StartsWith(command.xdg_command, "xdg-mime uninstall"))
            << command.xdg_command;
        is_file_handled = false;
      }
    }

    // Verify if the mimeinfo.cache is also updated. See
    // web_app_file_handler_registration_linux.cc for more information.
    if (base::StartsWith(command.xdg_command, "update-desktop-database")) {
      database_update_called =
          base::Contains(command.xdg_command, user_applications_dir.value());
    }
  }
  is_file_handled = is_file_handled && database_update_called;
#endif
  return is_file_handled;
}

absl::optional<SkColor>
OsIntegrationTestOverrideImpl::GetShortcutIconTopLeftColor(
    Profile* profile,
    base::FilePath shortcut_dir,
    const AppId& app_id,
    const std::string& app_name,
    SquareSizePx size_px) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::FilePath shortcut_path =
      GetShortcutPath(profile, shortcut_dir, app_id, app_name);
  if (!base::PathExists(shortcut_path)) {
    return absl::nullopt;
  }
  return GetIconTopLeftColorFromShortcutFile(shortcut_path);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return absl::nullopt;
  }
  return IconManagerReadIconTopLeftColorForSize(provider->icon_manager(),
                                                app_id, size_px);
#else
  NOTREACHED() << "Not implemented on Fuchsia";
  return absl::nullopt;
#endif
}

base::FilePath OsIntegrationTestOverrideImpl::GetShortcutPath(
    Profile* profile,
    base::FilePath shortcut_dir,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_WIN)
  base::FileEnumerator enumerator(shortcut_dir, false,
                                  base::FileEnumerator::FILES);
  while (!enumerator.Next().empty()) {
    const std::wstring shortcut_filename =
        enumerator.GetInfo().GetName().value();
    const std::string narrowed_filename =
        base::WideToUTF8(enumerator.GetInfo().GetName().value());
    if (re2::RE2::FullMatch(narrowed_filename, app_name + "(.*).lnk")) {
      base::FilePath shortcut_path = shortcut_dir.Append(shortcut_filename);
      if (GetShortcutProfile(shortcut_path) == profile->GetBaseName()) {
        return shortcut_path;
      }
    }
  }
#elif BUILDFLAG(IS_MAC)
  std::string shortcut_filename = app_name + ".app";
  base::FilePath shortcut_path = shortcut_dir.Append(shortcut_filename);
  // Exits early if the app id is empty because the verification won't work.
  // TODO(crbug.com/1289865): Figure a way to find the profile that has the app
  //                          installed without using app ID.
  if (app_id.empty()) {
    return shortcut_path;
  }

  AppShimRegistry* registry = AppShimRegistry::Get();
  std::set<base::FilePath> app_installed_profiles =
      registry->GetInstalledProfilesForApp(app_id);
  if (app_installed_profiles.find(profile->GetPath()) !=
      app_installed_profiles.end()) {
    return shortcut_path;
  }
#elif BUILDFLAG(IS_LINUX)
  std::string shortcut_filename =
      "chrome-" + app_id + "-" + profile->GetBaseName().value() + ".desktop";
  base::FilePath shortcut_path = shortcut_dir.Append(shortcut_filename);
  if (base::PathExists(shortcut_path)) {
    return shortcut_path;
  }
#endif
  return base::FilePath();
}

bool OsIntegrationTestOverrideImpl::IsShortcutCreated(
    Profile* profile,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_WIN)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  base::FilePath application_menu_shortcut_path =
      GetShortcutPath(profile, application_menu(), app_id, app_name);
  return (base::PathExists(desktop_shortcut_path) &&
          base::PathExists(application_menu_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  base::FilePath app_shortcut_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  return base::PathExists(app_shortcut_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  return base::PathExists(desktop_shortcut_path);
#else
  NOTREACHED() << "Not implemented on ChromeOS/Fuchsia ";
  return true;
#endif
}

bool OsIntegrationTestOverrideImpl::AreShortcutsMenuRegistered() {
  return !shortcut_menu_apps_registered_.empty();
}

#if BUILDFLAG(IS_WIN)

std::vector<SkColor>
OsIntegrationTestOverrideImpl::GetIconColorsForShortcutsMenu(
    const std::wstring& app_user_model_id) {
  CHECK(IsShortcutsMenuRegisteredForApp(app_user_model_id));
  std::vector<SkColor> icon_colors;
  for (auto& shell_link_item : jump_list_entry_map_[app_user_model_id]) {
    icon_colors.emplace_back(
        ReadColorFromShortcutMenuIcoFile(shell_link_item->icon_path()));
  }
  return icon_colors;
}

int OsIntegrationTestOverrideImpl::GetCountOfShortcutIconsCreated(
    const std::wstring& app_user_model_id) {
  CHECK(IsShortcutsMenuRegisteredForApp(app_user_model_id));
  return jump_list_entry_map_[app_user_model_id].size();
}

bool OsIntegrationTestOverrideImpl::IsShortcutsMenuRegisteredForApp(
    const std::wstring& app_user_model_id) {
  return base::Contains(jump_list_entry_map_, app_user_model_id);
}

base::expected<bool, std::string>
OsIntegrationTestOverrideImpl::IsUninstallRegisteredWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile) {
  constexpr wchar_t kUninstallRegistryKey[] =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

  base::win::RegKey uninstall_reg_key;
  LONG result = uninstall_reg_key.Open(HKEY_CURRENT_USER, kUninstallRegistryKey,
                                       KEY_READ);

  if (result == ERROR_FILE_NOT_FOUND) {
    return base::unexpected(
        "Cannot find the uninstall registry key. If a testing hive is being "
        "used, then this key needs to be created there on initialization.");
  }

  if (result != ERROR_SUCCESS) {
    return base::unexpected(
        base::StringPrintf("Cannot open the registry key: %ld", result));
  }

  const std::wstring key =
      GetUninstallStringKeyForTesting(profile->GetPath(), app_id);

  base::win::RegKey uninstall_reg_entry_key;
  result = uninstall_reg_entry_key.Open(uninstall_reg_key.Handle(), key.c_str(),
                                        KEY_READ);
  if (result == ERROR_FILE_NOT_FOUND) {
    return base::ok(false);
  }

  if (result != ERROR_SUCCESS) {
    return base::unexpected(
        base::StringPrintf("Error opening uninstall key for app: %ld", result));
  }

  std::wstring display_icon_path;
  std::wstring display_name;
  std::wstring display_version;
  std::wstring application_version;
  std::wstring publisher;
  std::wstring uninstall_string;
  DWORD no_repair;
  DWORD no_modify;
  bool read_success = true;
  read_success &= uninstall_reg_entry_key.ReadValue(
                      L"DisplayIcon", &display_icon_path) == ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValue(
                      L"DisplayName", &display_name) == ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValue(
                      L"DisplayVersion", &display_version) == ERROR_SUCCESS;
  read_success &=
      uninstall_reg_entry_key.ReadValue(L"ApplicationVersion",
                                        &application_version) == ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValue(L"Publisher", &publisher) ==
                  ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValue(
                      L"UninstallString", &uninstall_string) == ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValueDW(
                      L"NoRepair", &no_repair) == ERROR_SUCCESS;
  read_success &= uninstall_reg_entry_key.ReadValueDW(
                      L"NoModify", &no_modify) == ERROR_SUCCESS;
  if (!read_success) {
    return base::unexpected("Error reading registry values");
  }

  if (display_version != L"1.0" || application_version != L"1.0" ||
      no_repair != 1 || no_modify != 1 ||
      publisher != install_static::GetChromeInstallSubDirectory()) {
    return base::unexpected("Incorrect static registry data.");
  }

  base::FilePath web_app_icon_dir = GetOsIntegrationResourcesDirectoryForApp(
      profile->GetPath(), app_id, GURL());
  base::FilePath expected_icon_path =
      internals::GetIconFilePath(web_app_icon_dir, base::UTF8ToUTF16(app_name));
  if (expected_icon_path.value() != display_icon_path) {
    return base::unexpected(base::StrCat(
        {"Invalid icon path ", base::WideToUTF8(display_icon_path),
         ", expected ", base::WideToUTF8(expected_icon_path.value())}));
  }
  if (base::UTF8ToWide(app_name) != display_name) {
    return base::unexpected(
        base::StrCat({"Invalid display name ", base::WideToUTF8(display_name),
                      ", expected ", app_name}));
  }
  std::wstring expected_uninstall_substr =
      base::StrCat({L"--uninstall-app-id=", base::UTF8ToWide(app_id)});
  if (!base::Contains(uninstall_string, expected_uninstall_substr)) {
    return base::unexpected(base::StrCat({"Could not find uninstall flag: ",
                                          base::WideToUTF8(uninstall_string)}));
  }

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

const OsIntegrationTestOverrideImpl::AppProtocolList&
OsIntegrationTestOverrideImpl::protocol_scheme_registrations() {
  return protocol_scheme_registrations_;
}

OsIntegrationTestOverrideImpl*
OsIntegrationTestOverrideImpl::AsOsIntegrationTestOverrideImpl() {
  return this;
}

#if BUILDFLAG(IS_WIN)
void OsIntegrationTestOverrideImpl::AddShortcutsMenuJumpListEntryForApp(
    const std::wstring& app_user_model_id,
    const std::vector<scoped_refptr<ShellLinkItem>>& shell_link_items) {
  jump_list_entry_map_[app_user_model_id] = shell_link_items;
  shortcut_menu_apps_registered_.emplace(app_user_model_id);
}

void OsIntegrationTestOverrideImpl::DeleteShortcutsMenuJumpListEntryForApp(
    const std::wstring& app_user_model_id) {
  jump_list_entry_map_.erase(app_user_model_id);
  shortcut_menu_apps_registered_.erase(app_user_model_id);
}
const base::FilePath& OsIntegrationTestOverrideImpl::desktop() {
  return desktop_.GetPath();
}
const base::FilePath& OsIntegrationTestOverrideImpl::application_menu() {
  return application_menu_.GetPath();
}
const base::FilePath& OsIntegrationTestOverrideImpl::quick_launch() {
  return quick_launch_.GetPath();
}
const base::FilePath& OsIntegrationTestOverrideImpl::startup() {
  return startup_.GetPath();
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
bool OsIntegrationTestOverrideImpl::IsChromeAppsValid() {
  return chrome_apps_folder_.IsValid();
}
const base::FilePath& OsIntegrationTestOverrideImpl::chrome_apps_folder() {
  return chrome_apps_folder_.GetPath();
}
void OsIntegrationTestOverrideImpl::EnableOrDisablePathOnLogin(
    const base::FilePath& file_path,
    bool enable_on_login) {
  startup_enabled_[file_path] = enable_on_login;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
const base::FilePath& OsIntegrationTestOverrideImpl::desktop() {
  return desktop_.GetPath();
}
const base::FilePath& OsIntegrationTestOverrideImpl::startup() {
  return startup_.GetPath();
}
const base::FilePath& OsIntegrationTestOverrideImpl::applications_dir() {
  return applications_dir_.GetPath();
}
#endif  // BUILDFLAG(IS_LINUX)

void OsIntegrationTestOverrideImpl::RegisterProtocolSchemes(
    const AppId& app_id,
    std::vector<std::string> protocols) {
  protocol_scheme_registrations_.emplace_back(app_id, std::move(protocols));
}

OsIntegrationTestOverrideImpl::OsIntegrationTestOverrideImpl(
    const base::FilePath& base_path) {
  // Initialize all directories used. The success & the CHECK are separated to
  // ensure that these function calls occur on release builds.
  if (!base_path.empty()) {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
    success = application_menu_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
    success = quick_launch_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
    success = applications_dir_.CreateUniqueTempDirUnderPath(base_path);
    CHECK(success);
#endif
  } else {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDir();
    CHECK(success);
    success = application_menu_.CreateUniqueTempDir();
    CHECK(success);
    success = quick_launch_.CreateUniqueTempDir();
    CHECK(success);
    success = startup_.CreateUniqueTempDir();
    CHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDir();
    CHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDir();
    CHECK(success);
    success = startup_.CreateUniqueTempDir();
    CHECK(success);
    success = applications_dir_.CreateUniqueTempDir();
    CHECK(success);
#endif
  }

#if BUILDFLAG(IS_LINUX)
  auto callback = base::BindRepeating([](base::FilePath filename_in,
                                         std::string xdg_command,
                                         std::string file_contents) {
    auto test_override = OsIntegrationTestOverrideImpl::Get();
    CHECK(test_override);
    LinuxFileRegistration file_registration = LinuxFileRegistration();
    file_registration.file_name = filename_in;
    file_registration.xdg_command = xdg_command;
    file_registration.file_contents = file_contents;
    test_override->linux_file_registration_.push_back(file_registration);
    return true;
  });
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(std::move(callback));
#endif

#if BUILDFLAG(IS_WIN)
  registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
  base::win::RegKey key;
  // In a real registry, this key would exist, but since we're using
  // hive override, it's empty, so we create this key.
  const LONG result =
      key.Create(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                 KEY_SET_VALUE);
  CHECK_EQ(result, ERROR_SUCCESS);
#endif
}

OsIntegrationTestOverrideImpl::~OsIntegrationTestOverrideImpl() {
  std::vector<base::ScopedTempDir*> directories;
#if BUILDFLAG(IS_WIN)
  directories = {&desktop_, &application_menu_, &quick_launch_, &startup_};
#elif BUILDFLAG(IS_MAC)
  directories = {&chrome_apps_folder_};
  // Checks and cleans up possible hidden files in directories.
  std::vector<std::string> hidden_files{"Icon\r", ".localized"};
  for (base::ScopedTempDir* dir : directories) {
    if (dir->IsValid()) {
      for (auto& f : hidden_files) {
        base::FilePath path = dir->GetPath().Append(f);
        if (base::PathExists(path)) {
          base::DeletePathRecursively(path);
        }
      }
    }
  }
#elif BUILDFLAG(IS_LINUX)
  // Reset the file handling callback.
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(
      UpdateMimeInfoDatabaseOnLinuxCallback());
  directories = {&desktop_, &applications_dir_};
#endif
  for (base::ScopedTempDir* dir : directories) {
    if (!dir->IsValid()) {
      continue;
    }
    CHECK(base::IsDirectoryEmpty(dir->GetPath()))
        << "Directory not empty: " << dir->GetPath().AsUTF8Unsafe()
        << ". Please uninstall all webapps that have been installed while "
           "shortcuts were overriden. Contents:\n"
        << GetAllFilesInDir(dir->GetPath());
  }
  {
    base::AutoLock lock(destruction_closure_lock);
    on_destruction_.RunAndReset();
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
SkColor OsIntegrationTestOverrideImpl::GetIconTopLeftColorFromShortcutFile(
    const base::FilePath& shortcut_path) {
  CHECK(base::PathExists(shortcut_path));
#if BUILDFLAG(IS_MAC)
  base::FilePath icon_path =
      shortcut_path.AppendASCII("Contents/Resources/app.icns");
  base::ScopedCFTypeRef<CFDictionaryRef> empty_dict(
      CFDictionaryCreate(nullptr, nullptr, nullptr, 0, nullptr, nullptr));
  base::ScopedCFTypeRef<CFURLRef> url = base::mac::FilePathToCFURL(icon_path);
  base::ScopedCFTypeRef<CGImageSourceRef> source(
      CGImageSourceCreateWithURL(url, nullptr));
  if (!source) {
    return 0;
  }
  // Get the first icon in the .icns file (index 0)
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      CGImageSourceCreateImageAtIndex(source, 0, empty_dict));
  if (!cg_image) {
    return 0;
  }
  SkBitmap bitmap = skia::CGImageToSkBitmap(cg_image);
  if (bitmap.empty()) {
    return 0;
  }
  return bitmap.getColor(0, 0);
#elif BUILDFLAG(IS_WIN)
  SHFILEINFO file_info = {0};
  if (SHGetFileInfo(shortcut_path.value().c_str(), FILE_ATTRIBUTE_NORMAL,
                    &file_info, sizeof(file_info),
                    SHGFI_ICON | 0 | SHGFI_USEFILEATTRIBUTES)) {
    const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(file_info.hIcon);
    if (bitmap.empty()) {
      return 0;
    }
    return bitmap.getColor(0, 0);
  } else {
    return 0;
  }
#endif
}
#endif

#if BUILDFLAG(IS_WIN)
SkColor OsIntegrationTestOverrideImpl::ReadColorFromShortcutMenuIcoFile(
    const base::FilePath& file_path) {
  HICON icon = static_cast<HICON>(
      LoadImage(NULL, file_path.value().c_str(), IMAGE_ICON, 32, 32,
                LR_LOADTRANSPARENT | LR_LOADFROMFILE));
  base::win::ScopedHICON scoped_icon(icon);
  SkBitmap output_image =
      IconUtil::CreateSkBitmapFromHICON(scoped_icon.get(), gfx::Size(32, 32));
  SkColor color = output_image.getColor(output_image.dimensions().width() / 2,
                                        output_image.dimensions().height() / 2);
  return color;
}
#endif

}  // namespace web_app

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"

#include <codecvt>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/icon_util.h"
#endif

namespace web_app {

namespace {

struct OsIntegrationTestOverrideState {
  base::Lock lock;
  raw_ptr<OsIntegrationTestOverride> global_os_integration_test_override
      GUARDED_BY(lock) = nullptr;
};

OsIntegrationTestOverrideState&
GetMutableOsIntegrationTestOverrideStateForTesting() {
  static base::NoDestructor<OsIntegrationTestOverrideState>
      g_os_integration_test_override;
  return *g_os_integration_test_override.get();
}

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
  DCHECK_EQ(file_extensions_key.ReadValue(L"FileExtensions",
                                          &handled_file_extensions),
            ERROR_SUCCESS);
  return base::SplitString(handled_file_extensions, std::wstring(L";"),
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}
#endif

}  // namespace

OsIntegrationTestOverride::BlockingRegistration::BlockingRegistration() =
    default;
OsIntegrationTestOverride::BlockingRegistration::~BlockingRegistration() {
  base::ScopedAllowBlockingForTesting blocking;
  base::RunLoop wait_until_destruction_loop;
  // Lock the global state.
  {
    auto& global_state = GetMutableOsIntegrationTestOverrideStateForTesting();
    base::AutoLock state_lock(global_state.lock);
    DCHECK_EQ(global_state.global_os_integration_test_override,
              test_override.get());

    // Set the destruction closure for the scoped override object.
    DCHECK(!test_override->on_destruction_)
        << "Cannot have multiple registrations at the same time.";
    test_override->on_destruction_.ReplaceClosure(
        wait_until_destruction_loop.QuitClosure());

    // Unregister the override so new handles cannot be acquired.
    global_state.global_os_integration_test_override = nullptr;
  }

  // Release the override & wait until all references are released.
  // Note: The `test_override` MUST be released before waiting on the run
  // loop, as then it will hang forever.
  test_override.reset();
  wait_until_destruction_loop.Run();
}

// static
std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
OsIntegrationTestOverride::OverrideForTesting(const base::FilePath& base_path) {
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  DCHECK(!state.global_os_integration_test_override)
      << "Cannot have multiple registrations at the same time.";
  auto test_override =
      base::WrapRefCounted(new OsIntegrationTestOverride(base_path));
  state.global_os_integration_test_override = test_override.get();

  std::unique_ptr<BlockingRegistration> registration =
      std::make_unique<BlockingRegistration>();
  registration->test_override = test_override;
  return registration;
}

bool OsIntegrationTestOverride::IsRunOnOsLoginEnabled(
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

bool OsIntegrationTestOverride::IsFileExtensionHandled(
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

  base::win::RegKey key;
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  for (const auto& file_handler_prog_id : file_handler_prog_ids) {
    const std::vector<std::wstring> supported_file_extensions =
        GetFileExtensionsForProgId(file_handler_prog_id);
    std::wstring extension = converter.from_bytes(file_extension);
    if (base::Contains(supported_file_extensions, extension)) {
      const std::wstring reg_key = std::wstring(ShellUtil::kRegClasses) +
                                   base::FilePath::kSeparators[0] + extension +
                                   base::FilePath::kSeparators[0] +
                                   ShellUtil::kRegOpenWithProgids;
      DCHECK_EQ(ERROR_SUCCESS,
                key.Open(HKEY_CURRENT_USER, reg_key.data(), KEY_READ));
      return key.HasValue(file_handler_prog_id.data());
    }
  }
#elif BUILDFLAG(IS_MAC)
  base::ScopedTempDir temp_test_dir;
  DCHECK(temp_test_dir.CreateUniqueTempDirUnderPath(chrome_apps_folder()));
  const base::FilePath test_file_path =
      temp_test_dir.GetPath().AppendASCII("test" + file_extension);
  const base::File test_file(
      test_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const GURL test_file_url = net::FilePathToFileURL(test_file_path);
  base::FilePath app_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  is_file_handled =
      shell_integration::CanApplicationHandleURL(app_path, test_file_url);
#elif BUILDFLAG(IS_LINUX)
  for (const LinuxFileRegistration& command : linux_file_registration()) {
    if (base::Contains(command.xdg_command, app_id) &&
        base::Contains(command.xdg_command,
                       profile->GetPath().BaseName().value())) {
      if (base::StartsWith(command.xdg_command, "xdg-mime install")) {
        is_file_handled = base::Contains(command.file_contents,
                                         "\"*" + file_extension + "\"");
      } else {
        DCHECK(base::StartsWith(command.xdg_command, "xdg-mime uninstall"))
            << command.xdg_command;
        is_file_handled = false;
      }
    }
  }
#endif
  return is_file_handled;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
absl::optional<SkColor> OsIntegrationTestOverride::GetShortcutIconTopLeftColor(
    Profile* profile,
    base::FilePath shortcut_dir,
    const AppId& app_id,
    const std::string& app_name) {
  base::FilePath shortcut_path =
      GetShortcutPath(profile, shortcut_dir, app_id, app_name);
  if (!base::PathExists(shortcut_path)) {
    return absl::nullopt;
  }
  return GetIconTopLeftColorFromShortcutFile(shortcut_path);
}
#endif

#if BUILDFLAG(IS_WIN)
void OsIntegrationTestOverride::AddShortcutsMenuJumpListEntryForApp(
    const std::wstring& app_user_model_id,
    const std::vector<scoped_refptr<ShellLinkItem>>& shell_link_items) {
  jump_list_entry_map_[app_user_model_id] = shell_link_items;
  shortcut_menu_apps_registered_.emplace(app_user_model_id);
}

void OsIntegrationTestOverride::DeleteShortcutsMenuJumpListEntryForApp(
    const std::wstring& app_user_model_id) {
  jump_list_entry_map_.erase(app_user_model_id);
  shortcut_menu_apps_registered_.erase(app_user_model_id);
}

int OsIntegrationTestOverride::GetCountOfShortcutIconsCreated(
    const std::wstring& app_user_model_id) {
  DCHECK(IsShortcutsMenuRegisteredForApp(app_user_model_id));
  return jump_list_entry_map_[app_user_model_id].size();
}

std::vector<SkColor> OsIntegrationTestOverride::GetIconColorsForShortcutsMenu(
    const std::wstring& app_user_model_id) {
  DCHECK(IsShortcutsMenuRegisteredForApp(app_user_model_id));
  std::vector<SkColor> icon_colors;
  for (auto& shell_link_item : jump_list_entry_map_[app_user_model_id]) {
    icon_colors.emplace_back(
        ReadColorFromShortcutMenuIcoFile(shell_link_item->icon_path()));
  }
  return icon_colors;
}

bool OsIntegrationTestOverride::IsShortcutsMenuRegisteredForApp(
    const std::wstring& app_user_model_id) {
  return base::Contains(jump_list_entry_map_, app_user_model_id);
}
#endif

bool OsIntegrationTestOverride::AreShortcutsMenuRegistered() {
  return !shortcut_menu_apps_registered_.empty();
}

base::FilePath OsIntegrationTestOverride::GetShortcutPath(
    Profile* profile,
    base::FilePath shortcut_dir,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_WIN)
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  base::FileEnumerator enumerator(shortcut_dir, false,
                                  base::FileEnumerator::FILES);
  while (!enumerator.Next().empty()) {
    std::wstring shortcut_filename = enumerator.GetInfo().GetName().value();
    if (re2::RE2::FullMatch(converter.to_bytes(shortcut_filename),
                            app_name + "(.*).lnk")) {
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

bool OsIntegrationTestOverride::IsShortcutCreated(Profile* profile,
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

bool OsIntegrationTestOverride::SimulateDeleteShortcutsByUser(
    Profile* profile,
    const AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_WIN)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  DCHECK(base::PathExists(desktop_shortcut_path));
  base::FilePath app_menu_shortcut_path =
      GetShortcutPath(profile, application_menu(), app_id, app_name);
  DCHECK(base::PathExists(app_menu_shortcut_path));
  return base::DeleteFile(desktop_shortcut_path) &&
         base::DeleteFile(app_menu_shortcut_path);
#elif BUILDFLAG(IS_MAC)
  base::FilePath app_folder_shortcut_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  DCHECK(base::PathExists(app_folder_shortcut_path));
  return base::DeletePathRecursively(app_folder_shortcut_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  LOG(INFO) << desktop_shortcut_path;
  DCHECK(base::PathExists(desktop_shortcut_path));
  return base::DeleteFile(desktop_shortcut_path);
#else
  NOTREACHED() << "Not implemented on ChromeOS/Fuchsia ";
  return true;
#endif
}

bool OsIntegrationTestOverride::ForceDeleteAllShortcuts() {
#if BUILDFLAG(IS_WIN)
  return DeleteDesktopDirOnWin() && DeleteApplicationMenuDirOnWin();
#elif BUILDFLAG(IS_MAC)
  return DeleteChromeAppsDir();
#elif BUILDFLAG(IS_LINUX)
  return DeleteDesktopDirOnLinux();
#else
  NOTREACHED() << "Not implemented on ChromeOS/Fuchsia ";
  return true;
#endif
}

#if BUILDFLAG(IS_WIN)
bool OsIntegrationTestOverride::DeleteDesktopDirOnWin() {
  if (desktop_.IsValid()) {
    return desktop_.Delete();
  } else {
    return false;
  }
}

bool OsIntegrationTestOverride::DeleteApplicationMenuDirOnWin() {
  if (application_menu_.IsValid()) {
    return application_menu_.Delete();
  } else {
    return false;
  }
}

#elif BUILDFLAG(IS_MAC)
bool OsIntegrationTestOverride::DeleteChromeAppsDir() {
  if (chrome_apps_folder_.IsValid()) {
    return chrome_apps_folder_.Delete();
  } else {
    return false;
  }
}

void OsIntegrationTestOverride::EnableOrDisablePathOnLogin(
    const base::FilePath& file_path,
    bool enable_on_login) {
  startup_enabled_[file_path] = enable_on_login;
}

#elif BUILDFLAG(IS_LINUX)
bool OsIntegrationTestOverride::DeleteDesktopDirOnLinux() {
  if (desktop_.IsValid()) {
    return desktop_.Delete();
  } else {
    return false;
  }
}
#endif

void OsIntegrationTestOverride::RegisterProtocolSchemes(
    const AppId& app_id,
    std::vector<std::string> protocols) {
  protocol_scheme_registrations_.emplace_back(app_id, std::move(protocols));
}

OsIntegrationTestOverride::OsIntegrationTestOverride(
    const base::FilePath& base_path) {
  // Initialize all directories used. The success & the DCHECK are separated to
  // ensure that these function calls occur on release builds.
  if (!base_path.empty()) {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = application_menu_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = quick_launch_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#endif
  } else {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDir();
    DCHECK(success);
    success = application_menu_.CreateUniqueTempDir();
    DCHECK(success);
    success = quick_launch_.CreateUniqueTempDir();
    DCHECK(success);
    success = startup_.CreateUniqueTempDir();
    DCHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDir();
    DCHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDir();
    DCHECK(success);
    success = startup_.CreateUniqueTempDir();
    DCHECK(success);
#endif
  }

#if BUILDFLAG(IS_LINUX)
  auto callback =
      base::BindRepeating([](base::FilePath filename, std::string xdg_command,
                             std::string file_contents) {
        auto test_override = GetOsIntegrationTestOverride();
        DCHECK(test_override);
        LinuxFileRegistration file_registration = LinuxFileRegistration();
        file_registration.xdg_command = xdg_command;
        file_registration.file_contents = file_contents;
        test_override->linux_file_registration_.push_back(file_registration);
        return true;
      });
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(std::move(callback));
#endif
}

OsIntegrationTestOverride::~OsIntegrationTestOverride() {
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
  directories = {&desktop_};
#endif
  for (base::ScopedTempDir* dir : directories) {
    if (!dir->IsValid()) {
      continue;
    }
    DCHECK(base::IsDirectoryEmpty(dir->GetPath()))
        << "Directory not empty: " << dir->GetPath().AsUTF8Unsafe()
        << ". Please uninstall all webapps that have been installed while "
           "shortcuts were overriden. Contents:\n"
        << GetAllFilesInDir(dir->GetPath());
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
SkColor OsIntegrationTestOverride::GetIconTopLeftColorFromShortcutFile(
    const base::FilePath& shortcut_path) {
  DCHECK(base::PathExists(shortcut_path));
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
SkColor OsIntegrationTestOverride::ReadColorFromShortcutMenuIcoFile(
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

scoped_refptr<OsIntegrationTestOverride> GetOsIntegrationTestOverride() {
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  return base::WrapRefCounted(state.global_os_integration_test_override.get());
}

}  // namespace web_app

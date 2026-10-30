// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"

#include <fcntl.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base_paths.h"
#include "base/check_is_test.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/os_integration/dynamic_launcher_portal.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/auto_start_linux.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace web_app {

BASE_FEATURE(kWebAppsUseDynamicLauncherPortal,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldUseDynamicLauncherPortal() {
  // Host installs keep writing .desktop files directly, which also allows
  // silent launcher updates. The portal is only needed in app sandboxes, where
  // the host's .desktop files can't be written. Snaps keep using the legacy
  // path, since the DynamicLauncher portal doesn't support them
  // (https://github.com/flatpak/xdg-desktop-portal/issues/2014).
  return base::nix::GetAppSandbox() == base::nix::AppSandbox::kFlatpak &&
         base::FeatureList::IsEnabled(kWebAppsUseDynamicLauncherPortal);
}

namespace {

// UMA metric name for creating shortcut result.
constexpr const char* kCreateShortcutResult =
    "Apps.CreateShortcuts.Linux.Result";

// UMA metric name for creating shortcut icon result.
constexpr const char* kCreateShortcutIconResult =
    "Apps.CreateShortcutIcon.Linux.Result";

// Testing hook for shell_integration_linux
LaunchXdgUtilityForTesting& GetInstalledLaunchXdgUtilityForTesting() {
  static base::NoDestructor<LaunchXdgUtilityForTesting> instance;
  return *instance;
}

base::FilePath GetDesktopPath() {
  base::FilePath desktop_path;
  base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path);
  return desktop_path;
}

base::FilePath GetAutostartPath(base::Environment* env) {
  return AutoStart::GetAutostartDirectory(env);
}

constexpr int kMaxPortalIconSize = 512;

}  // namespace

std::vector<uint8_t> GetShortcutIconBytes(const gfx::ImageFamily& icon_images) {
  // Prefer the smallest icon that is at least kMaxPortalIconSize, downscaled if
  // needed, and otherwise the largest smaller icon, which isn't upscaled.
  const gfx::Image* best_image =
      icon_images.GetBest(kMaxPortalIconSize, kMaxPortalIconSize);
  if (!best_image) {
    return {};
  }
  gfx::Image resized = gfx::ResizedImageForMaxDimensions(
      *best_image, kMaxPortalIconSize, kMaxPortalIconSize, /*max_area=*/0);
  scoped_refptr<base::RefCountedMemory> best_png = resized.As1xPNGBytes();

  if (!best_png || best_png->size() == 0) {
    return {};
  }
  base::span<const uint8_t> span = *best_png;
  return std::vector<uint8_t>(span.begin(), span.end());
}

// Strips Name=, Icon=, Actions=, and non-[Desktop Entry] groups from desktop
// file. This is required because org.freedesktop.portal.DynamicLauncher
// enforces that desktop entries passed to Install() must contain exactly one
// group ([Desktop Entry]).
std::string StripNameAndIconFromDesktopFile(std::string_view contents) {
  std::vector<std::string_view> lines = base::SplitStringPiece(
      contents, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string_view> filtered_lines;
  bool in_main_group = false;
  for (std::string_view line : lines) {
    std::string_view trimmed =
        base::TrimWhitespaceASCII(line, base::TRIM_LEADING);
    if (trimmed.starts_with("[")) {
      in_main_group = (trimmed == "[Desktop Entry]");
    }
    if (in_main_group) {
      if (trimmed.starts_with("Name=") || trimmed.starts_with("Icon=") ||
          trimmed.starts_with("Name[") || trimmed.starts_with("Icon[") ||
          trimmed.starts_with("Actions=")) {
        continue;
      }
      filtered_lines.push_back(line);
    }
  }
  return base::JoinString(filtered_lines, "\n");
}

// Returns the portal desktop_file_id, formatted as <app_id>.<shortcut_filename>
// as required by org.freedesktop.portal.DynamicLauncher (e.g.
// org.chromium.Chromium.chrome-app_id-profile.desktop).
std::string GetPortalDesktopFileId(base::Environment* env,
                                   const base::FilePath& shortcut_filename) {
  std::string name = shortcut_filename.BaseName().value();
  std::unique_ptr<base::Environment> local_env;
  if (!env) {
    local_env = base::Environment::Create();
    env = local_env.get();
  }
  std::string app_name = env ? version_info::nix::GetAppName(*env) : "";
  if (app_name.empty()) {
    return name;
  }
  std::string prefix = base::StrCat({app_name, "."});
  if (base::StartsWith(name, prefix)) {
    return name;
  }
  return base::StrCat({prefix, name});
}

// Result of creating app shortcut icon.
// Success is recorded for each icon image, but the first two errors
// are per app, so the success/error ratio might not be very meaningful.
enum class CreateShortcutIconResult {
  kSuccess = 0,
  kEmptyIconImages = 1,
  kFailToCreateTempDir = 2,
  kFailToEncodeImageToPng = 3,
  kImageCorrupted = 4,
  kFailToInstallIcon = 5,
  kMaxValue = kFailToInstallIcon
};

// Result of creating app shortcut.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateShortcutResult {
  kSuccess = 0,
  kFailToGetShortcutFilename = 1,
  kFailToGetChromeExePath = 2,
  kFailToGetDesktopPath = 3,
  kFailToOpenDesktopDir = 4,
  kFailToOpenShortcutFilepath = 5,
  kCorruptDesktopShortcut = 6,
  kFailToCreateTempDir = 7,
  kCorruptDirectoryContents = 8,
  kCorruptApplicationsMenuShortcut = 9,
  kFailToInstallShortcut = 10,
  kPortalInstallFailed = 11,
  kMaxValue = kPortalInstallFailed
};

// Record UMA metric for creating shortcut icon.
void RecordCreateIcon(CreateShortcutIconResult result) {
  UMA_HISTOGRAM_ENUMERATION(kCreateShortcutIconResult, result);
}

// Record UMA metric for creating shortcut.
void RecordCreateShortcut(CreateShortcutResult result) {
  UMA_HISTOGRAM_ENUMERATION(kCreateShortcutResult, result);
}

bool LaunchXdgUtility(const std::vector<std::string>& argv, int* exit_code) {
  if (GetInstalledLaunchXdgUtilityForTesting()) {
    return GetInstalledLaunchXdgUtilityForTesting().Run(argv, exit_code);
  }

  return shell_integration_linux::LaunchXdgUtility(argv, exit_code);
}

bool DeleteShortcutInApplicationsMenuLegacy(
    const base::FilePath& shortcut_filename,
    const base::FilePath& directory_filename) {
  // TODO(crbug.com/40808705): Support shortcut testing in Applications Menu.
  DCHECK(!OsIntegrationTestOverride::Get() ||
         GetInstalledLaunchXdgUtilityForTesting());  // IN-TEST
  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("uninstall");

  // Uninstall in user mode, to match the install.
  argv.push_back("--mode");
  argv.push_back("user");

  // The file does not need to exist anywhere - xdg-desktop-menu will uninstall
  // items from the menu with a matching name.
  // If |directory_filename| is supplied, this will also remove the item from
  // the directory, and remove the directory if it is empty.
  if (!directory_filename.empty()) {
    argv.push_back(directory_filename.value());
  }
  argv.push_back(shortcut_filename.value());
  int exit_code;
  return LaunchXdgUtility(argv, &exit_code);
}

const char kDirectoryFilename[] = "chrome-apps.directory";

std::string CreateShortcutIcon(base::Environment* env,
                               const gfx::ImageFamily& icon_images,
                               const base::FilePath& shortcut_filename) {
  if (icon_images.empty()) {
    RecordCreateIcon(CreateShortcutIconResult::kEmptyIconImages);
    return std::string();
  }

  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    RecordCreateIcon(CreateShortcutIconResult::kFailToCreateTempDir);
    return std::string();
  }

  base::FilePath temp_file_path =
      temp_dir.GetPath().Append(shortcut_filename.ReplaceExtension("png"));
  std::string icon_name = temp_file_path.BaseName().RemoveExtension().value();
  std::string desktop_icon = icon_name;
  int largest_installed_icon_width = 0;

  for (gfx::ImageFamily::const_iterator it = icon_images.begin();
       it != icon_images.end(); ++it) {
    int width = it->Width();
    scoped_refptr<base::RefCountedMemory> png_data = it->As1xPNGBytes();
    if (png_data->size() == 0) {
      // If the bitmap could not be encoded to PNG format, skip it.
      LOG(WARNING) << "Could not encode icon " << icon_name << ".png at size "
                   << width << ".";
      RecordCreateIcon(CreateShortcutIconResult::kFailToEncodeImageToPng);
      continue;
    }
    if (!base::WriteFile(temp_file_path, *png_data)) {
      RecordCreateIcon(CreateShortcutIconResult::kImageCorrupted);
      return std::string();
    }
    // TODO(https://crbug.com/332935566): Have the os integration override save
    // these calls instead of exiting early here.
    if (OsIntegrationTestOverride::Get() &&
        !GetInstalledLaunchXdgUtilityForTesting()) {
      CHECK_IS_TEST();
      continue;
    }

    std::vector<std::string> argv;
    argv.push_back("xdg-icon-resource");
    argv.push_back("install");

    // Always install in user mode, even if someone runs the browser as root
    // (people do that).
    argv.push_back("--mode");
    argv.push_back("user");

    argv.push_back("--size");
    argv.push_back(base::NumberToString(width));

    argv.push_back(temp_file_path.value());
    argv.push_back(icon_name);
    int exit_code;
    if (!LaunchXdgUtility(argv, &exit_code) || exit_code) {
      LOG(WARNING) << "Could not install icon " << icon_name << ".png at size "
                   << width << ".";
      RecordCreateIcon(CreateShortcutIconResult::kFailToInstallIcon);
    } else {
      RecordCreateIcon(CreateShortcutIconResult::kSuccess);
      if (width > largest_installed_icon_width) {
        base::FilePath icon_path =
            base::nix::GetXDGDataWriteLocation(env)
                .Append("icons")
                .Append("hicolor")
                .AppendASCII(base::NumberToString(width) + "x" +
                             base::NumberToString(width))
                .Append("apps")
                .Append(base::FilePath(icon_name).AddExtension("png"));
        if (icon_path.IsAbsolute()) {
          desktop_icon = icon_path.value();
          largest_installed_icon_width = width;
        }
      }
    }
  }
  return desktop_icon;
}

bool CreateShortcutAtLocation(const base::FilePath location_path,
                              const base::FilePath& shortcut_filename,
                              const std::string& contents) {
  // Make sure that we will later call openat in a secure way.
  CHECK_EQ(shortcut_filename.BaseName().value(), shortcut_filename.value(),
           base::NotFatalUntil::M162);

  int location_fd = open(location_path.value().c_str(), O_RDONLY | O_DIRECTORY);
  if (location_fd < 0) {
    RecordCreateShortcut(CreateShortcutResult::kFailToOpenDesktopDir);
    return false;
  }

  int fd = openat(location_fd, shortcut_filename.value().c_str(),
                  O_CREAT | O_EXCL | O_WRONLY,
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (fd < 0) {
    if (IGNORE_EINTR(close(location_fd)) < 0) {
      PLOG(ERROR) << "close";
    }
    RecordCreateShortcut(CreateShortcutResult::kFailToOpenShortcutFilepath);
    return false;
  }

  if (!base::WriteFileDescriptor(fd, contents)) {
    // Delete the file. No shortcut is better than corrupted one. Use unlinkat
    // to make sure we're deleting the file in the directory we think we are.
    // Even if an attacker manager to put something other at
    // |shortcut_filename| we'll just undo their action.
    RecordCreateShortcut(CreateShortcutResult::kCorruptDesktopShortcut);
    unlinkat(location_fd, shortcut_filename.value().c_str(), 0);
  }

  if (IGNORE_EINTR(close(fd)) < 0) {
    PLOG(ERROR) << "close";
  }

  if (IGNORE_EINTR(close(location_fd)) < 0) {
    PLOG(ERROR) << "close";
  }

  return true;
}

bool CreateShortcutOnDesktop(const base::FilePath& shortcut_filename,
                             const std::string& contents) {
  base::FilePath desktop_path = GetDesktopPath();
  if (desktop_path.empty()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToGetDesktopPath);
    return false;
  }

  return CreateShortcutAtLocation(desktop_path, shortcut_filename, contents);
}

bool CreateShortcutInAutoStart(base::Environment* env,
                               const base::FilePath& shortcut_filename,
                               const std::string& contents) {
  base::FilePath autostart_path = GetAutostartPath(env);
  if (!base::DirectoryExists(autostart_path) &&
      !base::CreateDirectory(autostart_path)) {
    return false;
  }

  return CreateShortcutAtLocation(autostart_path, shortcut_filename, contents);
}

// Creates a shortcut with |shortcut_filename| and |contents| in the system
// applications menu. If |directory_filename| is non-empty, creates a sub-menu
// with |directory_filename| and |directory_contents|, and stores the shortcut
// under the sub-menu.
bool CreateShortcutInApplicationsMenuLegacy(
    base::Environment* env,
    const base::FilePath& shortcut_filename,
    const std::string& contents,
    const base::FilePath& directory_filename,
    const std::string& directory_contents) {
  // Application menu shortcuts are not yet supported by
  // OsIntegrationTestOverride, so this function should only be called if it
  // isn't populated OR if the test has overridden
  // `GetInstalledLaunchXdgUtilityForTesting()`.
  DCHECK(!OsIntegrationTestOverride::Get() ||
         GetInstalledLaunchXdgUtilityForTesting());  // IN-TEST
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToCreateTempDir);
    return false;
  }

  base::FilePath temp_directory_path;
  if (!directory_filename.empty()) {
    temp_directory_path = temp_dir.GetPath().Append(directory_filename);
    if (!base::WriteFile(temp_directory_path, directory_contents)) {
      RecordCreateShortcut(CreateShortcutResult::kCorruptDirectoryContents);
      return false;
    }
  }

  base::FilePath temp_file_path = temp_dir.GetPath().Append(shortcut_filename);
  if (!base::WriteFile(temp_file_path, contents)) {
    RecordCreateShortcut(
        CreateShortcutResult::kCorruptApplicationsMenuShortcut);
    return false;
  }

  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  // If provided, install the shortcut file inside the given directory.
  if (!directory_filename.empty()) {
    argv.push_back(temp_directory_path.value());
  }
  argv.push_back(temp_file_path.value());
  int exit_code;
  LaunchXdgUtility(argv, &exit_code);

  if (exit_code != 0) {
    RecordCreateShortcut(CreateShortcutResult::kFailToInstallShortcut);
    return false;
  }

  // Some Linux file managers (Nautilus and Nemo) depend on an up to date
  // mimeinfo.cache file to detect whether applications can open files, so
  // manually run update-desktop-database on the user applications folder.
  // See this bug on xdg desktop-file-utils
  // https://gitlab.freedesktop.org/xdg/desktop-file-utils/issues/54
  base::FilePath user_dir = base::nix::GetXDGDataWriteLocation(env);
  base::FilePath user_applications_dir = user_dir.Append("applications");
  argv.clear();
  argv.push_back("update-desktop-database");
  argv.push_back(user_applications_dir.value());

  // Ignore the exit code of update-desktop-database, if it fails it isn't
  // important (the shortcut is created and usable when xdg-desktop-menu install
  // completes). Failure means the file type associations for this desktop entry
  // may not show up in some file managers, but this is non-critical.
  int ignored_exit_code = 0;
  LaunchXdgUtility(argv, &ignored_exit_code);

  return true;
}

// Runs on the shortcut sequence once the portal's Install call has finished.
// If the portal failed, falls back to the legacy install, so the user still
// gets a launcher where the legacy path works.
void FinishPortalInstall(base::FilePath applications_dir,
                         base::OnceCallback<bool()> legacy_install,
                         base::OnceCallback<void(bool)> callback,
                         bool portal_success) {
  if (!portal_success) {
    LOG(ERROR) << "Portal shortcut installation failed, falling back to the "
                  "legacy install";
    RecordCreateShortcut(CreateShortcutResult::kPortalInstallFailed);
    bool success = std::move(legacy_install).Run();
    std::move(callback).Run(success);
    return;
  }
  if (!applications_dir.empty()) {
    std::vector<std::string> argv = {"update-desktop-database",
                                     applications_dir.value()};
    int exit_code;
    LaunchXdgUtility(argv, &exit_code);
  }
  std::move(callback).Run(true);
}

void OnPortalInstallResult(
    std::unique_ptr<DynamicLauncherPortal> portal,
    base::FilePath applications_dir,
    base::OnceCallback<bool()> legacy_install,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> origin_runner,
    bool portal_success) {
  origin_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&FinishPortalInstall, std::move(applications_dir),
                     std::move(legacy_install), std::move(callback),
                     portal_success));
}

void TryInstallPortalShortcutOnUIThread(
    std::string portal_desktop_file_id,
    std::string stripped_contents,
    std::string token,
    base::FilePath applications_dir,
    base::OnceCallback<bool()> legacy_install,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> origin_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (token.empty()) {
    origin_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::OnceCallback<bool()> legacy_install,
                          base::OnceCallback<void(bool)> callback) {
                         bool success = std::move(legacy_install).Run();
                         std::move(callback).Run(success);
                       },
                       std::move(legacy_install), std::move(callback)));
    return;
  }
  auto portal = std::make_unique<DynamicLauncherPortal>();
  auto* portal_ptr = portal.get();
  portal_ptr->Install(
      token, portal_desktop_file_id, stripped_contents,
      base::BindOnce(&OnPortalInstallResult, std::move(portal),
                     std::move(applications_dir), std::move(legacy_install),
                     std::move(callback), origin_runner));
}

void CreateShortcutInApplicationsMenu(base::Environment* env,
                                      const ShortcutInfo& shortcut_info,
                                      const base::FilePath& shortcut_filename,
                                      const std::string& contents,
                                      const base::FilePath& directory_filename,
                                      const std::string& directory_contents,
                                      base::OnceCallback<void(bool)> callback) {
  auto legacy_install = base::BindOnce(
      [](base::Environment* env, base::FilePath shortcut_filename,
         std::string contents, base::FilePath directory_filename,
         std::string directory_contents) {
        return CreateShortcutInApplicationsMenuLegacy(
            env, shortcut_filename, contents, directory_filename,
            directory_contents);
      },
      env, shortcut_filename, contents, directory_filename, directory_contents);

  base::FilePath applications_dir =
      env ? base::nix::GetXDGDataWriteLocation(env).AppendASCII("applications")
          : base::FilePath();

  if (GetInstalledLaunchXdgUtilityForTesting() ||
      !ShouldUseDynamicLauncherPortal() || shortcut_info.portal_token.empty()) {
    base::FilePath portal_filename(
        GetPortalDesktopFileId(env, shortcut_filename));
    if (portal_filename != shortcut_filename && !applications_dir.empty() &&
        base::PathExists(applications_dir.Append(portal_filename))) {
      if (ShouldUseDynamicLauncherPortal()) {
        // The launcher was installed through the portal, and replacing it
        // requires a new user-approved token. Without one (e.g. for silent
        // manifest updates), leave the existing launcher untouched rather than
        // replacing it with a legacy one, which wouldn't work in a sandbox.
        std::move(callback).Run(true);
        return;
      }
      // The feature was turned off: replace the portal launcher with a legacy
      // one.
      DeleteShortcutInApplicationsMenuLegacy(portal_filename,
                                             directory_filename);
    }
    bool success = std::move(legacy_install).Run();
    std::move(callback).Run(success);
    return;
  }

  std::string portal_desktop_file_id =
      GetPortalDesktopFileId(env, shortcut_filename);
  std::string stripped_contents = StripNameAndIconFromDesktopFile(contents);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TryInstallPortalShortcutOnUIThread,
                     portal_desktop_file_id, stripped_contents,
                     shortcut_info.portal_token, std::move(applications_dir),
                     std::move(legacy_install), std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

void PreparePortalInstallOnUIThread(
    std::string display_name,
    std::vector<uint8_t> icon_bytes,
    GURL target_url,
    base::OnceCallback<void(std::optional<std::string> token)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (GetInstalledLaunchXdgUtilityForTesting() ||
      !ShouldUseDynamicLauncherPortal()) {
    std::move(callback).Run("");
    return;
  }
  auto portal = std::make_unique<DynamicLauncherPortal>();
  auto* portal_ptr = portal.get();
  portal_ptr->IsAvailable(base::BindOnce(
      [](std::unique_ptr<DynamicLauncherPortal> portal,
         std::string display_name, std::vector<uint8_t> icon_bytes,
         GURL target_url,
         base::OnceCallback<void(std::optional<std::string> token)> callback,
         bool available) {
        if (!available) {
          std::move(callback).Run("");
          return;
        }
        auto* portal_ptr = portal.get();
        portal_ptr->PrepareInstall(
            display_name, icon_bytes, target_url,
            base::BindOnce(
                [](std::unique_ptr<DynamicLauncherPortal> portal,
                   base::OnceCallback<void(std::optional<std::string> token)>
                       callback,
                   std::optional<std::string> token) {
                  std::move(callback).Run(token);
                },
                std::move(portal), std::move(callback)));
      },
      std::move(portal), std::move(display_name), std::move(icon_bytes),
      std::move(target_url), std::move(callback)));
}

DesktopActionInfo::DesktopActionInfo() = default;

DesktopActionInfo::DesktopActionInfo(const std::string& id,
                                     const std::string& name,
                                     const GURL& exec_launch_url)
    : id(id), name(name), exec_launch_url(exec_launch_url) {
#if DCHECK_IS_ON()
  // Check that `id` only contains characters A-Za-z0-9-.
  auto is_character_allowed = [](auto c) {
    return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-';
  };
  CHECK(std::all_of(id.begin() + 1, id.end(), is_character_allowed),
        base::NotFatalUntil::M162);
#endif  // DCHECK_IS_ON()
}

DesktopActionInfo::DesktopActionInfo(const DesktopActionInfo&) = default;

bool DesktopActionInfo::operator<(const DesktopActionInfo& other) const {
  return this->id < other.id;
}

DesktopActionInfo::~DesktopActionInfo() = default;

void SetLaunchXdgUtilityForTesting(
    LaunchXdgUtilityForTesting launchXdgUtilityForTesting) {
  GetInstalledLaunchXdgUtilityForTesting() =
      std::move(launchXdgUtilityForTesting);
}

std::string GetShortcutFileName(const base::FilePath& profile_path,
                                const std::string& app_id) {
  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename(chrome::kBrowserProcessExecutableName);
  filename.append("-").append(app_id).append("-").append(
      profile_path.BaseName().value());
  base::i18n::ReplaceIllegalCharactersInPath(&filename, '_');
  // Spaces in filenames break xdg-desktop-menu
  // (see https://bugs.freedesktop.org/show_bug.cgi?id=66605).
  base::ReplaceChars(filename, " ", "_", &filename);
  return filename;
}

base::FilePath GetAppDesktopShortcutFilename(const base::FilePath& profile_path,
                                             const std::string& app_id) {
  CHECK(!app_id.empty());
  CHECK(!profile_path.empty());
  std::string shortcut_filename = GetShortcutFileName(profile_path, app_id);
  return base::FilePath(shortcut_filename.append(".desktop"));
}

base::FilePath GetAppIconShortcutFilename(const base::FilePath& profile_path,
                                          const std::string& app_id) {
  CHECK(!app_id.empty());
  std::string shortcut_filename = GetShortcutFileName(profile_path, app_id);
  return base::FilePath(shortcut_filename.append(".png"));
}

bool DeleteShortcutOnDesktop(const base::FilePath& shortcut_filename) {
  base::FilePath desktop_path = GetDesktopPath();
  bool result = false;
  if (!desktop_path.empty()) {
    result = base::DeleteFile(desktop_path.Append(shortcut_filename));
  }
  return result;
}

bool DeleteShortcutInAutoStart(base::Environment* env,
                               const base::FilePath& shortcut_filename) {
  base::FilePath autostart_path = GetAutostartPath(env);
  return base::DeleteFile(autostart_path.Append(shortcut_filename));
}

void OnPortalUninstallResult(
    std::unique_ptr<DynamicLauncherPortal> portal,
    base::OnceCallback<bool()> legacy_uninstall,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> origin_runner,
    bool portal_success) {
  if (portal_success) {
    origin_runner->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), true));
    return;
  }
  // Fall back to legacy uninstall if portal uninstallation failed or app was
  // installed prior to portal availability.
  origin_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::OnceCallback<bool()> legacy_uninstall,
                        base::OnceCallback<void(bool)> callback) {
                       bool success = std::move(legacy_uninstall).Run();
                       std::move(callback).Run(success);
                     },
                     std::move(legacy_uninstall), std::move(callback)));
}

void TryUninstallPortalShortcutOnUIThread(
    std::string portal_desktop_file_id,
    base::OnceCallback<bool()> legacy_uninstall,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> origin_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto portal = std::make_unique<DynamicLauncherPortal>();
  auto* portal_ptr = portal.get();
  portal_ptr->IsAvailable(base::BindOnce(
      [](std::unique_ptr<DynamicLauncherPortal> portal,
         std::string portal_desktop_file_id,
         base::OnceCallback<bool()> legacy_uninstall,
         base::OnceCallback<void(bool)> callback,
         scoped_refptr<base::SequencedTaskRunner> origin_runner,
         bool available) {
        if (!available) {
          origin_runner->PostTask(
              FROM_HERE, base::BindOnce(
                             [](base::OnceCallback<bool()> legacy_uninstall,
                                base::OnceCallback<void(bool)> callback) {
                               bool success = std::move(legacy_uninstall).Run();
                               std::move(callback).Run(success);
                             },
                             std::move(legacy_uninstall), std::move(callback)));
          return;
        }
        auto* portal_ptr = portal.get();
        portal_ptr->Uninstall(
            portal_desktop_file_id,
            base::BindOnce(&OnPortalUninstallResult, std::move(portal),
                           std::move(legacy_uninstall), std::move(callback),
                           origin_runner));
      },
      std::move(portal), std::move(portal_desktop_file_id),
      std::move(legacy_uninstall), std::move(callback), origin_runner));
}

void DeleteShortcutInApplicationsMenu(base::Environment* env,
                                      const base::FilePath& shortcut_filename,
                                      const base::FilePath& directory_filename,
                                      base::OnceCallback<void(bool)> callback) {
  auto legacy_uninstall = base::BindOnce(
      [](base::Environment* env, base::FilePath shortcut_filename,
         base::FilePath directory_filename) {
        base::FilePath applications_dir =
            env ? base::nix::GetXDGDataWriteLocation(env).AppendASCII(
                      "applications")
                : base::FilePath();
        base::FilePath portal_filename(
            GetPortalDesktopFileId(env, shortcut_filename));

        bool success = true;
        bool deleted_any = false;

        if (!applications_dir.empty() &&
            base::PathExists(applications_dir.Append(portal_filename))) {
          success = DeleteShortcutInApplicationsMenuLegacy(
                        portal_filename, directory_filename) &&
                    success;
          deleted_any = true;
        }

        if (portal_filename != shortcut_filename &&
            ((!applications_dir.empty() &&
              base::PathExists(applications_dir.Append(shortcut_filename))) ||
             !deleted_any)) {
          success = DeleteShortcutInApplicationsMenuLegacy(
                        shortcut_filename, directory_filename) &&
                    success;
        }

        return success;
      },
      env, shortcut_filename, directory_filename);

  if (GetInstalledLaunchXdgUtilityForTesting() ||
      !ShouldUseDynamicLauncherPortal()) {
    bool success = std::move(legacy_uninstall).Run();
    std::move(callback).Run(success);
    return;
  }

  std::string portal_desktop_file_id =
      GetPortalDesktopFileId(env, shortcut_filename);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TryUninstallPortalShortcutOnUIThread,
                     portal_desktop_file_id, std::move(legacy_uninstall),
                     std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

// Removes temporary shortcut icons left over in `./local/share/icons/hicolor`
// post shortcut deletion.
bool DeleteShortcutIcons(const base::FilePath& shortcut_icon_filename) {
  CHECK(!shortcut_icon_filename.empty());
  bool result = true;
  for (const auto& size : web_app::GetDesiredIconSizesForShortcut()) {
    std::vector<std::string> argv;
    argv.push_back("xdg-icon-resource");
    argv.push_back("uninstall");

    // Uninstall in user mode, to match the install.
    argv.push_back("--mode");
    argv.push_back("user");
    argv.push_back("--size");
    argv.push_back(base::ToString(size));

    argv.push_back(shortcut_icon_filename.value());
    int exit_code;
    result = result && LaunchXdgUtility(argv, &exit_code);
  }
  return result;
}

void CreateDesktopShortcut(const ShortcutInfo& shortcut_info,
                           const ShortcutLocations& creation_locations,
                           base::OnceCallback<void(bool)> callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<base::Environment> owned_env = base::Environment::Create();
  base::Environment* env = owned_env.get();
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use it.
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();

  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  bool create_shortcut_in_startup = creation_locations.in_startup;

  ApplicationsMenuLocation applications_menu_location =
      creation_locations.applications_menu_location;
  // Do not create the shortcuts in startup directory when testing because
  // xdg-utility (which creates this shortcut) doesn't work well with temp
  // directories.
  if (test_override && !GetInstalledLaunchXdgUtilityForTesting()) {
    CHECK_IS_TEST();
    applications_menu_location = APP_MENU_LOCATION_NONE;
  }

  base::FilePath shortcut_filename;
  if (!shortcut_info.app_id.empty()) {
    shortcut_filename = GetAppDesktopShortcutFilename(
        shortcut_info.profile_path, shortcut_info.app_id);
    // For extensions we do not want duplicate shortcuts. So, delete any that
    // already exist and replace them.
    if (creation_locations.on_desktop) {
      DeleteShortcutOnDesktop(shortcut_filename);
    }

    if (create_shortcut_in_startup) {
      DeleteShortcutInAutoStart(env, shortcut_filename);
    }

    if (applications_menu_location != APP_MENU_LOCATION_NONE) {
      DeleteShortcutInApplicationsMenuLegacy(shortcut_filename,
                                             base::FilePath());
    }
    if (!test_override && !GetInstalledLaunchXdgUtilityForTesting()) {
      base::FilePath icon_filename = GetAppIconShortcutFilename(
          shortcut_info.profile_path, shortcut_info.app_id);
      DeleteShortcutIcons(icon_filename);
    }
  } else if (std::optional<base::SafeBaseName> opt_shortcut_filename =
                 shell_integration_linux::GetUniqueWebShortcutFilename(
                     shortcut_info.url.spec());
             opt_shortcut_filename.has_value()) {
    shortcut_filename = opt_shortcut_filename->path();
  }
  if (shortcut_filename.empty()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToGetShortcutFilename);
    std::move(callback).Run(false);
    return;
  }

  std::string icon_name =
      CreateShortcutIcon(env, shortcut_info.favicon, shortcut_filename);

  std::string app_name = GenerateApplicationNameFromInfo(shortcut_info);

  bool success = true;

  base::FilePath chrome_exe_path =
      shell_integration_linux::internal::GetChromeExePath();
  if (chrome_exe_path.empty()) {
    RecordCreateShortcut(CreateShortcutResult::kFailToGetChromeExePath);
    NOTREACHED();
  }

  if (creation_locations.on_desktop) {
    std::string contents = shell_integration_linux::GetDesktopFileContents(
        chrome_exe_path, app_name, shortcut_info.url, shortcut_info.app_id,
        shortcut_info.title, icon_name, shortcut_info.profile_path, "", "",
        false, "", shortcut_info.actions);
    success = CreateShortcutOnDesktop(shortcut_filename, contents);
  }

  if (create_shortcut_in_startup) {
    // if (creation_locations.in_startup) {
    std::string contents = shell_integration_linux::GetDesktopFileContents(
        chrome_exe_path, app_name, shortcut_info.url, shortcut_info.app_id,
        shortcut_info.title, icon_name, shortcut_info.profile_path, "", "",
        false, kRunOnOsLoginModeWindowed, shortcut_info.actions);
    success =
        CreateShortcutInAutoStart(env, shortcut_filename, contents) && success;
  }

  if (test_override && !shortcut_info.protocol_handlers.empty()) {
    CHECK_IS_TEST();
    std::vector<std::string> protocol_handler(
        shortcut_info.protocol_handlers.begin(),
        shortcut_info.protocol_handlers.end());
    test_override->RegisterProtocolSchemes(shortcut_info.app_id,
                                           std::move(protocol_handler));
  }

  if (applications_menu_location == APP_MENU_LOCATION_NONE) {
    if (success) {
      RecordCreateShortcut(CreateShortcutResult::kSuccess);
    }
    std::move(callback).Run(success);
    return;
  }

  base::FilePath directory_filename;
  std::string directory_contents;
  switch (creation_locations.applications_menu_location) {
    case APP_MENU_LOCATION_HIDDEN:
      break;
    case APP_MENU_LOCATION_SUBDIR_CHROMEAPPS:
      directory_filename = base::FilePath(kDirectoryFilename);
      directory_contents = shell_integration_linux::GetDirectoryFileContents(
          shell_integration::GetAppShortcutsSubdirName(), "");
      break;
    default:
      NOTREACHED();
  }

  std::vector<std::string> mime_types;
  // Filter out any "x-scheme-handler/*" pseudo-MIME types from file handlers.
  // In FreeDesktop .desktop files, "x-scheme-handler/*" entries associate URL
  // schemes and must only originate from validated protocol_handlers below.
  for (const auto& file_handler_mime_type :
       shortcut_info.file_handler_mime_types) {
    if (!base::EqualsCaseInsensitiveASCII(file_handler_mime_type,
                                          "x-scheme-handler") &&
        !base::StartsWith(file_handler_mime_type, "x-scheme-handler/",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      mime_types.push_back(file_handler_mime_type);
    }
  }

  // Convert protocol handlers into mime types for registration in the
  // .desktop file.
  for (const auto& protocol_handler : shortcut_info.protocol_handlers) {
    mime_types.push_back("x-scheme-handler/" + protocol_handler);
  }

  // Set NoDisplay=true if hidden. This will hide the application from
  // user-facing menus.
  std::string contents = shell_integration_linux::GetDesktopFileContents(
      chrome_exe_path, app_name, shortcut_info.url, shortcut_info.app_id,
      shortcut_info.title, icon_name, shortcut_info.profile_path, "",
      base::JoinString(mime_types, ";"),
      creation_locations.applications_menu_location == APP_MENU_LOCATION_HIDDEN,
      "", shortcut_info.actions);

  CreateShortcutInApplicationsMenu(
      env, shortcut_info, shortcut_filename, contents, directory_filename,
      directory_contents,
      // `owned_env` is bound to keep `env` alive until the operation finishes.
      base::BindOnce(
          [](std::unique_ptr<base::Environment> owned_env, bool success,
             base::OnceCallback<void(bool)> callback, bool app_menu_success) {
            bool result = success && app_menu_success;
            if (result) {
              RecordCreateShortcut(CreateShortcutResult::kSuccess);
            }
            std::move(callback).Run(result);
          },
          std::move(owned_env), success, std::move(callback)));
}

ShortcutLocations GetExistingShortcutLocations(
    base::Environment* env,
    const base::FilePath& profile_path,
    const std::string& extension_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  base::FilePath shortcut_filename =
      GetAppDesktopShortcutFilename(profile_path, extension_id);
  CHECK(!shortcut_filename.empty(), base::NotFatalUntil::M162);
  ShortcutLocations locations;

  // Determine whether there is a shortcut on desktop.
  base::FilePath desktop_path = GetDesktopPath();

  if (!desktop_path.empty()) {
    locations.on_desktop =
        base::PathExists(desktop_path.Append(shortcut_filename));
  }

  base::FilePath autostart_path = GetAutostartPath(env);
  if (!autostart_path.empty()) {
    locations.in_startup =
        base::PathExists(autostart_path.Append(shortcut_filename));
  }

  // Determine whether there is a shortcut in the applications directory.
  std::string shortcut_contents;
  if (shell_integration_linux::GetExistingShortcutContents(
          env, shortcut_filename, &shortcut_contents) ||
      shell_integration_linux::GetExistingShortcutContents(
          env, base::FilePath(GetPortalDesktopFileId(env, shortcut_filename)),
          &shortcut_contents)) {
    // If the shortcut contents contain NoDisplay=true, it should be hidden.
    // Otherwise since these shortcuts are for apps, they are always in the
    // "Chrome Apps" directory.
    locations.applications_menu_location =
        shell_integration_linux::internal::GetNoDisplayFromDesktopFile(
            shortcut_contents)
            ? APP_MENU_LOCATION_HIDDEN
            : APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }

  return locations;
}

void DeleteDesktopShortcuts(const base::FilePath& profile_path,
                            const std::string& extension_id,
                            base::OnceCallback<void(bool)> callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<base::Environment> owned_env = base::Environment::Create();
  base::Environment* env = owned_env.get();
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use it.
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  base::FilePath shortcut_filename =
      GetAppDesktopShortcutFilename(profile_path, extension_id);
  CHECK(!shortcut_filename.empty(), base::NotFatalUntil::M162);

  bool deleted_from_desktop = DeleteShortcutOnDesktop(shortcut_filename);
  // Delete shortcuts from |kDirectoryFilename|.
  // Note that it is possible that shortcuts were not created in the Chrome Apps
  // directory. It doesn't matter: this will still delete the shortcut even if
  // it isn't in the directory.

  // Autostart items aren't fully supported with test_override, so don't fail if
  // it's populated.
  // TODO(https://crbug.com/332935566): Support this by saving the xdg calls.
  bool deleted_from_autostart = true;
  if (!test_override || GetInstalledLaunchXdgUtilityForTesting()) {
    deleted_from_autostart = DeleteShortcutInAutoStart(env, shortcut_filename);
  }

  if (test_override) {
    CHECK_IS_TEST();
    test_override->RegisterProtocolSchemes(extension_id,
                                           std::vector<std::string>());
  }

  bool initial_success = deleted_from_desktop && deleted_from_autostart;

  // `owned_env` is bound to keep `env` alive until the operation finishes.
  auto on_app_menu_deleted = base::BindOnce(
      [](std::unique_ptr<base::Environment> owned_env,
         base::FilePath profile_path, std::string extension_id,
         scoped_refptr<OsIntegrationTestOverride> test_override,
         bool initial_success, base::OnceCallback<void(bool)> callback,
         bool deleted_from_application_menu) {
        bool deleted_from_icon_storage = true;
        if (!test_override && !GetInstalledLaunchXdgUtilityForTesting()) {
          base::FilePath icon_filename =
              GetAppIconShortcutFilename(profile_path, extension_id);
          deleted_from_icon_storage = DeleteShortcutIcons(icon_filename);
        }
        std::move(callback).Run(initial_success &&
                                deleted_from_application_menu &&
                                deleted_from_icon_storage);
      },
      std::move(owned_env), profile_path, extension_id, test_override,
      initial_success, std::move(callback));

  // Shortcuts items aren't fully supported with test_override, so don't fail if
  // it's populated.
  // TODO(https://crbug.com/332935566): Support this by saving the xdg calls.
  if (!test_override || GetInstalledLaunchXdgUtilityForTesting()) {
    DeleteShortcutInApplicationsMenu(env, shortcut_filename,
                                     base::FilePath(kDirectoryFilename),
                                     std::move(on_app_menu_deleted));
  } else {
    std::move(on_app_menu_deleted).Run(/*deleted_from_application_menu=*/true);
  }
}

void DeleteAllDesktopShortcuts(const base::FilePath& profile_path,
                               base::OnceCallback<void(bool)> callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<base::Environment> owned_env = base::Environment::Create();
  base::Environment* env = owned_env.get();
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use it.
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  bool result = true;
  // Delete shortcuts from Desktop and collect all icon files that need to be
  // removed from the staging directory `~/.local/share/icons/hicolor/`.
  base::FilePath desktop_path = GetDesktopPath();
  std::vector<base::FilePath> icon_files_to_be_deleted;
  if (!desktop_path.empty()) {
    std::vector<base::FilePath> shortcut_filenames_desktop =
        shell_integration_linux::GetExistingProfileShortcutFilenames(
            profile_path, desktop_path);
    for (const auto& shortcut : shortcut_filenames_desktop) {
      if (!DeleteShortcutOnDesktop(shortcut)) {
        result = false;
      }
      icon_files_to_be_deleted.push_back(
          shortcut.RemoveExtension().AddExtension("png"));
    }
  }

  base::FilePath autostart_path = GetAutostartPath(env);
  std::vector<base::FilePath> shortcut_filenames_autostart =
      shell_integration_linux::GetExistingProfileShortcutFilenames(
          profile_path, autostart_path);
  for (const auto& shortcut : shortcut_filenames_autostart) {
    // Autostart items aren't fully supported with test_override, so don't fail
    // if it's populated.
    // TODO(https://crbug.com/332935566): Support this by saving the xdg calls.
    if (!test_override || GetInstalledLaunchXdgUtilityForTesting()) {
      result = DeleteShortcutInAutoStart(env, shortcut) && result;
    }
  }

  // Delete shortcuts from |kDirectoryFilename|.
  base::FilePath applications_menu = base::nix::GetXDGDataWriteLocation(env);
  applications_menu = applications_menu.AppendASCII("applications");
  std::vector<base::FilePath> shortcut_filenames_app_menu =
      shell_integration_linux::GetExistingProfileShortcutFilenames(
          profile_path, applications_menu);

  // `owned_env` is bound to keep `env` alive until the operation finishes.
  auto on_app_menu_deleted = base::BindOnce(
      [](std::unique_ptr<base::Environment> owned_env,
         std::vector<base::FilePath> icon_files_to_be_deleted,
         scoped_refptr<OsIntegrationTestOverride> test_override,
         bool initial_result, base::OnceCallback<void(bool)> callback,
         const std::vector<bool>& results) {
        bool overall =
            initial_result && std::all_of(results.begin(), results.end(),
                                          [](bool r) { return r; });
        // Delete the shortcut icons from the staging area.
        if (!test_override && !GetInstalledLaunchXdgUtilityForTesting()) {
          for (const auto& icon_files : icon_files_to_be_deleted) {
            overall = DeleteShortcutIcons(icon_files) && overall;
          }
        }
        std::move(callback).Run(overall);
      },
      std::move(owned_env), icon_files_to_be_deleted, test_override, result,
      std::move(callback));

  if (shortcut_filenames_app_menu.empty() ||
      (test_override && !GetInstalledLaunchXdgUtilityForTesting())) {
    std::move(on_app_menu_deleted).Run(/*results=*/{});
    return;
  }

  auto barrier_callback = base::BarrierCallback<bool>(
      shortcut_filenames_app_menu.size(), std::move(on_app_menu_deleted));

  for (const auto& menu : shortcut_filenames_app_menu) {
    // Shortcut items aren't fully supported with test_override, so don't fail
    // if it's populated.
    // TODO(https://crbug.com/332935566): Support this by saving the xdg calls.
    DeleteShortcutInApplicationsMenu(
        env, menu, base::FilePath(kDirectoryFilename), barrier_callback);
  }
}

void UpdateDesktopShortcuts(
    const ShortcutInfo& shortcut_info,
    std::optional<ShortcutLocations> user_specified_locations,
    base::OnceCallback<void(bool)> callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<base::Environment> owned_env = base::Environment::Create();
  base::Environment* env = owned_env.get();
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  // Find out whether shortcuts are already installed.
  ShortcutLocations existing_locations = GetExistingShortcutLocations(
      env, shortcut_info.profile_path, shortcut_info.app_id);
  ShortcutLocations creation_locations = existing_locations;

  // If an update is triggered due to 2 installs being scheduled on top of one
  // another, ensure that old user specified locations are still taken into
  // account during shortcut recreation.
  if (user_specified_locations.has_value()) {
    creation_locations =
        MergeLocations(user_specified_locations.value(), existing_locations);
  }

  // Always create a hidden shortcut in applications if a visible one is not
  // being created. This allows the operating system to identify the app, but
  // not show it in the menu.
  if (creation_locations.applications_menu_location == APP_MENU_LOCATION_NONE) {
    creation_locations.applications_menu_location = APP_MENU_LOCATION_HIDDEN;
  }

  CreateDesktopShortcut(shortcut_info, creation_locations, std::move(callback));
}

std::vector<base::FilePath> GetShortcutLocations(
    base::Environment* env,
    const ShortcutLocations& locations,
    const base::FilePath& profile_path,
    const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use it.
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    env = test_override->environment();
  }

  std::vector<base::FilePath> shortcut_locations;
  base::FilePath shortcut_filename =
      GetAppDesktopShortcutFilename(profile_path, app_id);
  CHECK(!shortcut_filename.empty(), base::NotFatalUntil::M162);

  if (locations.on_desktop) {
    base::FilePath desktop_path = GetDesktopPath();
    if (!desktop_path.empty()) {
      base::FilePath desktop_shortcut_path =
          desktop_path.Append(shortcut_filename);
      if (base::PathExists(desktop_shortcut_path)) {
        shortcut_locations.push_back(desktop_shortcut_path);
      }
    }
  }

  if (locations.in_startup) {
    base::FilePath autostart_path = GetAutostartPath(env);
    if (!autostart_path.empty()) {
      base::FilePath autostart_shortcut_path =
          autostart_path.Append(shortcut_filename);
      if (base::PathExists(autostart_shortcut_path)) {
        shortcut_locations.push_back(autostart_shortcut_path);
      }
    }
  }

  // Can't retrieve file name for applications menu location.
  CHECK(!locations.applications_menu_location, base::NotFatalUntil::M162);
  return shortcut_locations;
}

namespace internals {

void CreatePlatformShortcuts(const base::FilePath& /*web_app_path*/,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason /*creation_reason*/,
                             const ShortcutInfo& shortcut_info,
                             CreateShortcutsCallback callback) {
  CreateDesktopShortcut(shortcut_info, creation_locations, std::move(callback));
}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return GetExistingShortcutLocations(env.get(), shortcut_info.profile_path,
                                      shortcut_info.app_id);
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback) {
  DeleteDesktopShortcuts(
      shortcut_info.profile_path, shortcut_info.app_id,
      base::BindOnce(
          [](scoped_refptr<base::TaskRunner> result_runner,
             DeleteShortcutsCallback callback, bool success) {
            result_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), success));
          },
          result_runner, std::move(callback)));
}

void UpdatePlatformShortcuts(
    const base::FilePath& /*web_app_path*/,
    const std::u16string& /*old_app_title*/,
    std::optional<ShortcutLocations> user_specified_locations,
    ResultCallback callback,
    const ShortcutInfo& shortcut_info) {
  UpdateDesktopShortcuts(
      shortcut_info, user_specified_locations,
      base::BindOnce(
          [](ResultCallback callback, bool success) {
            std::move(callback).Run(success ? Result::kOk : Result::kError);
          },
          std::move(callback)));
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  DeleteAllDesktopShortcuts(profile_path, base::DoNothing());
}

std::vector<base::FilePath> GetShortcutLocations(
    const ShortcutLocations& locations,
    const base::FilePath& profile_path,
    const std::string& app_id) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return web_app::GetShortcutLocations(env.get(), locations, profile_path,
                                       app_id);
}

std::string StripNameAndIconFromDesktopFile(std::string_view contents) {
  return web_app::StripNameAndIconFromDesktopFile(contents);
}

std::string GetPortalDesktopFileId(base::Environment* env,
                                   const base::FilePath& shortcut_filename) {
  return web_app::GetPortalDesktopFileId(env, shortcut_filename);
}

void FinishPortalInstall(base::FilePath applications_dir,
                         base::OnceCallback<bool()> legacy_install,
                         base::OnceCallback<void(bool)> callback,
                         bool portal_success) {
  web_app::FinishPortalInstall(std::move(applications_dir),
                               std::move(legacy_install), std::move(callback),
                               portal_success);
}

}  // namespace internals

}  // namespace web_app

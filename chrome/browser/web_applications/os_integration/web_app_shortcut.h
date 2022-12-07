// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace base {
class TaskRunner;
}

namespace gfx {
class ImageSkia;
}

namespace web_app {

#if BUILDFLAG(IS_LINUX)
struct LinuxFileRegistration {
  std::string xdg_command;
  std::string file_contents;
};
#endif

// This class is used across multiple different sequenced task runners:
// - Created on the UI thread.
// - Accessed & sometimes modified by the shortcut task runner.
// - Accessed by the UI thread.
// It is up to the user to ensure thread safety of this struct through
// ordering guarantees.
struct ShortcutOverrideForTesting
    : public base::RefCountedThreadSafe<ShortcutOverrideForTesting> {
 public:
  // Destroying this class blocks the thread until all users of
  // GetShortcutOverrideForTesting() have completed.
  struct BlockingRegistration {
    BlockingRegistration();
    ~BlockingRegistration();

    scoped_refptr<ShortcutOverrideForTesting> shortcut_override;
  };

  ShortcutOverrideForTesting(const ShortcutOverrideForTesting&) = delete;

  // Overrides applicable directories for shortcut integration and returns an
  // object that:
  // 1) Contains the directories.
  // 2) Keeps the override active until the object is destroyed.
  // 3) DCHECK-fails on destruction if any of the shortcut directories / os
  //    hooks are NOT cleanup by the test. This ensures that trybots don't have
  //    old test artifacts on them that can make future tests flaky.
  // All installs that occur during the lifetime of the
  // ShortcutOverrideForTesting MUST be uninstalled before it is
  // destroyed.
  // The returned value, on destruction, will block until all usages of the
  // GetShortcutOverrideForTesting() are destroyed.
  static std::unique_ptr<BlockingRegistration> OverrideForTesting(
      const base::FilePath& base_path = base::FilePath());

#if BUILDFLAG(IS_WIN)
  base::ScopedTempDir desktop;
  base::ScopedTempDir application_menu;
  base::ScopedTempDir quick_launch;
  base::ScopedTempDir startup;
#elif BUILDFLAG(IS_MAC)
  base::ScopedTempDir chrome_apps_folder;
  std::map<base::FilePath, bool> startup_enabled;
#elif BUILDFLAG(IS_LINUX)
  base::ScopedTempDir desktop;
  base::ScopedTempDir startup;
  std::vector<LinuxFileRegistration> linux_file_registration;
  // std::string xdg_mime_install_cmd;
  // std::string mime_types_file_contents;
#endif

  // Records all registration events for a given app id & protocol list. Due to
  // simplification on the OS-side, unregistrations are not recorded, and
  // instead this list can be checked for an empty registration.
  std::vector<std::tuple<AppId, std::vector<std::string>>>
      protocol_scheme_registrations;

 private:
  friend class base::RefCountedThreadSafe<ShortcutOverrideForTesting>;

  explicit ShortcutOverrideForTesting(const base::FilePath& base_path);
  ~ShortcutOverrideForTesting();

  // |on_destruction| has it's closure set only once (when BlockingRegistration
  // is destroyed) and executed when ShortcutOverrideForTesting is destroyed.
  // The destructor of BlockingRegistration explicitly sets this closure with a
  // global lock, then destroys the object, then waits on the closure, so it is
  // thread-compatible.
  base::ScopedClosureRunner on_destruction;
};

// Returns an active shortcut override for testing, if there is one.
scoped_refptr<ShortcutOverrideForTesting> GetShortcutOverrideForTesting();

// Represents the info required to create a shortcut for an app.
struct ShortcutInfo {
  ShortcutInfo();
  ShortcutInfo(const ShortcutInfo&) = delete;
  ShortcutInfo& operator=(const ShortcutInfo&) = delete;
  ~ShortcutInfo();

  GURL url;
  // If |extension_id| is non-empty, this is short cut is to an extension-app
  // and the launch url will be detected at start-up. In this case, |url|
  // is still used to generate the app id (windows app id, not chrome app id).
  // TODO(loyso): Rename it to app_id.
  std::string extension_id;
  std::u16string title;
  std::u16string description;
  gfx::ImageFamily favicon;
  base::FilePath profile_path;
  std::string profile_name;
  std::string version_for_display;
  std::set<std::string> file_handler_extensions;
  std::set<std::string> file_handler_mime_types;
  std::set<std::string> protocol_handlers;
#if BUILDFLAG(IS_LINUX)
  std::set<DesktopActionInfo> actions;
#endif  // BUILDFLAG(IS_LINUX)

  // An app is multi-profile if there is a single shortcut and single app shim
  // for all profiles. The app itself has a profile switcher that may be used
  // to open windows for the various profiles. This is relevant only on macOS.
  bool is_multi_profile = false;

#if BUILDFLAG(IS_MAC)
  // On Mac OS creating shortcuts also needs some of this information for other
  // profiles if this is a multi profile app.
  using HandlerInfo = AppShimRegistry::HandlerInfo;
  std::map<base::FilePath, HandlerInfo> handlers_per_profile;
#endif

 private:
  // Since gfx::ImageFamily |favicon| has a non-thread-safe reference count in
  // its member and is bound to current thread, always destroy ShortcutInfo
  // instance on the same thread.
  SEQUENCE_CHECKER(sequence_checker_);
};

// This specifies a folder in the system applications menu (e.g the Start Menu
// on Windows).
//
// These represent the applications menu root, the "Google Chrome" folder and
// the "Chrome Apps" folder respectively.
//
// APP_MENU_LOCATION_HIDDEN specifies a shortcut that is used to register the
// app with the OS (in order to give its windows shelf icons, and correct icons
// and titles), but the app should not show up in menus or search results.
//
// NB: On Linux, these locations may not be used by the window manager (e.g
// Unity and Gnome Shell).
enum ApplicationsMenuLocation {
  APP_MENU_LOCATION_NONE,
  APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
  APP_MENU_LOCATION_HIDDEN,
};

// Info about which locations to create app shortcuts in.
struct ShortcutLocations {
  bool on_desktop = false;

  ApplicationsMenuLocation applications_menu_location = APP_MENU_LOCATION_NONE;

  // For Windows, this refers to quick launch bar prior to Win7. In Win7,
  // this means "pin to taskbar". For Mac/Linux, this could be used for
  // Mac dock or the gnome/kde application launcher. However, those are not
  // implemented yet.
  bool in_quick_launch_bar = false;

  // For Windows, this refers to the Startup folder.
  // For Mac, this refers to the Login Items list.
  // For Linux, this refers to the autostart folder.
  bool in_startup = false;
};

// This encodes the cause of shortcut creation as the correct behavior in each
// case is implementation specific.
enum ShortcutCreationReason {
  SHORTCUT_CREATION_BY_USER,
  SHORTCUT_CREATION_AUTOMATED,
};

// Compute a deterministic name based on data in the shortcut_info.
std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info);

// Returns a per-app directory for OS-specific web app data to handle OS
// registration and unregistration. To store manifest resources, use
// GetManifestResourcesDirectoryForApp() declared in web_app_utils.h.
//
// The path for the directory is based on |app_id|. If |app_id| is empty then
// |url| is used to construct a unique ID.
base::FilePath GetOsIntegrationResourcesDirectoryForApp(
    const base::FilePath& profile_path,
    const std::string& app_id,
    const GURL& url);

// Callback made when CreateShortcuts has finished trying to create the
// platform shortcuts indicating whether or not they were successfully
// created.
using CreateShortcutsCallback = base::OnceCallback<void(bool shortcut_created)>;
// Callback made when DeletePlatformShortcuts has finished trying to delete the
// platform shortcuts indicating whether or not they were successfully
// deleted.
using DeleteShortcutsCallback = base::OnceCallback<void(bool shortcut_deleted)>;

// Returns an array of desired icon sizes (in px) to be contained in an app OS
// shortcut, sorted in ascending order (biggest desired icon size is last).
base::span<const int> GetDesiredIconSizesForShortcut();

// Load the standard application icon from resources.
gfx::ImageSkia CreateDefaultApplicationIcon(int size);

namespace internals {

// Implemented for each platform, does the platform specific parts of creating
// shortcuts. Used internally by CreateShortcuts methods.
// |shortcut_data_path| is where to store any resources created for the
// shortcut, and is also used as the UserDataDir for platform app shortcuts.
// |shortcut_info| contains info about the shortcut to create, and
// |creation_locations| contains information about where to create them.
// Performs blocking IO operations.
bool CreatePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info);

// Implemented for each platform, does the platform specific parts of checking
// desktop and application menu to get shortcut locations.
ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info);

// Schedules a call to |CreatePlatformShortcuts| on the Shortcut IO thread and
// invokes |callback| when complete. This function must be called from the UI
// thread.
void ScheduleCreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason reason,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    CreateShortcutsCallback callback);

void ScheduleDeletePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    DeleteShortcutsCallback callback);

void ScheduleDeleteMultiProfileShortcutsForApp(const std::string& app_id,
                                               ResultCallback callback);

// Delete all the shortcuts we have added for this extension, returning the
// result via `callback` posted to `result_runner`. This is the platform
// specific implementation of the DeleteAllShortcuts function, and is executed
// on the FILE thread.
void DeletePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback);

// Delete the multi-profile (non-profile_scoped) shortcuts for the specified
// app. This is the multi-profile complement of DeletePlatformShortcuts.
void DeleteMultiProfileShortcutsForApp(const std::string& app_id);

// Updates all the shortcuts we have added for this extension. This is the
// platform specific implementation of the UpdateAllShortcuts function, and
// is executed on the FILE thread. On Windows, this also updates shortcuts in
// the pinned taskbar directories.
// Returns true if update was performed successfully and false otherwise
Result UpdatePlatformShortcuts(const base::FilePath& shortcut_data_path,
                               const std::u16string& old_app_title,
                               const ShortcutInfo& shortcut_info);

// Run an IO task on a worker thread. Ownership of |shortcut_info| transfers
// to a closure that deletes it on the UI thread when the task is complete.
// Tasks posted here run with BEST_EFFORT priority and block shutdown.
void PostShortcutIOTask(base::OnceCallback<void(const ShortcutInfo&)> task,
                        std::unique_ptr<ShortcutInfo> shortcut_info);
void PostShortcutIOTaskAndReplyWithResult(
    base::OnceCallback<Result(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback reply);

// The task runner for running shortcut tasks. On Windows this will be a task
// runner that permits access to COM libraries. Shortcut tasks typically deal
// with ensuring Profile changes are reflected on disk, so shutdown is always
// blocked so that an inconsistent shortcut state is not left on disk.
scoped_refptr<base::TaskRunner> GetShortcutIOTaskRunner();

base::FilePath GetShortcutDataDir(const ShortcutInfo& shortcut_info);

// Delete all the shortcuts for an entire profile.
// This is executed on the FILE thread.
void DeleteAllShortcutsForProfile(const base::FilePath& profile_path);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_H_

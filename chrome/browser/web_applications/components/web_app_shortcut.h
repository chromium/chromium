// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace base {
class TaskRunner;
}

namespace web_app {

// Represents the info required to create a shortcut for an app.
struct ShortcutInfo {
  ShortcutInfo();
  ~ShortcutInfo();

  GURL url;
  // If |extension_id| is non-empty, this is short cut is to an extension-app
  // and the launch url will be detected at start-up. In this case, |url|
  // is still used to generate the app id (windows app id, not chrome app id).
  // TODO(loyso): Rename it to app_id.
  std::string extension_id;
  base::string16 title;
  base::string16 description;
  gfx::ImageFamily favicon;
  base::FilePath profile_path;
  std::string profile_name;
  std::string version_for_display;
  std::vector<std::string> mime_types;

 private:
  // Since gfx::ImageFamily |favicon| has a non-thread-safe reference count in
  // its member and is bound to current thread, always destroy ShortcutInfo
  // instance on the same thread.
  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(ShortcutInfo);
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
  ShortcutLocations();

  bool on_desktop;

  ApplicationsMenuLocation applications_menu_location;

  // For Windows, this refers to quick launch bar prior to Win7. In Win7,
  // this means "pin to taskbar". For Mac/Linux, this could be used for
  // Mac dock or the gnome/kde application launcher. However, those are not
  // implemented yet.
  bool in_quick_launch_bar;
};

// This encodes the cause of shortcut creation as the correct behavior in each
// case is implementation specific.
enum ShortcutCreationReason {
  SHORTCUT_CREATION_BY_USER,
  SHORTCUT_CREATION_AUTOMATED,
};

// Compute a deterministic name based on data in the shortcut_info.
std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info);

// Gets the user data directory for given web app. The path for the directory is
// based on |extension_id|. If |extension_id| is empty then |url| is used
// to construct a unique ID.
base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const std::string& extension_id,
                                      const GURL& url);

// Callback made when CreateShortcuts has finished trying to create the
// platform shortcuts indicating whether or not they were successfully
// created.
using CreateShortcutsCallback = base::OnceCallback<void(bool shortcut_created)>;

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

// Schedules a call to |CreatePlatformShortcuts| on the Shortcut IO thread and
// invokes |callback| when complete. This function must be called from the UI
// thread.
void ScheduleCreatePlatformShortcuts(
    const base::FilePath& shortcut_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason reason,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    CreateShortcutsCallback callback);

// Delete all the shortcuts we have added for this extension. This is the
// platform specific implementation of the DeleteAllShortcuts function, and
// is executed on the FILE thread.
void DeletePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const ShortcutInfo& shortcut_info);

// Updates all the shortcuts we have added for this extension. This is the
// platform specific implementation of the UpdateAllShortcuts function, and
// is executed on the FILE thread.
void UpdatePlatformShortcuts(const base::FilePath& shortcut_data_path,
                             const base::string16& old_app_title,
                             const ShortcutInfo& shortcut_info);

// Run an IO task on a worker thread. Ownership of |shortcut_info| transfers
// to a closure that deletes it on the UI thread when the task is complete.
// Tasks posted here run with BEST_EFFORT priority and block shutdown.
void PostShortcutIOTask(base::OnceCallback<void(const ShortcutInfo&)> task,
                        std::unique_ptr<ShortcutInfo> shortcut_info);
void PostShortcutIOTaskAndReply(
    base::OnceCallback<void(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    base::OnceClosure reply);

// The task runner for running shortcut tasks. On Windows this will be a task
// runner that permits access to COM libraries. Shortcut tasks typically deal
// with ensuring Profile changes are reflected on disk, so shutdown is always
// blocked so that an inconsistent shortcut state is not left on disk.
scoped_refptr<base::TaskRunner> GetShortcutIOTaskRunner();

// Sanitizes |name| and returns a version of it that is safe to use as an
// on-disk file name .
base::FilePath GetSanitizedFileName(const base::string16& name);

base::FilePath GetShortcutDataDir(const ShortcutInfo& shortcut_info);

// Delete all the shortcuts for an entire profile.
// This is executed on the FILE thread.
void DeleteAllShortcutsForProfile(const base::FilePath& profile_path);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_H_

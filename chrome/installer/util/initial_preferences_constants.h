// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the constants used to process initial_preferences files
// used by setup and first run.

#ifndef CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_CONSTANTS_H_

namespace installer {
namespace initial_preferences {
// All the preferences below are expected to be inside the JSON "distribution"
// block. Some of them also have equivalent command line option. If same option
// is specified in initial preference as well as command line, the command line
// value takes precedence.

// Boolean pref that disables all logging.
inline constexpr char kDisableLogging[] = "disable_logging";
// Name of the dictionary that holds the distribution values.
inline constexpr char kDistroDict[] = "distribution";
// String pref that triggers silent import of bookmarks from the html file at
// given path.
inline constexpr char kDistroImportBookmarksFromFilePref[] =
    "import_bookmarks_from_file";
// String of Chrome version for which the "set as default browser" infobar will
// never be shown.
inline constexpr char kDistroSuppressDefaultBrowserPromptPref[] =
    "suppress_default_browser_prompt_for_version";
// Boolean. Prevent creation of all shortcuts to chrome, including the
// desktop, quick launch, taskbar and the start menu shortcuts.
inline constexpr char kDoNotCreateAnyShortcuts[] =
    "do_not_create_any_shortcuts";
// Boolean. Prevent creation of the Desktop shortcut on install (and later on
// Active Setup for each user on a system-level install).
inline constexpr char kDoNotCreateDesktopShortcut[] =
    "do_not_create_desktop_shortcut";
// Boolean. Prevent creation of the Quick Launch shortcut on install (and later
// on Active Setup for each user on a system-level install).
inline constexpr char kDoNotCreateQuickLaunchShortcut[] =
    "do_not_create_quick_launch_shortcut";
// Boolean. Prevent creation of the Taskbar (since Windows 7) shortcut on
// install (and later on Active Setup for each user on a system-level install).
inline constexpr char kDoNotCreateTaskbarShortcut[] =
    "do_not_create_taskbar_shortcut";
// Boolean. Do not launch Chrome after first install. Cmd line override present.
inline constexpr char kDoNotLaunchChrome[] = "do_not_launch_chrome";
// Boolean. Do not register with Google Update to have Chrome launched after
// install. Cmd line override present.
inline constexpr char kDoNotRegisterForUpdateLaunch[] =
    "do_not_register_for_update_launch";
// String.  Specifies the file path to write logging info to.
inline constexpr char kLogFile[] = "log_file";
// Boolean. Register Chrome as default browser for the current user.
inline constexpr char kMakeChromeDefaultForUser[] =
    "make_chrome_default_for_user";
// Boolean. Expect to be run by an MSI installer. Cmd line override present.
inline constexpr char kMsi[] = "msi";
// String. The MSI Product ID under which the MSI stores its information.  This
// is used to update the DisplayVersion to match Chrome's version number.
inline constexpr char kMsiProductId[] = "msi_product_id";
// Installs Chrome in the location specified by the msi PROGRAMFILESDIR custom
// property. The only accepted values are the values mapped to
// CSIDL_PROGRAM_FILESX86 and CSIDL_PROGRAM_FILES. This property will only be
// applied on fresh system installs.
inline constexpr char kProgramFilesDir[] = "program_files_dir";
// Boolean. Show EULA dialog before install.
inline constexpr char kRequireEula[] = "require_eula";
// Boolean. Install Chrome to system wise location. Cmd line override present.
inline constexpr char kSystemLevel[] = "system_level";
// Boolean. Run installer in verbose mode. Cmd line override present.
inline constexpr char kVerboseLogging[] = "verbose_logging";
// Name of the block that contains the extensions on the initial preferences.
inline constexpr char kExtensionsBlock[] = "extensions.settings";
// A preference that indicates the name of the provider of the initial
// extensions list.
inline constexpr char kInitialExtensionsProviderName[] =
    "initial_extensions.provider_name";
// Name of the block that contains the extensions ids in the initial
// preferences.
inline constexpr char kInitialExtensionsList[] = "initial_extensions.list";
// Name of the block that contains the bookmarks on the initial preferences.
inline constexpr char kBookmarksBlock[] = "bookmarks";
// Boolean. Allow Chrome to be downgraded to a previous version if true.
inline constexpr char kAllowDowngrade[] = "allow_downgrade";

}  // namespace initial_preferences
}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INITIAL_PREFERENCES_CONSTANTS_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_window_property_manager_win.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"

using extensions::ExtensionRegistry;

BrowserWindowPropertyManager::BrowserWindowPropertyManager(
    const BrowserView* view,
    HWND hwnd)
    : view_(view), hwnd_(hwnd) {
  // At this point, the HWND is unavailable from BrowserView.
  DCHECK(hwnd);
  profile_pref_registrar_.Init(view_->browser()->profile()->GetPrefs());

  // Monitor the profile icon version on Windows so that we can set the browser
  // relaunch icon when the version changes (e.g on initial icon creation).
  profile_pref_registrar_.Add(
      prefs::kProfileIconVersion,
      base::BindRepeating(
          &BrowserWindowPropertyManager::OnProfileIconVersionChange,
          base::Unretained(this)));
}

BrowserWindowPropertyManager::~BrowserWindowPropertyManager() {
  ui::win::ClearWindowPropertyStore(hwnd_);
}

void BrowserWindowPropertyManager::UpdateWindowProperties() {
  const Browser* browser = view_->browser();
  Profile* profile = browser->profile();

  // Set the app user model id for this application to that of the application
  // name. See http://crbug.com/7028.
  std::wstring app_id =
      browser->is_type_app() || browser->is_type_app_popup() ||
              browser->is_type_devtools()
          ? shell_integration::win::GetAppUserModelIdForApp(
                base::UTF8ToWide(browser->app_name()), profile->GetPath())
          : shell_integration::win::GetAppUserModelIdForBrowser(
                profile->GetPath());
  // Apps set their relaunch details based on app's details.
  if (browser->is_type_app() || browser->is_type_app_popup()) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
    const extensions::Extension* extension = registry->GetExtensionById(
        web_app::GetAppIdFromApplicationName(browser->app_name()),
        ExtensionRegistry::EVERYTHING);
    if (extension) {
      ui::win::SetAppIdForWindow(app_id, hwnd_);
      web_app::UpdateRelaunchDetailsForApp(profile, extension, hwnd_);
      return;
    }
  }

  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  ProfileShortcutManager* const shortcut_manager =
      profile_manager ? profile_manager->profile_shortcut_manager() : nullptr;
  // The profile manager may be null in testing.

  base::FilePath icon_path;
  std::wstring command_line_string;
  std::wstring pinned_name;
  if ((browser->is_type_normal() || browser->is_type_popup()) &&
      shortcut_manager &&
      profile->GetPrefs()->HasPrefPath(prefs::kProfileIconVersion)) {
    // Set relaunch details to use profile.
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    shortcut_manager->GetShortcutProperties(profile->GetPath(), &command_line,
                                            &pinned_name, &icon_path);
    command_line_string = command_line.GetCommandLineString();
  }
  ui::win::SetAppDetailsForWindow(app_id, icon_path, 0, command_line_string,
                                  pinned_name, hwnd_);
}

// static
std::unique_ptr<BrowserWindowPropertyManager>
BrowserWindowPropertyManager::CreateBrowserWindowPropertyManager(
    const BrowserView* view,
    HWND hwnd) {
  std::unique_ptr<BrowserWindowPropertyManager> browser_window_property_manager(
      new BrowserWindowPropertyManager(view, hwnd));
  browser_window_property_manager->UpdateWindowProperties();
  return browser_window_property_manager;
}

void BrowserWindowPropertyManager::OnProfileIconVersionChange() {
  UpdateWindowProperties();
}

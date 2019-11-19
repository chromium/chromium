// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>
#include <propkey.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_propvariant.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/views/win/hwnd_util.h"

typedef extensions::ExtensionBrowserTest BrowserWindowPropertyManagerTest;

namespace {

std::wstring AddIdToIconPath(const std::wstring& path) {
  return path + L",0";
}

// Checks that the relaunch name, relaunch command and app icon for the given
// |browser| are correct.
void ValidateBrowserWindowProperties(
    const Browser* browser,
    const base::string16& expected_profile_name) {
  // Let shortcut creation finish before we validate the results.
  content::RunAllTasksUntilIdle();

  HWND hwnd = views::HWNDForNativeWindow(browser->window()->GetNativeWindow());

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  HRESULT result = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps));
  EXPECT_TRUE(SUCCEEDED(result));

  base::win::ScopedPropVariant prop_var;
  // The relaunch name should be of the form "Chromium" if there is only 1
  // profile and "First User - Chromium" if there are more. The expected value
  // is given by |expected_profile_name|.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchDisplayNameResource,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(base::FilePath(profiles::internal::GetShortcutFilenameForProfile(
                               expected_profile_name))
                .RemoveExtension()
                .value(),
            prop_var.get().pwszVal);
  prop_var.Reset();

  // The relaunch command should specify the profile.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchCommand,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  base::CommandLine cmd_line(
      base::CommandLine::FromString(prop_var.get().pwszVal));
  EXPECT_EQ(browser->profile()->GetPath().BaseName().value(),
            cmd_line.GetSwitchValueNative(switches::kProfileDirectory));
  prop_var.Reset();

  // The app icon should be set to the profile icon.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchIconResource,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(AddIdToIconPath(profiles::internal::GetProfileIconPath(
                                browser->profile()->GetPath())
                                .value()),
            prop_var.get().pwszVal);
  prop_var.Reset();
}

void ValidateHostedAppWindowProperties(const Browser* browser,
                                       const extensions::Extension* extension) {
  content::RunAllTasksUntilIdle();

  HWND hwnd = views::HWNDForNativeWindow(browser->window()->GetNativeWindow());

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  HRESULT result = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps));
  EXPECT_TRUE(SUCCEEDED(result));

  base::win::ScopedPropVariant prop_var;
  // The relaunch name should be the extension name.
  EXPECT_EQ(S_OK,
            pps->GetValue(PKEY_AppUserModel_RelaunchDisplayNameResource,
                          prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(base::UTF8ToWide(extension->name()), prop_var.get().pwszVal);
  prop_var.Reset();

  // The relaunch command should specify the profile and the app id.
  EXPECT_EQ(
      S_OK,
      pps->GetValue(PKEY_AppUserModel_RelaunchCommand, prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  base::CommandLine cmd_line(
      base::CommandLine::FromString(prop_var.get().pwszVal));
  EXPECT_EQ(browser->profile()->GetPath().BaseName().value(),
            cmd_line.GetSwitchValueNative(switches::kProfileDirectory));
  EXPECT_EQ(base::UTF8ToWide(extension->id()),
            cmd_line.GetSwitchValueNative(switches::kAppId));
  prop_var.Reset();

  // The app icon should be set to the extension app icon.
  base::FilePath web_app_dir = web_app::GetWebAppDataDirectory(
      browser->profile()->GetPath(), extension->id(), GURL());
  EXPECT_EQ(S_OK,
            pps->GetValue(PKEY_AppUserModel_RelaunchIconResource,
                          prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(
      AddIdToIconPath(web_app::internals::GetIconFilePath(
                          web_app_dir, base::UTF8ToUTF16(extension->name()))
                          .value()),
      prop_var.get().pwszVal);
  prop_var.Reset();
}

}  // namespace

// Tests that require the profile shortcut manager to be instantiated despite
// having --user-data-dir specified.
class BrowserTestWithProfileShortcutManager : public InProcessBrowserTest {
 public:
  BrowserTestWithProfileShortcutManager() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableProfileShortcutManager);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestWithProfileShortcutManager);
};

// Check that the window properties on Windows are properly set.
IN_PROC_BROWSER_TEST_F(BrowserTestWithProfileShortcutManager,
                       DISABLED_WindowProperties) {
  // Single profile case. The profile name should not be shown.
  ValidateBrowserWindowProperties(browser(), base::string16());

  // If multiprofile mode is not enabled, we can't test the behavior when there
  // are multiple profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // Two profile case. Both profile names should be shown.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  profile_manager->CreateProfileAsync(path_profile2,
                                      ProfileManager::CreateCallback(),
                                      base::string16(), std::string());
  // The default profile's name should be part of the relaunch name.
  ValidateBrowserWindowProperties(
      browser(), base::UTF8ToUTF16(browser()->profile()->GetProfileUserName()));

  // The second profile's name should be part of the relaunch name.
  Browser* profile2_browser =
      CreateBrowser(profile_manager->GetProfileByPath(path_profile2));
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager->GetProfileAttributesStorage().
              GetProfileAttributesWithPath(path_profile2, &entry));
  ValidateBrowserWindowProperties(profile2_browser, entry->GetName());
}

IN_PROC_BROWSER_TEST_F(BrowserWindowPropertyManagerTest, DISABLED_HostedApp) {
  // Load an app.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app/"));
  EXPECT_TRUE(extension);

  apps::LaunchService::Get(browser()->profile())
      ->OpenApplication(apps::AppLaunchParams(
          extension->id(), apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          apps::mojom::AppLaunchSource::kSourceTest));

  // Check that the new browser has an app name.
  // The launch should have created a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* app_browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser())
      app_browser = b;
  }
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser != browser());

  ValidateHostedAppWindowProperties(app_browser, extension);
}

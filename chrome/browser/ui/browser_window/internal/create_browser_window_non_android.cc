// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

BrowserWindowInterface* CreateAppBrowserWindow(
    BrowserWindowCreateParams create_params) {
  CHECK(create_params.type == BrowserWindowInterface::TYPE_APP ||
        create_params.type == BrowserWindowInterface::TYPE_APP_POPUP)
      << "Unexpected browser type with `app_name`: "
      << static_cast<int>(create_params.type);
  Browser::CreateParams browser_params =
      create_params.type == BrowserWindowInterface::TYPE_APP
          ? Browser::CreateParams::CreateForApp(
                create_params.app_name, create_params.is_trusted_source,
                create_params.initial_bounds, &*create_params.profile,
                create_params.from_user_gesture)
          : Browser::CreateParams::CreateForAppPopup(
                create_params.app_name, create_params.is_trusted_source,
                create_params.initial_bounds, &*create_params.profile,
                create_params.from_user_gesture);

  browser_params.initial_show_state = create_params.initial_show_state;

  return Browser::Create(browser_params);
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsOnKioskSplashScreen() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (!session_manager) {
    return false;
  }
  // We have to check this way because of CHECK() in UserManager::Get().
  if (!user_manager::UserManager::IsInitialized()) {
    return false;
  }
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }
  if (session_manager->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return false;
  }
  return true;
}
#endif

}  // namespace

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  CHECK_EQ(BrowserWindowInterface::CreationStatus::kOk,
           GetBrowserWindowCreationStatusForProfile(*create_params.profile));

  if (!create_params.app_name.empty()) {
    return CreateAppBrowserWindow(std::move(create_params));
  }

  Browser::CreateParams browser_params(create_params.type,
                                       &*create_params.profile,
                                       create_params.from_user_gesture);
  browser_params.trusted_source = create_params.is_trusted_source;
  browser_params.initial_bounds = std::move(create_params.initial_bounds);
  browser_params.initial_show_state = create_params.initial_show_state;

  return Browser::Create(browser_params);
}

void CreateBrowserWindow(
    BrowserWindowCreateParams create_params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  auto* browser_window = CreateBrowserWindow(std::move(create_params));

  // Although browser window creation is synchronous on non-Android platforms,
  // we still invoke the callback asynchronously, but on the same thread as the
  // caller, to maintain the asynchronous behavior across all platforms.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), browser_window));
}

BrowserWindowInterface::CreationStatus GetBrowserWindowCreationStatusForProfile(
    Profile& profile) {
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    return BrowserWindowInterface::CreationStatus::kErrorShuttingDown;
  }

  if (!IncognitoModePrefs::CanOpenBrowser(&profile) ||
      !profile.AllowsBrowserWindows() ||
      IsProfileDirectoryMarkedForDeletion(profile.GetPath())) {
    return BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (IsOnKioskSplashScreen()) {
    return BrowserWindowInterface::CreationStatus::kErrorLoadingKiosk;
  }
#endif

  return BrowserWindowInterface::CreationStatus::kOk;
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#endif

BrowserWindowCreateParams BrowserWindowCreateParams::Clone() const {
  BrowserWindowCreateParams clone(type, *profile, from_user_gesture);
  clone.initial_bounds = initial_bounds;
  clone.is_trusted_source = is_trusted_source;
  clone.app_name = app_name;
  clone.initial_show_state = initial_show_state;
  clone.omit_from_session_restore = omit_from_session_restore;
  clone.should_trigger_session_restore = should_trigger_session_restore;
  clone.initial_origin_specified = initial_origin_specified;
  clone.initial_workspace = initial_workspace;
  clone.initial_visible_on_all_workspaces_state =
      initial_visible_on_all_workspaces_state;
  clone.creation_source = creation_source;
  clone.in_tab_dragging = in_tab_dragging;
  clone.window = window;
  clone.user_title = user_title;
  clone.can_resize = can_resize;
  clone.can_maximize = can_maximize;
  clone.can_fullscreen = can_fullscreen;
  clone.pip_options = pip_options;
  clone.vertical_tab_strip_collapsed = vertical_tab_strip_collapsed;
  clone.vertical_tab_strip_uncollapsed_width =
      vertical_tab_strip_uncollapsed_width;
  clone.focused_tab_group_id = focused_tab_group_id;
#if BUILDFLAG(IS_CHROMEOS)
  clone.display_id = display_id;
#endif
#if BUILDFLAG(IS_LINUX)
  clone.startup_id = startup_id;
#endif
#if BUILDFLAG(IS_OZONE)
  clone.restore_id = restore_id;
#endif
  return clone;
}

// static
BrowserWindowCreateParams BrowserWindowCreateParams::CreateForApp(
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    bool user_gesture) {
  DCHECK(!app_name.empty());
  BrowserWindowCreateParams params(
      BrowserWindowInterface::TYPE_APP, profile, user_gesture);
  params.app_name = app_name;
  params.is_trusted_source = trusted_source;
  params.initial_bounds = window_bounds;
  return params;
}

// static
BrowserWindowCreateParams BrowserWindowCreateParams::CreateForAppPopup(
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    bool user_gesture) {
  DCHECK(!app_name.empty());
  BrowserWindowCreateParams params(
      BrowserWindowInterface::TYPE_APP_POPUP, profile, user_gesture);
  params.app_name = app_name;
  params.is_trusted_source = trusted_source;
  params.initial_bounds = window_bounds;
  return params;
}

// static
BrowserWindowCreateParams BrowserWindowCreateParams::CreateForPictureInPicture(
    const std::string& app_name,
    bool trusted_source,
    Profile* profile,
    bool user_gesture) {
  BrowserWindowCreateParams params(
      BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE, profile, user_gesture);
  params.app_name = app_name;
  params.is_trusted_source = trusted_source;
  return params;
}

// static
BrowserWindowCreateParams BrowserWindowCreateParams::CreateForDevTools(
    Profile* profile) {
  BrowserWindowCreateParams params(
      BrowserWindowInterface::TYPE_DEVTOOLS, profile, true);
  params.app_name = DevToolsWindow::kDevToolsApp;
  params.is_trusted_source = true;
  return params;
}

namespace {

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

  return Browser::Create(std::move(create_params));
}

std::unique_ptr<Browser> DeprecatedCreateOwnedBrowserWindowForTesting(
    BrowserWindowCreateParams create_params) {
  return Browser::DeprecatedCreateOwnedForTesting(
      std::move(create_params));  // IN-TEST
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

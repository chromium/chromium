// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_util_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace fullscreen_utils {

bool IsInContentFullscreen(
    const BrowserWindowInterface* browser_window_interface) {
  // Const cast because ExclusiveAccessManager and its accessors are not
  // const-correct.
  auto* const manager =
      const_cast<BrowserWindowInterface*>(browser_window_interface)
          ->GetExclusiveAccessManager();
  if (!manager) {
    return false;
  }
  FullscreenController* const controller = manager->fullscreen_controller();
  return controller && (controller->IsWindowFullscreenForTabOrPending() ||
                        controller->IsExtensionFullscreenOrPending());
}

bool IsAlwaysShowToolbarEnabled(const Browser* browser) {
  if (web_app::AppBrowserController::IsWebApp(browser)) {
    const web_app::AppBrowserController* controller = browser->app_controller();
    return controller->AlwaysShowToolbarInFullscreen();
  }
  return browser->profile()->GetPrefs()->GetBoolean(
      prefs::kShowFullscreenToolbar);
}

}  // namespace fullscreen_utils

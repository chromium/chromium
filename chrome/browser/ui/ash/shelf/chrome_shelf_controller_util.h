// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_UTIL_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_UTIL_H_

#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"

class AccountId;

namespace ash {
class BrowserDelegate;
class ShelfModel;
}

namespace extensions {
class Extension;
}

// Returns the extension identified by |app_id|.
const extensions::Extension* GetExtensionForAppID(const std::string& app_id,
                                                  Profile* profile);

// Returns whether the app can be pinned, and whether the pinned app are
// editable or fixed
AppListControllerDelegate::Pinnable GetPinnableForAppID(
    const std::string& app_id,
    Profile* profile);

// Helper function to return whether the app with `app_id` should explicitly
// be hidden from shelf, as indicated by `AppUpdate::ShowInShelf()` app state.
bool IsAppHiddenFromShelf(Profile* profile, const std::string& app_id);

// Helper function to return whether the promise app with `promise_package_id`
// should be visible in the shelf by reading the should_show field of the
// promise app in the PromiseAppRegistryCache.
bool IsPromiseAppReadyToShowInShelf(Profile* profile,
                                    const std::string& promise_package_id);

// Whether the pin state of the app with `app_id` is editable according to its
// `app_type`.
bool IsAppPinEditable(apps::AppType app_type,
                      const std::string& app_id,
                      Profile* profile);

// Returns true when the given |browser| is listed in the browser
// application list.
bool IsBrowserRepresentedInBrowserList(const ash::BrowserDelegate* browser,
                                       const ash::ShelfModel* model);

void ShowAndActivateBrowser(bool move_to_current_desktop,
                            ash::BrowserDelegate* browser);

// Pins an app to the shelf using only an app_id.
// If the app is already present in the shelf and is unpinned, mark it as
// pinned.
// If the app is already present in the shelf and is pinned, do nothing.
// If the app is not in the shelf, use ChromeShelfItemFactory to create a
// ShelfItem and ShelfItemDelegate, add it to the shelf, and mark it as pinned.
// If the app_id cannot be converted, does nothing.
void PinAppWithIDToShelf(const std::string& app_id);

// Unpins an app from the shelf, if it is in the shelf. Otherwise does nothing.
void UnpinAppWithIDFromShelf(const std::string& app_id);

apps::LaunchSource ShelfLaunchSourceToAppsLaunchSource(
    ash::ShelfLaunchSource source);

// Returns the (non-empty) account id of the user associated with the currently
// active session.
const AccountId& GetActiveAccountId();

bool IsAppBrowser(const ash::BrowserDelegate& browser);

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_UTIL_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_UTIL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_UTIL_H_

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

class Browser;

namespace ash {
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

// Returns true when the given |browser| is listed in the browser application
// list.
bool IsBrowserRepresentedInBrowserList(Browser* browser,
                                       const ash::ShelfModel* model);

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_UTIL_H_

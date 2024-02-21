// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_

#include <string_view>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ui/events/event_constants.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace views {
class View;
}  // namespace views

namespace ash::test {

// Performs a click on `view` with optional `flags`.
void Click(const views::View* view, int flags = ui::EF_NONE);

// Creates a file at the root of the downloads mount point with the specified
// `extension`. The default extension is "txt". Returns the path of the created
// file.
base::FilePath CreateFile(Profile* profile, std::string_view extension = "txt");

// Moves mouse to `view` over `count` number of events. `count` is 1 by default.
void MoveMouseTo(const views::View* view, size_t count = 1u);

void InstallSystemAppsForTesting(Profile* profile);

// Creates a system web app window (os settings, camera, files, etc.). Note that
// a test needs to call `InstallSystemWebAppsForTesting()` prior to using this.
void CreateSystemWebApp(Profile* profile, ash::SystemWebAppType app_type);

}  // namespace ash::test

#endif  // CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_

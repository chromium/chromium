// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_INFOBAR_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_INFOBAR_UTILS_H_

#include "chrome/browser/ui/startup/startup_types.h"

class BrowserWindowInterface;
class Profile;

namespace base {
class CommandLine;
}

// Adds any startup infobars to the selected tab of the given browser.
void AddInfoBarsIfNecessary(BrowserWindowInterface* browser,
                            Profile* profile,
                            const base::CommandLine& startup_command_line,
                            chrome::startup::IsFirstRun is_first_run,
                            bool is_web_app,
                            bool is_post_crash_launch,
                            bool was_restarted);

#endif  // CHROME_BROWSER_UI_STARTUP_INFOBAR_UTILS_H_

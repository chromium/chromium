// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_MAC_H_
#define CHROME_BROWSER_UI_BROWSER_MAC_H_

#include "chrome/browser/ui/chrome_pages.h"

class Profile;

namespace chrome {

// Open a new window with history/downloads/help/options (needed on Mac when
// there are no windows).
void OpenAboutWindow(Profile* profile);
void OpenHistoryWindow(Profile* profile);
void OpenDownloadsWindow(Profile* profile);
void OpenHelpWindow(Profile* profile, HelpSource source);
void OpenOptionsWindow(Profile* profile);
void OpenClearBrowsingDataDialogWindow(Profile* profile);
void OpenImportSettingsDialogWindow(Profile* profile);
void OpenBookmarkManagerWindow(Profile* profile);
void OpenExtensionsWindow(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_MAC_H_

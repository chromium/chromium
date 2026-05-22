// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/base_window.h"

namespace chrome {

void OpenAboutWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowAboutChrome(browser);
  browser->GetWindow()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowHistory(browser);
  browser->GetWindow()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowDownloads(browser);
  browser->GetWindow()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowHelp(browser, source);
  browser->GetWindow()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowSettings(browser);
  browser->GetWindow()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowClearBrowsingDataDialog(browser);
  browser->GetWindow()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowImportDialog(browser);
  browser->GetWindow()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowBookmarkManager(browser);
  browser->GetWindow()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowExtensions(browser);
  browser->GetWindow()->Show();
}

}  // namespace chrome

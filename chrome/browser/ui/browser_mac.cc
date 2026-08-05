// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "ui/base/base_window.h"

namespace chrome {

void OpenAboutWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowAboutChrome(browser);
  browser->GetWindow()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowHistory(browser);
  browser->GetWindow()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowDownloads(browser);
  browser->GetWindow()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowHelp(browser, source);
  browser->GetWindow()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowSettings(browser);
  browser->GetWindow()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowClearBrowsingDataDialog(browser);
  browser->GetWindow()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowImportDialog(browser);
  browser->GetWindow()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowBookmarkManager(browser);
  browser->GetWindow()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  BrowserWindowInterface* browser =
      CreateBrowserWindow(BrowserWindowCreateParams(profile, true));
  ShowExtensions(browser);
  browser->GetWindow()->Show();
}

}  // namespace chrome

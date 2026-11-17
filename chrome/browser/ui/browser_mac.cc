// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/waap/window_creation.h"
#include "ui/base/base_window.h"

namespace chrome {

namespace {

BrowserWindowInterface* CreateEmptyBrowserWindow(Profile* profile) {
  return waap::CreateBrowserWindowAndRecordCreationSource(
      BrowserWindowCreateParams(profile, true),
      waap::NewWindowCreationSource::kNewEmptyWindow);
}

}  // namespace

void OpenAboutWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowAboutChrome(browser);
  browser->GetWindow()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowHistory(browser);
  browser->GetWindow()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowDownloads(browser);
  browser->GetWindow()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowHelp(browser, source);
  browser->GetWindow()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowSettings(browser);
  browser->GetWindow()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowClearBrowsingDataDialog(browser);
  browser->GetWindow()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowImportDialog(browser);
  browser->GetWindow()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowBookmarkManager(browser);
  browser->GetWindow()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  BrowserWindowInterface* browser = CreateEmptyBrowserWindow(profile);
  ShowExtensions(browser);
  browser->GetWindow()->Show();
}

}  // namespace chrome

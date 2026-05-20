// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_

#include "base/auto_reset.h"
#include "chrome/browser/ui/views/web_apps/web_app_testing_flags.h"

namespace web_app::test {

// Scoped helper to enable auto-accepting all web app dialogs during its
// lifetime.
class ScopedAutoAcceptWebAppDialogs {
 public:
  ScopedAutoAcceptWebAppDialogs();
  ScopedAutoAcceptWebAppDialogs(const ScopedAutoAcceptWebAppDialogs&) = delete;
  ScopedAutoAcceptWebAppDialogs& operator=(
      const ScopedAutoAcceptWebAppDialogs&) = delete;
  ~ScopedAutoAcceptWebAppDialogs();

 private:
  base::AutoReset<bool> auto_accept_all_install_dialogs_;
};

// Scoped helper to enable auto-declining dialogs during its
// lifetime.
class ScopedAutoDeclineInstallDialogs {
 public:
  ScopedAutoDeclineInstallDialogs();
  ScopedAutoDeclineInstallDialogs(const ScopedAutoDeclineInstallDialogs&) =
      delete;
  ScopedAutoDeclineInstallDialogs& operator=(
      const ScopedAutoDeclineInstallDialogs&) = delete;
  ~ScopedAutoDeclineInstallDialogs();

 private:
  base::AutoReset<bool> auto_decline_install_dialogs_;
};

// Scoped helper to prevent closing dialogs on deactivate during
// its lifetime.
class ScopedDontCloseInstallDialogsOnDeactivate {
 public:
  ScopedDontCloseInstallDialogsOnDeactivate();
  ScopedDontCloseInstallDialogsOnDeactivate(
      const ScopedDontCloseInstallDialogsOnDeactivate&) = delete;
  ScopedDontCloseInstallDialogsOnDeactivate& operator=(
      const ScopedDontCloseInstallDialogsOnDeactivate&) = delete;
  ~ScopedDontCloseInstallDialogsOnDeactivate();

 private:
  base::AutoReset<bool> dont_close_install_dialogs_on_deactivate_;
};

// Scoped helper to auto-check the open in window checkbox specifically on
// ChromeOS.
class ScopedAutoCheckChromeOsOpenInWindow {
 public:
  ScopedAutoCheckChromeOsOpenInWindow();
  ScopedAutoCheckChromeOsOpenInWindow(
      const ScopedAutoCheckChromeOsOpenInWindow&) = delete;
  ScopedAutoCheckChromeOsOpenInWindow& operator=(
      const ScopedAutoCheckChromeOsOpenInWindow&) = delete;
  ~ScopedAutoCheckChromeOsOpenInWindow();

 private:
  base::AutoReset<bool> auto_check_chromeos_open_in_window_;
};

// Scoped helper to auto-accept the create shortcut dialog.
class ScopedAutoAcceptCreateShortcutDialog {
 public:
  ScopedAutoAcceptCreateShortcutDialog();
  ScopedAutoAcceptCreateShortcutDialog(
      const ScopedAutoAcceptCreateShortcutDialog&) = delete;
  ScopedAutoAcceptCreateShortcutDialog& operator=(
      const ScopedAutoAcceptCreateShortcutDialog&) = delete;
  ~ScopedAutoAcceptCreateShortcutDialog();

 private:
  base::AutoReset<bool> auto_accept_create_shortcut_dialog_;
};

}  // namespace web_app::test

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_

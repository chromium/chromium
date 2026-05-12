// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_

#include "base/auto_reset.h"

namespace web_app {

// Global flag to auto-accept all web app install and launch dialogs in tests.
extern bool g_auto_accept_all_install_dialogs_for_testing;

// Global flag to auto-check the open in window checkbox in web app dialogs in
// tests.
extern bool g_auto_check_open_in_window_for_testing;

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
  base::AutoReset<bool> auto_reset_;
};

// Scoped helper to auto-check the open in window checkbox in web app dialogs.
class ScopedAutoCheckOpenInWindow {
 public:
  ScopedAutoCheckOpenInWindow();
  ScopedAutoCheckOpenInWindow(const ScopedAutoCheckOpenInWindow&) = delete;
  ScopedAutoCheckOpenInWindow& operator=(const ScopedAutoCheckOpenInWindow&) =
      delete;
  ~ScopedAutoCheckOpenInWindow();

 private:
  base::AutoReset<bool> auto_reset_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_SUPPORT_H_

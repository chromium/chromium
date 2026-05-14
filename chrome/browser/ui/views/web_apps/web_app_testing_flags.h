// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_TESTING_FLAGS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_TESTING_FLAGS_H_

namespace web_app::test {

// Global flag to auto-accept all web app install dialogs in tests.
extern bool g_auto_accept_all_install_dialogs_for_testing;

// Global flag to auto-check the open in window checkbox specifically on
// ChromeOS in the create shortcut dialog.
extern bool g_auto_check_chromeos_open_in_window_for_testing;

// Global flag to auto-accept the create shortcut dialog in tests.
extern bool g_auto_accept_create_shortcut_dialog_for_testing;

}  // namespace web_app::test

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_TESTING_FLAGS_H_

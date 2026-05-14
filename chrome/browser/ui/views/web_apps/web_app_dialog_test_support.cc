// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_support.h"

#include "chrome/browser/ui/web_applications/web_app_dialogs.h"

namespace web_app::test {

ScopedAutoAcceptWebAppDialogs::ScopedAutoAcceptWebAppDialogs()
    : auto_reset_(&g_auto_accept_all_install_dialogs_for_testing, true) {
}  // IN-TEST

ScopedAutoAcceptWebAppDialogs::~ScopedAutoAcceptWebAppDialogs() = default;

ScopedAutoCheckChromeOsOpenInWindow::ScopedAutoCheckChromeOsOpenInWindow()
    : auto_reset_(&g_auto_check_chromeos_open_in_window_for_testing, true) {
}  // IN-TEST

ScopedAutoCheckChromeOsOpenInWindow::~ScopedAutoCheckChromeOsOpenInWindow() =
    default;

ScopedAutoAcceptCreateShortcutDialog::ScopedAutoAcceptCreateShortcutDialog()
    : auto_reset_(&g_auto_accept_create_shortcut_dialog_for_testing, true) {
}  // IN-TEST

ScopedAutoAcceptCreateShortcutDialog::~ScopedAutoAcceptCreateShortcutDialog() =
    default;

}  // namespace web_app::test

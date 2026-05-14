// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_testing_flags.h"

namespace web_app::test {

bool g_auto_accept_all_install_dialogs_for_testing = false;
bool g_auto_check_chromeos_open_in_window_for_testing = false;
bool g_auto_accept_create_shortcut_dialog_for_testing = false;

}  // namespace web_app::test

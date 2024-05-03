// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class CookieClearOnExitMigrationNoticePixelTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    ShowCookieClearOnExitMigrationNotice(*browser(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticePixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

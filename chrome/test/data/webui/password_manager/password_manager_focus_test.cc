// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"

class PasswordManagerUIFocusTest : public WebUIMochaBrowserTest {
 protected:
  PasswordManagerUIFocusTest() {
    set_test_loader_host(password_manager::kChromeUIPasswordManagerHost);
  }
};

// https://crbug.com/1444623: Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
IN_PROC_BROWSER_TEST_F(PasswordManagerUIFocusTest, MAYBE_All) {
  RunTest("password_manager/password_manager_focus_test.js", "mocha.run()");
}

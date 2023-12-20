// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest AshCommonCrElementsTest;

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrToast) {
  RunTest("chromeos/ash_common/cr_elements/cr_toast_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrToastManager) {
  RunTest("chromeos/ash_common/cr_elements/cr_toast_manager_test.js",
          "mocha.run()");
}

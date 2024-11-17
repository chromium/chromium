// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "content/public/test/browser_test.h"

class EduCoexistenceMochaTest : public WebUIMochaBrowserTest {
 protected:
  EduCoexistenceMochaTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunEduCoexistenceTest(const std::string& test_path) {
    RunTest(base::StrCat({"chromeos/edu_coexistence/", test_path}),
            "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, App) {
  RunEduCoexistenceTest("edu_coexistence_app_test.js");
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, Controller) {
  RunEduCoexistenceTest("edu_coexistence_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, Ui) {
  RunEduCoexistenceTest("edu_coexistence_ui_test.js");
}

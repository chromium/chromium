// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/graduation/url_constants.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class GraduationMochaTest : public WebUIMochaBrowserTest {
 protected:
  GraduationMochaTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kGraduation);
    set_test_loader_host(ash::graduation::kChromeUIGraduationAppHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();

    base::Value::Dict status;
    status.Set("is_enabled", true);
    browser()->profile()->GetPrefs()->SetDict(
        ash::prefs::kGraduationEnablementStatus, status.Clone());
  }

  void RunGraduationTest(const std::string& test_path) {
    RunTest(base::StrCat({"chromeos/graduation/", test_path}), "mocha.run()");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GraduationMochaTest, App) {
  RunGraduationTest("graduation_app_test.js");
}

IN_PROC_BROWSER_TEST_F(GraduationMochaTest, TakeoutUi) {
  RunGraduationTest("graduation_takeout_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(GraduationMochaTest, WelcomeScreen) {
  RunGraduationTest("graduation_welcome_test.js");
}

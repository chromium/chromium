// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/multistep_filter/core/features.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

class MultistepFilterInternalsTest : public WebUIMochaBrowserTest {
 public:
  MultistepFilterInternalsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        multistep_filter::kMultistepFilter);
    set_test_loader_host(chrome::kChromeUIMultistepFilterInternalsHost);
  }

 protected:
  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MultistepFilterInternalsTest, App) {
  RunTest("multistep_filter_internals/app_test.js", "mocha.run()");
}

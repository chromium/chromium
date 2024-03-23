// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/manta/features.h"
#include "content/public/test/browser_test.h"

namespace ash::vc_background_ui {

class VcBackgroundUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  VcBackgroundUIBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kVcBackgroundReplace, manta::features::kMantaService,
         features::kFeatureManagementVideoConference},
        {});
    set_test_loader_host(std::string(kChromeUIVcBackgroundHost));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VcBackgroundUIBrowserTest, LoadsIndexHtml) {
  RunTest("chromeos/vc_background_ui/vc_background_ui_test.js", "mocha.run()",
          /*skip_test_loader=*/true);
}

}  // namespace ash::vc_background_ui

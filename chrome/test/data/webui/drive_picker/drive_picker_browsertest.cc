// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

class DrivePickerHostTest : public WebUIMochaBrowserTest {
 protected:
  DrivePickerHostTest() {
    set_test_loader_host(chrome::kChromeUIDrivePickerHostHost);
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kComposeboxDriveContextMenuOption);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DrivePickerHostTest, App) {
  RunTest("drive_picker/drive_picker_host_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DrivePickerHostTest, Sanitizer) {
  RunTest("drive_picker/drive_picker_sanitizer_test.js", "mocha.run()");
}

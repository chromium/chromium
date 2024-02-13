// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://os-print.
 */
namespace ash {

namespace {

class PrintPreviewCrosBrowserTest : public WebUIMochaBrowserTest {
 public:
  PrintPreviewCrosBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPrintPreviewCrosApp},
        /*disabled_features=*/{});
    set_test_loader_host(::ash::kChromeUIPrintPreviewCrosHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrintPreviewCrosBrowserTest, PrintPreviewCrosAppTest) {
  RunTest("chromeos/print_preview_cros/print_preview_cros_app_test.js",
          "mocha.run()");
}

}  // namespace

}  // namespace ash

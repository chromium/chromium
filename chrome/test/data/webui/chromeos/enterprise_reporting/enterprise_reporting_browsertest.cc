// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace reporting {
namespace {

class EnterpriseReportingUITest : public WebUIMochaBrowserTest {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kEnterpriseReportingUI};
};

IN_PROC_BROWSER_TEST_F(EnterpriseReportingUITest, App) {
  set_test_loader_host(chrome::kChromeUIEnterpriseReportingHost);
  RunTest("chromeos/enterprise_reporting/enterprise_reporting_test.js",
          "mocha.run()");
}

}  // namespace
}  // namespace reporting

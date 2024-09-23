// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

class HealthdInternalsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  HealthdInternalsBrowserTest() {
    set_test_loader_host(::chrome::kChromeUIHealthdInternalsHost);
  }

  void RunTestAtPath(const std::string& testFilePath) {
    RunTest("chromeos/healthd_internals/" + testFilePath, "mocha.run()");
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kHealthdInternalsTabs};
};

IN_PROC_BROWSER_TEST_F(HealthdInternalsBrowserTest, AppTest) {
  RunTestAtPath("healthd_internals_test.js");
}

}  // namespace
}  // namespace ash

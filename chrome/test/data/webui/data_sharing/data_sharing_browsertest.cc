// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_test.h"

class DataSharingWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  DataSharingWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUIUntrustedDataSharingHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      data_sharing::features::kDataSharingFeature};
};

IN_PROC_BROWSER_TEST_F(DataSharingWebUIBrowserTest, ConversionUtils) {
  RunTest("data_sharing/mojom_conversion_utils_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DataSharingWebUIBrowserTest, RunFlows) {
  // Skip branded chrome that need to make server calls.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GTEST_SKIP() << "N/A for Google Chrome Branding Build";
#else
  RunTest("data_sharing/data_sharing_test.js", "mocha.run()");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

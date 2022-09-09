// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

class SupportToolUIBrowserTest : public InProcessBrowserTest {
 public:
  SupportToolUIBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSupportTool);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SupportToolUIBrowserTest, OpenSupportToolWebUI) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(content::GetWebUIURL(chrome::kChromeUISupportToolHost))));
}

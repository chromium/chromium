// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace craft {

class CraftUIDisabledBrowserTest : public InProcessBrowserTest {
 public:
  CraftUIDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kCraft);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CraftUIDisabledBrowserTest, NotAvailable) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_FALSE(content::NavigateToURL(
      web_contents, content::GetWebUIURL(chrome::kChromeUICraftHost)));
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsErrorDocument());
}

}  // namespace craft

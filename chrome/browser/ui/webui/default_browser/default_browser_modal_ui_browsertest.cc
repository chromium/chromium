// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

class DefaultBrowserModalUISharedRendererBrowserTest
    : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kTopChromeWebUISharedRenderer};
};

// With kTopChromeWebUISharedRenderer, the modal is created in
// TopChromeWebUIProfile for the profile of the window, and loads.
IN_PROC_BROWSER_TEST_F(DefaultBrowserModalUISharedRendererBrowserTest,
                       LoadsInTopChromeWebUIProfile) {
  Profile* const user_profile = browser()->GetProfile();
  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<DefaultBrowserModalUI>>(
          GURL(chrome::kChromeUIDefaultBrowserModalURL),
          &webui::GetProfileForTopChromeWebUIContents(user_profile),
          /*task_manager_string_id=*/0,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false, user_profile);
  content::WebContents* web_contents = contents_wrapper->web_contents();
  EXPECT_TRUE(Profile::FromBrowserContext(web_contents->GetBrowserContext())
                  ->IsTopChromeWebUIProfile());
  EXPECT_EQ(user_profile, webui::GetUserProfile(web_contents));
  // The page binds its handler when it loads.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(content::ExecJs(web_contents, "true"));
}

}  // namespace

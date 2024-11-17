// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_NAVIGATION_CAPTURING_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_NAVIGATION_CAPTURING_BROWSERTEST_BASE_H_

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"

class Browser;
class GURL;
namespace content {
class WebContents;
}

namespace web_app {

// Base class for tests of user interface support for web applications.
class WebAppNavigationCapturingBrowserTestBase : public WebAppBrowserTestBase {
 public:
  WebAppNavigationCapturingBrowserTestBase();
  WebAppNavigationCapturingBrowserTestBase(
      const WebAppNavigationCapturingBrowserTestBase&) = delete;
  WebAppNavigationCapturingBrowserTestBase& operator=(
      const WebAppNavigationCapturingBrowserTestBase&) = delete;
  ~WebAppNavigationCapturingBrowserTestBase() override = 0;

  Browser* CallWindowOpenExpectNewBrowser(content::WebContents* contents,
                                          const GURL& url,
                                          bool with_opener);
  content::WebContents* CallWindowOpenExpectNewTab(
      content::WebContents* contents,
      const GURL& url,
      bool with_opener);

  void WaitForLaunchParams(content::WebContents* contents,
                           int min_launch_params_to_wait_for);

  std::vector<GURL> GetLaunchParams(content::WebContents* contents,
                                    const std::string& params);

 private:
  void CallWindowOpen(content::WebContents* contents,
                      const GURL& url,
                      bool with_opener);

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_NAVIGATION_CAPTURING_BROWSERTEST_BASE_H_

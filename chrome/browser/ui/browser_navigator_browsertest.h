// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

#include <stddef.h>

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"

class Profile;

namespace content {
class WebContents;
}

// Browsertest class for testing the browser navigation. It is also a base class
// for the |BrowserGuestModeNavigation| which tests navigation while in guest
// mode.
class BrowserNavigatorTest : public InProcessBrowserTest {
 public:
  BrowserNavigatorTest();

 protected:
  void SetUpOnMainThread() override;
  NavigateParams MakeNavigateParams() const;
  NavigateParams MakeNavigateParams(Browser* browser) const;

  Browser* CreateEmptyBrowserForType(Browser::Type type, Profile* profile);
  Browser* CreateEmptyBrowserForApp(Profile* profile);

  std::unique_ptr<content::WebContents> CreateWebContents(
      bool initialize_renderer);

  void RunSuppressTest(WindowOpenDisposition disposition);
  void RunUseNonIncognitoWindowTest(const GURL& url,
                                    const ui::PageTransition& page_transition);
  void RunDoNothingIfIncognitoIsForcedTest(const GURL& url);

  bool OpenPOSTURLInNewForegroundTabAndGetTitle(const GURL& url,
                                                const std::string& post_data,
                                                bool is_browser_initiated,
                                                std::u16string* title);

  // Navigate `browser` to `url`.  If `wait_for_navigation` is true, then this
  // will also wait for the WebContents to signal that loading has stopped.  It
  // is up to the test to tell us, in this case, which WebContents should be
  // the one that is navigated.  If `expected_web_contents` is not null, then
  // that is the WebContents that the test expects to load.  If it's null, then
  // the behavior depends on the window disposition.  In almost all cases, it
  // indicates that a new WebContents will be created and navigated.  However,
  // for `CURRENT_TAB`, we'll assume that the active WebContents is the right
  // one as a convenience, since it's always the intended case anyway.
  Browser* NavigateHelper(
      const GURL& url,
      Browser* browser,
      WindowOpenDisposition disposition,
      bool wait_for_navigation,
      content::WebContents* expected_web_contents = nullptr);

  size_t created_tab_contents_count_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

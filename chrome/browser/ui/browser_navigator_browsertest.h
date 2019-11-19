// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

#include <stddef.h>

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace content {
class WebContents;
}

// Browsertest class for testing the browser navigation. It is also a base class
// for the |BrowserGuestModeNavigation| which tests navigation while in guest
// mode.
class BrowserNavigatorTest : public InProcessBrowserTest,
                             public content::NotificationObserver {
 protected:
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

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  bool OpenPOSTURLInNewForegroundTabAndGetTitle(const GURL& url,
                                                const std::string& post_data,
                                                bool is_browser_initiated,
                                                base::string16* title);

  Browser* NavigateHelper(const GURL& url,
                          Browser* browser,
                          WindowOpenDisposition disposition,
                          bool wait_for_navigation);

  size_t created_tab_contents_count_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/test_navigation_observer.h"

void InProcessBrowserTest::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  // Opening a Devtools Window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    ASSERT_FALSE(content::DevToolsAgentHost::HasFor(web_contents));
    DevToolsWindow::OpenDevToolsWindow(web_contents);
    ASSERT_TRUE(content::DevToolsAgentHost::HasFor(web_contents));
  }
}

Browser* InProcessBrowserTest::OpenURLOffTheRecord(Profile* profile,
                                                   const GURL& url) {
  // Opening an incognito window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    chrome::OpenURLOffTheRecord(profile, url);
    Browser* browser =
        chrome::FindTabbedBrowser(profile->GetOffTheRecordProfile(), false);
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    observer.Wait();
    return browser;
  }
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  // Making a browser window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    Browser* browser = new Browser(Browser::CreateParams(profile, true));
    AddBlankTabAndShow(browser);
    return browser;
  }
}

Browser* InProcessBrowserTest::CreateIncognitoBrowser(Profile* profile) {
  // Making a browser window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    // Use active profile if default nullptr was passed.
    if (!profile)
      profile = browser()->profile();

    // Create a new browser with using the incognito profile.
    Browser* incognito = new Browser(
        Browser::CreateParams(profile->GetOffTheRecordProfile(), true));
    AddBlankTabAndShow(incognito);
    return incognito;
  }
}

Browser* InProcessBrowserTest::CreateBrowserForPopup(Profile* profile) {
  // Making a browser window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    Browser* browser =
        new Browser(Browser::CreateParams(Browser::TYPE_POPUP, profile, true));
    AddBlankTabAndShow(browser);
    return browser;
  }
}

Browser* InProcessBrowserTest::CreateBrowserForApp(const std::string& app_name,
                                                   Profile* profile) {
  // Making a browser window can cause AppKit to throw objects into the
  // autorelease pool. Flush the pool when this function returns.
  @autoreleasepool {
    Browser* browser = new Browser(Browser::CreateParams::CreateForApp(
        app_name, false /* trusted_source */, gfx::Rect(), profile, true));
    AddBlankTabAndShow(browser);
    return browser;
  }
}

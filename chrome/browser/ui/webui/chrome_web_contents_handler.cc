// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

ChromeWebContentsHandler::ChromeWebContentsHandler() {
}

ChromeWebContentsHandler::~ChromeWebContentsHandler() {
}

// Opens a new URL inside |source|. |context| is the browser context that the
// browser should be owned by. |params| contains the URL to open and various
// attributes such as disposition. On return |out_new_contents| contains the
// WebContents the URL is opened in. Returns the web contents opened by the
// browser.
WebContents* ChromeWebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    WebContents* source,
    const OpenURLParams& params) {
  if (!context)
    return NULL;

  Profile* profile = Profile::FromBrowserContext(context);

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser) {
    // TODO(erg): OpenURLParams should pass a user_gesture flag, pass it to
    // CreateParams, and pass the real value to nav_params below.
    browser =
        new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  }
  NavigateParams nav_params(browser, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  if (source && source->IsCrashed() &&
      params.disposition == WindowOpenDisposition::CURRENT_TAB &&
      ui::PageTransitionCoreTypeIs(params.transition,
                                   ui::PAGE_TRANSITION_LINK)) {
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else {
    nav_params.disposition = params.disposition;
  }
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&nav_params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != nav_params.browser))
    browser->window()->Close();

  return nav_params.navigated_or_inserted_contents;
}

// Creates a new tab with |new_contents|. |context| is the browser context that
// the browser should be owned by. |source| is the WebContent where the
// operation originated. |disposition| controls how the new tab should be
// opened. |initial_rect| is the position and size of the window if a new window
// is created.  |user_gesture| is true if the operation was started by a user
// gesture.
void ChromeWebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {
  if (!context)
    return;

  Profile* profile = Profile::FromBrowserContext(context);

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser) {
    browser = new Browser(
        Browser::CreateParams(Browser::TYPE_NORMAL, profile, user_gesture));
  }
  NavigateParams params(browser, std::move(new_contents));
  params.source_contents = source;
  params.disposition = disposition;
  params.window_bounds = initial_rect;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

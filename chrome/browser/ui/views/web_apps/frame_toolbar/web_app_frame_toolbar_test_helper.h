// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_

class Browser;
class BrowserNonClientFrameView;
class BrowserView;
class GURL;
class WebAppFrameToolbarView;

// Mixin for setting up and launching a web app in a browser test.
class WebAppFrameToolbarTestHelper {
 public:
  WebAppFrameToolbarTestHelper();
  WebAppFrameToolbarTestHelper(const WebAppFrameToolbarTestHelper&) = delete;
  WebAppFrameToolbarTestHelper& operator=(const WebAppFrameToolbarTestHelper&) =
      delete;
  ~WebAppFrameToolbarTestHelper();

  void InstallAndLaunchWebApp(Browser* browser, const GURL& start_url);

  Browser* app_browser() { return app_browser_; }
  BrowserView* browser_view() { return browser_view_; }
  BrowserNonClientFrameView* frame_view() { return frame_view_; }
  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return web_app_frame_toolbar_;
  }

 private:
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  BrowserNonClientFrameView* frame_view_ = nullptr;
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_

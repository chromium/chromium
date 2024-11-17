// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"

class Browser;
class BrowserNonClientFrameView;
class BrowserView;
class Profile;
class GURL;
class WebAppFrameToolbarView;
class WebAppOriginText;

namespace base {
class ScopedTempDir;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace views {
class View;
}  // namespace views

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

// Mixin for setting up and launching a web app in a browser test.
class WebAppFrameToolbarTestHelper {
 public:
  WebAppFrameToolbarTestHelper();
  WebAppFrameToolbarTestHelper(const WebAppFrameToolbarTestHelper&) = delete;
  WebAppFrameToolbarTestHelper& operator=(const WebAppFrameToolbarTestHelper&) =
      delete;
  ~WebAppFrameToolbarTestHelper();

  // Installs but does not launch a web app with the given `start_url`. This
  // does not modify state of this test helper.
  webapps::AppId InstallWebApp(Profile* profile, const GURL& start_url);

  // These methods install and launch the given web app; additionally the
  // various getters in this test helper will start returning objects and
  // views related to this latest launched web app.
  webapps::AppId InstallAndLaunchWebApp(Profile* profile,
                                        const GURL& start_url);
  webapps::AppId InstallAndLaunchWebApp(Browser* browser,
                                        const GURL& start_url);
  webapps::AppId InstallAndLaunchCustomWebApp(
      Browser* browser,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
      const GURL& start_url);

  GURL LoadTestPageWithDataAndGetURL(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      base::ScopedTempDir* temp_dir,
      std::string_view test_html);

  GURL LoadWindowControlsOverlayTestPageWithDataAndGetURL(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      base::ScopedTempDir* temp_dir);

  // Loads a page where the whole WebContents is a draggable region.
  GURL LoadWholeAppIsDraggableTestPageWithDataAndGetURL(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      base::ScopedTempDir* temp_dir);

  // WebContents is used to run JS to parse rectangle values into a list value.
  static base::Value::List GetXYWidthHeightListValue(
      content::WebContents* web_contents,
      const std::string& rect_value_list,
      const std::string& rect_var_name);

  // WebContents is used to run JS to parse rectangle values into a rectangle
  // object.
  static gfx::Rect GetXYWidthHeightRect(content::WebContents* web_contents,
                                        const std::string& rect_value_list,
                                        const std::string& rect_var_name);

  // Add window-controls-overlay's ongeometrychange callback into the document.
  void SetupGeometryChangeCallback(content::WebContents* web_contents);

  void TestDraggableRegions();

  // Opens a new popup window from |app_browser_| by running
  // |window_open_script| and returns the |BrowserView| it opened in.
  BrowserView* OpenPopup(const std::string& window_open_script);

  static void GrantWindowManagementPermission(
      content::WebContents* web_contents);
  void GrantWindowManagementPermission();

  Browser* app_browser() { return app_browser_; }
  BrowserView* browser_view() { return browser_view_; }
  BrowserNonClientFrameView* frame_view() { return frame_view_; }
  views::View* root_view() { return root_view_; }
  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return web_app_frame_toolbar_;
  }
  WebAppOriginText* origin_text_view();
  void SetOriginTextLabelForTesting(const std::u16string& label_text);

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;
  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<BrowserNonClientFrameView, AcrossTasksDanglingUntriaged> frame_view_ =
      nullptr;
  raw_ptr<views::View, AcrossTasksDanglingUntriaged> root_view_ = nullptr;
  raw_ptr<WebAppFrameToolbarView, AcrossTasksDanglingUntriaged>
      web_app_frame_toolbar_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_

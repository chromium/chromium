// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/gurl.h"

class WebUIToolbarWebView;

namespace content {
class WebContents;
}

// Base test fixture for WebUI home button / home control tests.
class WebUIHomeControlTestBase : public InProcessBrowserTest {
 public:
  WebUIHomeControlTestBase();
  ~WebUIHomeControlTestBase() override;

  void SetUpOnMainThread() override;

 protected:
  GURL GetHomeURL();
  void WaitForUndoBubble(WebUIToolbarWebView* webui_toolbar_view);
  void SimulateDropOnHomeButton(content::WebContents* web_contents,
                                const std::string& url);
  WebUIToolbarWebView* PerformDragAndDrop(const std::string& new_home_url);
  void PerformUndo(WebUIToolbarWebView* webui_toolbar_view);

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_TEST_BASE_H_

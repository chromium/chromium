// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

class ToolbarView;
class WebUIToolbarWebView;

// Base test fixture providing common feature setup and helper methods for
// WebUIToolbarWebView browser tests and interactive UI tests.
class WebUIToolbarWebViewTestBase : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewTestBase();
  ~WebUIToolbarWebViewTestBase() override;

  void SetUpOnMainThread() override;

 protected:
  WebUIToolbarWebViewTestBase(
      const std::vector<base::test::FeatureRef>& enabled,
      const std::vector<base::test::FeatureRef>& disabled);

  ToolbarView* GetToolbarView();

  void SimulateDropOnToolbar(content::WebContents* web_contents,
                             const std::string& text);

  void SimulateUriListDropOnToolbar(content::WebContents* web_contents,
                                    const std::string& url);

  scoped_refptr<const extensions::Extension> LoadAndPinExtension(
      WebUIToolbarWebView* webui_toolbar_view,
      base::ScopedTempDir& temp_dir,
      bool has_background_script = false,
      bool has_popup = false);

  scoped_refptr<const extensions::Extension> LoadExtension(
      base::ScopedTempDir& temp_dir,
      bool has_background_script = false,
      bool has_popup = false);

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_

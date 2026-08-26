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
#include "content/public/test/browser_test_utils.h"

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

  ToolbarView* GetToolbarView();
  WebUIToolbarWebView* GetWebUIToolbar();
  content::WebContents* GetWebUIWebContents();

  // Sets the size of a test-only element on the toolbar-app with the provided
  // size, which should cause responsive controls to be asynchronously laid out
  // to accommodate it. Does not wait for that layout to occur. Adds the element
  // on the first call, and resizes it on subsequent calls.
  //
  // This is more flexible than resizing the window due to the window having a
  // min size. It also provides test coverage that adding/sizing
  // non-ResponsiveControls to the toolbar-app correctly causes
  // layoutResponsiveControls() to be invoked.
  //
  // Note that first adding the spacer will likely add some extra
  // margins/padding in addition to `width`.
  [[nodiscard]] content::EvalJsResult SetSpacerWidth(int width);

 protected:
  WebUIToolbarWebViewTestBase(
      const std::vector<base::test::FeatureRef>& enabled,
      const std::vector<base::test::FeatureRef>& disabled);

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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/action_id.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace views {
class BubbleAnchor;
class View;
}  // namespace views

class PinnedToolbarActionsModel;
class ToolbarView;
class WebUIPinnedToolbarActions;
class WebUIToolbarWebView;

// Base test fixture providing common feature setup and helper methods for
// WebUIToolbarWebView browser tests and interactive UI tests.
class WebUIToolbarWebViewTestBase : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewTestBase();
  ~WebUIToolbarWebViewTestBase() override;

  void SetUpOnMainThread() override;

  ToolbarView* GetToolbarView();

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

// Base test fixture for pinned toolbar actions tests in WebUI toolbar.
class WebUIPinnedToolbarActionsTestBase : public WebUIToolbarWebViewTestBase {
 public:
  WebUIPinnedToolbarActionsTestBase();
  ~WebUIPinnedToolbarActionsTestBase() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  content::WebContents* GetWebContents();

  WebUIPinnedToolbarActions* GetPinnedToolbarActions();

  content::EvalJsResult EvalJsOnPinnedButton(
      content::WebContents* web_contents,
      toolbar_ui_api::mojom::PinnedToolbarAction action,
      const std::string& script_body);

  bool IsPinnedButtonVisible(content::WebContents* web_contents,
                             toolbar_ui_api::mojom::PinnedToolbarAction action);

  bool ClickPinnedButton(content::WebContents* web_contents,
                         toolbar_ui_api::mojom::PinnedToolbarAction action);

  void SetPinnableProperty(actions::ActionId id, bool pinnable);

  views::View* GetLocationBarView();

  views::BubbleAnchor GetToolbarBubbleAnchor(actions::ActionId action_id);

  void PinAction(actions::ActionId action_id,
                 toolbar_ui_api::mojom::PinnedToolbarAction mojom_action);

  void UnpinAction(actions::ActionId action_id,
                   toolbar_ui_api::mojom::PinnedToolbarAction mojom_action);

  void VerifyPinnedToolbarWidth();

  raw_ptr<PinnedToolbarActionsModel> model_ = nullptr;

  const std::vector<
      std::pair<actions::ActionId, toolbar_ui_api::mojom::PinnedToolbarAction>>
      kActionMappings;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_

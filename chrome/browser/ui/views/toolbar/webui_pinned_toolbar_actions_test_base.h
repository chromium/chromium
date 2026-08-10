// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_TEST_BASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/action_id.h"

namespace content {
class WebContents;
}

namespace views {
class BubbleAnchor;
class View;
}  // namespace views

class PinnedToolbarActionsModel;
class WebUIPinnedToolbarActions;

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

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_TEST_BASE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

class PinnedActionToolbarButtonMenuModel;
class WebUIToolbarWebView;

namespace views {
class MenuRunner;
}

// The C++ side of a WebUI implementation of pinned toolbar actions.
class WebUIPinnedToolbarActions : public PinnedToolbarActions,
                                  public PinnedToolbarActionsModel::Observer {
 public:
  explicit WebUIPinnedToolbarActions(
      WebUIToolbarWebView* webui_toolbar_web_view);
  WebUIPinnedToolbarActions(const WebUIPinnedToolbarActions&) = delete;
  WebUIPinnedToolbarActions& operator=(const WebUIPinnedToolbarActions&) =
      delete;
  ~WebUIPinnedToolbarActions() override;

  void Init();

  // Handle a context menu request from WebUI.
  void HandleContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                         const gfx::Rect& screen_rect,
                         ui::mojom::MenuSourceType source_type);

  // Invoke an action that is currently displaying.
  void Invoke(toolbar_ui_api::mojom::PinnedToolbarAction action_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckPinnedToolbarActionColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIPinnedToolbarActionsBrowserTest,
                           RightClickPinnedAction);
  FRIEND_TEST_ALL_PREFIXES(WebUIPinnedToolbarActionsInteractiveTest,
                           RightClickPinnedAction);
  FRIEND_TEST_ALL_PREFIXES(WebUIPinnedToolbarActionsBrowserTest,
                           LongPressPinnedAction);

  // PinnedToolbarActionsModel::Observer:
  void OnActionsChanged() override;

  // ToolbarController::PinnedActionsDelegate:
  const std::vector<actions::ActionId>& PinnedActionIds() const override;
  actions::ActionItem* GetActionItemFor(actions::ActionId id) override;
  bool IsOverflowed(actions::ActionId id) override;
  views::View* GetContainerView() override;
  bool ShouldAnyButtonsOverflow(gfx::Size available_size) const override;

  // PinnedToolbarActions:
  void UpdateActionState(actions::ActionId id, bool is_active) override;
  void ShowActionEphemerallyInToolbar(actions::ActionId id, bool show) override;
  bool IsActionPinned(actions::ActionId id) override;
  bool IsActionPoppedOut(actions::ActionId id) override;
  bool IsActionPinnedOrPoppedOut(actions::ActionId id) override;
  void PostOrQueueActionAfterAnimation(base::OnceClosure action) override;
  ToolbarButton* GetDownloadButton() override;
  views::BubbleAnchor GetBubbleAnchor(actions::ActionId action_id) override;
  PinnedActionToolbarButton* GetChromeLabsButton() override;
  void UpdatePinnedStateAndAnnounce(actions::ActionId id, bool pin) override;

  // Parent toolbar.
  const raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;
  // The model whose state we use to populate this view.
  raw_ptr<PinnedToolbarActionsModel> model_;
  // Allow this class to observe |model_|.
  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      model_observation_{this};
  // List of ephemeral popped out actions.
  std::vector<actions::ActionId> popped_out_actions_;
  // Allow this class to observe actions for currently displaying buttons.
  std::vector<base::CallbackListSubscription> action_subscriptions_;
  std::unique_ptr<PinnedActionToolbarButtonMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // The action ID associated with the most recently opened context menu.
  // Note: This is unset initially, and is not cleared when the menu closes.
  // It should only be evaluated when `menu_runner_->IsRunning()` is true.
  std::optional<actions::ActionId> active_context_menu_action_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_

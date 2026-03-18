// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"

class WebUIToolbarWebView;
class PinnedToolbarActionsModel;

// The C++ side of a WebUI implementation of pinned toolbar actions.
class WebUIPinnedToolbarActions : public PinnedToolbarActions {
 public:
  explicit WebUIPinnedToolbarActions(
      WebUIToolbarWebView* webui_toolbar_web_view);
  WebUIPinnedToolbarActions(const WebUIPinnedToolbarActions&) = delete;
  WebUIPinnedToolbarActions& operator=(const WebUIPinnedToolbarActions&) =
      delete;

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
  ToolbarButton* GetCastButton() override;
  views::BubbleAnchor GetBubbleAnchor(actions::ActionId action_id) override;
  void SetActionElementIdentifier(actions::ActionId action_id,
                                  ui::ElementIdentifier element_id) override;
  PinnedActionToolbarButton* GetChromeLabsButton() override;
  void UpdatePinnedStateAndAnnounce(actions::ActionId id, bool pin) override;

 private:
  // Parent toolbar.
  const raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;
  // The model whose state we use to populate this view.
  raw_ptr<PinnedToolbarActionsModel> model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_PINNED_TOOLBAR_ACTIONS_H_

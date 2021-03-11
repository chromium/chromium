// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_BLOCKED_ACTION_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_BLOCKED_ACTION_BUBBLE_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"

// The delegate for the bubble to ask the user if they want to refresh the page
// in order to run any blocked actions the extension may have.
class BlockedActionBubbleDelegate : public ToolbarActionsBarBubbleDelegate {
 public:
  BlockedActionBubbleDelegate(base::OnceCallback<void(CloseAction)> callback,
                              const std::string& extension_id);
  ~BlockedActionBubbleDelegate() override;

 private:
  // ToolbarActionsBarBubbleDelegate:
  bool ShouldShow() override;
  bool ShouldCloseOnDeactivate() override;
  std::u16string GetHeadingText() override;
  std::u16string GetBodyText(bool anchored_to_action) override;
  std::u16string GetItemListText() override;
  std::u16string GetActionButtonText() override;
  std::u16string GetDismissButtonText() override;
  ui::DialogButton GetDefaultDialogButton() override;
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
  GetExtraViewInfo() override;
  std::string GetAnchorActionId() override;
  void OnBubbleShown(base::OnceClosure close_bubble_callback) override;
  void OnBubbleClosed(CloseAction action) override;

  base::OnceCallback<void(CloseAction)> callback_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(BlockedActionBubbleDelegate);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_BLOCKED_ACTION_BUBBLE_DELEGATE_H_

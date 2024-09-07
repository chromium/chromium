// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

// A test delegate for a bubble to hang off the toolbar actions bar.
class TestToolbarActionsBarBubbleDelegate {
 public:
  TestToolbarActionsBarBubbleDelegate(const std::u16string& heading,
                                      const std::u16string& body,
                                      const std::u16string& action = u"",
                                      const std::u16string& dismiss = u"");

  TestToolbarActionsBarBubbleDelegate(
      const TestToolbarActionsBarBubbleDelegate&) = delete;
  TestToolbarActionsBarBubbleDelegate& operator=(
      const TestToolbarActionsBarBubbleDelegate&) = delete;

  ~TestToolbarActionsBarBubbleDelegate();

  // Returns a delegate to pass to the bubble. Since the bubble typically owns
  // the delegate, it means we can't have this object be the delegate, because
  // it would be deleted once the bubble closes.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> GetDelegate();

  void set_learn_more_button_text(const std::u16string& learn_more) {
    learn_more_ = learn_more;

    if (!info_) {
      info_ =
          std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
    }
    info_->text = learn_more;
    info_->is_learn_more = true;
  }
  void set_default_dialog_button(ui::mojom::DialogButton default_button) {
    default_button_ = default_button;
  }
  void set_extra_view_info(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo> info) {
    info_ = std::move(info);
  }
  void set_action_id(std::string id) { action_id_ = std::move(id); }

  const ToolbarActionsBarBubbleDelegate::CloseAction* close_action() const {
    return close_action_.get();
  }
  bool shown() const { return shown_; }

 private:
  class DelegateImpl;

  // Whether or not the bubble has been shown.
  bool shown_;

  // The action that was taken to close the bubble.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::CloseAction> close_action_;

  // Strings for the bubble.
  std::u16string heading_;
  std::u16string body_;
  std::u16string action_;
  std::u16string dismiss_;
  std::u16string learn_more_;

  // The id to associate with this bubble, if any.
  std::string action_id_;

  // The default button for the bubble.
  ui::mojom::DialogButton default_button_;

  // Information about the extra view to show, if any.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo> info_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_

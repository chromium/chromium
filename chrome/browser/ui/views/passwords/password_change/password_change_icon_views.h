// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_ICON_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_ICON_VIEWS_H_

#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/password_change_icon_views_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the icon displayed in the Omnibox bar during the password change
// flow.
class PasswordChangeIconViews : public ManagePasswordsIconView,
                                public PageActionIconView {
  METADATA_HEADER(PasswordChangeIconViews, PageActionIconView)

 public:
  PasswordChangeIconViews(
      CommandUpdater* updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Browser* browser);
  PasswordChangeIconViews(const PasswordChangeIconViews&) = delete;
  PasswordChangeIconViews& operator=(const PasswordChangeIconViews&) = delete;
  ~PasswordChangeIconViews() override;

  // ManagePasswordsIconView:
  void SetState(password_manager::ui::State state,
                bool is_blocklisted) override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

 private:
  void SetTooltipForToolbarPinningEnabled(const std::u16string& tooltip);
  void UpdateIconAndLabel();

  PasswordChangeIconViewsController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_ICON_VIEWS_H_

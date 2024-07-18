// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_

#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

class CommandUpdater;

// View for the password icon in the Omnibox.
class ManagePasswordsIconViews : public ManagePasswordsIconView,
                                 public PageActionIconView {
  METADATA_HEADER(ManagePasswordsIconViews, PageActionIconView)

 public:
  ManagePasswordsIconViews(
      CommandUpdater* updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Browser* browser);
  ManagePasswordsIconViews(const ManagePasswordsIconViews&) = delete;
  ManagePasswordsIconViews& operator=(const ManagePasswordsIconViews&) = delete;
  ~ManagePasswordsIconViews() override;

  // ManagePasswordsIconView:
  void SetState(password_manager::ui::State state) override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

 private:
  friend class ManagePasswordsIconViewTest;

  // Updates the UI to match |state_|.
  void UpdateUiForState();

  password_manager::ui::State state_ = password_manager::ui::INACTIVE_STATE;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_

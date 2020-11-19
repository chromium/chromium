// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_member.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class CommandUpdater;
class StarMenuModel;

// The star icon to show a bookmark bubble.
class StarView : public PageActionIconView,
                 public ui::SimpleMenuModel::Delegate {
 public:
  StarView(CommandUpdater* command_updater,
           Browser* browser,
           IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
           PageActionIconView::Delegate* page_action_icon_delegate);
  ~StarView() override;

  StarMenuModel* menu_model_for_test() { return menu_model_.get(); }

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void ExecuteCommand(ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  const char* GetClassName() const override;

 private:
  void EditBookmarksPrefUpdated();
  bool IsBookmarkStarHiddenByExtension() const;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  Browser* const browser_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<StarMenuModel> menu_model_;

  BooleanPrefMember edit_bookmarks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(StarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

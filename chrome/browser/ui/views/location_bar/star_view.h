// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include <memory>

#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_member.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class MenuRunner;
}

class Browser;
class CommandUpdater;
class StarMenuModel;

// The star icon to show a bookmark bubble.
class StarView : public PageActionIconView,
                 public ui::SimpleMenuModel::Delegate {
 public:
  METADATA_HEADER(StarView);
  StarView(CommandUpdater* command_updater,
           Browser* browser,
           IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
           PageActionIconView::Delegate* page_action_icon_delegate);
  StarView(const StarView&) = delete;
  StarView& operator=(const StarView&) = delete;
  ~StarView() override;

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  StarMenuModel* menu_model_for_test() { return menu_model_.get(); }
  views::MenuRunner* menu_runner_for_test() { return menu_runner_.get(); }

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void ExecuteCommand(ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 private:
  void EditBookmarksPrefUpdated();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;
  bool IsCommandIdAlerted(int command_id) const override;

  Browser* const browser_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<StarMenuModel> menu_model_;

  BooleanPrefMember edit_bookmarks_enabled_;

  base::Optional<FeaturePromoController::PromoHandle>
      reading_list_entry_point_promo_handle_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

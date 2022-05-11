// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_

#include <string>

#include "components/tab_groups/tab_group_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"

namespace gfx {
class Canvas;
}

// The display button for the Saved Tab Group in the bookmarks bar.
class SavedTabGroupButton : public views::MenuButton {
 public:
  METADATA_HEADER(SavedTabGroupButton);
  SavedTabGroupButton(
      PressedCallback callback,
      const std::u16string& title,
      bool is_group_in_tabstrip,
      tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey,
      bool animations_enabled = true);

  SavedTabGroupButton(const SavedTabGroupButton&) = delete;
  SavedTabGroupButton& operator=(const SavedTabGroupButton&) = delete;
  ~SavedTabGroupButton() override;

  // views::MenuButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  void OnThemeChanged() override;

  void RemoveButtonOutline();
  bool HasButtonOutline() const;

  tab_groups::TabGroupColorId tab_group_color_id() {
    return tab_group_color_id_;
  }

 private:
  class ContextMenuController : public views::ContextMenuController {
   public:
    ContextMenuController();
    ~ContextMenuController() override;

   private:
    void ShowContextMenuForViewImpl(View* source,
                                    const gfx::Point& point,
                                    ui::MenuSourceType source_type) override;

    // TODO(pbos): Comment
    std::unique_ptr<ui::MenuModel> menu_model_;
    std::unique_ptr<views::MenuRunner> menu_runner_;
  };

  // The animations for button movement.
  std::unique_ptr<gfx::SlideAnimation> show_animation_;

  // The color of the TabGroup this button is associated with.
  tab_groups::TabGroupColorId tab_group_color_id_;

  // Denotes if the tabgroup is currently open in the tabstrip.
  bool is_group_in_tabstrip_;

  ContextMenuController context_menu_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_

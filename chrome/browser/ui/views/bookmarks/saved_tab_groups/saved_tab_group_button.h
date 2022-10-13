// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_

#include <memory>
#include <string>
#include <vector>

#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/dialog_model_context_menu_controller.h"

namespace gfx {
class Canvas;
}

// The display button for the Saved Tab Group in the bookmarks bar.
// Note: we currently recreate this button if any content (title, tabs, color,
// etc.) changes
// TODO(dljames): Find a way to not recreate the button for each update.
class SavedTabGroupButton : public views::MenuButton {
 public:
  METADATA_HEADER(SavedTabGroupButton);
  SavedTabGroupButton(
      const SavedTabGroup& group,
      base::RepeatingCallback<content::PageNavigator*()> page_navigator,
      PressedCallback callback,
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

  const base::GUID guid() { return guid_; }

 private:
  std::unique_ptr<ui::DialogModel> CreateDialogModelForContextMenu();

  // The animations for button movement.
  std::unique_ptr<gfx::SlideAnimation> show_animation_;

  // The color of the TabGroup this button is associated with.
  tab_groups::TabGroupColorId tab_group_color_id_;

  // Denotes if the tabgroup is currently open in the tabstrip.
  bool is_group_in_tabstrip_;

  // The guid used to identify the group this button represents.
  base::GUID guid_;

  // The tabs to be displayed in the context menu. Currently supports tab title,
  // url, and favicon.
  const std::vector<SavedTabGroupTab> tabs_;

  // A callback used to fetch the current PageNavigator used to open URLs.
  const base::RepeatingCallback<content::PageNavigator*()>
      page_navigator_callback_;

  // Context menu controller used for this View.
  views::DialogModelContextMenuController context_menu_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_

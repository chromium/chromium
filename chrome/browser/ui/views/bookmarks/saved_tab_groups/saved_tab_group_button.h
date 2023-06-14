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
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_navigator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/dialog_model_context_menu_controller.h"
#include "ui/views/drag_controller.h"

class Browser;
class SavedTabGroupKeyedService;

namespace gfx {
class Canvas;
}

// The visual representation of a SavedTabGroup shown in the bookmarks bar.
class SavedTabGroupButton : public views::MenuButton,
                            public views::DragController {
 public:
  METADATA_HEADER(SavedTabGroupButton);
  SavedTabGroupButton(
      const SavedTabGroup& group,
      base::RepeatingCallback<content::PageNavigator*()> page_navigator,
      PressedCallback callback,
      Browser* browser,
      bool animations_enabled = true);

  SavedTabGroupButton(const SavedTabGroupButton&) = delete;
  SavedTabGroupButton& operator=(const SavedTabGroupButton&) = delete;
  ~SavedTabGroupButton() override;

  // views::MenuButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  void OnThemeChanged() override;

  // views::View
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnFocus() override;

  // views::DragController
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // Updates the buttons visuals (title and color) alongside its list of tabs
  // displayed in the context menu.
  void UpdateButtonData(const SavedTabGroup& group);

  tab_groups::TabGroupColorId tab_group_color_id() const {
    return tab_group_color_id_;
  }

  const base::Uuid guid() const { return guid_; }

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeleteGroupMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMoveGroupToNewWindowMenuItem);

 private:
  std::u16string GetAccessibleNameForButton();
  void SetTextProperties(const SavedTabGroup& group);
  void UpdateButtonLayout();
  void TabMenuItemPressed(const GURL& url, int event_flags);
  void MoveGroupToNewWindowPressed(int event_flags);
  void DeleteGroupPressed(int event_flags);

  std::unique_ptr<ui::DialogModel> CreateDialogModelForContextMenu();

  // The animations for button movement.
  std::unique_ptr<gfx::SlideAnimation> show_animation_;

  // The color of the TabGroup this button is associated with.
  tab_groups::TabGroupColorId tab_group_color_id_;

  // The guid used to identify the group this button represents.
  base::Uuid guid_;

  // The local guid used to identify the group in the tabstrip if it is open.
  absl::optional<tab_groups::TabGroupId> local_group_id_;

  // The tabs to be displayed in the context menu. Currently supports tab
  // title, url, and favicon.
  std::vector<SavedTabGroupTab> tabs_;

  const raw_ref<Browser> browser_;

  const raw_ref<SavedTabGroupKeyedService> service_;

  // A callback used to fetch the current PageNavigator used to open URLs.
  const base::RepeatingCallback<content::PageNavigator*()>
      page_navigator_callback_;

  // Context menu controller used for this View.
  views::DialogModelContextMenuController context_menu_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BUTTON_H_

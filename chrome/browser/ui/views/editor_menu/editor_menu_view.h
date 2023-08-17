// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos::editor_menu {

// A bubble style view to show Editor Menu.
class EditorMenuView : public views::View {
 public:
  METADATA_HEADER(EditorMenuView);

  explicit EditorMenuView(const gfx::Rect& anchor_view_bounds);

  EditorMenuView(const EditorMenuView&) = delete;
  EditorMenuView& operator=(const EditorMenuView&) = delete;

  ~EditorMenuView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void AddTitleContainer();

  // Containing title, badge, and icons.
  raw_ptr<views::View> title_container_ = nullptr;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

#include <vector>

#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageButton;
class FlexLayoutView;
}  // namespace views

namespace chromeos::editor_menu {

class EditorMenuChipView;
class EditorMenuTextfieldView;
class PreTargetHandler;

// A bubble style view to show Editor Menu.
class EditorMenuView : public views::View, public views::WidgetObserver {
 public:
  METADATA_HEADER(EditorMenuView);

  explicit EditorMenuView(const gfx::Rect& anchor_view_bounds);

  EditorMenuView(const EditorMenuView&) = delete;
  EditorMenuView& operator=(const EditorMenuView&) = delete;

  ~EditorMenuView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds);

  // views::View:
  void AddedToWidget() override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void AddTitleContainer();
  void AddChipsContainer();
  void AddTextfield();

  std::unique_ptr<PreTargetHandler> pre_target_handler_;

  // Containing title, badge, and icons.
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  // Containing chips.
  raw_ptr<views::FlexLayoutView> chips_container_ = nullptr;
  std::vector<raw_ptr<EditorMenuChipView>> chips_;

  raw_ptr<EditorMenuTextfieldView> textfield_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

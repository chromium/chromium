// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageButton;
class FlexLayoutView;
class View;
}  // namespace views

namespace chromeos::editor_menu {

class EditorMenuTextfieldView;
class EditorMenuViewDelegate;
class PreTargetHandler;

// A bubble style view to show Editor Menu.
class EditorMenuView : public views::View, public views::WidgetObserver {
 public:
  METADATA_HEADER(EditorMenuView);

  EditorMenuView(const PresetTextQueries& preset_text_queries,
                 const gfx::Rect& anchor_view_bounds,
                 EditorMenuViewDelegate* delegate);

  EditorMenuView(const EditorMenuView&) = delete;
  EditorMenuView& operator=(const EditorMenuView&) = delete;

  ~EditorMenuView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const PresetTextQueries& preset_text_queries,
      const gfx::Rect& anchor_view_bounds,
      EditorMenuViewDelegate* delegate);

  // views::View:
  void AddedToWidget() override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

  const views::View* chips_container_for_testing() const {
    return chips_container_;
  }

 private:
  void InitLayout(const PresetTextQueries& preset_text_queries);
  void AddTitleContainer();
  void AddChipsContainer(const PresetTextQueries& preset_text_queries);
  void AddTextfield();

  void UpdateChipsContainer(int editor_menu_width);

  views::View* AddChipsRow();

  void OnSettingsButtonPressed();
  void OnChipButtonPressed(const std::string& text_query_id);

  void ResetPreTargetHandler();

  std::unique_ptr<PreTargetHandler> pre_target_handler_;

  // `delegate_` outlives `this`.
  raw_ptr<EditorMenuViewDelegate> delegate_ = nullptr;

  // Containing title, badge, and icons.
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  // Containing chips.
  raw_ptr<views::FlexLayoutView> chips_container_ = nullptr;

  raw_ptr<EditorMenuTextfieldView> textfield_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<EditorMenuView> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_H_

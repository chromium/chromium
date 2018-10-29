// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class TabStripModel;

namespace test {
class ToolbarButtonTestApi;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

// This class provides basic drawing and mouse-over behavior for buttons
// appearing in the toolbar.
// TODO: Consider making ToolbarButton and AppMenuButton share a common base
// class https://crbug.com/819854.
class ToolbarButton : public views::LabelButton,
                      public views::ContextMenuController {
 public:
  // More convenient form of the ctor below, when |model| and |tab_strip_model|
  // are both nullptr.
  explicit ToolbarButton(views::ButtonListener* listener);

  // |listener| and |tab_strip_model| must outlive this class.
  // |model| can be null if no menu is to be shown.
  // |tab_strip_model| is only needed if showing the menu with |model| requires
  // an active tab. There may be no active tab in |tab_strip_model| during
  // shutdown.
  ToolbarButton(views::ButtonListener* listener,
                std::unique_ptr<ui::MenuModel> model,
                TabStripModel* tab_strip_model,
                bool trigger_menu_on_long_press = true);

  ~ToolbarButton() override;

  // Set up basic mouseover border behavior.
  // Should be called before first paint.
  void Init();

  void SetHighlightColor(base::Optional<SkColor> color);

  // Sets |margin_leading_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetLeadingMargin(int margin);

  // Methods for handling ButtonDropDown-style menus.
  void ClearPendingMenu();
  bool IsMenuShowing() const;

  // views::LabelButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  // Showing the drop down results in a MouseCaptureLost, we need to ignore it.
  void OnMouseCaptureLost() override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;

  // views::ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  ui::MenuModel* menu_model_for_test() { return model_.get(); }

 protected:
  // Returns if menu should be shown. Override this to change default behavior.
  virtual bool ShouldShowMenu();

  // Function to show the dropdown menu.
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type);

  // Sets |layout_inset_delta_|, see comment there.
  void SetLayoutInsetDelta(const gfx::Insets& insets);

 private:
  friend test::ToolbarButtonTestApi;

  void UpdateHighlightBackgroundAndInsets();

  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  // views::ImageButton:
  const char* GetClassName() const override;

  // The model that populates the attached menu.
  std::unique_ptr<ui::MenuModel> model_;

  TabStripModel* const tab_strip_model_;

  // Indicates if menu is currently showing.
  bool menu_showing_ = false;

  // Whether the menu should be shown when there is a long mouse press or a drag
  // event.
  const bool trigger_menu_on_long_press_;

  // Y position of mouse when left mouse button is pressed.
  int y_position_on_lbuttondown_ = 0;

  // The model adapter for the drop down menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Menu runner to display drop down menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Leading margin to be applied. Used when the browser is in a maximized state
  // to extend to the full window width.
  int leading_margin_ = 0;

  // Delta from regular toolbar-button insets. This is necessary for buttons
  // that use smaller or larger icons than regular ToolbarButton instances.
  // AvatarToolbarButton for instance uses smaller insets to accommodate for a
  // larger-than-16dp avatar avatar icon outside of touchable mode.
  gfx::Insets layout_inset_delta_;

  // A highlight color is used to signal error states. When set this color is
  // used as a base for background, text and ink drops. When not set, uses the
  // default ToolbarButton ink drop.
  base::Optional<SkColor> highlight_color_;

  // A factory for tasks that show the dropdown context menu for the button.
  base::WeakPtrFactory<ToolbarButton> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

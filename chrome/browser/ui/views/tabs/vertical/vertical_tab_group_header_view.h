// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_tracker.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/flex_layout_view.h"

namespace tab_groups {
class TabGroupVisualData;
}

namespace views {
class LabelButton;
class ImageView;
class Label;
}

// View for a tab group header in the vertical tabstrip.
class VerticalTabGroupHeaderView : public views::FlexLayoutView,
                                   public views::ContextMenuController {
  METADATA_HEADER(VerticalTabGroupHeaderView, views::FlexLayoutView)

 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ToggleCollapsedState(
        ToggleTabGroupCollapsedStateOrigin origin) = 0;
    virtual views::Widget* ShowGroupEditorBubble(
        bool stop_context_menu_propagation) = 0;
    virtual std::u16string GetGroupContentString() const = 0;
  };

  explicit VerticalTabGroupHeaderView(
      Delegate* delegate,
      const tab_groups::TabGroupVisualData* tab_group_visual_data);
  VerticalTabGroupHeaderView(const VerticalTabGroupHeaderView&) = delete;
  VerticalTabGroupHeaderView& operator=(const VerticalTabGroupHeaderView&) =
      delete;
  ~VerticalTabGroupHeaderView() override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  void OnDataChanged(
      const tab_groups::TabGroupVisualData* tab_group_visual_data,
      bool needs_attention,
      bool is_shared);

  views::LabelButton* editor_bubble_button() { return editor_bubble_button_; }
  views::ImageView* collapse_icon_for_testing() { return collapse_icon_; }
  views::ImageView* attention_indicator_for_testing() {
    return attention_indicator_;
  }

 private:
  void UpdateEditorBubbleButtonVisibility();
  void ShowEditorBubble();
  void UpdateAccessibleName(
      const tab_groups::TabGroupVisualData* tab_group_visual_data);
  void UpdateIsCollapsed(
      const tab_groups::TabGroupVisualData* tab_group_visual_data);

  // The sync icon that is displayed in the tab group header of saved groups in
  // the tabstrip.
  const raw_ptr<views::ImageView> sync_icon_ = nullptr;

  const raw_ptr<views::Label> group_header_label_ = nullptr;

  // The circle indicator rendered after the title when a tab group needs
  // attention.
  const raw_ptr<views::ImageView> attention_indicator_ = nullptr;

  const raw_ptr<views::LabelButton> editor_bubble_button_ = nullptr;

  const raw_ptr<views::ImageView> collapse_icon_ = nullptr;
  const raw_ptr<Delegate> delegate_ = nullptr;

  TabGroupEditorBubbleTracker editor_bubble_tracker_;
  base::CallbackListSubscription editor_bubble_opened_subscription_;
  base::CallbackListSubscription editor_bubble_closed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_

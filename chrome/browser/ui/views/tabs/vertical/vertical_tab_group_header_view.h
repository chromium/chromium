// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_tracker.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout_view.h"

class TabGroup;

namespace tabs {
struct TabGroupData;
class VerticalTabStripStateController;
}

namespace views {
class LabelButton;
class ImageView;
class Label;
}  // namespace views

// The view for the tab group header. It displays the tab group
// title, editor icon and the collapsed/expand icon.
class VerticalTabGroupHeaderView : public views::FlexLayoutView,
                                   public views::ContextMenuController,
                                   public views::FocusChangeListener,
                                   public HoverCardAnchorTarget {
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

    virtual bool IsValid() const = 0;
    virtual void InitHeaderDrag(const ui::LocatedEvent& event) = 0;
    virtual bool ContinueHeaderDrag(const ui::LocatedEvent& event) = 0;
    virtual void CancelHeaderDrag() = 0;
    virtual const TabGroup& GetTabGroup() const = 0;
    virtual void UpdateHoverCard(int update_type) const = 0;
    virtual void HideHoverCard(int update_type) const = 0;
    virtual bool IsFocusInTabStrip() = 0;
    virtual std::unique_ptr<ExpandOnHoverLock> AcquireExpandOnHoverLock() = 0;

    virtual void ShiftGroupUp() = 0;
    virtual void ShiftGroupDown() = 0;
  };

  VerticalTabGroupHeaderView(
      Delegate& delegate,
      tabs::VerticalTabStripStateController* state_controller,
      const tab_groups::TabGroupVisualData* tab_group_visual_data);
  VerticalTabGroupHeaderView(const VerticalTabGroupHeaderView&) = delete;
  VerticalTabGroupHeaderView& operator=(const VerticalTabGroupHeaderView&) =
      delete;
  ~VerticalTabGroupHeaderView() override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // HoverCardAnchorTarget:
  bool NeedsToShowThumbnail() const override;
  bool IsValidHoverCardTarget() const override;
  views::BubbleAnchor GetAnchor() override;
  views::BubbleBorder::Arrow GetAnchorPosition() const override;

  void OnDataChanged(const tabs::TabGroupData& tab_group_data);

  views::LabelButton* editor_bubble_button() { return editor_bubble_button_; }
  views::ImageView* collapse_icon_for_testing() { return collapse_icon_; }
  views::ImageView* attention_indicator_for_testing() {
    return attention_indicator_;
  }

 private:
  void UpdateEditorBubbleButtonVisibility();
  void OnBubbleOpened();
  void OnBubbleClosed();
  // Bypasses the synchronous IsMouseHovered() check which can be stale on Linux
  // Wayland/X11 due to asynchronous cursor updates during mouse exit events.
  void SetEditorBubbleButtonVisibilityOnHover(bool is_hovered);
  void ShowEditorBubble();
  void UpdateAccessibleName();
  void UpdateTooltipText();
  void UpdateIsCollapsed();
  void UpdateAttentionState(bool needs_attention);

  SkColor GetForegroundColor() const;

  tab_groups::TabGroupVisualData tab_group_visual_data_;
  bool needs_attention_ = false;
  bool is_shared_ = false;

  // The sync icon that is displayed in the tab group header of saved groups in
  // the tabstrip.
  const raw_ptr<views::ImageView> sync_icon_ = nullptr;

  const raw_ptr<views::Label> group_header_label_ = nullptr;

  // The circle indicator rendered after the title when a tab group needs
  // attention.
  const raw_ptr<views::ImageView> attention_indicator_ = nullptr;

  const raw_ptr<views::LabelButton> editor_bubble_button_ = nullptr;

  const raw_ptr<views::ImageView> collapse_icon_ = nullptr;
  const raw_ref<Delegate> delegate_;

  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;
  TabGroupEditorBubbleTracker editor_bubble_tracker_;
  base::CallbackListSubscription editor_bubble_opened_subscription_;
  base::CallbackListSubscription editor_bubble_closed_subscription_;
  base::CallbackListSubscription tab_group_data_changed_subscription_;
  base::WeakPtrFactory<VerticalTabGroupHeaderView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_

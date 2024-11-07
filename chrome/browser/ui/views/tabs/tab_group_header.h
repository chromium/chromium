// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"

class TabSlotController;
class TabGroupStyle;
struct TabSizeInfo;
class TabStyle;

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

// View for tab group headers in the tab strip, which are markers of group
// boundaries. There is one header for each group, which is included in the tab
// strip flow and positioned left of the leftmost tab in the group.
class TabGroupHeader : public TabSlotView,
                       public views::ContextMenuController,
                       public views::ViewTargeterDelegate {
  METADATA_HEADER(TabGroupHeader, TabSlotView)

 public:
  TabGroupHeader(TabSlotController& tab_slot_controller,
                 const tab_groups::TabGroupId& group,
                 const TabGroupStyle& style);
  TabGroupHeader(const TabGroupHeader&) = delete;
  TabGroupHeader& operator=(const TabGroupHeader&) = delete;
  ~TabGroupHeader() override;

  // TabSlotView:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnFocus() override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  TabSlotView::ViewType GetTabSlotViewType() const override;
  TabSizeInfo GetTabSizeInfo() const override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  gfx::Rect GetAnchorBoundsInScreen() const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Updates our visual state according to the tab_groups::TabGroupVisualData
  // for our group.
  // TODO(crbug.com/372296676): Make TabGroupHeader observe the group for
  // changes to cut down on the number of times we recalculate the view.
  void VisualsChanged();

  int GetCollapsedHeaderWidth() const;

  // Removes {editor_bubble_tracker_} from observing the widget.
  void RemoveObserverFromWidget(views::Widget* widget);

 private:
  friend class TabGroupEditorBubbleViewDialogBrowserTest;

  // Calculate the width for this View.
  int GetDesiredWidth() const;
  // Determines if the sync icon should be shown in the header.
  bool ShouldShowHeaderIcon() const;

  void UpdateIsCollapsed();

  void UpdateTitleView();
  void UpdateSyncIconView();

  // Creates a squircle (cross between a square and a circle).
  void CreateHeaderWithoutTitle();
  // Creates a round rect, similar to the shape of a tab when hovered but not
  // selected.
  void CreateHeaderWithTitle();

  const raw_ref<TabSlotController> tab_slot_controller_;

  // The title chip for the tab group header which comprises of title text if
  // there is any, and a background color. The size and color of the chip are
  // set in VisualsChanged().
  const raw_ptr<views::View> title_chip_;

  // The title of the tab group. Text and color of the title are set in
  // VisualsChanged().
  const raw_ptr<views::Label> title_;

  // The sync icon that is displayed in the tab group header of saved groups in
  // the tabstrip.
  const raw_ptr<views::ImageView> sync_icon_;

  const raw_ref<const TabGroupStyle> group_style_;
  const raw_ptr<const TabStyle> tab_style_;

  // The current title of the group.
  std::u16string group_title_;

  // The current color of the group.
  SkColor color_;

  // Determines if we should show the header icon in front of the title.
  bool should_show_header_icon_;

  // Local saved collapsed state. When this differs from
  // `TabSlotController::IsGroupCollapsed()`, then the collapsed state has
  // changed in the model and we need to react to that.
  bool is_collapsed_;

  // Tracks whether our editor bubble is open. At most one can be open
  // at once.
  class EditorBubbleTracker : public views::WidgetObserver {
   public:
    explicit EditorBubbleTracker(TabSlotController& tab_slot_controller);
    ~EditorBubbleTracker() override;

    void Opened(views::Widget* bubble_widget);
    bool is_open() const { return is_open_; }
    views::Widget* widget() const { return widget_; }

    // views::WidgetObserver:
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    bool is_open_ = false;
    raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> widget_;
    // Outlives this because it's a dependency inversion interface for the
    // header's parent View.
    raw_ref<TabSlotController> tab_slot_controller_;
  };

  EditorBubbleTracker editor_bubble_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/common/feature_switch.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"
#include "ui/views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer::Delegate

bool BrowserActionsContainer::Delegate::CanShowIconInToolbar() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer::DropPosition

struct BrowserActionsContainer::DropPosition {
  DropPosition(size_t row, size_t icon_in_row);

  // The (0-indexed) row into which the action will be dropped.
  size_t row;

  // The (0-indexed) icon in the row before the action will be dropped.
  size_t icon_in_row;
};

BrowserActionsContainer::DropPosition::DropPosition(
    size_t row, size_t icon_in_row)
    : row(row), icon_in_row(icon_in_row) {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(
    Browser* browser,
    BrowserActionsContainer* main_container,
    Delegate* delegate,
    bool interactive)
    : AnimationDelegateViews(this),
      delegate_(delegate),
      browser_(browser),
      main_container_(main_container),
      interactive_(interactive) {
  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  toolbar_actions_bar_ = delegate_->CreateToolbarActionsBar(
      this, browser,
      main_container ? main_container->toolbar_actions_bar_.get() : nullptr);

  if (!ShownInsideMenu()) {
    if (interactive_) {
      resize_area_ = new views::ResizeArea(this);
      AddChildView(resize_area_);
    }
    resize_animation_ = std::make_unique<gfx::SlideAnimation>(this);

    if (GetSeparatorAreaWidth() > 0) {
      separator_ = new views::Separator();
      AddChildView(separator_);
    }
  } else {
    DCHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  }
}

BrowserActionsContainer::~BrowserActionsContainer() {
  if (active_bubble_)
    active_bubble_->GetWidget()->Close();
  // We should synchronously receive the OnWidgetClosing() event, so we should
  // always have cleared the active bubble by now.
  DCHECK(!active_bubble_);

  toolbar_actions_bar_->DeleteActions();
  // All views should be removed as part of ToolbarActionsBar::DeleteActions().
  DCHECK(toolbar_action_views_.empty());
}

std::string BrowserActionsContainer::GetIdAt(size_t index) const {
  return toolbar_action_views_[index]->view_controller()->GetId();
}

ToolbarActionView* BrowserActionsContainer::GetViewForId(
    const std::string& id) {
  for (const auto& view : toolbar_action_views_) {
    if (view->view_controller()->GetId() == id)
      return view.get();
  }
  return nullptr;
}

void BrowserActionsContainer::RefreshToolbarActionViews() {
  toolbar_actions_bar_->Update();
}

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (const auto& view : toolbar_action_views_) {
    if (view->GetVisible())
      ++visible_actions;
  }
  return visible_actions;
}

size_t BrowserActionsContainer::VisibleBrowserActionsAfterAnimation() const {
  if (!animating())
    return VisibleBrowserActions();

  return WidthToIconCount(animation_target_size_);
}

bool BrowserActionsContainer::ShownInsideMenu() const {
  return main_container_ != nullptr;
}

bool BrowserActionsContainer::CanShowIconInToolbar() const {
  return delegate_->CanShowIconInToolbar();
}

void BrowserActionsContainer::OnToolbarActionViewDragDone() {
  toolbar_actions_bar_->OnDragEnded();
}

views::LabelButton* BrowserActionsContainer::GetOverflowReferenceView() const {
  return delegate_->GetOverflowReferenceView();
}

gfx::Size BrowserActionsContainer::GetToolbarActionSize() {
  return toolbar_actions_bar_->GetViewSize();
}

void BrowserActionsContainer::AddViewForAction(
    ToolbarActionViewController* view_controller,
    size_t index) {
  ToolbarActionView* view = new ToolbarActionView(view_controller, this);
  toolbar_action_views_.insert(toolbar_action_views_.begin() + index,
                               base::WrapUnique(view));
  AddChildViewAt(view, index);
  PreferredSizeChanged();
}

void BrowserActionsContainer::RemoveViewForAction(
    ToolbarActionViewController* action) {
  std::unique_ptr<ToolbarActionView> view;
  for (auto iter = toolbar_action_views_.begin();
       iter != toolbar_action_views_.end(); ++iter) {
    if ((*iter)->view_controller() == action) {
      std::swap(view, *iter);
      toolbar_action_views_.erase(iter);
      break;
    }
  }
  PreferredSizeChanged();
}

void BrowserActionsContainer::RemoveAllViews() {
  toolbar_action_views_.clear();
  PreferredSizeChanged();
}

void BrowserActionsContainer::Redraw(bool order_changed) {
  if (!added_to_view_) {
    // We don't want to redraw before the view has been fully added to the
    // hierarchy.
    return;
  }

  // Need to update the resize area because resizing is not allowed when the
  // actions bar is highlighting.
  UpdateResizeArea();

  if (order_changed) {
    // Run through the views and compare them to the desired order. If something
    // is out of place, find the correct spot for it.
    std::vector<ToolbarActionViewController*> actions =
        toolbar_actions_bar_->GetActions();
    for (int i = 0; i < static_cast<int>(actions.size()) - 1; ++i) {
      if (actions[i] != toolbar_action_views_[i]->view_controller()) {
        // Find where the correct view is (it's guaranteed to be after our
        // current index, since everything up to this point is correct).
        int j = i + 1;
        while (actions[i] != toolbar_action_views_[j]->view_controller())
          ++j;
        std::swap(toolbar_action_views_[i], toolbar_action_views_[j]);
        // Also move the view in the child views vector.
        ReorderChildView(toolbar_action_views_[i].get(), i);
      }
    }
  }

  if (separator_)
    ReorderChildView(separator_, -1);

  Layout();
}

void BrowserActionsContainer::ResizeAndAnimate(gfx::Tween::Type tween_type,
                                               int target_width) {
  // TODO(pbos): Make this method show N icons and derive target_width using
  // GetWidthForIconCount.
  if (toolbar_actions_bar_->WidthToIconCount(target_width) > 0)
    target_width += GetSeparatorAreaWidth();
  target_width += GetResizeAreaWidth();

  if (resize_animation_ && !toolbar_actions_bar_->suppress_animation()) {
    if (!ShownInsideMenu()) {
      // Make sure we don't try to animate to wider than the allowed width.
      base::Optional<int> max_width = delegate_->GetMaxBrowserActionsWidth();
      if (max_width && target_width > max_width.value())
        target_width = GetWidthForMaxWidth(max_width.value());
    }
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    resize_starting_width_ = width();
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_width;
    resize_animation_->Show();
  } else {
    if (resize_animation_)
      resize_animation_->Reset();
    animation_target_size_ = target_width;
    AnimationEnded(resize_animation_.get());
  }
}

int BrowserActionsContainer::GetWidth(GetWidthTime get_width_time) const {
  // This call originates from ToolbarActionsBar which wants to know how much
  // space is / will be used for action icons (excluding the separator).
  const int target_width =
      get_width_time == GET_WIDTH_AFTER_ANIMATION && animating()
          ? animation_target_size_
          : width();
  const int icon_area_width =
      target_width - GetSeparatorAreaWidth() - GetResizeAreaWidth();
  // This needs to be clamped to non-zero as ToolbarActionsBar::ResizeDelegate
  // uses this value to distinguish between an empty bar without items and a bar
  // that is showing no items.
  // TODO(pbos): This is landed to fix to https://crbug.com/836182. Remove the
  // need for this when ToolbarActionsBar and BrowserActionsContainer merges.
  return std::max(toolbar_actions_bar_->GetMinimumWidth(), icon_area_width);
}

bool BrowserActionsContainer::IsAnimating() const {
  return animating();
}

void BrowserActionsContainer::StopAnimating() {
  animation_target_size_ = width();
  resize_animation_->Reset();
}

void BrowserActionsContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  // The container shouldn't be asked to show a bubble if it's animating.
  DCHECK(!animating());
  DCHECK(!active_bubble_);

  views::View* anchor_view = nullptr;
  bool anchored_to_action_view = false;
  if (!controller->GetAnchorActionId().empty()) {
    ToolbarActionView* action_view =
        GetViewForId(controller->GetAnchorActionId());
    if (action_view) {
      anchor_view =
          action_view->GetVisible() ? action_view : GetOverflowReferenceView();
      anchored_to_action_view = true;
    } else {
      anchor_view = BrowserView::GetBrowserViewForBrowser(browser_)
                        ->toolbar_button_provider()
                        ->GetAppMenuButton();
    }
  } else {
    anchor_view = this;
  }

  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_view, anchored_to_action_view, std::move(controller));
  active_bubble_ = bubble;
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->GetWidget()->AddObserver(this);
  bubble->Show();
}

bool BrowserActionsContainer::CloseOverflowMenuIfOpen() {
  AppMenuButton* app_menu_button =
      BrowserView::GetBrowserViewForBrowser(browser_)
          ->toolbar_button_provider()
          ->GetAppMenuButton();
  if (!app_menu_button || !app_menu_button->IsMenuShowing())
    return false;

  app_menu_button->CloseMenu();
  return true;
}

void BrowserActionsContainer::OnWidgetClosing(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void BrowserActionsContainer::OnWidgetDestroying(views::Widget* widget) {
  ClearActiveBubble(widget);
}

int BrowserActionsContainer::GetWidthForMaxWidth(int max_width) const {
  DCHECK_GE(max_width, 0);
  int preferred_width = GetPreferredSize().width();
  if (preferred_width > max_width) {
    // If we're trying to be nonzero width, we should make sure we at least ask
    // for enough space to show the resize handle (if there are no icons, we
    // will ask for a width of zero so it won't matter).
    preferred_width =
        std::max(GetResizeAreaWidth(), GetWidthForIconCount(WidthToIconCount(
                                           max_width - GetResizeAreaWidth())));
  }
  return preferred_width;
}

// static
views::FlexRule BrowserActionsContainer::GetFlexRule() {
  // We only want to flex to widths which are integer multiples of the icon
  // size, plus the size of the drag handle. The one exception is if there are
  // no extensions at all.
  return base::BindRepeating(
      [](const views::View* view, const views::SizeBounds& maximum_size) {
        const BrowserActionsContainer* browser_actions =
            static_cast<const BrowserActionsContainer*>(view);
        gfx::Size preferred_size = browser_actions->GetPreferredSize();
        if (maximum_size.width()) {
          int width;
          if (browser_actions->resizing() || browser_actions->animating()) {
            // When there are actions present, the floor on the size of the
            // browser actions bar should be the resize handle.
            const int min_width = browser_actions->num_toolbar_actions() == 0
                                      ? 0
                                      : browser_actions->GetResizeAreaWidth();
            // The ceiling on the value is the lesser of the preferred and
            // available size.
            width = std::max(min_width, std::min(preferred_size.width(),
                                                 *maximum_size.width()));
          } else {
            // When not animating or resizing, the desired width should always
            // be based on the number of icons that can be displayed.
            width = browser_actions->GetWidthForMaxWidth(*maximum_size.width());
          }
          preferred_size =
              gfx::Size(width, browser_actions->GetHeightForWidth(width));
        }
        return preferred_size;
      });
}

void BrowserActionsContainer::SetSeparatorColor(SkColor color) {
  if (separator_)
    separator_->SetColor(color);
}

gfx::Size BrowserActionsContainer::CalculatePreferredSize() const {
  if (ShownInsideMenu())
    return toolbar_actions_bar_->GetFullSize();

  // If there are no actions to show, then don't show the container at all.
  if (toolbar_action_views_.empty())
    return gfx::Size();

  int preferred_width;
  if (resize_starting_width_) {
    // When resizing, preferred width is the starting width - resize amount.
    preferred_width = *resize_starting_width_ - resize_amount_;
  } else {
    // Otherwise, use the normal preferred width.
    preferred_width =
        GetResizeAreaWidth() + toolbar_actions_bar_->GetFullSize().width();
    if (toolbar_actions_bar_->GetIconCount() > 0)
      preferred_width += GetSeparatorAreaWidth();
  }

  // The view should never be resized past the largest size or smaller than the
  // empty width (including drag handle), clamp preferred size to reflect this.
  preferred_width = base::ClampToRange(preferred_width, GetResizeAreaWidth(),
                                       GetWidthWithAllActionsVisible());

  return gfx::Size(preferred_width,
                   toolbar_actions_bar_->GetViewSize().height());
}

int BrowserActionsContainer::GetHeightForWidth(int width) const {
  if (ShownInsideMenu())
    toolbar_actions_bar_->SetOverflowRowWidth(width);
  return GetPreferredSize().height();
}

gfx::Size BrowserActionsContainer::GetMinimumSize() const {
  DCHECK(interactive_);
  return gfx::Size(GetResizeAreaWidth(),
                   toolbar_actions_bar_->GetViewSize().height());
}

void BrowserActionsContainer::Layout() {
  if (toolbar_actions_bar()->suppress_layout())
    return;

  if (toolbar_action_views_.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  if (resize_area_)
    resize_area_->SetBounds(0, 0, GetResizeAreaWidth(), height());

  // The range of visible icons, from start_index (inclusive) to end_index
  // (exclusive).
  size_t start_index = toolbar_actions_bar()->GetStartIndexInBounds();
  size_t end_index = toolbar_actions_bar()->GetEndIndexInBounds();

  // Now draw the icons for the actions in the available space. Once all the
  // variables are in place, the layout works equally well for the main and
  // overflow container.
  for (size_t i = 0u; i < toolbar_action_views_.size(); ++i) {
    ToolbarActionView* view = toolbar_action_views_[i].get();
    if (i < start_index || i >= end_index) {
      view->SetVisible(false);
    } else {
      gfx::Rect bounds = toolbar_actions_bar()->GetFrameForIndex(i);
      // Offset all icons by GetResizeAreaWidth() so that they start on the
      // resize area's right edge. ToolbarActionsBar is not aware of the
      // separate resize area.
      // TODO(pbos): Remove this workaround when the files merge.
      bounds.set_x(bounds.x() + GetResizeAreaWidth());
      // Vertically center the icons if the available height is not enough.
      // TODO(https://889745): Remove the possibility of there not being enough
      // available height.
      if (bounds.height() > height())
        bounds.set_y((height() - bounds.height()) / 2);
      view->SetBoundsRect(bounds);
      view->SetVisible(true);
      // TODO(corising): Move setting background to
      // ToolbarActionsBar::OnToolbarHighlightModeChanged when the files merge.
      if (!ShownInsideMenu() && (toolbar_actions_bar()->is_highlighting() !=
                                 (view->background() != nullptr))) {
        // Sets background to reflect whether the item is being highlighted.
        const gfx::Insets bg_insets(
            (height() - GetLayoutConstant(LOCATION_BAR_HEIGHT)) / 2);
        const int corner_radius = height() / 2;
        const SkColor bg_color = SkColorSetA(view->GetInkDropBaseColor(),
                                             kToolbarButtonBackgroundAlpha);
        view->SetBackground(
            toolbar_actions_bar()->is_highlighting()
                ? views::CreateBackgroundFromPainter(
                      views::Painter::CreateSolidRoundRectPainter(
                          bg_color, corner_radius, bg_insets))
                : nullptr);
      }
    }
  }
  if (separator_) {
    separator_->SetSize(gfx::Size(views::Separator::kThickness,
                                  GetLayoutConstant(LOCATION_BAR_ICON_SIZE)));
    if (width() < GetResizeAreaWidth() + GetSeparatorAreaWidth()) {
      separator_->SetVisible(false);
    } else {
      // Position separator_ in the center of the separator area.
      separator_->SetPosition(gfx::Point(
          width() - GetSeparatorAreaWidth() / 2 - separator_->width(),
          (height() - separator_->height()) / 2));
      separator_->SetVisible(true);
    }
  }
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool BrowserActionsContainer::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserActionsContainer::CanDrop(const OSExchangeData& data) {
  return interactive_ &&
         BrowserActionDragData::CanDrop(data, browser_->profile());
}

int BrowserActionsContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  size_t row_index = 0;
  size_t before_icon_in_row = 0;
  // If there are no visible actions (such as when dragging an icon to an empty
  // overflow/main container), then 0, 0 for row, column is correct.
  if (VisibleBrowserActions() != 0) {
    // Figure out where to display the indicator.

    // First, since we want to switch from displaying the indicator before an
    // icon to after it when the event passes the midpoint of the icon, add
    // (icon width / 2) and divide by the icon width. This will convert the
    // event coordinate into the index of the icon we want to display the
    // indicator before. We also mirror the event.x() so that our calculations
    // are consistent with left-to-right.
    const auto size = toolbar_actions_bar_->GetViewSize();
    const int offset_into_icon_area = GetMirroredXInView(event.x()) -
                                      GetResizeAreaWidth() + (size.width() / 2);
    const int before_icon_unclamped =
        toolbar_actions_bar_->WidthToIconCount(offset_into_icon_area);

    // We need to figure out how many icons are visible on the relevant row.
    // In the main container, this will just be the visible actions.
    int visible_icons_on_row = VisibleBrowserActionsAfterAnimation();
    if (ShownInsideMenu()) {
      // Next, figure out what row we're on.
      const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
      row_index =
          (event.y() + element_padding) / (size.height() + element_padding);

      const int icons_per_row = platform_settings().icons_per_overflow_menu_row;
      // If this is the final row of the overflow, then this is the remainder of
      // visible icons. Otherwise, it's a full row (kIconsPerRow).
      visible_icons_on_row =
          row_index ==
              static_cast<size_t>(visible_icons_on_row / icons_per_row) ?
                  visible_icons_on_row % icons_per_row : icons_per_row;
    }

    // Because the user can drag outside the container bounds, we need to clamp
    // to the valid range. Note that the maximum allowable value is (num icons),
    // not (num icons - 1), because we represent the indicator being past the
    // last icon as being "before the (last + 1) icon".
    before_icon_in_row =
        base::ClampToRange(before_icon_unclamped, 0, visible_icons_on_row);
  }

  if (!drop_position_.get() ||
      !(drop_position_->row == row_index &&
        drop_position_->icon_in_row == before_icon_in_row)) {
    drop_position_ =
        std::make_unique<DropPosition>(row_index, before_icon_in_row);
    SchedulePaint();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
  drop_position_.reset();
  SchedulePaint();
}

int BrowserActionsContainer::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  // Make sure we have the same view as we started with.
  DCHECK_EQ(GetIdAt(data.index()), data.id());

  size_t i = GetDropPositionIndex();

  // |i| now points to the item to the right of the drop indicator*, which is
  // correct when dragging an icon to the left. When dragging to the right,
  // however, we want the icon being dragged to get the index of the item to
  // the left of the drop indicator, so we subtract one.
  // * Well, it can also point to the end, but not when dragging to the left. :)
  if (i > data.index())
    --i;

  ToolbarActionsBar::DragType drag_type = ToolbarActionsBar::DRAG_TO_SAME;
  if (!toolbar_action_views_[data.index()]->GetVisible())
    drag_type = ShownInsideMenu() ? ToolbarActionsBar::DRAG_TO_OVERFLOW :
        ToolbarActionsBar::DRAG_TO_MAIN;

  toolbar_actions_bar_->OnDragDrop(data.index(), i, drag_type);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_EXTENSIONS));
}

void BrowserActionsContainer::WriteDragDataForView(View* sender,
                                                   const gfx::Point& press_pt,
                                                   OSExchangeData* data) {
  DCHECK(data);

  auto it =
      std::find_if(toolbar_action_views_.cbegin(), toolbar_action_views_.cend(),
                   [sender](const std::unique_ptr<ToolbarActionView>& ptr) {
                     return ptr.get() == sender;
                   });
  DCHECK(it != toolbar_action_views_.cend());

  size_t index = it - toolbar_action_views_.cbegin();
  toolbar_actions_bar_->OnDragStarted(index);

  ToolbarActionViewController* view_controller = (*it)->view_controller();
  data->provider().SetDragImage(
      view_controller
          ->GetIcon(GetCurrentWebContents(),
                    toolbar_actions_bar_->GetViewSize())
          .AsImageSkia(),
      press_pt.OffsetFromOrigin());
  // Fill in the remaining info.
  BrowserActionDragData drag_data(view_controller->GetId(), index);
  drag_data.Write(browser_->profile(), data);
}

int BrowserActionsContainer::GetDragOperationsForView(View* sender,
                                                      const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDragForView(View* sender,
                                                  const gfx::Point& press_pt,
                                                  const gfx::Point& p) {
  // We don't allow dragging while we're highlighting.
  return interactive_ && !toolbar_actions_bar_->is_highlighting();
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  // We don't allow resize while the toolbar is highlighting a subset of
  // actions, since this is a temporary and entirely browser-driven sequence in
  // order to warn the user about potentially dangerous items.
  // We also don't allow resize when the bar is already animating, since we
  // don't want two competing size changes.
  if (toolbar_actions_bar_->is_highlighting() || animating())
    return;

  // If this is the start of the resize gesture, initialize the starting
  // width.
  if (!resize_starting_width_)
    resize_starting_width_ = width();

  resize_amount_ = resize_amount;

  if (!done_resizing) {
    PreferredSizeChanged();
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  int icon_area_width =
      std::max(toolbar_actions_bar_->GetMinimumWidth(),
               CalculatePreferredSize().width() - GetSeparatorAreaWidth() -
                   GetResizeAreaWidth());
  // As we're done resizing, reset the starting width to reflect this after
  // calculating the final size based on it.
  resize_starting_width_.reset();
  toolbar_actions_bar_->OnResizeComplete(icon_area_width);
}

void BrowserActionsContainer::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // When bounds change, it's possible that the amount of space available to the
  // view changes as well. If the amount of space is not enough to fit a single
  // icon, the resize handle should be disabled.
  UpdateResizeArea();
}

void BrowserActionsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  DCHECK(resize_starting_width_);
  resize_amount_ =
      static_cast<int>(resize_animation_->GetCurrentValue() *
                       (*resize_starting_width_ - animation_target_size_));
  PreferredSizeChanged();
}

void BrowserActionsContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void BrowserActionsContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_target_size_ = 0;
  resize_amount_ = 0;
  resize_starting_width_.reset();
  PreferredSizeChanged();

  toolbar_actions_bar_->OnAnimationEnded();
}

content::WebContents* BrowserActionsContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserActionsContainer::OnPaint(gfx::Canvas* canvas) {
  // TODO(sky/glen): Instead of using a drop indicator, animate the icons while
  // dragging (like we do for tab dragging).
  if (drop_position_) {
    // The two-pixel width drop indicator.
    constexpr int kDropIndicatorWidth = 2;

    const size_t i = GetDropPositionIndex();
    const gfx::Rect frame = toolbar_actions_bar_->GetFrameForIndex(i);
    gfx::Rect indicator_bounds = GetMirroredRect(
        gfx::Rect(GetResizeAreaWidth() + frame.x() -
                      GetLayoutConstant(TOOLBAR_ELEMENT_PADDING) / 2 -
                      kDropIndicatorWidth / 2,
                  frame.y(), kDropIndicatorWidth, frame.height()));
    // Clamp the indicator to the view bounds so that heading / trailing markers
    // don't paint outside the controller. It's OK if they paint over the resize
    // area or separator (but the in-menu container has neither).
    indicator_bounds.set_x(base::ClampToRange(
        indicator_bounds.x(), 0, width() - indicator_bounds.width()));

    // Color of the drop indicator.
    // Always get the theme provider of the browser widget, since if this view
    // is shown within the menu widget, GetThemeProvider() would return the
    // ui::DefaultThemeProvider which doesn't return the correct colors.
    // https://crbug.com/831510.
    const ui::ThemeProvider* theme_provider =
        BrowserView::GetBrowserViewForBrowser(browser_)
            ->frame()
            ->GetThemeProvider();

    const SkColor drop_indicator_color = color_utils::GetColorWithMaxContrast(
        theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));
    canvas->FillRect(indicator_bounds, drop_indicator_color);
  }
}

void BrowserActionsContainer::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (!toolbar_actions_bar_->enabled())
    return;

  if (details.is_add && details.child == this) {
    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because adding views
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    toolbar_actions_bar_->CreateActions();

    added_to_view_ = true;
  }
}

void BrowserActionsContainer::ClearActiveBubble(views::Widget* widget) {
  DCHECK(active_bubble_);
  DCHECK_EQ(active_bubble_->GetWidget(), widget);
  widget->RemoveObserver(this);
  active_bubble_ = nullptr;
  toolbar_actions_bar_->OnBubbleClosed();
}

size_t BrowserActionsContainer::WidthToIconCount(int width) const {
  // TODO(pbos): Ideally we would just calculate the icon count ourselves, but
  // until that point we need to subtract the separator width before asking
  // |toolbar_actions_bar_| how many icons to show, so that it doesn't try to
  // place an icon over the separator.
  return toolbar_actions_bar_->WidthToIconCount(width -
                                                GetSeparatorAreaWidth());
}

int BrowserActionsContainer::GetWidthForIconCount(size_t num_icons) const {
  if (num_icons == 0)
    return 0;
  return GetResizeAreaWidth() + GetSeparatorAreaWidth() +
         toolbar_actions_bar_->IconCountToWidth(num_icons);
}

int BrowserActionsContainer::GetWidthWithAllActionsVisible() const {
  return GetWidthForIconCount(
      toolbar_actions_bar_->toolbar_actions_unordered().size());
}

size_t BrowserActionsContainer::GetDropPositionIndex() const {
  size_t i =
      drop_position_->row * platform_settings().icons_per_overflow_menu_row +
      drop_position_->icon_in_row;
  if (ShownInsideMenu())
    i += main_container_->VisibleBrowserActionsAfterAnimation();
  return i;
}

int BrowserActionsContainer::GetResizeAreaWidth() const {
  if (!resize_area_)
    return 0;
  return platform_settings().item_spacing;
}

int BrowserActionsContainer::GetSeparatorAreaWidth() const {
  // The separator is not applicable to the app menu.
  if (ShownInsideMenu())
    return 0;
  return 2 * GetLayoutConstant(TOOLBAR_STANDARD_SPACING) +
         views::Separator::kThickness;
}

void BrowserActionsContainer::UpdateResizeArea() {
  if (!resize_area_)
    return;

  const base::Optional<int> max_width = delegate_->GetMaxBrowserActionsWidth();
  const bool enable_resize_area =
      interactive_ && !toolbar_actions_bar()->is_highlighting() &&
      (!max_width || *max_width >= GetWidthForIconCount(1));
  resize_area_->SetEnabled(enable_resize_area);
}

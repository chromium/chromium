// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include <algorithm>
#include <optional>
#include <variant>

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/shadow_frame_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kResizeAreaWidth = 5;
constexpr int kCollapsedResizeAreaWidth = 2;
constexpr int kKeyboardResizeWidth = 50;
constexpr int kSnapDistance = 15;

// Shadow is used in expand-on-hover mode. Shadow radius and opacity are dynamic
// and set by the layout.
constexpr int kExpandOnHoverShadowElevation = 4;
constexpr ShadowFrameView::ShadowAlpha kExpandOnHoverShadowAlpha(
    {.light_key = 0.3,
     .light_ambient = 0.0,
     .dark_key = 0.6,
     .dark_ambient = 0.0});
}  // namespace

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(VerticalTabStripRegionView,
                                       kAnimationCompletedEvent);

VerticalTabStripRegionView::VerticalTabStripRegionView(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserView* browser_view)
    : browser_view_(browser_view),
      resize_area_width_(kResizeAreaWidth),
      tab_strip_model_(browser_view->browser()->GetTabStripModel()),
      state_controller_(state_controller),
      root_action_item_(root_action_item),
      hover_card_controller_(
          std::make_unique<TabHoverCardController>(this,
                                                   browser_view->browser())),
      hover_tab_selector_(
          std::make_unique<HoverTabSelector>(tab_strip_model_)) {
  SetNotifyEnterExitOnChild(true);
  // For z-ordering purposes this needs to be on a layer.
  SetPaintToLayer();
  // Because corners may be transparent, this must be set to false.
  layer()->SetFillsBoundsOpaquely(false);

  const int region_horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  flex_layout_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kVertical,
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
  flex_layout_->SetInteriorMargin(gfx::Insets::TLBR(
      0, 0,
      GetLayoutConstant(
          LayoutConstant::kVerticalTabStripUncollapsedVerticalPadding),
      0));

  // Create child views.
  top_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripTopContainer>(
          state_controller_, root_action_item, browser_view->browser()));
  top_button_container_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(0, region_horizontal_padding));

  top_button_separator_ = AddChildView(std::make_unique<views::Separator>());
  // The TopContainer handles the padding distance to the separator so that we
  // can control how far it is in the various states.
  top_button_separator_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(0, region_horizontal_padding));

  bottom_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripBottomContainer>(
          state_controller_, root_action_item, browser_view->browser(),
          base::BindRepeating(
              &VerticalTabStripRegionView::RecordNewTabButtonPressed,
              base::Unretained(this))));
  bottom_button_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  bottom_button_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          GetLayoutConstant(
              LayoutConstant::kVerticalTabStripCollapsedVerticalPadding),
          region_horizontal_padding, 0, region_horizontal_padding));

  gemini_button_ = AddChildView(std::make_unique<views::View>());

  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));
  resize_area_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  resize_area_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Install(resize_area_);
  resize_area_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_VERTICAL_RESIZE_AREA));
  resize_area_->GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);

  state_controller_->SetDelegate(this);
  target_collapse_state_ = state_controller_->GetState();
  OnCollapseStateChanged(state_controller_->GetCollapseState());
  collapsed_state_changed_subscription_ =
      state_controller_->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripRegionView::OnCollapseStateChanged,
          base::Unretained(this)));

  SetProperty(views::kElementIdentifierKey, kTabStripRegionElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);

  SetBackground(std::make_unique<CustomCornersBackground>(
      *this, *browser_view,
      /*primary_color=*/CustomCornersBackground::FrameTheme(),
      /*corner_color=*/CustomCornersBackground::ToolbarTheme()));

  shadow_frame_ = AddChildView(std::make_unique<ShadowFrameView>(
      kExpandOnHoverShadowElevation, kExpandOnHoverShadowAlpha));
  shadow_frame_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  UpdateColors();
}

VerticalTabStripRegionView::~VerticalTabStripRegionView() {
  for (const auto& lock : hover_locks_) {
    lock->ClearRegionViewOnDestruction();
  }
  hover_locks_.clear();

  if (root_node_) {
    root_node_->SetController(nullptr);
  }

  tab_strip_controller_.reset();

  if (drag_handler_) {
    auto handler = RemoveChildViewT(drag_handler_->GetDragContext());
    drag_handler_ = nullptr;
  }

  // Prevent dangling pointer.
  state_controller_->SetDelegate(nullptr);
}

VerticalPinnedTabContainerView*
VerticalTabStripRegionView::GetPinnedTabsContainer() {
  return tab_strip_view_->GetPinnedTabsContainer();
}

VerticalUnpinnedTabContainerView*
VerticalTabStripRegionView::GetUnpinnedTabsContainer() {
  return tab_strip_view_->GetUnpinnedTabsContainer();
}

bool VerticalTabStripRegionView::IsAnimatingSize() const {
  return BrowserAnimationController::From(browser_view_->browser())
      ->IsAnimating(TabStripAnimations::kVerticalTabStrip);
}

void VerticalTabStripRegionView::OnAnimationProgressed(
    const BrowserAnimationController* controller,
    BrowserAnimationUpdate status) {
  switch (status) {
    case BrowserAnimationUpdate::kStarted: {
      const auto motion =
          controller->GetCurrentMotion(TabStripAnimations::kVerticalTabStrip);
      if (motion == TabStripAnimations::kExpand) {
        update_state_controller_collapsed_callback_.Run(false);
      }
      hover_card_animation_lock_ =
          hover_card_controller_->GetHoverCardHideLock();
      if (tab_strip_view_) {
        tab_strip_view_->SetIsAnimatingSize(true);
      }
      break;
    }
    case BrowserAnimationUpdate::kProgressed:
      InvalidateLayout();
      break;
    case BrowserAnimationUpdate::kEnded: {
      hover_card_animation_lock_.reset();
      if (tab_strip_view_) {
        tab_strip_view_->SetIsAnimatingSize(false);
      }
      const auto motion =
          controller->GetCurrentMotion(TabStripAnimations::kVerticalTabStrip);
      if (motion == TabStripAnimations::kCollapse ||
          motion == TabStripAnimations::kExpand) {
        views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
            kAnimationCompletedEvent, this);
      }
      if (motion == TabStripAnimations::kCollapse) {
        update_state_controller_collapsed_callback_.Run(true);
      }
      InvalidateLayout();
      break;
    }
    case BrowserAnimationUpdate::kCanceled:
      hover_card_animation_lock_.reset();
      if (tab_strip_view_) {
        tab_strip_view_->SetIsAnimatingSize(false);
      }

      // If the collapse animation is cancelled by requesting the tabstrip to
      // expand, then we will not receive a BrowserAnimationUpdate::kEnded event
      // which is usually where we update collapsed state of the controller to
      // true.
      // The current motion in the animation controller has already been cleared
      // at this point, so instead check that the target collapse state is
      // expanded. As a result, this also gets executed when cancelling expand
      // on hover animations, but since the collapse state must have already
      // been true in that case, this would be a no-op.
      if (!target_collapse_state_.collapsed) {
        update_state_controller_collapsed_callback_.Run(true);
      }
      break;
  }
}

bool VerticalTabStripRegionView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  // Check the resize area first, it should always take precedence over other
  // children regardless of order.
  if (IsHitInView(resize_area_, point)) {
    return false;
  }

  if (IsHitInView(top_button_container_, point)) {
    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, top_button_container_,
                                      &point_in_child);
    return top_button_container_->IsPositionInWindowCaption(point_in_child);
  }

  if (IsHitInView(bottom_button_container_, point)) {
    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, bottom_button_container_,
                                      &point_in_child);
    return bottom_button_container_->IsPositionInWindowCaption(point_in_child);
  }

  // For any of the other children, absorb the click as non window caption.
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, child, &point_in_child);
    if (child->HitTestPoint(point_in_child)) {
      return false;
    }
  }

  // If the click doesnt fall under any view,then it counts as window caption.
  return true;
}

void VerticalTabStripRegionView::SetToolbarHeightForLayout(int toolbar_height) {
  top_button_container_->SetToolbarHeightForLayout(toolbar_height);
}

void VerticalTabStripRegionView::SetCaptionButtonWidthForLayout(
    int caption_button_width) {
  top_button_container_->SetCaptionButtonWidthForLayout(caption_button_width);
}

void VerticalTabStripRegionView::SetIsExitingExpandOnHoverForLayout(
    bool is_exiting_expand_on_hover) {
  top_button_container_->SetIsExitingExpandOnHoverForLayout(
      is_exiting_expand_on_hover);
}

bool VerticalTabStripRegionView::WillWrapDueToOverflow(
    int available_width) const {
  return top_button_container_->WillWrapDueToOverflow(available_width);
}

TabDragTarget* VerticalTabStripRegionView::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  if (!drag_handler_) {
    return nullptr;
  }
  gfx::Rect tab_strip_draggable_bounds = GetTabStripDraggableBounds();
  if (!tab_strip_draggable_bounds.Contains(point_in_screen)) {
    return nullptr;
  }

  // Note: if the drag has not attached to this tab strip yet, it doesn't matter
  // which container is used because the first drag loop iteration just attaches
  // it.
  if (drag_handler_->IsDraggingPinnedTabs()) {
    return &GetPinnedTabsContainer()->GetTabDragTarget(point_in_screen);
  }
  return &GetUnpinnedTabsContainer()->GetTabDragTarget(point_in_screen);
}

void VerticalTabStripRegionView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          [](VerticalTabStripRegionView* view) {
            view->UpdateColors();
            view->UpdateExpandOnHoverState();
          },
          base::Unretained(this)));
  if (GetFocusManager()) {
    GetFocusManager()->AddFocusChangeListener(&focus_listener_);
  }
}

void VerticalTabStripRegionView::RemovedFromWidget() {
  if (GetFocusManager()) {
    GetFocusManager()->RemoveFocusChangeListener(&focus_listener_);
  }
  TabStripRegionView::RemovedFromWidget();
}

void VerticalTabStripRegionView::Layout(PassKey) {
  LayoutSuperclass<views::AccessiblePaneView>(this);

  // Manually position the resize area as it overlaps views handled by the flex
  // layout.
  resize_area_->SetBoundsRect(gfx::Rect(bounds().right() - resize_area_width_,
                                        0, resize_area_width_,
                                        bounds().height()));
  shadow_frame_->SetBoundsRect(GetLocalBounds());

  // Ensure that we update the drop arrow position so that it does not render in
  // the collapsed state when expand on hover is active.
  if (drop_arrow_) {
    drop_arrow_->SetIndex(drop_arrow_->index());
  }
}

views::View* VerticalTabStripRegionView::GetDefaultFocusableChild() {
  const int active_index = tab_strip_model_->active_index();
  if (active_index != TabStripModel::kNoTab) {
    return GetTabAnchorViewAt(active_index);
  }

  return top_button_container_;
}

gfx::Size VerticalTabStripRegionView::GetMinimumSize() const {
  auto min_size = TabStripRegionView::GetMinimumSize();
  min_size.set_width((state_controller_->IsCollapsed() || IsAnimatingSize())
                         ? kCollapsedWidth
                         : kUncollapsedMinWidth);
  return min_size;
}

gfx::Size VerticalTabStripRegionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto size = TabStripRegionView::CalculatePreferredSize(available_size);
  const auto* controller =
      BrowserAnimationController::From(browser_view_->browser());
  const auto motion =
      controller->GetCurrentMotion(TabStripAnimations::kVerticalTabStrip);
  const int expand_amount =
      std::max(0, target_collapse_state_.uncollapsed_width - kCollapsedWidth);
  const double expand_percent =
      *controller->GetCurrentValue(TabStripAnimations::kVerticalTabStrip,
                                   TabStripAnimations::kTabStripWidth);
  size.set_width(kCollapsedWidth +
                 (motion == TabStripAnimations::kExpand
                      ? std::floor<double>
                      : std::ceil<double>)(expand_amount * expand_percent));
  return size;
}

bool VerticalTabStripRegionView::OnKeyPressed(const ui::KeyEvent& event) {
  if (resize_area_->HasFocus()) {
    int resize_amount = 0;

    if (event.key_code() == ui::VKEY_LEFT) {
      resize_amount =
          base::i18n::IsRTL() ? kKeyboardResizeWidth : -kKeyboardResizeWidth;
    } else if (event.key_code() == ui::VKEY_RIGHT) {
      resize_amount =
          base::i18n::IsRTL() ? -kKeyboardResizeWidth : kKeyboardResizeWidth;
    }

    if (resize_amount != 0) {
      OnResize(resize_amount, /*done_resizing=*/true);

      float tab_strip_width = width();
      float total_width = GetWidget()->GetRootView()->width();

      int tab_strip_percentage = (tab_strip_width / total_width) * 100;
      int content_percentage = 100 - tab_strip_percentage;

      resize_area_->GetViewAccessibility().AnnounceText(
          l10n_util::GetStringFUTF16(IDS_VERTICAL_TAB_RESIZE_ACCESSIBLE_ALERT,
                                     base::FormatPercent(tab_strip_percentage),
                                     base::FormatPercent(content_percentage)));

      return true;
    }
  }

  return TabStripRegionView::OnKeyPressed(event);
}

void VerticalTabStripRegionView::OnMouseEntered(const ui::MouseEvent& event) {
  if (mouse_exit_timer_.IsRunning()) {
    mouse_exit_timer_.Stop();
    return;
  }
  UpdateExpandOnHoverState(true);
}

void VerticalTabStripRegionView::OnMouseMoved(const ui::MouseEvent& event) {
  if (mouse_exit_timer_.IsRunning()) {
    mouse_exit_timer_.Stop();
  }
  UpdateExpandOnHoverState(true);
}

void VerticalTabStripRegionView::OnMouseExited(const ui::MouseEvent& event) {
  HandleMouseExited();
}

void VerticalTabStripRegionView::HandleMouseExited() {
  // On Windows, we get mouse exit events when moving between the caption area
  // and client as well as when we transition between web contents area
  // underneath the expanded on hover overlay to outside it.
#if BUILDFLAG(IS_WIN)
  constexpr base::TimeDelta kMouseExitDebounceTimer = base::Milliseconds(100);
  if (IsMouseHovered()) {
    mouse_exit_timer_.Start(
        FROM_HERE, kMouseExitDebounceTimer,
        base::BindOnce(&VerticalTabStripRegionView::HandleMouseExited,
                       base::Unretained(this)));
    return;
  }
#endif
  UpdateExpandOnHoverState(false);
}

void VerticalTabStripRegionView::InitializeTabStrip() {
  if (root_node_) {
    return;
  }

  root_node_ = std::make_unique<RootTabCollectionNode>(
      tab_strip_model_,
      base::BindRepeating(&VerticalTabStripRegionView::SetTabStripView,
                          base::Unretained(this)),
      base::BindRepeating(&VerticalTabStripRegionView::ClearTabStripView,
                          base::Unretained(this)));

  std::unique_ptr<TabMenuModelFactory> tab_menu_model_factory;
  if (browser_view_ &&
      web_app::AppBrowserController::From(browser_view_->browser())) {
    tab_menu_model_factory =
        web_app::AppBrowserController::From(browser_view_->browser())
            ->GetTabMenuModelFactory();
  }

  TabStripModel* tab_strip_model = browser_view_->browser()->GetTabStripModel();
  CHECK(tab_strip_model);
  auto drag_handler = std::make_unique<VerticalTabDragHandlerImpl>(
      *tab_strip_model, *root_node_.get(), *this);
  drag_handler_ = drag_handler.get();

  CHECK(!tab_strip_controller_);
  tab_strip_controller_ = std::make_unique<VerticalTabStripController>(
      tab_strip_model, browser_view_, *AddChildView(std::move(drag_handler)),
      hover_card_controller_.get(), std::move(tab_menu_model_factory));

  root_node_->SetController(tab_strip_controller_.get());

  root_node_->Init();

  new_tab_button_pressed_start_time_ = std::nullopt;
  on_children_added_subscription_ = root_node_->RegisterOnChildrenAddedCallback(
      base::BindRepeating(&VerticalTabStripRegionView::OnChildrenAdded,
                          base::Unretained(this)));
  on_children_removed_subscription_ =
      root_node_->RegisterOnChildRemovedCallback(
          base::BindRepeating(&VerticalTabStripRegionView::OnChildrenRemoved,
                              base::Unretained(this)));
  on_child_moved_subscription_ =
      root_node_->RegisterOnChildMovedCallback(base::BindRepeating(
          &VerticalTabStripRegionView::OnChildMoved, base::Unretained(this)));
}

void VerticalTabStripRegionView::ResetTabStrip() {
  if (!root_node_) {
    return;
  }

  on_children_added_subscription_.reset();
  on_children_removed_subscription_.reset();
  on_child_moved_subscription_.reset();

  root_node_->Reset();

  root_node_->SetController(nullptr);
  tab_strip_controller_.reset();

  CHECK(drag_handler_);
  auto* drag_handler = drag_handler_.get();
  drag_handler_ = nullptr;
  RemoveChildViewT(drag_handler->GetDragContext());

  hover_tab_selector_->CancelTabTransition();

  root_node_.reset();
}

bool VerticalTabStripRegionView::IsTabStripEditable() const {
  return tab_strip_editable_for_testing_ &&
         (!drag_handler_ ||
          !drag_handler_->GetDragContext()->GetDragController());
}

bool VerticalTabStripRegionView::IsTabStripCloseable() const {
  if (!drag_handler_) {
    return true;
  }
  if (auto* drag_controller =
          drag_handler_->GetDragContext()->GetDragController()) {
    return drag_controller->IsMovingLastTab();
  }
  return true;
}

void VerticalTabStripRegionView::UpdateLoadingAnimations(
    const base::TimeDelta& elapsed_time) {
  for (tabs::TabInterface* tab : *tab_strip_model_) {
    const TabCollectionNode* node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    VerticalTabView* tab_view =
        views::AsViewClass<VerticalTabView>(node->view());
    CHECK(tab_view);
    tab_view->StepLoadingAnimation(elapsed_time);
  }
}

std::optional<int> VerticalTabStripRegionView::GetFocusedTabIndex() const {
  const views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return std::nullopt;
  }

  const views::View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view) {
    return std::nullopt;
  }

  for (int i = 0; i < tab_strip_model_->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(i);
    const TabCollectionNode* node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    if (node && node->view() == focused_view) {
      return i;
    }
  }

  return std::nullopt;
}

const tabs::TabData& VerticalTabStripRegionView::GetTabData(
    const tabs::TabHandle& tab) {
  const TabCollectionNode* node = root_node_->GetNodeForHandle(tab);
  CHECK(node);

  VerticalTabView* tab_view = views::AsViewClass<VerticalTabView>(node->view());
  CHECK(tab_view);

  return tab_view->data();
}

views::View* VerticalTabStripRegionView::GetTabAnchorViewAt(int tab_index) {
  tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);
  CHECK(tab) << "No tab found for tab_index: " << tab_index;

  const TabCollectionNode* node =
      root_node_->GetNodeForHandle(tab->GetHandle());
  CHECK(node) << "No node found for tab handle";

  return node->view();
}

views::View* VerticalTabStripRegionView::GetTabGroupAnchorView(
    const tab_groups::TabGroupId& group) {
  if (!tab_strip_model_->SupportsTabGroups()) {
    return nullptr;
  }

  if (const TabGroup* tab_group =
          tab_strip_model_->group_model()->GetTabGroup(group)) {
    return root_node_->GetNodeForHandle(tab_group->GetCollectionHandle())
        ->view();
  }

  return nullptr;
}

void VerticalTabStripRegionView::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  top_button_container_->GetUnfocusButton()->SetVisible(
      new_focused_group_id.has_value());
  // Temporarily, we are updating the visibility of the collapse action to be
  // inverse to the unfocus button because of horizontal space constraints in
  // the top container.
  actions::ActionItem* collapse_action =
      actions::ActionManager::Get().FindAction(kActionToggleCollapseVertical,
                                               root_action_item_);
  collapse_action->SetVisible(!new_focused_group_id.has_value());
}

TabDragContext* VerticalTabStripRegionView::GetDragContext() {
  return drag_handler_->GetDragContext();
}

std::optional<BrowserRootView::DropIndex>
VerticalTabStripRegionView::GetDropIndex(const ui::DropTargetEvent& event) {
  // Check pinned tabs.
  VerticalPinnedTabContainerView* pinned_container = GetPinnedTabsContainer();
  if (pinned_container && !pinned_container->children().empty()) {
    gfx::Point loc_in_pinned = views::View::ConvertPointToTarget(
        this, pinned_container, event.location());
    if (loc_in_pinned.y() < 0) {
      // If the point is above the pinned container, return the beginning of the
      // container.
      return pinned_container->GetLinkDropIndex(gfx::Point(0, 0));
    } else if (loc_in_pinned.y() >= 0 &&
               loc_in_pinned.y() < pinned_container->height()) {
      return pinned_container->GetLinkDropIndex(loc_in_pinned);
    }
  }

  // Check unpinned tabs.
  VerticalUnpinnedTabContainerView* unpinned_container =
      GetUnpinnedTabsContainer();
  if (unpinned_container && !unpinned_container->children().empty()) {
    gfx::Point loc_in_unpinned = views::View::ConvertPointToTarget(
        this, unpinned_container, event.location());
    if (loc_in_unpinned.y() < 0) {
      // If the point is above the unpinned container, return the beginning of
      // the container.
      return unpinned_container->GetLinkDropIndex(gfx::Point(0, 0));
    } else if (loc_in_unpinned.y() >= 0 &&
               loc_in_unpinned.y() < unpinned_container->height()) {
      return unpinned_container->GetLinkDropIndex(loc_in_unpinned);
    }
  }

  // If it's at the end, return the end of the unpinned container.
  if (unpinned_container) {
    return unpinned_container->GetLinkDropIndex(
        gfx::Point(0, unpinned_container->height()));
  }

  return std::nullopt;
}

BrowserRootView::DropTarget* VerticalTabStripRegionView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  if (tab_strip_view_ && IsTabStripEditable() &&
      GetLocalBounds().Contains(loc_in_local_coords)) {
    return this;
  }
  return nullptr;
}

views::View* VerticalTabStripRegionView::GetViewForDrop() {
  return this;
}

bool VerticalTabStripRegionView::CanDrop(const OSExchangeData& data) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->CanDrop(data);
  }
  return false;
}

bool VerticalTabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->GetDropFormats(formats,
                                                           format_types);
  }
  return false;
}

void VerticalTabStripRegionView::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  drag_handler_->GetDragContext()->OnDragEntered(event);
}

int VerticalTabStripRegionView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  return drag_handler_->GetDragContext()->OnDragUpdated(event);
}

void VerticalTabStripRegionView::OnDragExited() {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  drag_handler_->GetDragContext()->OnDragExited();
}

void VerticalTabStripRegionView::SetTabStripObserver(
    TabStripObserver* observer) {
  // Do nothing.
}

views::View* VerticalTabStripRegionView::GetTabStripView() {
  return tab_strip_view_;
}

bool VerticalTabStripRegionView::TraverseUsingUpDownKeys() {
  return true;
}

std::unique_ptr<ExpandOnHoverLock>
VerticalTabStripRegionView::GetExpandOnHoverLock(
    ExpandOnHoverLockType lock_type) {
  return std::make_unique<VerticalTabStripExpandOnHoverLock>(this, lock_type);
}

void VerticalTabStripRegionView::HandleDragUpdate(
    const std::optional<BrowserRootView::DropIndex>& index) {
  SetLinkDropArrow(index);
  UpdateExpandOnHoverState(true);
  if (is_expanded_on_hover_ && !link_drag_lock_) {
    link_drag_lock_ =
        GetExpandOnHoverLock(ExpandOnHoverLockType::kKeepCurrentState);
  }
}

void VerticalTabStripRegionView::HandleDragExited() {
  SetLinkDropArrow(std::nullopt);
  link_drag_lock_.reset();
  UpdateExpandOnHoverState(false);
}

void VerticalTabStripRegionView::OnResize(int resize_amount,
                                          bool done_resizing) {
  CHECK(tab_strip_view_);
  tab_strip_view_->SetIsAnimatingSize(!done_resizing);
  if (!starting_width_on_resize_.has_value()) {
    starting_width_on_resize_ = width();
  }
  bool previously_collapsed = target_collapse_state_.collapsed;
  const int proposed_width = starting_width_on_resize_.value() + resize_amount;
  if (done_resizing) {
    starting_width_on_resize_ = std::nullopt;
  }

  tabs::VerticalTabStripState new_state;
  new_state.collapsed = proposed_width <= kCollapseSnapWidth;
  new_state.uncollapsed_width = target_collapse_state_.uncollapsed_width;
  if (!new_state.collapsed) {
    new_state.uncollapsed_width =
        std::clamp(proposed_width, kUncollapsedMinWidth, kUncollapsedMaxWidth);
    if (std::abs(new_state.uncollapsed_width -
                 tabs::kVerticalTabStripDefaultUncollapsedWidth) <
        kSnapDistance) {
      new_state.uncollapsed_width =
          tabs::kVerticalTabStripDefaultUncollapsedWidth;
    }
    if (done_resizing) {
      // We only want to save the uncollapsed width to the state controller if
      // the user has lifted their mouse, otherwise dragging the resize area to
      // collapse will cause a subsequent collapse button click to only expand
      // to the minimum expanded width, and not to the starting width of the
      // drag-to-collapse operation.
      state_controller_->SetUncollapsedWidth(new_state.uncollapsed_width);
    }
  } else if (done_resizing) {
    // If we are done resizing while in collapsed state, update the target state
    // to use the uncollapsed width from the state controller.
    new_state.uncollapsed_width = state_controller_->GetUncollapsedWidth();
  }

  if (done_resizing) {
    resize_area_->SetVisible(!state_controller_->IsCollapsed() ||
                             !state_controller_->IsExpandOnHoverEnabled());
    base::RecordAction(base::UserMetricsAction(
        new_state.collapsed ? "VerticalTabs_TabStrip_ResizeToCollapsed"
                            : "VerticalTabs_TabStrip_ResizeToUncollapsed"));
    base::UmaHistogramCounts1000(
        "Tabs.VerticalTabs.TabStripSize",
        new_state.collapsed ? kCollapsedWidth : new_state.uncollapsed_width);
  }

  target_collapse_state_ = new_state;

  if (previously_collapsed != target_collapse_state_.collapsed) {
    state_controller_->RequestCollapse(target_collapse_state_.collapsed);
  } else if (!target_collapse_state_.collapsed && !IsAnimatingSize()) {
    // If we are still in the expanding animation, invalidating the layout will
    // happen in AnimationProgressed, instead of here.
    InvalidateLayout();
  }
}

void VerticalTabStripRegionView::SetCollapsedStateUpdatedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  update_state_controller_collapsed_callback_ = std::move(callback);
}

bool VerticalTabStripRegionView::IsCollapsing() {
  return BrowserAnimationController::From(browser_view_->browser())
             ->GetCurrentMotion(TabStripAnimations::kVerticalTabStrip) ==
         TabStripAnimations::kCollapse;
}

void VerticalTabStripRegionView::RequestCollapse(bool collapse) {
  target_collapse_state_.collapsed = collapse;
  CHECK(tab_strip_view_);
  const auto motion =
      collapse ? TabStripAnimations::kCollapse : TabStripAnimations::kExpand;
  BrowserAnimationController::From(browser_view_->browser())
      ->Start(TabStripAnimations::kVerticalTabStrip, motion);
}

void VerticalTabStripRegionView::DisableTabStripEditingForTesting() {
  tab_strip_editable_for_testing_ = false;
}

gfx::Rect VerticalTabStripRegionView::GetLinkDropBoundsForTesting(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  return GetLinkDropBounds(drop_index, direction);
}

VerticalTabStripRegionView::RegionViewFocusListener::RegionViewFocusListener(
    VerticalTabStripRegionView* region_view)
    : region_view_(region_view) {}

void VerticalTabStripRegionView::RegionViewFocusListener::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  if ((focused_before && region_view_->Contains(focused_before)) ||
      (focused_now && region_view_->Contains(focused_now))) {
    region_view_->UpdateExpandOnHoverState();
  }
}

VerticalTabStripRegionView::ClickEventHandler::ClickEventHandler(
    VerticalTabStripRegionView* region_view)
    : region_view_(region_view) {}

void VerticalTabStripRegionView::ClickEventHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed &&
      region_view_->state_controller_->IsExpandOnHoverEnabled() &&
      !region_view_->is_expanded_on_hover()) {
    region_view_->RestartExpandOnHoverTimer(
        tabs::kVerticalTabsExpandOnHoverClickDelay.Get());
  }
}

views::View* VerticalTabStripRegionView::SetTabStripView(
    std::unique_ptr<views::View> view) {
  CHECK(views::IsViewClass<VerticalTabStripView>(view.get()));

  tab_strip_view_ =
      static_cast<VerticalTabStripView*>(AddChildView(std::move(view)));
  tab_strip_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  tab_strip_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(
          GetLayoutConstant(
              LayoutConstant::kVerticalTabStripCollapsedVerticalPadding),
          0));

  on_active_tab_changed_subscription_ =
      root_node_->RegisterOnActiveTabChangedCallback(
          base::BindRepeating(&VerticalTabStripRegionView::OnActiveTabChanged,
                              base::Unretained(this)));

  // Pre-set the animation values to the appropriate state.
  auto* const animation_controller =
      BrowserAnimationController::From(browser_view_->browser());
  animation_controller->Reset(TabStripAnimations::kVerticalTabStrip,
                              target_collapse_state_.collapsed
                                  ? TabStripAnimations::kCollapse
                                  : TabStripAnimations::kExpand);

  on_animation_update_subscription_ = animation_controller->Subscribe(
      TabStripAnimations::kVerticalTabStrip,
      base::BindRepeating(&VerticalTabStripRegionView::OnAnimationProgressed,
                          base::Unretained(this)));

  expand_on_hover_enabled_changed_subscription_ =
      state_controller_->RegisterOnExpandOnHoverEnabledChanged(
          base::BindRepeating(
              &VerticalTabStripRegionView::OnExpandOnHoverEnabledChanged,
              base::Unretained(this)));

  std::optional<size_t> separator_index = GetIndexOf(top_button_separator_);
  CHECK(separator_index.has_value());
  ReorderChildView(tab_strip_view_, separator_index.value() + 1);

  OnCollapseStateChanged(state_controller_->GetCollapseState());

  return tab_strip_view_;
}

void VerticalTabStripRegionView::ClearTabStripView(views::View* view) {
  on_animation_update_subscription_.reset();
  on_active_tab_changed_subscription_.reset();
  expand_on_hover_enabled_changed_subscription_.reset();
  omnibox_tab_helper_observation_.Reset();

  ResetExpandOnHoverTimers();
  is_expanded_on_hover_ = false;

  CHECK(tab_strip_view_);
  CHECK(tab_strip_view_ == view);
  RemoveChildViewT(std::exchange(tab_strip_view_, nullptr));
}

void VerticalTabStripRegionView::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  // Apply padding immediately at the start of the animation by including
  // the collapsing state.
  bool collapsed = state != tabs::VerticalTabStripCollapseState::kExpanded;

  resize_area_->SetVisible(!collapsed ||
                           !state_controller_->IsExpandOnHoverEnabled() ||
                           resize_area_->is_resizing());

  resize_area_width_ = collapsed ? kCollapsedResizeAreaWidth : kResizeAreaWidth;

  if (tab_strip_view_) {
    tab_strip_view_->SetCollapsedState(collapsed);
  }

  if (state == tabs::VerticalTabStripCollapseState::kExpanded ||
      state == tabs::VerticalTabStripCollapseState::kCollapsed) {
    UpdateExpandOnHoverState();
  }
}

void VerticalTabStripRegionView::UpdateColors() {
  top_button_separator_->SetColorId(IsFrameActive()
                                        ? kColorTabDividerFrameActive
                                        : kColorTabDividerFrameInactive);
}

bool VerticalTabStripRegionView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

gfx::Rect VerticalTabStripRegionView::GetTabStripDraggableBounds() const {
  // Tabs should be draggable from the top of the tab strip to the bottom of the
  // tab strip's max size, saving space for the bottom button container and
  // padding.
  gfx::Rect tab_strip_draggable_bounds = tab_strip_view_->GetBoundsInScreen();
  tab_strip_draggable_bounds.set_height(
      GetBoundsInScreen().bottom() -
      bottom_button_container_->GetMinimumSize().height() -
      flex_layout_->interior_margin().height() -
      tab_strip_draggable_bounds.y());
  return tab_strip_draggable_bounds;
}

void VerticalTabStripRegionView::RecordNewTabButtonPressed() {
  new_tab_button_pressed_start_time_ = base::TimeTicks::Now();

  base::RecordAction(base::UserMetricsAction("NewTab_Button"));
}

void VerticalTabStripRegionView::OnChildrenAdded() {
  if (new_tab_button_pressed_start_time_.has_value()) {
    base::UmaHistogramTimes(
        "TabStrip.TimeToCreateNewTabFromPress",
        base::TimeTicks::Now() - new_tab_button_pressed_start_time_.value());
    new_tab_button_pressed_start_time_.reset();
  }
  hover_tab_selector_->CancelTabTransition();
}

void VerticalTabStripRegionView::OnChildrenRemoved() {
  hover_tab_selector_->CancelTabTransition();
}

void VerticalTabStripRegionView::OnChildMoved() {
  hover_tab_selector_->CancelTabTransition();
}

void VerticalTabStripRegionView::OnExpandOnHoverEnabledChanged(bool enabled) {
  resize_area_->SetVisible(!state_controller_->IsCollapsed() || !enabled ||
                           resize_area_->is_resizing());
  UpdateExpandOnHoverState();
}

void VerticalTabStripRegionView::UpdateExpandOnHoverState(
    std::optional<bool> hovered) {
  // If not collapsed, then we shouldn't be in or entering the expand on hover
  // state.
  if (!state_controller_->ShouldDisplayVerticalTabs() ||
      !state_controller_->IsCollapsed()) {
    ResetExpandOnHoverTimers();
    is_expanded_on_hover_ = false;
    return;
  }
  // If the force collapse lock is held, collapse the tab strip.
  if (force_collapse_lock_count_ > 0) {
    if (is_expanded_on_hover_) {
      AnimateExpandOnHover(/*expand=*/false);
    }
    return;
  }

  // If the current state lock is held, reset the timers and wait.
  if (keep_current_state_lock_count_ > 0) {
    ResetExpandOnHoverTimers();
    return;
  }

  // If the keep expanded lock is held, expand the tab strip.
  if (keep_expanded_lock_count_ > 0) {
    if (!is_expanded_on_hover_) {
      AnimateExpandOnHover(/*expand=*/true);
    }
    return;
  }

  // If the window becomes inactive, then we should not enter the expand on
  // hover state or exit it if already expanded. We evaluate this after the
  // locks because IsFrameActive can also return false when a WebUI bubble is
  // open.
  if (!IsFrameActive()) {
    if (is_expanded_on_hover_) {
      AnimateExpandOnHover(/*expand=*/false);
    }
    return;
  }

  const bool should_expand =
      state_controller_->IsExpandOnHoverEnabled() &&
      (hovered.value_or(IsMouseHovered()) ||
       (GetFocusManager() && Contains(GetFocusManager()->GetFocusedView())));

  if (!should_expand) {
    if (is_expanded_on_hover_ && keep_current_state_lock_count_ == 0) {
      AnimateExpandOnHover(/*expand=*/false);
    } else {
      ResetExpandOnHoverTimers();
    }
    return;
  }

  // If the region is already expanded, do nothing.
  if (is_expanded_on_hover_) {
    return;
  }

  if (!hover_card_animation_lock_) {
    hover_card_animation_lock_ = hover_card_controller_->GetHoverCardHideLock();
  }

  if (tabs::kVerticalTabsExpandOnHoverUseVelocityHeuristic.Get()) {
    CalculateMouseVelocityForExpandOnHover();
  } else if (expand_on_hover_timer_.IsRunning()) {
    // If the timer is already running then we are already waiting to
    // expand, so do nothing.
    return;
  } else {
    expand_on_hover_timer_.Start(
        FROM_HERE, tabs::kVerticalTabsExpandOnHoverDelay.Get(),
        base::BindOnce(&VerticalTabStripRegionView::AnimateExpandOnHover,
                       base::Unretained(this),
                       /*expand=*/true));
    if (tabs::IsExpandOnHoverClickDelayEnabled()) {
      AddPreTargetHandler(&click_handler_);
    }
  }
}

void VerticalTabStripRegionView::RestartExpandOnHoverTimer(
    const base::TimeDelta& delay) {
  if (expand_on_hover_timer_.IsRunning()) {
    expand_on_hover_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&VerticalTabStripRegionView::AnimateExpandOnHover,
                       base::Unretained(this),
                       /*expand=*/true));
  }
}

void VerticalTabStripRegionView::OnMouseVelocityHeuristicInterval() {
  const bool should_expand =
      state_controller_->IsExpandOnHoverEnabled() &&
      (IsMouseHovered() ||
       (GetFocusManager() && Contains(GetFocusManager()->GetFocusedView())));

  if (!should_expand || is_expanded_on_hover_) {
    ResetExpandOnHoverTimers();
    return;
  }

  CalculateMouseVelocityForExpandOnHover();
}

void VerticalTabStripRegionView::CalculateMouseVelocityForExpandOnHover() {
  gfx::Point current_point = display::Screen::Get()->GetCursorScreenPoint();
  ConvertPointFromScreen(this, &current_point);

  // If this is the first mouse event within the region, initialize values.
  if (!time_at_expand_on_hover_timer_start_.has_value()) {
    point_at_expand_on_hover_timer_start_ = current_point;
    time_at_expand_on_hover_timer_start_ = base::TimeTicks::Now();
    expand_on_hover_heuristic_samples_ = 1;
    expand_on_hover_heuristic_timer_.Start(
        FROM_HERE,
        tabs::kVerticalTabsExpandOnHoverVelocityHeuristicInterval.Get(),
        base::BindRepeating(
            &VerticalTabStripRegionView::OnMouseVelocityHeuristicInterval,
            base::Unretained(this)));
    return;
  }

  // Reset the timer so it will trigger if a mouse event isn't received in the
  // specified interval.
  expand_on_hover_heuristic_timer_.Reset();

  // Increment the number of samples received.
  expand_on_hover_heuristic_samples_ += 1;

  const int dx = std::abs(current_point.x() -
                          (*point_at_expand_on_hover_timer_start_).x());
  const base::TimeDelta dt =
      base::TimeTicks::Now() - *time_at_expand_on_hover_timer_start_;

  // Wait a minimum amount of time before potentially expanding. This also
  // avoids divide by zero errors because this param is at least 0.
  if (dt <= tabs::kVerticalTabsExpandOnHoverVelocityHeuristicDelay.Get()) {
    return;
  }

  // If the mouse is close to the inside edge, wait longer till the mouse is
  // more fully inside the tab strip.
  const int distance_from_inside_edge =
      std::abs(current_point.x() - GetContentsBounds().right());
  if (dt <= tabs::kVerticalTabsExpandOnHoverVelocityHeuristicEdgeDelay.Get() &&
      distance_from_inside_edge <=
          tabs::kVerticalTabsExpandOnHoverVelocityHeuristicDistanceFromEdge
              .Get()) {
    return;
  }

  if (expand_on_hover_heuristic_samples_ >=
          tabs::kVerticalTabsExpandOnHoverVelocityHeuristicMinSamples.Get() &&
      static_cast<double>(dx) / dt.InMilliseconds() <
          tabs::kVerticalTabsExpandOnHoverVelocityHeuristicThreshold.Get()) {
    AnimateExpandOnHover(/*expand=*/true);
  }
}

void VerticalTabStripRegionView::ResetExpandOnHoverTimers() {
  hover_card_animation_lock_.reset();

  if (expand_on_hover_timer_.IsRunning()) {
    expand_on_hover_timer_.Stop();

    if (tabs::IsExpandOnHoverClickDelayEnabled()) {
      RemovePreTargetHandler(&click_handler_);
    }
  }

  if (tabs::kVerticalTabsExpandOnHoverUseVelocityHeuristic.Get()) {
    expand_on_hover_heuristic_timer_.Stop();
    time_at_expand_on_hover_timer_start_ = std::nullopt;
    point_at_expand_on_hover_timer_start_ = std::nullopt;
    expand_on_hover_heuristic_samples_ = 0;
  }
}

void VerticalTabStripRegionView::AnimateExpandOnHover(bool expand) {
  is_expanded_on_hover_ = expand;
  ResetExpandOnHoverTimers();

  if (expand) {
    expand_on_hover_start_time_ = base::TimeTicks::Now();
    base::RecordAction(
        base::UserMetricsAction("VerticalTabs_ExpandOnHover_Show"));
  } else {
    if (expand_on_hover_start_time_.has_value()) {
      base::UmaHistogramLongTimes(
          "Tabs.VerticalTabs.ExpandOnHover.ShowDuration",
          base::TimeTicks::Now() - expand_on_hover_start_time_.value());
      expand_on_hover_start_time_.reset();
    }
    base::RecordAction(
        base::UserMetricsAction("VerticalTabs_ExpandOnHover_Hide"));
  }

  BrowserAnimationController::From(browser_view_->browser())
      ->Start(TabStripAnimations::kVerticalTabStrip,
              expand ? TabStripAnimations::kExpandOnHover
                     : TabStripAnimations::kCollapseOnHover);
}

void VerticalTabStripRegionView::RegisterExpandOnHoverLock(
    VerticalTabStripExpandOnHoverLock* lock) {
  hover_locks_.insert(lock);
  ExpandOnHoverLockType lock_type = lock->lock_type();
  switch (lock_type) {
    case ExpandOnHoverLockType::kForceCollapse: {
      force_collapse_lock_count_++;
      break;
    }
    case ExpandOnHoverLockType::kKeepCurrentState: {
      keep_current_state_lock_count_++;
      break;
    }
    case ExpandOnHoverLockType::kKeepExpanded: {
      keep_expanded_lock_count_++;
      break;
    }
  }
  UpdateExpandOnHoverState();
}

void VerticalTabStripRegionView::UnregisterExpandOnHoverLock(
    VerticalTabStripExpandOnHoverLock* lock) {
  hover_locks_.erase(lock);
  ExpandOnHoverLockType lock_type = lock->lock_type();
  switch (lock_type) {
    case ExpandOnHoverLockType::kForceCollapse: {
      CHECK_GT(force_collapse_lock_count_, 0);
      force_collapse_lock_count_--;
      if (force_collapse_lock_count_ == 0) {
        UpdateExpandOnHoverState();
      }
      break;
    }
    case ExpandOnHoverLockType::kKeepCurrentState: {
      CHECK_GT(keep_current_state_lock_count_, 0);
      keep_current_state_lock_count_--;
      if (keep_current_state_lock_count_ == 0) {
        UpdateExpandOnHoverState();
      }
      break;
    }
    case ExpandOnHoverLockType::kKeepExpanded: {
      CHECK_GT(keep_expanded_lock_count_, 0);
      keep_expanded_lock_count_--;
      if (keep_expanded_lock_count_ == 0) {
        UpdateExpandOnHoverState();
      }
      break;
    }
  }
}

void VerticalTabStripRegionView::OnOmniboxPopupVisibilityChanged(bool is_open) {
  if (is_open) {
    if (!omnibox_open_lock_) {
      omnibox_open_lock_ =
          GetExpandOnHoverLock(ExpandOnHoverLockType::kForceCollapse);
      UpdateExpandOnHoverState();
    }
  } else {
    omnibox_open_lock_.reset();
  }
}

void VerticalTabStripRegionView::OnActiveTabChanged(
    const tabs::TabInterface* active_tab) {
  omnibox_tab_helper_observation_.Reset();

  if (active_tab) {
    OmniboxTabHelper* const tab_helper =
        OmniboxTabHelper::FromWebContents(active_tab->GetContents());
    if (tab_helper) {
      omnibox_tab_helper_observation_.Observe(tab_helper);
    }
  }

  if (tab_strip_view_) {
    tab_strip_view_->OnActiveTabChanged(active_tab);
  }
}

void VerticalTabStripRegionView::SetLinkDropArrow(
    const std::optional<BrowserRootView::DropIndex>& index) {
  if (!tab_strip_controller_) {
    return;
  }

  if (!index.has_value()) {
    hover_tab_selector_->CancelTabTransition();
    drop_arrow_.reset();
    return;
  }

  if (index->relative_to_index ==
      BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex) {
    hover_tab_selector_->CancelTabTransition();
  } else {
    hover_tab_selector_->StartTabTransition(index->index);
  }

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(
        *index, GetWidget()->GetNativeWindow(),
        base::BindRepeating(&VerticalTabStripRegionView::GetLinkDropBounds,
                            base::Unretained(this)));
  } else if (index != drop_arrow_->index()) {
    drop_arrow_->SetIndex(*index);
  }
}

gfx::Rect VerticalTabStripRegionView::GetLinkDropBounds(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  CHECK_GE(drop_index.index, 0);

  if (tab_strip_model_->count() == 0) {
    return gfx::Rect();
  }

  gfx::Point arrow_position = GetLinkDropArrowPosition(drop_index, direction);
  gfx::Rect drop_bounds =
      GetLinkDropBoundsFromPosition(arrow_position, *direction);

  // If the rect doesn't fit on the monitor, push the arrow to the other side.
  display::Screen* screen = display::Screen::Get();
  gfx::Rect display_bounds =
      screen->GetDisplayNearestView(GetWidget()->GetNativeView()).bounds();

  DropArrow::MaybeAdjustDisplayBounds(display_bounds);

  if (!display_bounds.Contains(drop_bounds)) {
    if (base::i18n::IsRTL() && *direction == DropArrow::Direction::kLeft) {
      *direction = DropArrow::Direction::kRight;
      drop_bounds.Offset(-GetBoundsInScreen().width() - DropArrow::kSize, 0);
    } else if (!base::i18n::IsRTL() &&
               *direction == DropArrow::Direction::kRight) {
      *direction = DropArrow::Direction::kLeft;
      drop_bounds.Offset(GetBoundsInScreen().width() + DropArrow::kSize, 0);
    }
  }

  return drop_bounds;
}

gfx::Point VerticalTabStripRegionView::GetLinkDropArrowPosition(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  int target_x = GetBoundsInScreen().x();
  int target_y = 0;

  // By default have the arrow outside of the browser window pointing towards
  // the tab strip.
  *direction = base::i18n::IsRTL() ? DropArrow::Direction::kLeft
                                   : DropArrow::Direction::kRight;

  const bool replace_index =
      drop_index.relative_to_index ==
      BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;

  if (drop_index.index < tab_strip_model_->count()) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(drop_index.index);
    TabCollectionNode* node = root_node_->GetNodeForHandle(tab->GetHandle());
    views::View* target_view = node->view();

    if ((tab->IsPinned() || (tab->IsSplit() && replace_index)) &&
        !state_controller_->IsCollapsed()) {
      // In the expanded view, pinned and split tabs will have an arrow pointing
      // down at the tab. We don't support dropping a tab in the middle of a
      // split view.
      if (replace_index) {
        target_x = target_view->GetBoundsInScreen().CenterPoint().x();
      } else {
        target_x = base::i18n::IsRTL()
                       ? target_view->GetBoundsInScreen().right()
                       : target_view->GetBoundsInScreen().x();
      }
      target_y = target_view->GetBoundsInScreen().y();
      *direction = DropArrow::Direction::kDown;
    } else if (replace_index) {
      // When a tab is being replaced, point at the middle of the tab.
      target_y = target_view->GetBoundsInScreen().CenterPoint().y();
    } else {
      // Otherwise, we are pointing at the slot before the tab.
      if (drop_index.group_inclusion ==
              BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup &&
          tab->GetGroup().has_value() &&
          tab_strip_model_->group_model()
                  ->GetTabGroup(tab->GetGroup().value())
                  ->GetFirstTab() == tab) {
        // Drop before the group header.
        TabCollectionNode* group_node = root_node_->GetNodeForHandle(
            tab_strip_model_->group_model()
                ->GetTabGroup(tab->GetGroup().value())
                ->GetCollectionHandle());
        target_y = group_node->view()->GetBoundsInScreen().y();
      } else {
        target_y = target_view->GetBoundsInScreen().y();
      }
    }
  } else {
    // Drop at the end of the unpinned container.
    if (auto* unpinned_container = GetUnpinnedTabsContainer()) {
      target_y = unpinned_container->GetBoundsInScreen().bottom();
    } else {
      target_y = tab_strip_view_->GetBoundsInScreen().bottom();
    }
  }

  if (*direction == DropArrow::Direction::kLeft) {
    // In RTL, shift `target_x` to the other side of the tab strip.
    target_x += GetBoundsInScreen().width();
  }

  return gfx::Point(target_x, target_y);
}

gfx::Rect VerticalTabStripRegionView::GetLinkDropBoundsFromPosition(
    gfx::Point position,
    DropArrow::Direction direction) {
  if (direction == DropArrow::Direction::kRight) {
    position.Offset(-DropArrow::kSize, -DropArrow::kSize / 2);
  } else if (direction == DropArrow::Direction::kLeft) {
    position.Offset(DropArrow::kSize, -DropArrow::kSize / 2);
  } else {
    position.Offset(-DropArrow::kSize / 2, -DropArrow::kSize);
  }

  return gfx::Rect(position, gfx::Size(DropArrow::kSize, DropArrow::kSize));
}

BEGIN_METADATA(VerticalTabStripRegionView)
END_METADATA

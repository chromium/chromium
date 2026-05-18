// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"

#include <numeric>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

VerticalSplitTabView::VerticalSplitTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? std::make_unique<GlowHoverController>(
                                  this,
                                  VerticalTabView::kGlowHoverAnimationDuration)
                            : nullptr) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  collection_node->set_detach_child_from_node(
      base::BindRepeating(&VerticalSplitTabView::RemoveChildViewForReparenting,
                          base::Unretained(this)));

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalSplitTabView::ResetCollectionNode, base::Unretained(this)));

  CHECK(collection_node_->GetController());
  auto* state_controller =
      collection_node_->GetController()->GetStateController();
  CHECK(state_controller);
  OnCollapseStateChanged(state_controller->GetCollapseState());
  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(
          base::BindRepeating(&VerticalSplitTabView::OnCollapseStateChanged,
                              base::Unretained(this)));

  // Ensures this view gets mouse events as well its children.
  SetNotifyEnterExitOnChild(true);
}

VerticalSplitTabView::~VerticalSplitTabView() = default;

void VerticalSplitTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBorder();
}

void VerticalSplitTabView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalSplitTabView::UpdateBorder, base::Unretained(this)));

  OnDataChanged();
  UpdateHovered(IsMouseHovered());
}

void VerticalSplitTabView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void VerticalSplitTabView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHovered(true);
}

void VerticalSplitTabView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHovered(false);
}

void VerticalSplitTabView::OnMouseMoved(const ui::MouseEvent& event) {
  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  UpdateHovered(true);
}

void VerticalSplitTabView::OnPaint(gfx::Canvas* canvas) {
  if (pinned_) {
    const std::vector<views::View*> children =
        collection_node_ ? collection_node_->GetDirectChildren()
                         : std::vector<views::View*>();
    std::optional<SkColor> background_color =
        !children.empty() ? views::AsViewClass<VerticalTabView>(children[0])
                                ->GetBackgroundColor()
                          : std::nullopt;
    if (background_color.has_value()) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(background_color.value());
      const float corner_radius =
          GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius) -
          GetInsets().top() / 2.0;
      canvas->DrawRoundRect(GetContentsBounds(), corner_radius, flags);
    }
  }

  views::View::OnPaint(canvas);
}

views::ProposedLayout VerticalSplitTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;

  const std::vector<views::View*> children =
      collection_node_ ? collection_node_->GetDirectChildren()
                       : std::vector<views::View*>();
  if (children.size() != 2) {
    layouts.host_size = gfx::Size(0, 0);
    return layouts;
  }

  const int border_thickness =
      pinned_
          ? GetLayoutConstant(LayoutConstant::kVerticalTabPinnedBorderThickness)
          : 0;

  // Layout children in order. Children will have their preferred height and
  // fill available width. If unbounded or uncollapsed and both children fit on
  // one row they will share it, otherwise they will be stacked vertically.
  if (!size_bounds.width().is_bounded() ||
      (!collapsed_ &&
       size_bounds.width().value() >=
           static_cast<int>(
               GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth) *
               children.size()))) {
    int x = 0;
    for (auto* child : children) {
      gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
      bounds.set_x(x);
      // Fill available width if bounded.
      if (size_bounds.width().is_bounded()) {
        bounds.set_width(
            x == 0 ? (std::floor(size_bounds.width().value() +
                                 2 * border_thickness - kSplitViewGap) /
                      2)
                   : size_bounds.width().value() - x);
      }
      x += bounds.width() - 2 * border_thickness + (x == 0 ? kSplitViewGap : 0);
      height = std::max(height, bounds.height());
      layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    }
    width = x;
  } else {
    int y = 0;
    for (auto* child : children) {
      gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
      bounds.set_y(y);
      bounds.set_width(size_bounds.width().value());
      bounds.set_height(bounds.height());
      y +=
          bounds.height() - 2 * border_thickness + (y == 0 ? kSplitViewGap : 0);
      layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    }
    width = size_bounds.width().value();
    height = y + 2 * border_thickness;
  }
  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

std::optional<BrowserRootView::DropIndex>
VerticalSplitTabView::GetLinkDropIndex(const gfx::Point& loc_in_view) {
  if (!collection_node_ || !collection_node_->GetController()) {
    return std::nullopt;
  }

  CHECK_EQ(collection_node_->children().size(), 2ul);

  const auto& start_node = collection_node_->children()[0];
  gfx::Point start_loc = views::View::ConvertPointToTarget(
      start_node->view(), this,
      start_node->view()->GetLocalBounds().CenterPoint());
  const auto& end_node = collection_node_->children()[1];
  gfx::Point end_loc = views::View::ConvertPointToTarget(
      end_node->view(), this, end_node->view()->GetLocalBounds().CenterPoint());

  VerticalTabDragHandler& drag_handler =
      collection_node_->GetController()->GetDragHandler();

  // Links can't be dropped between tabs in a split view so just determine the
  // closest tab to the drop point.
  if ((start_loc - loc_in_view).LengthSquared() <
      (end_loc - loc_in_view).LengthSquared()) {
    return drag_handler.GetLinkDropIndexForNode(*start_node, std::nullopt);
  } else {
    return drag_handler.GetLinkDropIndexForNode(*end_node, std::nullopt);
  }
}

double VerticalSplitTabView::GetHoverAnimationValue() const {
  if (!hover_controller_) {
    return hovered_ ? 1.0 : 0.0;
  }
  return hover_controller_->GetAnimationValue();
}

void VerticalSplitTabView::ResetCollectionNode() {
  CHECK(collection_node_);
  TabHoverCardController* hover_card_controller =
      collection_node_->GetController()->GetHoverCardController();
  if (hover_card_controller) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);
  }

  node_destroyed_subscription_ = {};
  collapsed_state_changed_subscription_ = {};
  collection_node_ = nullptr;
}

void VerticalSplitTabView::OnDataChanged() {
  const tabs::TabCollection* tab_collection =
      std::get<const tabs::TabCollection*>(collection_node_->GetNodeData());
  const std::vector<tabs::TabInterface*> tabs =
      tab_collection->GetTabsRecursive();
  pinned_ = tabs[0]->IsPinned();

  UpdateBorder();
}

void VerticalSplitTabView::UpdateBorder() {
  if (pinned_) {
    const bool is_frame_active =
        GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
    SetBorder(views::CreateRoundedRectBorder(
        GetLayoutConstant(LayoutConstant::kVerticalTabPinnedBorderThickness),
        GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius),
        is_frame_active ? kColorTabDividerFrameActive
                        : kColorTabDividerFrameInactive));
  } else if (GetBorder()) {
    SetBorder(nullptr);
  }
}

void VerticalSplitTabView::UpdateHovered(bool hovered) {
  if (hovered_ == hovered) {
    return;
  }

  hovered_ = hovered;

  float radial_highlight_opacity = 1.0f;
  for (views::View* child : children()) {
    if (auto* tab_view = views::AsViewClass<VerticalTabView>(child)) {
      tab_view->UpdateHovered(hovered_);
      radial_highlight_opacity = tab_view->radial_highlight_opacity();
    }
  }

  if (hover_controller_) {
    if (hovered_) {
      hover_controller_->SetSubtleOpacityScale(radial_highlight_opacity);
      hover_controller_->Show(TabStyle::ShowHoverStyle::kSubtle);
    } else {
      hover_controller_->Hide(TabStyle::HideHoverStyle::kGradual);
    }
  }

  SchedulePaint();
}

void VerticalSplitTabView::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  collapsed_ = state == tabs::VerticalTabStripCollapseState::kCollapsed;
}

std::unique_ptr<views::View>
VerticalSplitTabView::RemoveChildViewForReparenting(views::View* child_view) {
  DCHECK(std::ranges::contains(children(), child_view));
  CHECK(collection_node_);

  auto children = collection_node_->GetDirectChildren();
  auto source_layout_info = std::make_unique<
      TabCollectionAnimatingLayoutManager::SourceLayoutInfo>(
      TabCollectionAnimatingLayoutManager::SourceLayoutInfo{
          .animation_axis =
              TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal,
          // Note: Tabs are removed from the split view collection from the
          // front first so it is necessary to test the number of children
          // in the collection when computing the animation direction.
          .animation_direction =
              (children.size() == 2 && children[0] == child_view)
                  ? TabCollectionAnimatingLayoutManager::AnimationDirection::
                        kEndToStart
                  : TabCollectionAnimatingLayoutManager::AnimationDirection::
                        kStartToEnd,
      });

  // Ensure we remove the child view before setting source layout info to
  // prevent the manager from clearing the metadata.
  auto removed_child_view = RemoveChildViewT(child_view);
  TabCollectionAnimatingLayoutManager::SetSourceLayoutInfo(
      child_view, std::move(source_layout_info));

  return removed_child_view;
}

BEGIN_METADATA(VerticalSplitTabView)
END_METADATA

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"

#include <numeric>
#include <vector>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
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
#include "ui/views/widget/widget.h"

VerticalSplitTabView::VerticalSplitTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? std::make_unique<GlowHoverController>(this)
                            : nullptr) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  collection_node->set_detach_child_from_node(
      base::BindRepeating(&VerticalSplitTabView::RemoveChildViewForReparenting,
                          base::Unretained(this)));

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalSplitTabView::ResetCollectionNode, base::Unretained(this)));

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
        !children.empty()
            ? static_cast<VerticalTabView*>(children[0])->GetBackgroundColor()
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
  // fill available width. If unbounded or both children fit on one row they
  // will share it, otherwise they will be stacked vertically.
  if (!size_bounds.width().is_bounded() ||
      size_bounds.width().value() >=
          static_cast<int>(
              GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth) *
              children.size())) {
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

  VerticalTabDragHandler& drag_handler =
      collection_node_->GetController()->GetDragHandler();

  for (const auto& node : collection_node_->children()) {
    auto* view = node->view();
    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_view);

    // If the drag lands on any individual tab (using the horizontal position
    // to determine if it's near the center), then replace the contents of
    // that tab.
    constexpr double kDragOverMargins = 0.2;
    if (view->HitTestPoint(loc_in_child) &&
        loc_in_child.x() > view->width() * kDragOverMargins &&
        loc_in_child.x() < view->width() * (1.0 - kDragOverMargins)) {
      return drag_handler.GetLinkDropIndexForNode(*node, std::nullopt);
    }
  }

  // Fallback: If the drag appears in between the two tabs use the vertical
  // drag position to place the new tab before/after the split.
  return drag_handler.GetLinkDropIndexForNode(*collection_node_,
                                              loc_in_view.y() < height() / 2
                                                  ? DragPositionHint::kBefore
                                                  : DragPositionHint::kAfter);
}

double VerticalSplitTabView::GetHoverAnimationValue() const {
  if (!hover_controller_) {
    return hovered_ ? 1.0 : 0.0;
  }
  return hover_controller_->GetAnimationValue();
}

void VerticalSplitTabView::ResetCollectionNode() {
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

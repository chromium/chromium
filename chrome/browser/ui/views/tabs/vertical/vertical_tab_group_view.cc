// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include <numeric>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kTabVerticalPadding = 2;
constexpr int kGroupLineWidth = 2;
constexpr int kGroupLineCornerRadius = 4;
constexpr int kGroupHeaderHeight = 26;
constexpr int kGroupHeaderCornerRadius = 8;
constexpr int kGroupHeaderHorizontalInset = 8;
constexpr int kTabLeadingPadding = 10;

class VerticalTabGroupHeader : public views::Label {
  METADATA_HEADER(VerticalTabGroupHeader, views::Label)
 public:
  VerticalTabGroupHeader() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(0, kGroupHeaderHorizontalInset)));
  }
};

BEGIN_METADATA(VerticalTabGroupHeader)
END_METADATA
}  // namespace

VerticalTabGroupView::VerticalTabGroupView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      group_header_(AddChildView(std::make_unique<VerticalTabGroupHeader>())),
      group_line_(AddChildView(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabGroupView::ResetCollectionNode, base::Unretained(this)));

  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabGroupView::OnDataChanged, base::Unretained(this)));
  OnDataChanged();
}

VerticalTabGroupView::~VerticalTabGroupView() = default;

views::ProposedLayout VerticalTabGroupView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;

  gfx::Rect header_bounds;
  header_bounds.set_y(height);
  header_bounds.set_height(kGroupHeaderHeight);
  // If fully bounded, the group header should respect width constraints and
  // take up the available width excluding trailing horizontal padding.
  if (size_bounds.is_fully_bounded()) {
    header_bounds.set_width(size_bounds.width().value());
  }
  layouts.child_layouts.emplace_back(
      group_header_.get(), group_header_->GetVisible(), header_bounds);
  height += header_bounds.height() + kTabVerticalPadding;
  width = std::max(width, header_bounds.width());

  gfx::Rect group_line_bounds = gfx::Rect(
      (kTabLeadingPadding - kGroupLineWidth) / 2, height, kGroupLineWidth, 0);

  const auto children = collection_node_->GetDirectChildren();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
    bounds.set_y(height);
    bounds.set_x(kTabLeadingPadding);
    // If fully bounded, child views should respect width constraints and take
    // up the available width excluding trailing horizontal padding.
    if (size_bounds.is_fully_bounded()) {
      bounds.set_width(size_bounds.width().value() - kTabLeadingPadding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width());
  }
  // Remove excess padding if needed.
  height -= kTabVerticalPadding;

  if (!children.empty()) {
    group_line_bounds.set_height(height - group_line_bounds.y());
  }
  layouts.child_layouts.emplace_back(
      group_line_.get(), group_line_->GetVisible(), group_line_bounds);

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

void VerticalTabGroupView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabGroupView::OnDataChanged() {
  tabs_api::mojom::TabGroup tab_group =
      *collection_node_->data()->get_tab_group();
  group_header_->SetText(tab_group.data.title());
  if (GetColorProvider()) {
    SkColor color = GetColorProvider()->GetColor(GetTabGroupTabStripColorId(
        tab_group.data.color(), GetWidget()->ShouldPaintAsActive()));
    group_header_->SetEnabledColor(color_utils::GetColorWithMaxContrast(color));
    group_header_->SetBackground(
        views::CreateRoundedRectBackground(color, kGroupHeaderCornerRadius));
    group_line_->SetBackground(views::CreateRoundedRectBackground(
        color, gfx::RoundedCornersF(0, kGroupLineCornerRadius,
                                    kGroupLineCornerRadius, 0)));
  }
}

BEGIN_METADATA(VerticalTabGroupView)
END_METADATA

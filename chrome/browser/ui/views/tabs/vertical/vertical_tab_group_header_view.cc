// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <numeric>

#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupHeaderCornerRadius = 8;
constexpr int kGroupHeaderHorizontalInset = 8;

class VerticalTabGroupHeaderLabel : public views::Label {
  METADATA_HEADER(VerticalTabGroupHeaderLabel, views::Label)
 public:
  VerticalTabGroupHeaderLabel() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(0, kGroupHeaderHorizontalInset)));
  }
};

BEGIN_METADATA(VerticalTabGroupHeaderLabel)
END_METADATA
}  // namespace

VerticalTabGroupHeaderView::VerticalTabGroupHeaderView(
    const tab_groups::TabGroupVisualData* tab_group_visual_data)
    : group_header_label_(
          AddChildView(std::make_unique<VerticalTabGroupHeaderLabel>())) {
  OnDataChanged(tab_group_visual_data);
}

VerticalTabGroupHeaderView::~VerticalTabGroupHeaderView() = default;

void VerticalTabGroupHeaderView::OnDataChanged(
    const tab_groups::TabGroupVisualData* tab_group_visual_data) {
  group_header_label_->SetText(tab_group_visual_data->title());
  if (GetColorProvider()) {
    SkColor color = GetColorProvider()->GetColor(GetTabGroupTabStripColorId(
        tab_group_visual_data->color(), GetWidget()->ShouldPaintAsActive()));
    group_header_label_->SetEnabledColor(
        color_utils::GetColorWithMaxContrast(color));
    SetBackground(
        views::CreateRoundedRectBackground(color, kGroupHeaderCornerRadius));
  }
}

BEGIN_METADATA(VerticalTabGroupHeaderView)
END_METADATA

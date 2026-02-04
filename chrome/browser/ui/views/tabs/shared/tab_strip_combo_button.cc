// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

TabStripComboButton::TabStripComboButton(
    std::unique_ptr<TabStripFlatEdgeButton> start_button,
    std::unique_ptr<TabStripFlatEdgeButton> end_button) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      GetLayoutConstant(
          LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding)));

  if (start_button) {
    start_button_ = AddChildView(std::move(start_button));
  }
  if (end_button) {
    end_button_ = AddChildView(std::move(end_button));
  }
  UpdateStyles();
}

TabStripComboButton::~TabStripComboButton() = default;

void TabStripComboButton::SetOrientation(views::LayoutOrientation orientation) {
  if (orientation_ == orientation) {
    return;
  }
  orientation_ = orientation;

  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  layout->SetOrientation(orientation_);
  layout->set_between_child_spacing(GetLayoutConstant(
      LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding));

  UpdateStyles();
}

void TabStripComboButton::ChildVisibilityChanged(views::View* child) {
  UpdateStyles();
}

void TabStripComboButton::UpdateStyles() {
  const bool both_visible = start_button_ && start_button_->GetVisible() &&
                            end_button_ && end_button_->GetVisible();
  const bool is_vertical = orientation_ == views::LayoutOrientation::kVertical;

  if (start_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kBottom;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kLeft
                        : TabStripFlatEdgeButton::FlatEdge::kRight;
      }
    }
    start_button_->SetFlatEdge(flat_edge);
  }

  if (end_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kTop;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kRight
                        : TabStripFlatEdgeButton::FlatEdge::kLeft;
      }
    }
    end_button_->SetFlatEdge(flat_edge);
  }
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA

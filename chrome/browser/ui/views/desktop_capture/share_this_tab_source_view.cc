// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"

#include "ui/views/layout/box_layout.h"

namespace {

constexpr gfx::Size kPreviewSize(320, 240);
constexpr int kPadding = 8;

}  // namespace

ShareThisTabSourceView::ShareThisTabSourceView() {
  View* throbber_container = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* throbber_layout =
      throbber_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  throbber_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  throbber_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  // TODO(crbug.com/1428878): Use distances from LayoutProvider
  throbber_container->SetBoundsRect(
      gfx::Rect(gfx::Point(kPadding, kPadding), kPreviewSize));
  throbber_container->SetCanProcessEventsWithinSubtree(false);
  throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>());
  throbber_->Start();
}

ShareThisTabSourceView::~ShareThisTabSourceView() = default;

void ShareThisTabSourceView::Activate() {
  throbber_->Stop();
  throbber_->SetVisible(false);
}

gfx::Size ShareThisTabSourceView::CalculatePreferredSize() const {
  // TODO(crbug.com/1428878): Use distances from LayoutProvider
  return kPreviewSize + gfx::Size(2 * kPadding, 2 * kPadding);
}

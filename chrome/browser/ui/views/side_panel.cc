// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel.h"

#include "base/stl_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(pbos): Figure out what our preferred width should be.
constexpr int kDefaultWidth = 320;

}  // namespace

SidePanel::SidePanel() {
  AddObserver(this);
  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPanelWidth(kDefaultWidth);
}

SidePanel::~SidePanel() {
  RemoveObserver(this);
}

void SidePanel::SetPanelWidth(int width) {
  // Only the width is used by BrowserViewLayout.
  SetPreferredSize(gfx::Size(width, 1));
}

void SidePanel::ChildVisibilityChanged(View* child) {
  UpdateVisibility();
}

void SidePanel::OnChildViewAdded(View* observed_view, View* child) {
  UpdateVisibility();
}

void SidePanel::OnChildViewRemoved(View* observed_view, View* child) {
  UpdateVisibility();
}

void SidePanel::UpdateVisibility() {
  // TODO(pbos): Iterate content instead. Requires moving the owned pointer out
  // of owned contents before resetting it.
  for (const auto* view : children()) {
    if (view->GetVisible()) {
      SetVisible(true);
      return;
    }
  }
  SetVisible(false);
}

BEGIN_METADATA(SidePanel, views::View)
END_METADATA

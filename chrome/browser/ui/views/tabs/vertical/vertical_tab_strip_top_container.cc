// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTopButtonContainerHeight = 28;
}  // namespace

VerticalTabStripTopContainer::VerticalTabStripTopContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item)
    : state_controller_(state_controller),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripTopContainerElementId);
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

// TODO(crbug.com/445528000): Update height calculation after child components
// are added
views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  if (size_bounds.width().is_bounded()) {
    layout.host_size =
        gfx::Size(size_bounds.width().value(), kTopButtonContainerHeight);
  } else {
    layout.host_size = gfx::Size(parent()->width(), kTopButtonContainerHeight);
  }

  return layout;
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA

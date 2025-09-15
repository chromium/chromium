// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

namespace {
constexpr int kVerticalTabHeight = 32;
constexpr int kVerticalPinnedTabPreferredWidth = 40;
}  // namespace

VerticalTabView::VerticalTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.
}

VerticalTabView::~VerticalTabView() = default;

views::ProposedLayout VerticalTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.
  views::ProposedLayout layouts;
  layouts.host_size =
      gfx::Size(kVerticalPinnedTabPreferredWidth, kVerticalTabHeight);

  return layouts;
}

void VerticalTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

BEGIN_METADATA(VerticalTabView)
END_METADATA

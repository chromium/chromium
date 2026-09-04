// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_collection_z_order_manager.h"

#include <algorithm>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_utils.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(TabCollectionZOrderManager::ZOrderLevel)

DEFINE_UI_CLASS_PROPERTY_KEY(TabCollectionZOrderManager::ZOrderLevel,
                             kTabZOrderKey,
                             TabCollectionZOrderManager::ZOrderLevel::kDefault)

TabCollectionZOrderManager::TabCollectionZOrderManager() = default;

TabCollectionZOrderManager::~TabCollectionZOrderManager() = default;

void TabCollectionZOrderManager::InvalidateZOrder() {
  z_order_dirty_ = true;
  z_order_cache_.clear();
  UpdateOwnZOrderProperty();
}

void TabCollectionZOrderManager::OnChildZOrderChanged(views::View* child) {
  z_order_dirty_ = true;
  UpdateOwnZOrderProperty();
}

void TabCollectionZOrderManager::UpdateOwnZOrderProperty(
    const views::View* excluded_child) {
  auto max_child_z = ZOrderLevel::kDefault;

  for (const views::View* child : children()) {
    if (child == excluded_child) {
      continue;
    }
    ZOrderLevel child_z = child->GetProperty(kTabZOrderKey);

    // Formally filter out non-propagating levels (e.g. kGroupUnderline).
    if (ShouldPropagateZOrder(child_z)) {
      max_child_z = std::max(max_child_z, child_z);
    }
  }

  // If container's own effective level didn't change, don't bubble up.
  if (GetProperty(kTabZOrderKey) == max_child_z) {
    return;
  }

  SetProperty(kTabZOrderKey, max_child_z);

  // Bubble up to ancestor container.
  if (auto* parent_container =
          views::AsViewClass<TabCollectionZOrderManager>(parent())) {
    parent_container->OnChildZOrderChanged(this);
  }
}

views::View::Views TabCollectionZOrderManager::GetChildrenInZOrder() {
  if (z_order_dirty_ || z_order_cache_.size() != children().size()) {
    z_order_cache_ = children();
    std::stable_sort(z_order_cache_.begin(), z_order_cache_.end(),
                     [](const views::View* a, const views::View* b) {
                       return a->GetProperty(kTabZOrderKey) <
                              b->GetProperty(kTabZOrderKey);
                     });
    z_order_dirty_ = false;
  }
  return z_order_cache_;
}

void TabCollectionZOrderManager::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::View::ViewHierarchyChanged(details);

  if (details.parent == this) {
    z_order_dirty_ = true;
    z_order_cache_.clear();
    // The view to be removed shouldn't be part of the z-order calculation.
    UpdateOwnZOrderProperty(!details.is_add ? details.child.get() : nullptr);
  }
}

BEGIN_METADATA(TabCollectionZOrderManager)
END_METADATA

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_COLLECTION_Z_ORDER_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_COLLECTION_Z_ORDER_MANAGER_H_

#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// Base class for tab collection containers that caches child z-order,
// maintains natural children() order, and propagates z-order property changes
// upward.
class TabCollectionZOrderManager : public views::View {
  METADATA_HEADER(TabCollectionZOrderManager, views::View)

 public:
  enum class ZOrderLevel : int {
    // --- Propagating Levels (bubbles up to parent containers) ---
    kDefault = 0,   // Inactive tabs, regular group headers
    kHovered = 1,   // Hovered tab
    kSelected = 2,  // Selected tab
    kActive = 3,    // Active tab
    kMaxTabLevel = kActive,

    // --- Container-Local Levels (Does not bubble up) ---
    kGroupUnderline = 100,  // Group underline
  };

  // Predicate defining whether a level should elevate parent containers.
  static constexpr bool ShouldPropagateZOrder(ZOrderLevel level) {
    return level <= ZOrderLevel::kMaxTabLevel;
  }

  TabCollectionZOrderManager();
  TabCollectionZOrderManager(const TabCollectionZOrderManager&) = delete;
  TabCollectionZOrderManager& operator=(const TabCollectionZOrderManager&) =
      delete;
  ~TabCollectionZOrderManager() override;

  // Called when a child view's kTabZOrderKey changes.
  void OnChildZOrderChanged(views::View* child);

  // views::View:
  views::View::Views GetChildrenInZOrder() override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  bool is_z_order_cache_empty_for_testing() const {
    return z_order_cache_.empty();
  }

 protected:
  void InvalidateZOrder();

 private:
  void UpdateOwnZOrderProperty(const views::View* excluded_child = nullptr);

  bool z_order_dirty_ = true;
  views::View::Views z_order_cache_;
};

DECLARE_UI_CLASS_PROPERTY_TYPE(TabCollectionZOrderManager::ZOrderLevel)

// Property key storing a view's ZOrderLevel.
extern const ui::ClassProperty<TabCollectionZOrderManager::ZOrderLevel>* const
    kTabZOrderKey;

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_COLLECTION_Z_ORDER_MANAGER_H_

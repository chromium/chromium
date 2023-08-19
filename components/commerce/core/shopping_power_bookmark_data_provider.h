// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_POWER_BOOKMARK_DATA_PROVIDER_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_POWER_BOOKMARK_DATA_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"

namespace power_bookmarks {
class PowerBookmarkService;
class PowerBookmarkMeta;
}  // namespace power_bookmarks

namespace commerce {

class ShoppingService;

// Responsible for automatically attaching product information, if available,
// to a bookmarks when it is saved.
class ShoppingPowerBookmarkDataProvider
    : public power_bookmarks::PowerBookmarkDataProvider {
 public:
  ShoppingPowerBookmarkDataProvider(
      power_bookmarks::PowerBookmarkService* power_bookmark_service,
      ShoppingService* shopping_service);
  ShoppingPowerBookmarkDataProvider(const ShoppingPowerBookmarkDataProvider&) =
      delete;
  ShoppingPowerBookmarkDataProvider& operator=(
      const ShoppingPowerBookmarkDataProvider&) = delete;
  ~ShoppingPowerBookmarkDataProvider() override;

  // PowerBookmarkDataProvider implementation
  void AttachMetadataForNewBookmark(
      const bookmarks::BookmarkNode* node,
      power_bookmarks::PowerBookmarkMeta* meta) override;

 private:
  raw_ptr<power_bookmarks::PowerBookmarkService> power_bookmark_service_;
  raw_ptr<ShoppingService> shopping_service_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_POWER_BOOKMARK_DATA_PROVIDER_H_

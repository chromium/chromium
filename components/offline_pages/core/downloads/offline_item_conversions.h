// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_OFFLINE_ITEM_CONVERSIONS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_OFFLINE_ITEM_CONVERSIONS_H_

#include "components/offline_items_collection/core/offline_item.h"

using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;

namespace offline_pages {

struct OfflinePageItem;
class SavePageRequest;

const char kOfflinePageNamespace[] = "LEGACY_OFFLINE_PAGE";

// Allows to convert between internal offline pages types and their offline item
// collection representation (for displaying in UI).
class OfflineItemConversions {
 public:
  OfflineItemConversions() = delete;
  OfflineItemConversions(const OfflineItemConversions&) = delete;
  OfflineItemConversions& operator=(const OfflineItemConversions&) = delete;

  static OfflineItem CreateOfflineItem(const OfflinePageItem& page);
  static OfflineItem CreateOfflineItem(const SavePageRequest& request);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_OFFLINE_ITEM_CONVERSIONS_H_

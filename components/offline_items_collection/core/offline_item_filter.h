// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_FILTER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_FILTER_H_

#include <iosfwd>

namespace offline_items_collection {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
// GENERATED_JAVA_PREFIX_TO_STRIP: FILTER_
enum OfflineItemFilter {
  FILTER_PAGE = 0,
  FILTER_VIDEO,
  FILTER_AUDIO,
  FILTER_IMAGE,
  FILTER_DOCUMENT,
  FILTER_OTHER,

  FILTER_NUM_ENTRIES,
};

// Implemented for test-only. See test_support/offline_item_test_support.cc.
std::ostream& operator<<(std::ostream& os, OfflineItemFilter state);

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_FILTER_H_

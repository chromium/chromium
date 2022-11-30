// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_PENDING_STATE_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_PENDING_STATE_H_

#include <iosfwd>

namespace offline_items_collection {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class PendingState {
  // Enum for reason OfflineItem is pending, if any.
  NOT_PENDING,      // Download is not pending.
  PENDING_NETWORK,  // Download is pending due to no network connection.
  PENDING_ANOTHER_DOWNLOAD,  // Download is pending because another download
                             // is currently being downloaded.
};

// Implemented for testing only. See test_support/offline_item_test_support.cc.
std::ostream& operator<<(std::ostream& os, PendingState state);

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_PENDING_STATE_H_

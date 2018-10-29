// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_TYPES_H_

#include <stdint.h>

#include <utility>
#include <vector>

// This file contains common types and callbacks used by storage of various
// offline page related components.
namespace offline_pages {

// Current store state. When LOADED, the store is operational. When
// loading or reset fails, it is reflected appropriately.
enum class StoreState {
  NOT_LOADED,      // Store is not loaded yet.
  LOADED,          // Store is properly loaded and operational.
  FAILED_LOADING,  // Store initialization failed.
  FAILED_RESET,    // Resetting the store failed.
  INITIALIZING,    // Store is in the process of initializing.
};

// Statuses referring to actions taken on items in the stores.
// GENERATED_JAVA_ENUM_PACKAGE:org.chromium.components.offlinepages
enum class ItemActionStatus {
  SUCCESS,
  ALREADY_EXISTS,
  NOT_FOUND,
  STORE_ERROR,
};

// Result for synchronous operations (like database and file operations) that
// are part of the tasks used by Offline Pages.
// Keep it in sync with OfflinePagesSyncOperationResult in enums.xml for
// histograms usages.
enum class SyncOperationResult {
  SUCCESS,                   // Successful operation
  INVALID_DB_CONNECTION,     // Invalid database connection
  TRANSACTION_BEGIN_ERROR,   // Failed when start a DB transaction
  TRANSACTION_COMMIT_ERROR,  // Failed when commiting a DB transaction
  DB_OPERATION_ERROR,        // Failed when executing a DB statement
  FILE_OPERATION_ERROR,      // Failed while doing file operations
  kMaxValue = FILE_OPERATION_ERROR,
};

// List of item action statuses mapped to item ID.
typedef std::vector<std::pair<int64_t, ItemActionStatus>> MultipleItemStatuses;

// Collective result for store update.
template <typename T>
class StoreUpdateResult {
 public:
  explicit StoreUpdateResult(StoreState state) : store_state(state) {}
  ~StoreUpdateResult() {}

  // Move-only to avoid accidental copies.
  StoreUpdateResult(const StoreUpdateResult& other) = delete;
  StoreUpdateResult(StoreUpdateResult&& other) = default;

  StoreUpdateResult& operator=(const StoreUpdateResult&) = delete;
  StoreUpdateResult& operator=(StoreUpdateResult&&) = default;

  // List of Offline ID to item action status mappings.
  // It is meant to be consumed by the original caller of the operation.
  MultipleItemStatuses item_statuses;

  // List of successfully updated offline page items as seen after operation
  // concludes. It is meant to be used when passing to the observers.
  std::vector<T> updated_items;

  // State of the store after the operation is done.
  StoreState store_state;
};

// This enum is backed by a UMA histogram therefore its entries should not be
// deleted or re-ordered and new ones should only be appended.
// See enum definition with the same name in tools/metrics/histograms/enum.xml.
enum class OfflinePagesStoreEvent {
  kOpenedFirstTime = 0,
  kReopened = 1,
  kClosed = 2,
  kCloseSkipped = 3,
  kMaxValue = kCloseSkipped,
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_TYPES_H_

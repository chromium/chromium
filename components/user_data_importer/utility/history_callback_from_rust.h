// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_

#include <vector>

namespace user_data_importer {

struct HistoryEntry;

// The primary purpose of this class is to provide an API that
// 1) can wrap base::RepeatingCallback, and
// 2) can be handled by cxx-based C++/Rust FFI
class HistoryCallbackFromRust {
 public:
  // Callback function called from Rust to import history entries.
  // This input vector is cleared within this function call.
  // The "completed" argument must be false if there are still more history
  // entries to import and false if the history parsing is completed.
  virtual void ImportHistoryEntries(std::vector<HistoryEntry>& history_entries,
                                    bool completed) = 0;

  virtual ~HistoryCallbackFromRust() = default;

  // This type is non-copyable and non-movable.
  HistoryCallbackFromRust(const HistoryCallbackFromRust&) = delete;
  HistoryCallbackFromRust(HistoryCallbackFromRust&&) = delete;
  HistoryCallbackFromRust& operator=(const HistoryCallbackFromRust&) = delete;
  HistoryCallbackFromRust& operator=(HistoryCallbackFromRust&&) = delete;

 protected:
  HistoryCallbackFromRust() = default;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_

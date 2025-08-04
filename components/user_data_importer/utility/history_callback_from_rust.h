// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_

#include <memory>
#include <vector>

namespace user_data_importer {

struct SafariHistoryEntry;
struct StablePortabilityHistoryEntry;

// The primary purpose of this class is to provide an API that
// 1) can wrap base::RepeatingCallback, and
// 2) can be handled by cxx-based C++/Rust FFI
template <typename HistoryType>
class HistoryCallbackFromRust {
 public:
  // Callback function called from Rust to import history entries. The
  // "completed" argument must be false if there are still more history entries
  // to import and false if the history parsing is completed.
  virtual void ImportHistoryEntries(
      std::unique_ptr<std::vector<HistoryType>> history_entries,
      bool completed) = 0;

  // Called from Rust to signal that parsing has failed.
  virtual void Fail() = 0;

  virtual ~HistoryCallbackFromRust() = default;

  // This type is non-copyable and non-movable.
  HistoryCallbackFromRust(const HistoryCallbackFromRust&) = delete;
  HistoryCallbackFromRust(HistoryCallbackFromRust&&) = delete;
  HistoryCallbackFromRust& operator=(const HistoryCallbackFromRust&) = delete;
  HistoryCallbackFromRust& operator=(HistoryCallbackFromRust&&) = delete;

 protected:
  HistoryCallbackFromRust() = default;
};

// Cxx-friendly type aliases for the template class.
using SafariHistoryCallbackFromRust =
    HistoryCallbackFromRust<SafariHistoryEntry>;
using StablePortabilityHistoryCallbackFromRust =
    HistoryCallbackFromRust<StablePortabilityHistoryEntry>;

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_HISTORY_CALLBACK_FROM_RUST_H_

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_history.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace prerender {

NoStatePrefetchHistory::NoStatePrefetchHistory(size_t max_items)
    : max_items_(max_items) {
  DCHECK(max_items > 0);
}

NoStatePrefetchHistory::~NoStatePrefetchHistory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NoStatePrefetchHistory::AddEntry(const Entry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (entries_.size() >= max_items_) {
    entries_.pop_front();
  }
  entries_.push_back(entry);
}

void NoStatePrefetchHistory::Clear() {
  entries_.clear();
}

base::Value::List NoStatePrefetchHistory::CopyEntriesAsValue() const {
  base::Value::List return_list;
  // Javascript needs times in terms of milliseconds since Jan 1, 1970.
  base::Time epoch_start = base::Time::UnixEpoch();
  for (const Entry& entry : base::Reversed(entries_)) {
    base::Value::Dict entry_dict;
    entry_dict.Set("url", entry.url.spec());
    entry_dict.Set("final_status", NameFromFinalStatus(entry.final_status));
    entry_dict.Set("origin", NameFromOrigin(entry.origin));
    // Use a string to prevent overflow, as Values don't support 64-bit
    // integers.
    entry_dict.Set(
        "end_time",
        base::NumberToString((entry.end_time - epoch_start).InMilliseconds()));
    return_list.Append(std::move(entry_dict));
  }
  return return_list;
}

}  // namespace prerender

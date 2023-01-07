// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"

#include <algorithm>
#include <tuple>

#include "components/blocklist/opt_out_blocklist/opt_out_store.h"

namespace blocklist {

OptOutBlocklistItem::OptOutRecord::OptOutRecord(base::Time entry_time,
                                                bool opt_out)
    : entry_time_(entry_time), opt_out_(opt_out) {}

OptOutBlocklistItem::OptOutRecord::~OptOutRecord() = default;

OptOutBlocklistItem::OptOutRecord::OptOutRecord(OptOutRecord&&) noexcept =
    default;
OptOutBlocklistItem::OptOutRecord& OptOutBlocklistItem::OptOutRecord::operator=(
    OptOutRecord&&) noexcept = default;

bool OptOutBlocklistItem::OptOutRecord::operator<(
    const OptOutRecord& other) const {
  // Fresher entries are lower priority to evict, as are non-opt-outs.
  return std::tie(entry_time_, opt_out_) >
         std::tie(other.entry_time_, other.opt_out_);
}

OptOutBlocklistItem::OptOutBlocklistItem(size_t stored_history_length,
                                         int opt_out_block_list_threshold,
                                         base::TimeDelta block_list_duration)
    : max_stored_history_length_(stored_history_length),
      opt_out_block_list_threshold_(opt_out_block_list_threshold),
      max_block_list_duration_(block_list_duration),
      total_opt_out_(0) {}

OptOutBlocklistItem::~OptOutBlocklistItem() = default;

void OptOutBlocklistItem::AddEntry(bool opt_out, base::Time entry_time) {
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);

  opt_out_records_.emplace(entry_time, opt_out);

  if (opt_out && (!most_recent_opt_out_time_ ||
                  entry_time > most_recent_opt_out_time_.value())) {
    most_recent_opt_out_time_ = entry_time;
  }
  total_opt_out_ += opt_out ? 1 : 0;

  // Remove the oldest entry if the size exceeds the max history size.
  if (opt_out_records_.size() > max_stored_history_length_) {
    DCHECK_EQ(opt_out_records_.size(), max_stored_history_length_ + 1);
    DCHECK_LE(opt_out_records_.top().entry_time(), entry_time);
    total_opt_out_ -= opt_out_records_.top().opt_out() ? 1 : 0;
    opt_out_records_.pop();
  }
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);
}

bool OptOutBlocklistItem::IsBlockListed(base::Time now) const {
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);
  return most_recent_opt_out_time_ &&
         now - most_recent_opt_out_time_.value() < max_block_list_duration_ &&
         total_opt_out_ >= opt_out_block_list_threshold_;
}

size_t OptOutBlocklistItem::OptOutRecordsSizeForTesting() const {
  return opt_out_records_.size();
}

}  // namespace blocklist

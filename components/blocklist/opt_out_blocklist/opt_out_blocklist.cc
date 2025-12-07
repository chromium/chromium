// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/time/clock.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"

namespace blocklist {

OptOutBlocklist::OptOutBlocklist(std::unique_ptr<OptOutStore> opt_out_store,
                                 base::Clock* clock,
                                 OptOutBlocklistDelegate* blocklist_delegate)
    : loaded_(false),
      opt_out_store_(std::move(opt_out_store)),
      clock_(clock),
      blocklist_delegate_(blocklist_delegate) {
  DCHECK(clock_);
  DCHECK(blocklist_delegate_);
}

OptOutBlocklist::~OptOutBlocklist() = default;

void OptOutBlocklist::Init() {
  DCHECK(!loaded_);
  DCHECK(!blocklist_data_);
  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  std::unique_ptr<BlocklistData::Policy> session_policy;
  if (ShouldUseSessionPolicy(&duration, &history, &threshold)) {
    session_policy =
        std::make_unique<BlocklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlocklistData::Policy> persistent_policy;
  if (ShouldUsePersistentPolicy(&duration, &history, &threshold)) {
    persistent_policy =
        std::make_unique<BlocklistData::Policy>(duration, history, threshold);
  }

  size_t max_hosts = 0;
  std::unique_ptr<BlocklistData::Policy> host_policy;
  if (ShouldUseHostPolicy(&duration, &history, &threshold, &max_hosts)) {
    host_policy =
        std::make_unique<BlocklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlocklistData::Policy> type_policy;
  if (ShouldUseTypePolicy(&duration, &history, &threshold)) {
    type_policy =
        std::make_unique<BlocklistData::Policy>(duration, history, threshold);
  }

  auto blocklist_data = std::make_unique<BlocklistData>(
      std::move(session_policy), std::move(persistent_policy),
      std::move(host_policy), std::move(type_policy), max_hosts,
      GetAllowedTypes());

  if (opt_out_store_) {
    opt_out_store_->LoadBlockList(
        std::move(blocklist_data),
        base::BindOnce(&OptOutBlocklist::LoadBlockListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlockListDone(std::move(blocklist_data));
  }
}

base::Time OptOutBlocklist::AddEntry(const std::string& host_name,
                                     bool opt_out,
                                     int type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = clock_->Now();

  // If the |blocklist_data| has been loaded from |opt_out_store_|,  synchronous
  // operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    AddEntrySync(host_name, opt_out, type, now);
  } else {
    QueuePendingTask(base::BindOnce(&OptOutBlocklist::AddEntrySync,
                                    base::Unretained(this), host_name, opt_out,
                                    type, now));
  }

  return now;
}

void OptOutBlocklist::AddEntrySync(const std::string& host_name,
                                   bool opt_out,
                                   int type,
                                   base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);

  bool host_was_blocklisted =
      blocklist_data_->IsHostBlocklisted(host_name, time);
  bool user_was_blocklisted = blocklist_data_->IsUserOptedOutInGeneral(time);
  blocklist_data_->AddEntry(host_name, opt_out, type, time, false);

  if (!host_was_blocklisted &&
      blocklist_data_->IsHostBlocklisted(host_name, time)) {
    // Notify |blocklist_delegate_| about a new blocklisted host.
    blocklist_delegate_->OnNewBlocklistedHost(host_name, time);
  }

  if (user_was_blocklisted != blocklist_data_->IsUserOptedOutInGeneral(time)) {
    // Notify |blocklist_delegate_| about a new blocklisted host.
    blocklist_delegate_->OnUserBlocklistedStatusChange(
        blocklist_data_->IsUserOptedOutInGeneral(time));
  }

  if (!opt_out_store_) {
    return;
  }
  opt_out_store_->AddEntry(opt_out, host_name, type, time);
}

BlocklistReason OptOutBlocklist::IsLoadedAndAllowed(
    const std::string& host_name,
    int type,
    bool ignore_long_term_block_list_rules,
    std::vector<BlocklistReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return BlocklistReason::kBlocklistNotLoaded;
  }
  passed_reasons->push_back(BlocklistReason::kBlocklistNotLoaded);

  return blocklist_data_->IsAllowed(host_name, type,
                                    ignore_long_term_block_list_rules,
                                    clock_->Now(), passed_reasons);
}

void OptOutBlocklist::ClearBlockList(base::Time begin_time,
                                     base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(begin_time, end_time);
  // If the |blocklist_data| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    ClearBlockListSync(begin_time, end_time);
  } else {
    QueuePendingTask(base::BindOnce(&OptOutBlocklist::ClearBlockListSync,
                                    base::Unretained(this), begin_time,
                                    end_time));
  }
}

void OptOutBlocklist::ClearBlockListSync(base::Time begin_time,
                                         base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK_LE(begin_time, end_time);

  // Clear the in-memory rules entirely.
  blocklist_data_->ClearData();
  loaded_ = false;

  // Notify |blocklist_delegate_| on blocklist load status
  blocklist_delegate_->OnLoadingStateChanged(loaded_);

  // Notify |blocklist_delegate_| that the blocklist is cleared.
  blocklist_delegate_->OnBlocklistCleared(clock_->Now());

  // Delete relevant entries and reload the blocklist into memory.
  if (opt_out_store_) {
    opt_out_store_->ClearBlockList(begin_time, end_time);
    opt_out_store_->LoadBlockList(
        std::move(blocklist_data_),
        base::BindOnce(&OptOutBlocklist::LoadBlockListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlockListDone(std::move(blocklist_data_));
  }
}

void OptOutBlocklist::QueuePendingTask(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!loaded_);
  DCHECK(!callback.is_null());
  pending_callbacks_.push(std::move(callback));
}

void OptOutBlocklist::LoadBlockListDone(
    std::unique_ptr<BlocklistData> blocklist_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(blocklist_data);
  DCHECK(!loaded_);
  DCHECK(!blocklist_data_);
  loaded_ = true;
  blocklist_data_ = std::move(blocklist_data);

  // Notify |blocklist_delegate_| on current user blocklisted status.
  blocklist_delegate_->OnUserBlocklistedStatusChange(
      blocklist_data_->IsUserOptedOutInGeneral(clock_->Now()));

  // Notify the |blocklist_delegate_| on historical blocklisted hosts.
  for (const auto& entry : blocklist_data_->block_list_item_host_map()) {
    if (blocklist_data_->IsHostBlocklisted(entry.first, clock_->Now())) {
      blocklist_delegate_->OnNewBlocklistedHost(
          entry.first, entry.second.most_recent_opt_out_time().value());
    }
  }

  // Run all pending tasks. |loaded_| may change if ClearBlockList is queued.
  while (pending_callbacks_.size() > 0 && loaded_) {
    std::move(pending_callbacks_.front()).Run();
    pending_callbacks_.pop();
  }

  // Notify |blocklist_delegate_| on blocklist load status
  blocklist_delegate_->OnLoadingStateChanged(loaded_);
}

}  // namespace blocklist

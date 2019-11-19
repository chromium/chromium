// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blacklist/opt_out_blacklist/opt_out_blacklist.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_item.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"

namespace blacklist {

OptOutBlacklist::OptOutBlacklist(std::unique_ptr<OptOutStore> opt_out_store,
                                 base::Clock* clock,
                                 OptOutBlacklistDelegate* blacklist_delegate)
    : loaded_(false),
      opt_out_store_(std::move(opt_out_store)),
      clock_(clock),
      blacklist_delegate_(blacklist_delegate) {
  DCHECK(clock_);
  DCHECK(blacklist_delegate_);
}

OptOutBlacklist::~OptOutBlacklist() = default;

void OptOutBlacklist::Init() {
  DCHECK(!loaded_);
  DCHECK(!blacklist_data_);
  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  std::unique_ptr<BlacklistData::Policy> session_policy;
  if (ShouldUseSessionPolicy(&duration, &history, &threshold)) {
    session_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlacklistData::Policy> persistent_policy;
  if (ShouldUsePersistentPolicy(&duration, &history, &threshold)) {
    persistent_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  size_t max_hosts = 0;
  std::unique_ptr<BlacklistData::Policy> host_policy;
  if (ShouldUseHostPolicy(&duration, &history, &threshold, &max_hosts)) {
    host_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  std::unique_ptr<BlacklistData::Policy> type_policy;
  if (ShouldUseTypePolicy(&duration, &history, &threshold)) {
    type_policy =
        std::make_unique<BlacklistData::Policy>(duration, history, threshold);
  }

  auto blacklist_data = std::make_unique<BlacklistData>(
      std::move(session_policy), std::move(persistent_policy),
      std::move(host_policy), std::move(type_policy), max_hosts,
      GetAllowedTypes());

  if (opt_out_store_) {
    opt_out_store_->LoadBlackList(
        std::move(blacklist_data),
        base::BindOnce(&OptOutBlacklist::LoadBlackListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(std::move(blacklist_data));
  }
}

base::Time OptOutBlacklist::AddEntry(const std::string& host_name,
                                     bool opt_out,
                                     int type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = clock_->Now();

  // If the |blacklist_data| has been loaded from |opt_out_store_|,  synchronous
  // operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    AddEntrySync(host_name, opt_out, type, now);
  } else {
    QueuePendingTask(base::BindOnce(&OptOutBlacklist::AddEntrySync,
                                    base::Unretained(this), host_name, opt_out,
                                    type, now));
  }

  return now;
}

void OptOutBlacklist::AddEntrySync(const std::string& host_name,
                                   bool opt_out,
                                   int type,
                                   base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);

  bool host_was_blacklisted =
      blacklist_data_->IsHostBlacklisted(host_name, time);
  bool user_was_blacklisted = blacklist_data_->IsUserOptedOutInGeneral(time);
  blacklist_data_->AddEntry(host_name, opt_out, type, time, false);

  if (!host_was_blacklisted &&
      blacklist_data_->IsHostBlacklisted(host_name, time)) {
    // Notify |blacklist_delegate_| about a new blacklisted host.
    blacklist_delegate_->OnNewBlacklistedHost(host_name, time);
  }

  if (user_was_blacklisted != blacklist_data_->IsUserOptedOutInGeneral(time)) {
    // Notify |blacklist_delegate_| about a new blacklisted host.
    blacklist_delegate_->OnUserBlacklistedStatusChange(
        blacklist_data_->IsUserOptedOutInGeneral(time));
  }

  if (!opt_out_store_)
    return;
  opt_out_store_->AddEntry(opt_out, host_name, type, time);
}

BlacklistReason OptOutBlacklist::IsLoadedAndAllowed(
    const std::string& host_name,
    int type,
    bool ignore_long_term_black_list_rules,
    std::vector<BlacklistReason>* passed_reasons) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_)
    return BlacklistReason::kBlacklistNotLoaded;
  passed_reasons->push_back(BlacklistReason::kBlacklistNotLoaded);

  return blacklist_data_->IsAllowed(host_name, type,
                                    ignore_long_term_black_list_rules,
                                    clock_->Now(), passed_reasons);
}

void OptOutBlacklist::ClearBlackList(base::Time begin_time,
                                     base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(begin_time, end_time);
  // If the |blacklist_data| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    ClearBlackListSync(begin_time, end_time);
  } else {
    QueuePendingTask(base::BindOnce(&OptOutBlacklist::ClearBlackListSync,
                                    base::Unretained(this), begin_time,
                                    end_time));
  }
}

void OptOutBlacklist::ClearBlackListSync(base::Time begin_time,
                                         base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK_LE(begin_time, end_time);

  // Clear the in-memory rules entirely.
  blacklist_data_->ClearData();
  loaded_ = false;

  // Notify |blacklist_delegate_| that the blacklist is cleared.
  blacklist_delegate_->OnBlacklistCleared(clock_->Now());

  // Delete relevant entries and reload the blacklist into memory.
  if (opt_out_store_) {
    opt_out_store_->ClearBlackList(begin_time, end_time);
    opt_out_store_->LoadBlackList(
        std::move(blacklist_data_),
        base::BindOnce(&OptOutBlacklist::LoadBlackListDone,
                       weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(std::move(blacklist_data_));
  }
}

void OptOutBlacklist::QueuePendingTask(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!loaded_);
  DCHECK(!callback.is_null());
  pending_callbacks_.push(std::move(callback));
}

void OptOutBlacklist::LoadBlackListDone(
    std::unique_ptr<BlacklistData> blacklist_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(blacklist_data);
  DCHECK(!loaded_);
  DCHECK(!blacklist_data_);
  loaded_ = true;
  blacklist_data_ = std::move(blacklist_data);

  // Notify |blacklist_delegate_| on current user blacklisted status.
  blacklist_delegate_->OnUserBlacklistedStatusChange(
      blacklist_data_->IsUserOptedOutInGeneral(clock_->Now()));

  // Notify the |blacklist_delegate_| on historical blacklisted hosts.
  for (const auto& entry : blacklist_data_->black_list_item_host_map()) {
    if (blacklist_data_->IsHostBlacklisted(entry.first, clock_->Now())) {
      blacklist_delegate_->OnNewBlacklistedHost(
          entry.first, entry.second.most_recent_opt_out_time().value());
    }
  }

  // Run all pending tasks. |loaded_| may change if ClearBlackList is queued.
  while (pending_callbacks_.size() > 0 && loaded_) {
    std::move(pending_callbacks_.front()).Run();
    pending_callbacks_.pop();
  }
}

}  // namespace blacklist

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"

#include "base/memory/ptr_util.h"

namespace blacklist {

BlacklistData::BlacklistData(std::unique_ptr<Policy> session_policy,
                             std::unique_ptr<Policy> persistent_policy,
                             std::unique_ptr<Policy> host_policy,
                             std::unique_ptr<Policy> type_policy,
                             size_t max_hosts,
                             AllowedTypesAndVersions allowed_types)
    : session_policy_(std::move(session_policy)),
      persistent_policy_(std::move(persistent_policy)),
      host_policy_(std::move(host_policy)),
      max_hosts_(max_hosts),
      type_policy_(std::move(type_policy)),
      allowed_types_(std::move(allowed_types)) {
  DCHECK_GE(100u, max_hosts);
}
BlacklistData::~BlacklistData() {}

void BlacklistData::ClearData() {
  session_black_list_item_.reset();
  persistent_black_list_item_.reset();
  black_list_item_host_map_.clear();
  black_list_item_type_map_.clear();
}

void BlacklistData::AddEntry(const std::string& host_name,
                             bool opt_out,
                             int type,
                             base::Time time,
                             bool is_from_persistent_storage) {
  // Add to the session based rule if it is enabled.
  if (session_policy_ && !is_from_persistent_storage) {
    if (!session_black_list_item_) {
      session_black_list_item_ = std::make_unique<OptOutBlacklistItem>(
          session_policy_->history, session_policy_->threshold,
          session_policy_->duration);
    }
    session_black_list_item_->AddEntry(opt_out, time);
  }

  // Add to the persistent rule if it is enabled.
  if (persistent_policy_) {
    if (!persistent_black_list_item_) {
      persistent_black_list_item_ = std::make_unique<OptOutBlacklistItem>(
          persistent_policy_->history, persistent_policy_->threshold,
          persistent_policy_->duration);
    }
    persistent_black_list_item_->AddEntry(opt_out, time);
  }

  // Add to the host rule if it is enabled. Remove hosts if there are more than
  // |max_hosts_| in the map.
  if (host_policy_) {
    auto item = black_list_item_host_map_.find(host_name);
    if (item == black_list_item_host_map_.end()) {
      auto value = black_list_item_host_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(host_name),
          std::forward_as_tuple(host_policy_->history, host_policy_->threshold,
                                host_policy_->duration));
      DCHECK(value.second);
      item = value.first;
    }
    item->second.AddEntry(opt_out, time);
    if (max_hosts_ > 0 && black_list_item_host_map_.size() > max_hosts_)
      EvictOldestHost();
  }

  if (type_policy_) {
    auto item = black_list_item_type_map_.find(type);
    if (item == black_list_item_type_map_.end()) {
      auto value = black_list_item_type_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(type),
          std::forward_as_tuple(type_policy_->history, type_policy_->threshold,
                                type_policy_->duration));
      DCHECK(value.second);
      item = value.first;
    }
    item->second.AddEntry(opt_out, time);
  }
}

BlacklistReason BlacklistData::IsAllowed(
    const std::string& host_name,
    int type,
    bool ignore_long_term_black_list_rules,
    base::Time time,
    std::vector<BlacklistReason>* passed_reasons) const {
  // Check the session rule.
  if (session_policy_) {
    if (session_black_list_item_ &&
        session_black_list_item_->IsBlackListed(time)) {
      return BlacklistReason::kUserOptedOutInSession;
    }
    passed_reasons->push_back(BlacklistReason::kUserOptedOutInSession);
  }

  // Check whether the persistent rules should be checked this time.
  if (ignore_long_term_black_list_rules)
    return BlacklistReason::kAllowed;

  // Check the persistent rule.
  if (persistent_policy_) {
    if (IsUserOptedOutInGeneral(time)) {
      return BlacklistReason::kUserOptedOutInGeneral;
    }
    passed_reasons->push_back(BlacklistReason::kUserOptedOutInGeneral);
  }

  // Check the host rule.
  if (host_policy_) {
    if (IsHostBlacklisted(host_name, time))
      return BlacklistReason::kUserOptedOutOfHost;
    passed_reasons->push_back(BlacklistReason::kUserOptedOutOfHost);
  }

  // Only allowed types should be recorded.
  DCHECK(allowed_types_.find(type) != allowed_types_.end());

  // Check the type rule.
  if (type_policy_) {
    auto item = black_list_item_type_map_.find(type);
    if (item != black_list_item_type_map_.end() &&
        item->second.IsBlackListed(time)) {
      return BlacklistReason::kUserOptedOutOfType;
    }
    passed_reasons->push_back(BlacklistReason::kUserOptedOutOfType);
  }

  return BlacklistReason::kAllowed;
}

void BlacklistData::EvictOldestHost() {
  DCHECK_LT(max_hosts_, black_list_item_host_map_.size());
  base::Optional<base::Time> oldest_opt_out;
  std::string key_to_delete;
  for (auto& item : black_list_item_host_map_) {
    base::Optional<base::Time> most_recent_opt_out =
        item.second.most_recent_opt_out_time();
    if (!most_recent_opt_out) {
      // If there is no opt out time, this is a good choice to evict.
      key_to_delete = item.first;
      break;
    }
    if (!oldest_opt_out ||
        most_recent_opt_out.value() < oldest_opt_out.value()) {
      oldest_opt_out = most_recent_opt_out.value();
      key_to_delete = item.first;
    }
  }
  black_list_item_host_map_.erase(key_to_delete);
}

bool BlacklistData::IsHostBlacklisted(const std::string& host_name,
                                      base::Time time) const {
  auto item = black_list_item_host_map_.find(host_name);
  return item != black_list_item_host_map_.end() &&
         item->second.IsBlackListed(time);
}

bool BlacklistData::IsUserOptedOutInGeneral(base::Time time) const {
  return persistent_black_list_item_ &&
         persistent_black_list_item_->IsBlackListed(time);
}

}  // namespace blacklist

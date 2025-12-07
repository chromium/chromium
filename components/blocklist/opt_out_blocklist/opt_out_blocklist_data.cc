// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"

#include "base/memory/ptr_util.h"

namespace blocklist {

BlocklistData::BlocklistData(std::unique_ptr<Policy> session_policy,
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
BlocklistData::~BlocklistData() = default;

void BlocklistData::ClearData() {
  session_block_list_item_.reset();
  persistent_block_list_item_.reset();
  block_list_item_host_map_.clear();
  block_list_item_type_map_.clear();
}

void BlocklistData::AddEntry(const std::string& host_name,
                             bool opt_out,
                             int type,
                             base::Time time,
                             bool is_from_persistent_storage) {
  // Add to the session based rule if it is enabled.
  if (session_policy_ && !is_from_persistent_storage) {
    if (!session_block_list_item_) {
      session_block_list_item_ = std::make_unique<OptOutBlocklistItem>(
          session_policy_->history, session_policy_->threshold,
          session_policy_->duration);
    }
    session_block_list_item_->AddEntry(opt_out, time);
  }

  // Add to the persistent rule if it is enabled.
  if (persistent_policy_) {
    if (!persistent_block_list_item_) {
      persistent_block_list_item_ = std::make_unique<OptOutBlocklistItem>(
          persistent_policy_->history, persistent_policy_->threshold,
          persistent_policy_->duration);
    }
    persistent_block_list_item_->AddEntry(opt_out, time);
  }

  // Add to the host rule if it is enabled. Remove hosts if there are more than
  // |max_hosts_| in the map.
  if (host_policy_) {
    auto item = block_list_item_host_map_.find(host_name);
    if (item == block_list_item_host_map_.end()) {
      auto value = block_list_item_host_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(host_name),
          std::forward_as_tuple(host_policy_->history, host_policy_->threshold,
                                host_policy_->duration));
      DCHECK(value.second);
      item = value.first;
    }
    item->second.AddEntry(opt_out, time);
    if (max_hosts_ > 0 && block_list_item_host_map_.size() > max_hosts_) {
      EvictOldestHost();
    }
  }

  if (type_policy_) {
    auto item = block_list_item_type_map_.find(type);
    if (item == block_list_item_type_map_.end()) {
      auto value = block_list_item_type_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(type),
          std::forward_as_tuple(type_policy_->history, type_policy_->threshold,
                                type_policy_->duration));
      DCHECK(value.second);
      item = value.first;
    }
    item->second.AddEntry(opt_out, time);
  }
}

BlocklistReason BlocklistData::IsAllowed(
    const std::string& host_name,
    int type,
    bool ignore_long_term_block_list_rules,
    base::Time time,
    std::vector<BlocklistReason>* passed_reasons) const {
  // Check the session rule.
  if (session_policy_) {
    if (session_block_list_item_ &&
        session_block_list_item_->IsBlockListed(time)) {
      return BlocklistReason::kUserOptedOutInSession;
    }
    passed_reasons->push_back(BlocklistReason::kUserOptedOutInSession);
  }

  // Check whether the persistent rules should be checked this time.
  if (ignore_long_term_block_list_rules)
    return BlocklistReason::kAllowed;

  // Check the persistent rule.
  if (persistent_policy_) {
    if (IsUserOptedOutInGeneral(time)) {
      return BlocklistReason::kUserOptedOutInGeneral;
    }
    passed_reasons->push_back(BlocklistReason::kUserOptedOutInGeneral);
  }

  // Check the host rule.
  if (host_policy_) {
    if (IsHostBlocklisted(host_name, time)) {
      return BlocklistReason::kUserOptedOutOfHost;
    }
    passed_reasons->push_back(BlocklistReason::kUserOptedOutOfHost);
  }

  // Only allowed types should be recorded.
  DCHECK(allowed_types_.find(type) != allowed_types_.end());

  // Check the type rule.
  if (type_policy_) {
    auto item = block_list_item_type_map_.find(type);
    if (item != block_list_item_type_map_.end() &&
        item->second.IsBlockListed(time)) {
      return BlocklistReason::kUserOptedOutOfType;
    }
    passed_reasons->push_back(BlocklistReason::kUserOptedOutOfType);
  }

  return BlocklistReason::kAllowed;
}

void BlocklistData::EvictOldestHost() {
  DCHECK_LT(max_hosts_, block_list_item_host_map_.size());
  std::optional<base::Time> oldest_opt_out;
  std::string key_to_delete;
  for (auto& item : block_list_item_host_map_) {
    std::optional<base::Time> most_recent_opt_out =
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
  block_list_item_host_map_.erase(key_to_delete);
}

bool BlocklistData::IsHostBlocklisted(const std::string& host_name,
                                      base::Time time) const {
  auto item = block_list_item_host_map_.find(host_name);
  return item != block_list_item_host_map_.end() &&
         item->second.IsBlockListed(time);
}

bool BlocklistData::IsUserOptedOutInGeneral(base::Time time) const {
  return persistent_block_list_item_ &&
         persistent_block_list_item_->IsBlockListed(time);
}

}  // namespace blocklist

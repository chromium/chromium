// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DATA_H_
#define COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"

namespace blocklist {

// The various reasons the Blocklist may tell that the user is blocklisted.
// This should remain synchronized with enums.xml
enum class BlocklistReason {
  // The blocklist may not be loaded very early in the session or when the user
  // has cleared the blocklist history (usually by clearing their browsing
  // history).
  kBlocklistNotLoaded = 0,
  kUserOptedOutInSession = 1,
  kUserOptedOutInGeneral = 2,
  kUserOptedOutOfHost = 3,
  kUserOptedOutOfType = 4,
  kAllowed = 5,
  kMaxValue = kAllowed,

};

// This class describes all of the data used to determine whether an action is
// allowed based on four possible rules: Session: if the user has opted out
// of j of the last k entries this session, the action will be blocklisted for a
// set duration. Persistent: if the user has opted out of j of the last k
// entries, the action will be blocklisted for a set duration. Host: if the user
// has opted out of threshold of the last history entries for a specific host,
// the action will be blocklisted for a set duration. Type: if the user has
// opted out of j of the last k entries for a specific type, the action will be
// blocklisted for a set duration. This is the in-memory version of the block
// list policy. This object is moved from the embedder thread to a background
// thread, It is not safe to access concurrently on two threads.
class BlocklistData {
 public:
  // A struct describing the general blocklisting pattern used by all of the
  // blocklisting rules.
  // The most recent |history| entries are looked at and if |threshold| (or
  // more) of them are opt outs, new actions are considered blocklisted unless
  // the most recent opt out was longer than |duration| ago.
  struct Policy {
    Policy(base::TimeDelta duration, size_t history, int threshold)
        : duration(duration), history(history), threshold(threshold) {}

    ~Policy() = default;

    // Specifies how long the blocklisting rule lasts after the most recent opt
    // out.
    const base::TimeDelta duration;
    // Amount of entries evaluated for the rule.
    const size_t history;
    // The number of opt outs that will trigger blocklisting for the rule.
    const int threshold;
  };

  // A map of types that are allowed to be used in the blocklist as well as the
  // version that those types are in. Versioning allows removals from persistent
  // memory at session start.
  using AllowedTypesAndVersions = std::map<int, int>;

  // |session_policy| if non-null, is the policy that is not persisted across
  // sessions and is not specific to host or type. |persistent_policy| if
  // non-null, is the policy that is persisted across sessions and is not
  // specific to host or type. |host_policy| if non-null, is the policy that is
  // persisted across sessions applies at the per-host level. |host_policy| if
  // non-null, is the policy that is persisted across sessions and applies at
  // the per-type level. |max_hosts| is the maximum number of hosts stored in
  // memory. |allowed_types| contains the action types that are allowed in the
  // session and their corresponding versions. Conversioning is used to clear
  // stale data from the persistent storage.
  BlocklistData(std::unique_ptr<Policy> session_policy,
                std::unique_ptr<Policy> persistent_policy,
                std::unique_ptr<Policy> host_policy,
                std::unique_ptr<Policy> type_policy,
                size_t max_hosts,
                AllowedTypesAndVersions allowed_types);

  BlocklistData(const BlocklistData&) = delete;
  BlocklistData& operator=(const BlocklistData&) = delete;

  ~BlocklistData();

  // Adds a new entry for all rules to use when evaluating blocklisting state.
  // |is_from_persistent_storage| is used to delineate between data added from
  // this session, and previous sessions.
  void AddEntry(const std::string& host_name,
                bool opt_out,
                int type,
                base::Time time,
                bool is_from_persistent_storage);

  // Whether the user is opted out when considering all enabled rules. if
  // |ignore_long_term_block_list_rules| is true, this will only check the
  // session rule. For every reason that is checked, but does not trigger
  // blocklisting, a new reason will be appended to the end |passed_reasons|.
  // |time| is the time that decision should be evaluated at (usually now).
  BlocklistReason IsAllowed(const std::string& host_name,
                            int type,
                            bool ignore_long_term_block_list_rules,
                            base::Time time,
                            std::vector<BlocklistReason>* passed_reasons) const;

  // This clears all data in all rules.
  void ClearData();

  // The allowed types and what version they are. If it is non-empty, it is used
  // to remove stale entries from the database and to DCHECK that other methods
  // are not using disallowed types.
  const AllowedTypesAndVersions& allowed_types() const {
    return allowed_types_;
  }

  // Whether the specific |host_name| is blocklisted based only on the host
  // rule.
  bool IsHostBlocklisted(const std::string& host_name, base::Time time) const;

  // Whether the user is opted out based solely on the persistent blocklist
  // rule.
  bool IsUserOptedOutInGeneral(base::Time time) const;

  // Exposed for logging purposes only.
  const std::map<std::string, OptOutBlocklistItem>& block_list_item_host_map()
      const {
    return block_list_item_host_map_;
  }

 private:
  // Removes the oldest (or safest) host item from |block_list_item_host_map_|.
  // Oldest is defined by most recent opt out time, and safest is defined as an
  // item with no opt outs.
  void EvictOldestHost();

  // The session rule policy. If non-null the session rule is enforced.
  std::unique_ptr<Policy> session_policy_;
  // The session rule history.
  std::unique_ptr<OptOutBlocklistItem> session_block_list_item_;

  // The persistent rule policy. If non-null the persistent rule is enforced.
  std::unique_ptr<Policy> persistent_policy_;
  // The persistent rule history.
  std::unique_ptr<OptOutBlocklistItem> persistent_block_list_item_;

  // The host rule policy. If non-null the host rule is enforced.
  std::unique_ptr<Policy> host_policy_;
  // The maximum number of hosts allowed in the host blocklist.
  size_t max_hosts_;
  // The host rule history. Each host is stored as a separate blocklist history.
  std::map<std::string, OptOutBlocklistItem> block_list_item_host_map_;

  // The type rule policy. If non-null the type rule is enforced.
  std::unique_ptr<Policy> type_policy_;
  // The type rule history. Each type is stored as a separate blocklist history.
  std::map<int, OptOutBlocklistItem> block_list_item_type_map_;

  // The allowed types and what version they are. If it is non-empty, it is used
  // to remove stale entries from the database and to DCHECK that other methods
  // are not using disallowed types.
  AllowedTypesAndVersions allowed_types_;
};

}  // namespace blocklist

#endif  // COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DATA_H_

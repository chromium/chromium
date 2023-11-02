// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_USER_ACTIONS_COLLECTOR_H_
#define COMPONENTS_FEED_CORE_V2_USER_ACTIONS_COLLECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/values.h"

class GURL;
class PrefService;

namespace feed {

// UserActionsCollector stores, retrieves and summarizes the entity mids of the
// feed articles that the user clicked on in the past.
class UserActionsCollector {
 public:
  explicit UserActionsCollector(PrefService* profile_prefs);
  UserActionsCollector(const UserActionsCollector&) = delete;
  UserActionsCollector& operator=(const UserActionsCollector&) = delete;
  ~UserActionsCollector();
  void UpdateUserProfileOnLinkClick(const GURL& url,
                                    const std::vector<int64_t>& entity_mids);

  const base::Value::List& visit_metadata_string_list_pref_for_testing() const {
    return visit_metadata_string_list_pref_;
  }

 private:
  void InitStoreFromPrefs();

  // Returns a serialized form of
  // feedunsignedpersonalizationstore::VisitMetadata that's initialized using
  // |url| and |entity_mids|. Returns an empty string if |url| or |entity_mids|
  // is invalid.
  std::string EntryToString(const GURL& url,
                            const std::vector<int64_t>& entity_mids) const;

  bool ShouldIncludeVisitMetadataEntry(
      const std::string& visit_metadata_serialized) const;

  // Current prefs on the disk. The list is sorted by increasing timestamp.
  base::Value::List visit_metadata_string_list_pref_;

  raw_ptr<PrefService> profile_prefs_ = nullptr;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_USER_ACTIONS_COLLECTOR_H_

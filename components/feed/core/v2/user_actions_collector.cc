// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/user_actions_collector.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/user_actions_store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace feed {

namespace {

int64_t TimeToPrefValue(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InSeconds();
}

base::Time PrefValueToTime(int64_t value) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(value));
}

}  // namespace

// TODO: Hook up to browser clearing data delegate so we clear the pref when
// user clears data from Chrome settings.
UserActionsCollector::UserActionsCollector(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  if (!base::FeatureList::IsEnabled(kPersonalizeFeedUnsignedUsers))
    return;
  InitStoreFromPrefs();
}

UserActionsCollector::~UserActionsCollector() = default;

void UserActionsCollector::UpdateUserProfileOnLinkClick(
    const GURL& url,
    const std::vector<int64_t>& entity_mids) {
  if (!base::FeatureList::IsEnabled(kPersonalizeFeedUnsignedUsers))
    return;

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || entity_mids.empty())
    return;

  visit_metadata_string_list_pref_.Append(EntryToString(url, entity_mids));

  profile_prefs_->SetList(prefs::kFeedOnDeviceUserActionsCollector,
                          visit_metadata_string_list_pref_.Clone());
  profile_prefs_->SchedulePendingLossyWrites();
  UMA_HISTOGRAM_COUNTS(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1);
}

void UserActionsCollector::InitStoreFromPrefs() {
  const base::Value::List& list_value_from_disk =
      profile_prefs_->GetList(prefs::kFeedOnDeviceUserActionsCollector);
  size_t count_values_during_store_initialization = 0;

  const size_t entries_in_pref = list_value_from_disk.size();

  // If the store size exceeds its capacity, then skip some of the earlier
  // entries to reduce store size to its capacity.
  int entries_to_skip =
      entries_in_pref > GetFeedConfig().max_url_entries_in_cache
          ? entries_in_pref - GetFeedConfig().max_url_entries_in_cache
          : 0;
  DCHECK_GE(entries_to_skip, 0);
  DCHECK_LE(static_cast<size_t>(entries_to_skip), entries_in_pref);

  bool rewrite_prefs = entries_to_skip > 0;

  // Skip first |entries_to_skip| number of entries to reduce the size of
  // store.
  for (auto iter = list_value_from_disk.begin() + entries_to_skip;
       iter != list_value_from_disk.end(); ++iter) {
    if (!iter->is_string()) {
      rewrite_prefs = true;
      continue;
    }

    const std::string& visit_metadata_serialized = iter->GetString();
    if (!ShouldIncludeVisitMetadataEntry(visit_metadata_serialized)) {
      rewrite_prefs = true;
      continue;
    }

    ++count_values_during_store_initialization;
    visit_metadata_string_list_pref_.Append(visit_metadata_serialized);
  }
  UMA_HISTOGRAM_COUNTS(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      count_values_during_store_initialization);

  if (rewrite_prefs) {
    profile_prefs_->SetList(prefs::kFeedOnDeviceUserActionsCollector,
                            visit_metadata_string_list_pref_.Clone());
    profile_prefs_->SchedulePendingLossyWrites();
  }
}

std::string UserActionsCollector::EntryToString(
    const GURL& url,
    const std::vector<int64_t>& entity_mids) const {
  feedunsignedpersonalizationstore::VisitMetadata visit_metadata_proto;

  url::Origin origin = url::Origin::Create(url);

  visit_metadata_proto.set_timestamp_seconds_since_epoch(
      TimeToPrefValue(base::Time::Now()));
  visit_metadata_proto.set_origin(origin.Serialize());

  DCHECK(!entity_mids.empty());

  for (size_t i = 0; i < entity_mids.size() &&
                     i < GetFeedConfig().max_mid_entities_per_url_entry;
       ++i) {
    visit_metadata_proto.add_entity_mids(entity_mids[i]);
  }

  std::string serialized_entry;
  visit_metadata_proto.SerializeToString(&serialized_entry);
  DCHECK(!serialized_entry.empty());

  return base::Base64Encode(serialized_entry);
}

bool UserActionsCollector::ShouldIncludeVisitMetadataEntry(
    const std::string& visit_metadata_serialized) const {
  if (visit_metadata_serialized.empty())
    return false;

  std::string base64_decoded;
  if (!base::Base64Decode(visit_metadata_serialized, &base64_decoded)) {
    return false;
  }

  feedunsignedpersonalizationstore::VisitMetadata entry_proto;
  if (!entry_proto.ParseFromString(base64_decoded)) {
    return false;
  }

  // TODO(tbansal): Consider throwing out entries from the future.
  base::Time timestamp =
      PrefValueToTime(entry_proto.timestamp_seconds_since_epoch());
  if (base::Time::Now() - timestamp >= base::Days(28)) {
    return false;
  }

  return true;
}

}  // namespace feed

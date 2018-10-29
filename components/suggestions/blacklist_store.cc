// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/blacklist_store.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/suggestions_pref_names.h"

using base::TimeDelta;
using base::TimeTicks;

namespace suggestions {
namespace {

void PopulateBlacklistSet(const SuggestionsBlacklist& blacklist_proto,
                          std::set<std::string>* blacklist_set) {
  blacklist_set->clear();
  for (int i = 0; i < blacklist_proto.urls_size(); ++i) {
    blacklist_set->insert(blacklist_proto.urls(i));
  }
}

void PopulateBlacklistProto(const std::set<std::string>& blacklist_set,
                            SuggestionsBlacklist* blacklist_proto) {
  blacklist_proto->Clear();
  for (auto it = blacklist_set.begin(); it != blacklist_set.end(); ++it) {
    blacklist_proto->add_urls(*it);
  }
}

}  // namespace

BlacklistStore::BlacklistStore(PrefService* profile_prefs,
                               const base::TimeDelta& upload_delay)
    : pref_service_(profile_prefs), upload_delay_(upload_delay) {
  DCHECK(pref_service_);

  // Log the blacklist's size. A single BlacklistStore is created for the
  // SuggestionsService; this will run once.
  SuggestionsBlacklist blacklist_proto;
  LoadBlacklist(&blacklist_proto);
  UMA_HISTOGRAM_COUNTS_10000("Suggestions.LocalBlacklistSize",
                             blacklist_proto.urls_size());
}

BlacklistStore::~BlacklistStore() {}

bool BlacklistStore::BlacklistUrl(const GURL& url) {
  if (!url.is_valid()) return false;

  SuggestionsBlacklist blacklist_proto;
  LoadBlacklist(&blacklist_proto);
  std::set<std::string> blacklist_set;
  PopulateBlacklistSet(blacklist_proto, &blacklist_set);

  bool success = false;
  if (blacklist_set.insert(url.spec()).second) {
    PopulateBlacklistProto(blacklist_set, &blacklist_proto);
    success = StoreBlacklist(blacklist_proto);
  } else {
    // |url| was already in the blacklist.
    success = true;
  }

  if (success) {
    // Update the blacklist time.
    blacklist_times_[url.spec()] = TimeTicks::Now();
  }

  return success;
}

bool BlacklistStore::GetTimeUntilReadyForUpload(TimeDelta* delta) {
  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);
  if (!blacklist.urls_size())
    return false;

  // Note: the size is non-negative.
  if (blacklist_times_.size() < static_cast<size_t>(blacklist.urls_size())) {
    // A url is not in the timestamp map: it's candidate for upload. This can
    // happen after a restart. Another (undesired) case when this could happen
    // is if more than one instance were created.
    *delta = TimeDelta::FromSeconds(0);
    return true;
  }

  // Find the minimum blacklist time. Note: blacklist_times_ is NOT empty since
  // blacklist is non-empty and blacklist_times_ contains as many items.
  TimeDelta min_delay = TimeDelta::Max();
  for (const auto& kv : blacklist_times_) {
    min_delay = std::min(upload_delay_ - (TimeTicks::Now() - kv.second),
                         min_delay);
  }
  DCHECK(min_delay != TimeDelta::Max());
  *delta = std::max(min_delay, TimeDelta::FromSeconds(0));

  return true;
}

bool BlacklistStore::GetTimeUntilURLReadyForUpload(const GURL& url,
                                                   TimeDelta* delta) {
  auto it = blacklist_times_.find(url.spec());
  if (it != blacklist_times_.end()) {
    // The url is in the timestamps map.
    *delta = std::max(upload_delay_ - (TimeTicks::Now() - it->second),
                      TimeDelta::FromSeconds(0));
    return true;
  }

  // The url still might be in the blacklist.
  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);
  for (int i = 0; i < blacklist.urls_size(); ++i) {
    if (blacklist.urls(i) == url.spec()) {
      *delta = TimeDelta::FromSeconds(0);
      return true;
    }
  }

  return false;
}

bool BlacklistStore::GetCandidateForUpload(GURL* url) {
  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);

  for (int i = 0; i < blacklist.urls_size(); ++i) {
    bool is_candidate = true;
    auto it = blacklist_times_.find(blacklist.urls(i));
    if (it != blacklist_times_.end() &&
        TimeTicks::Now() < it->second + upload_delay_) {
      // URL was added too recently.
      is_candidate = false;
    }
    if (is_candidate) {
      GURL blacklisted(blacklist.urls(i));
      url->Swap(&blacklisted);
      return true;
    }
  }

  return false;
}

bool BlacklistStore::RemoveUrl(const GURL& url) {
  if (!url.is_valid()) return false;
  const std::string removal_candidate = url.spec();

  SuggestionsBlacklist blacklist;
  LoadBlacklist(&blacklist);

  bool removed = false;
  SuggestionsBlacklist updated_blacklist;
  for (int i = 0; i < blacklist.urls_size(); ++i) {
    if (blacklist.urls(i) == removal_candidate) {
      removed = true;
    } else {
      updated_blacklist.add_urls(blacklist.urls(i));
    }
  }

  if (removed && StoreBlacklist(updated_blacklist)) {
    blacklist_times_.erase(url.spec());
    return true;
  }

  return false;
}

void BlacklistStore::FilterSuggestions(SuggestionsProfile* profile) {
  if (!profile->suggestions_size())
    return;  // Empty profile, nothing to filter.

  SuggestionsBlacklist blacklist_proto;
  if (!LoadBlacklist(&blacklist_proto)) {
    // There was an error loading the blacklist. The blacklist was cleared and
    // there's nothing to be done about it.
    return;
  }
  if (!blacklist_proto.urls_size())
    return;  // Empty blacklist, nothing to filter.

  std::set<std::string> blacklist_set;
  PopulateBlacklistSet(blacklist_proto, &blacklist_set);

  // Populate the filtered suggestions.
  SuggestionsProfile filtered_profile;
  filtered_profile.set_timestamp(profile->timestamp());
  for (int i = 0; i < profile->suggestions_size(); ++i) {
    if (blacklist_set.find(profile->suggestions(i).url()) ==
        blacklist_set.end()) {
      // This suggestion is not blacklisted.
      ChromeSuggestion* suggestion = filtered_profile.add_suggestions();
      // Note: swapping!
      suggestion->Swap(profile->mutable_suggestions(i));
    }
  }

  // Swap |profile| and |filtered_profile|.
  profile->Swap(&filtered_profile);
}

// static
void BlacklistStore::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSuggestionsBlacklist, std::string());
}


// Test seam. For simplicity of mock creation.
BlacklistStore::BlacklistStore() {
}

bool BlacklistStore::LoadBlacklist(SuggestionsBlacklist* blacklist) {
  DCHECK(blacklist);

  const std::string base64_blacklist_data =
      pref_service_->GetString(prefs::kSuggestionsBlacklist);
  if (base64_blacklist_data.empty()) {
    blacklist->Clear();
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string blacklist_data;
  if (!base::Base64Decode(base64_blacklist_data, &blacklist_data) ||
      !blacklist->ParseFromString(blacklist_data)) {
    VLOG(1) << "Suggestions blacklist data in profile pref is corrupt, "
            << " clearing it.";
    blacklist->Clear();
    ClearBlacklist();
    return false;
  }

  return true;
}

bool BlacklistStore::StoreBlacklist(const SuggestionsBlacklist& blacklist) {
  std::string blacklist_data;
  if (!blacklist.SerializeToString(&blacklist_data)) return false;

  std::string base64_blacklist_data;
  base::Base64Encode(blacklist_data, &base64_blacklist_data);

  pref_service_->SetString(prefs::kSuggestionsBlacklist, base64_blacklist_data);
  return true;
}

void BlacklistStore::ClearBlacklist() {
  pref_service_->ClearPref(prefs::kSuggestionsBlacklist);
}

}  // namespace suggestions

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "url/gurl.h"

namespace {

const char kExpirationKey[] = "expiration";
const char kLastUsedKey[] = "last_used";
const char kLastVisitKey[] = "last_visit";
const char kSessionModelKey[] = "model";
const char kSettingKey[] = "setting";
const char kLastModifiedKey[] = "last_modified";
const char kLifetimeKey[] = "lifetime";
const char kDecidedByRelatedWebsiteSets[] = "decided_by_related_website_sets";

const base::TimeDelta kLastUsedPermissionExpiration = base::Hours(24);

bool IsValueAllowedForType(const base::Value& value, ContentSettingsType type) {
  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  if (info) {
    if (!value.is_int())
      return false;
    if (value.GetInt() == CONTENT_SETTING_DEFAULT)
      return false;
    return info->IsSettingValid(IntToContentSetting(value.GetInt()));
  }

  // TODO(raymes): We should permit different types of base::Value for
  // website settings.
  return value.is_dict();
}

// Extract a timestamp from `dict[key]`.
// Will return base::Time() if no timestamp exists.
base::Time GetTimeFromDictKey(const base::Value::Dict& dict,
                              const std::string& key) {
  return base::ValueToTime(dict.Find(key)).value_or(base::Time());
}

// Extract a timestamp from `dict[key]`.
// Will return base::Time() if no timestamp exists.
base::TimeDelta GetTimeDeltaFromDictKey(const base::Value::Dict& dict,
                                        const std::string& key) {
  return base::ValueToTimeDelta(dict.Find(key)).value_or(base::TimeDelta());
}

// Extract a timestamp from `dictionary[kLastModifiedKey]`.
// Will return base::Time() if no timestamp exists.
base::Time GetLastModified(const base::Value::Dict& dictionary) {
  return GetTimeFromDictKey(dictionary, kLastModifiedKey);
}

// Extract a timestamp from `dictionary[kExpirationKey]`.
// Will return base::Time() if no timestamp exists.
base::Time GetExpiration(const base::Value::Dict& dictionary) {
  return GetTimeFromDictKey(dictionary, kExpirationKey);
}

// Extract a timestamp from `dictionary[kLastUsedKey]`.
// Will return base::Time() if no timestamp exists.
base::Time GetLastUsed(const base::Value::Dict& dictionary) {
  return GetTimeFromDictKey(dictionary, kLastUsedKey);
}

// Extract a timestamp from `dictionary[kLastVisit]`.
// Will return base::Time() if no timestamp exists.
base::Time GetLastVisit(const base::Value::Dict& dictionary) {
  return GetTimeFromDictKey(dictionary, kLastVisitKey);
}

// Extract a TimeDelta from `dictionary[kLifetimeKey]`.
// Will return base::TimeDelta() if no value exists for that key.
base::TimeDelta GetLifetime(const base::Value::Dict& dictionary) {
  return GetTimeDeltaFromDictKey(dictionary, kLifetimeKey);
}

// Extract a bool from `dictionary[kDecidedByRelatedWebsiteSets]`.
// Will return false if no value exists for that key.
bool GetDecidedByRelatedWebsiteSets(const base::Value::Dict& dictionary) {
  return dictionary.FindBool(kDecidedByRelatedWebsiteSets).value_or(false);
}

// Extract a SessionModel from |dictionary[kSessionModelKey]|. Will return
// SessionModel::DURABLE if no model exists.
content_settings::mojom::SessionModel GetSessionModel(
    const base::Value::Dict& dictionary) {
  int model_int = dictionary.FindInt(kSessionModelKey).value_or(0);
  if ((model_int >
       static_cast<int>(content_settings::mojom::SessionModel::kMaxValue)) ||
      (model_int < 0)) {
    model_int = 0;
  }

  content_settings::mojom::SessionModel session_model =
      static_cast<content_settings::mojom::SessionModel>(model_int);
  return session_model;
}

}  // namespace

namespace content_settings {

ContentSettingsPref::ContentSettingsPref(
    ContentSettingsType content_type,
    PrefService* prefs,
    PrefChangeRegistrar* registrar,
    const std::string& pref_name,
    const std::string& partitioned_pref_name,
    bool off_the_record,
    bool restore_session,
    NotifyObserversCallback notify_callback)
    : content_type_(content_type),
      prefs_(prefs),
      registrar_(registrar),
      pref_name_(pref_name),
      partitioned_pref_name_(partitioned_pref_name),
      off_the_record_(off_the_record),
      restore_session_(restore_session),
      updating_preferences_(false),
      notify_callback_(notify_callback),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(prefs_);
  ReadContentSettingsFromPref();

  for (const auto& path : {pref_name_, partitioned_pref_name_}) {
    registrar_->Add(path,
                    base::BindRepeating(&ContentSettingsPref::OnPrefChanged,
                                        base::Unretained(this)));
  }
}

ContentSettingsPref::~ContentSettingsPref() = default;

std::unique_ptr<RuleIterator> ContentSettingsPref::GetRuleIterator(
    bool off_the_record,
    const PartitionKey& partition_key) const {
  if (off_the_record)
    return off_the_record_value_map_.GetRuleIterator(content_type_,
                                                     partition_key);
  return value_map_.GetRuleIterator(content_type_, partition_key);
}

std::unique_ptr<Rule> ContentSettingsPref::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    bool off_the_record,
    const PartitionKey& partition_key) const {
  if (off_the_record) {
    base::AutoLock auto_lock(off_the_record_value_map_.GetLock());
    return off_the_record_value_map_.GetRule(primary_url, secondary_url,
                                             content_type_, partition_key);
  }
  base::AutoLock auto_lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type_,
                            partition_key);
}

void ContentSettingsPref::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value value,
    const RuleMetaData& metadata,
    const PartitionKey& partition_key) {
  DCHECK(value.is_none() || IsValueAllowedForType(value, content_type_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  DCHECK(primary_pattern != ContentSettingsPattern::Wildcard() ||
         secondary_pattern != ContentSettingsPattern::Wildcard());

  // Update in memory value map.
  auto* map_to_modify = &off_the_record_value_map_;
  if (!off_the_record_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(map_to_modify->GetLock());
    if (!value.is_none()) {
      if (!map_to_modify->SetValue(primary_pattern, secondary_pattern,
                                   content_type_, value.Clone(), metadata,
                                   partition_key)) {
        return;
      }
    } else {
      if (!map_to_modify->DeleteValue(primary_pattern, secondary_pattern,
                                      content_type_, partition_key)) {
        return;
      }
    }
  }
  // Update the content settings preference.
  if (!off_the_record_ && !partition_key.in_memory()) {
    UpdatePref(primary_pattern, secondary_pattern, std::move(value), metadata,
               partition_key);
  }

  notify_callback_.Run(primary_pattern, secondary_pattern, content_type_,
                       &partition_key);
}

void ContentSettingsPref::ClearAllContentSettingsRules(
    const PartitionKey& partition_key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  auto* map_to_modify = &value_map_;
  if (off_the_record_) {
    map_to_modify = &off_the_record_value_map_;
  }

  {
    base::AutoLock auto_lock(map_to_modify->GetLock());
    map_to_modify->DeleteValues(content_type_, partition_key);
  }

  if (!off_the_record_ && !partition_key.in_memory()) {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    if (partition_key.is_default()) {
      prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_name_);
      update->Clear();
    } else {
      prefs::ScopedDictionaryPrefUpdate update(prefs_, partitioned_pref_name_);
      update->RemoveWithoutPathExpansion(partition_key.Serialize(), nullptr);
    }
  }

  notify_callback_.Run(ContentSettingsPattern::Wildcard(),
                       ContentSettingsPattern::Wildcard(), content_type_,
                       &partition_key);
}

void ContentSettingsPref::OnShutdown() {
  prefs_ = nullptr;
  registrar_ = nullptr;
}

size_t ContentSettingsPref::GetNumExceptions() {
  base::AutoLock auto_lock(value_map_.GetLock());
  return value_map_.size();
}

bool ContentSettingsPref::TryLockForTesting() const {
  if (!value_map_.GetLock().Try()) {
    return false;
  }
  value_map_.GetLock().Release();
  return true;
}

void ContentSettingsPref::ReadContentSettingsFromPref() {
  // |ScopedDictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |lock_| is
  // not held when the notifications are sent. Also, |auto_reset| must be still
  // valid when the notifications are sent, so that |Observe| skips the
  // notification.
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  base::AutoLock auto_lock(value_map_.GetLock());

  value_map_.clear();

  // Read for the default PartitionKey.
  {
    std::unique_ptr<prefs::ScopedDictionaryPrefUpdate> update;
    std::unique_ptr<prefs::DictionaryValueUpdate> mutable_partition;
    // Don't create `update` in off the record mode to avoid accidentally
    // modifying the pref.
    if (!off_the_record_) {
      update = std::make_unique<prefs::ScopedDictionaryPrefUpdate>(prefs_,
                                                                   pref_name_);
      mutable_partition = update->Get();
    }
    ReadContentSettingsFromPrefForPartition(
        PartitionKey(), prefs_->GetDict(pref_name_), mutable_partition.get());
  }

  // Read for non-default PartitionKeys.
  {
    std::unique_ptr<prefs::ScopedDictionaryPrefUpdate> update;
    // Don't create `update` in off the record mode to avoid accidentally
    // modifying the pref.
    if (!off_the_record_) {
      update = std::make_unique<prefs::ScopedDictionaryPrefUpdate>(
          prefs_, partitioned_pref_name_);
    }

    std::vector<std::string> partitions_to_remove;

    const auto& partitions = prefs_->GetDict(partitioned_pref_name_);
    for (const auto&& [key, value] : partitions) {
      auto partition_key = PartitionKey::Deserialize(key);
      if (!partition_key.has_value()) {
        LOG(ERROR) << "failed to deserialize partition key " << key;
        partitions_to_remove.emplace_back(key);
        continue;
      }

      std::unique_ptr<prefs::DictionaryValueUpdate> mutable_partition;
      if (update) {
        (*update)->GetDictionaryWithoutPathExpansion(key, &mutable_partition);
      }
      ReadContentSettingsFromPrefForPartition(*partition_key, value.GetDict(),
                                              mutable_partition.get());

      if (mutable_partition && mutable_partition->empty()) {
        partitions_to_remove.push_back(key);
      }
    }

    if (update) {
      CHECK(!off_the_record_);
      for (auto partition : partitions_to_remove) {
        (*update)->RemoveWithoutPathExpansion(partition, nullptr);
      }
    }
  }
}

void ContentSettingsPref::ReadContentSettingsFromPrefForPartition(
    const PartitionKey& partition_key,
    const base::Value::Dict& partition,
    prefs::DictionaryValueUpdate* mutable_partition) {
  CHECK(!partition_key.in_memory());
  // Accumulates non-canonical pattern strings found in Prefs for which the
  // corresponding canonical pattern is also in Prefs. In these cases the
  // canonical version takes priority, and the non-canonical pattern is removed.
  std::vector<std::string> non_canonical_patterns_to_remove;

  // Keeps track of expired pattern strings found in Prefs, in these cases we
  // will remove the expired entries.
  std::vector<std::string> expired_patterns_to_remove;

  // Keeps track of pattern strings with expired last used permission found in
  // Prefs, in these cases we will remove the expired field.
  std::vector<std::string> expired_permission_usage_to_remove;

  // Accumulates non-canonical pattern strings found in Prefs for which the
  // canonical pattern is not found in Prefs. The exception data for these
  // patterns is to be re-keyed under the canonical pattern.
  base::StringPairs non_canonical_patterns_to_canonical_pattern;

  for (const auto&& i : partition) {
    const std::string& pattern_str(i.first);
    PatternPair pattern_pair = ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() || !pattern_pair.second.IsValid()) {
      // TODO: Change this to DFATAL when crbug.com/132659 is fixed.
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    const std::string canonicalized_pattern_str =
        CreatePatternString(pattern_pair.first, pattern_pair.second);
    DCHECK(!canonicalized_pattern_str.empty());
    if (canonicalized_pattern_str != pattern_str) {
      if (partition.Find(canonicalized_pattern_str)) {
        non_canonical_patterns_to_remove.push_back(pattern_str);
        continue;
      } else {
        // Need a container that preserves ordering of insertions, so that if
        // multiple non-canonical patterns map to the same canonical pattern,
        // the Preferences updating logic after this loop will preserve the same
        // value in Prefs that this loop ultimately leaves in |value_map_|.
        non_canonical_patterns_to_canonical_pattern.emplace_back(
            pattern_str, canonicalized_pattern_str);
      }
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    DCHECK(i.second.is_dict());
    const base::Value::Dict& settings_dictionary = i.second.GetDict();

    // Check to see if the setting is expired or not. This may be due to a past
    // expiration date or a SessionModel of UserSession.
    base::Time expiration = GetExpiration(settings_dictionary);
    mojom::SessionModel session_model = GetSessionModel(settings_dictionary);
    if (ShouldRemoveSetting(expiration, session_model)) {
      expired_patterns_to_remove.push_back(pattern_str);
      continue;
    }
    // Users may edit the stored fields directly, so we cannot assume their
    // presence and validity.
    base::TimeDelta lifetime = content_settings::RuleMetaData::ComputeLifetime(
        /*lifetime=*/GetLifetime(settings_dictionary),
        /*expiration=*/expiration);

    const base::Value* value = settings_dictionary.Find(kSettingKey);
    if (value) {
      base::Time last_modified;
      base::Time last_used;
      base::Time last_visited;
      if (!off_the_record_) {
        // Don't copy over timestamps for OTR profiles because some features
        // rely on this to differentiate inherited from fresh OTR permissions.
        // See RecentSiteSettingsHelperTest.IncognitoPermissionTimestamps
        last_modified = GetLastModified(settings_dictionary);
        last_used = GetLastUsed(settings_dictionary);
        if (last_used != base::Time() &&
            clock_->Now() - last_used >= kLastUsedPermissionExpiration) {
          expired_permission_usage_to_remove.push_back(pattern_str);
          last_used = base::Time();
        }
        last_visited = GetLastVisit(settings_dictionary);
      }
      DCHECK(IsValueAllowedForType(*value, content_type_));
      RuleMetaData metadata;
      metadata.set_last_modified(last_modified);
      metadata.set_last_used(last_used);
      metadata.set_last_visited(last_visited);
      metadata.SetExpirationAndLifetime(expiration, lifetime);
      metadata.set_session_model(session_model);
      metadata.set_decided_by_related_website_sets(
          GetDecidedByRelatedWebsiteSets(settings_dictionary));

      // Migrating grants by Related Website Sets to DURABLE.
      // TODO(b/344678400): Delete after NON_RESTORABLE_USER_SESSION is
      // removed.
      if ((content_type_ == ContentSettingsType::STORAGE_ACCESS ||
           content_type_ == ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS) &&
          session_model == mojom::SessionModel::NON_RESTORABLE_USER_SESSION) {
        metadata.set_session_model(mojom::SessionModel::DURABLE);
        metadata.set_decided_by_related_website_sets(true);
      }

      value_map_.SetValue(std::move(pattern_pair.first),
                          std::move(pattern_pair.second), content_type_,
                          value->Clone(), metadata, partition_key);
    }
  }

  // Canonicalization and expiration are unnecessary when |off_the_record_|.
  // Both off_the_record and non off_the_record read from the same pref and non
  // off_the_record reads occur before off_the_record reads. Thus, by the time
  // the off_the_record call to ReadContentSettingsFromPref() occurs, the
  // regular profile will have canonicalized the stored pref data.
  if (!off_the_record_) {
    for (const auto& pattern : non_canonical_patterns_to_remove) {
      mutable_partition->RemoveWithoutPathExpansion(pattern, nullptr);
    }

    for (const auto& pattern : expired_patterns_to_remove) {
      mutable_partition->RemoveWithoutPathExpansion(pattern, nullptr);
    }

    for (const auto& pattern : expired_permission_usage_to_remove) {
      if (mutable_partition->HasKey(pattern)) {
        std::unique_ptr<prefs::DictionaryValueUpdate> dict;
        mutable_partition->GetDictionaryWithoutPathExpansion(pattern, &dict);
        dict->RemoveWithoutPathExpansion(kLastUsedKey, nullptr);
      }
    }

    for (const auto& old_to_new_pattern :
         non_canonical_patterns_to_canonical_pattern) {
      base::Value pattern_settings_dictionary;
      mutable_partition->RemoveWithoutPathExpansion(
          old_to_new_pattern.first, &pattern_settings_dictionary);
      mutable_partition->SetWithoutPathExpansion(
          old_to_new_pattern.second, std::move(pattern_settings_dictionary));
    }
  }
}

bool ContentSettingsPref::ShouldRemoveSetting(
    base::Time expiration,
    content_settings::mojom::SessionModel session_model) {
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kActiveContentSettingExpiry) &&
      !expiration.is_null() && expiration < clock_->Now()) {
    // Delete if an expiration date is set and in the past.
    return true;
  }

  // Off the Record preferences are inherited from the parent profile, which
  // has already been culled.
  if (off_the_record_) {
    return false;
  }

  // Clear non-restorable user session settings, or non-Durable settings when no
  // restoring a previous session.
  switch (session_model) {
    case content_settings::mojom::SessionModel::DURABLE:
      return false;
    case content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION:
      // Restore NON_RESTORABLE_USER_SESSION Storage Access permissions to
      // migrate them to DURABLE session model.
      // TODO(b/344678400): Delete after NON_RESTORABLE_USER_SESSION is removed.
      if (content_type_ == ContentSettingsType::STORAGE_ACCESS ||
          content_type_ == ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS) {
        return false;
      }
      return true;
    case content_settings::mojom::SessionModel::USER_SESSION:
    case content_settings::mojom::SessionModel::ONE_TIME:
      return !restore_session_;
  }
}

void ContentSettingsPref::OnPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (updating_preferences_)
    return;

  ReadContentSettingsFromPref();

  notify_callback_.Run(ContentSettingsPattern::Wildcard(),
                       ContentSettingsPattern::Wildcard(), content_type_,
                       nullptr);
}

void ContentSettingsPref::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value value,
    const RuleMetaData& metadata,
    const PartitionKey& partition_key) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~ScopedDictionaryPrefUpdate|).
  AssertLockNotHeld();
  CHECK(!off_the_record_);
  CHECK(!partition_key.in_memory());

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    prefs::ScopedDictionaryPrefUpdate update(
        prefs_,
        partition_key.is_default() ? pref_name_ : partitioned_pref_name_);
    std::unique_ptr<prefs::DictionaryValueUpdate> pattern_pairs_settings;
    const auto serialized_partition_key = partition_key.Serialize();
    if (partition_key.is_default()) {
      pattern_pairs_settings = update.Get();
    } else {
      if (!update->GetDictionaryWithoutPathExpansion(serialized_partition_key,
                                                     &pattern_pairs_settings)) {
        // The partition does not have any data.

        if (value.is_none()) {
          // Nothing to do.
          return;
        } else {
          pattern_pairs_settings = update->SetDictionaryWithoutPathExpansion(
              serialized_partition_key, base::Value::Dict());
        }
      }
    }

    // Get settings dictionary for the given patterns.
    std::string pattern_str(
        CreatePatternString(primary_pattern, secondary_pattern));
    std::unique_ptr<prefs::DictionaryValueUpdate> settings_dictionary;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found && !value.is_none()) {
      settings_dictionary =
          pattern_pairs_settings->SetDictionaryWithoutPathExpansion(
              pattern_str, base::Value::Dict());
    }

    if (!settings_dictionary) {
      // This happens when there isn't existing data for the pattern, and
      // `value` is none. So there is nothing to do.
      return;
    }

    // Update settings dictionary.
    if (value.is_none()) {
      settings_dictionary->RemoveWithoutPathExpansion(kSettingKey, nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(kLastModifiedKey,
                                                      nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(kLastVisitKey, nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(kExpirationKey, nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(kSessionModelKey,
                                                      nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(kLifetimeKey, nullptr);
      settings_dictionary->RemoveWithoutPathExpansion(
          kDecidedByRelatedWebsiteSets, nullptr);
    } else {
      settings_dictionary->SetKey(kSettingKey, std::move(value));
      if (metadata.last_modified() != base::Time()) {
        settings_dictionary->SetKey(
            kLastModifiedKey, base::TimeToValue(metadata.last_modified()));
      }
      if (metadata.expiration() != base::Time()) {
        settings_dictionary->SetKey(kExpirationKey,
                                    base::TimeToValue(metadata.expiration()));
      }
      if (metadata.session_model() != mojom::SessionModel::DURABLE) {
        settings_dictionary->SetKey(
            kSessionModelKey,
            base::Value(static_cast<int>(metadata.session_model())));
      }
      if (metadata.last_used() != base::Time()) {
        settings_dictionary->SetKey(kLastUsedKey,
                                    base::TimeToValue(metadata.last_used()));
      }
      if (metadata.last_visited() != base::Time()) {
        settings_dictionary->SetKey(kLastVisitKey,
                                    base::TimeToValue(metadata.last_visited()));
      }
      if (!metadata.lifetime().is_zero()) {
        settings_dictionary->SetKey(
            kLifetimeKey, base::TimeDeltaToValue(metadata.lifetime()));
      }
      if (metadata.decided_by_related_website_sets()) {
        settings_dictionary->SetKey(
            kDecidedByRelatedWebsiteSets,
            base::Value(metadata.decided_by_related_website_sets()));
      }
    }

    // Remove the settings dictionary if it is empty.
    if (settings_dictionary->empty()) {
      pattern_pairs_settings->RemoveWithoutPathExpansion(pattern_str, nullptr);
    }
    if (!partition_key.is_default() && pattern_pairs_settings->empty()) {
      update->RemoveWithoutPathExpansion(serialized_partition_key, nullptr);
    }
  }
}

void ContentSettingsPref::AssertLockNotHeld() const {
#if !defined(NDEBUG)
  // |Lock::Acquire()| will assert if the lock is held by this thread.
  value_map_.GetLock().Acquire();
  value_map_.GetLock().Release();
#endif
}

void ContentSettingsPref::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
  value_map_.SetClockForTesting(clock);                 // IN-TEST
  off_the_record_value_map_.SetClockForTesting(clock);  // IN-TEST
}

}  // namespace content_settings

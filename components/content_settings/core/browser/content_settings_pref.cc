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
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

const char kExpirationKey[] = "expiration";
const char kLastUsedKey[] = "last_used";
const char kLastVisitKey[] = "last_visit";
const char kSessionModelKey[] = "model";
const char kSettingKey[] = "setting";
const char kLastModifiedKey[] = "last_modified";
const char kLifetimeKey[] = "lifetime";

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

// Extract a SessionModel from |dictionary[kSessionModelKey]|. Will return
// SessionModel::Durable if no model exists.
content_settings::SessionModel GetSessionModel(
    const base::Value::Dict& dictionary) {
  int model_int = dictionary.FindInt(kSessionModelKey).value_or(0);
  if ((model_int >
       static_cast<int>(content_settings::SessionModel::kMaxValue)) ||
      (model_int < 0)) {
    model_int = 0;
  }

  content_settings::SessionModel session_model =
      static_cast<content_settings::SessionModel>(model_int);
  return session_model;
}

bool ShouldRemoveSetting(bool off_the_record,
                         base::Time expiration,
                         bool restore_session,
                         content_settings::SessionModel session_model) {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kActiveContentSettingExpiry)) {
    return false;
  }
  // Delete if an expriation date is set and in the past.
  if (!expiration.is_null() && (expiration < base::Time::Now()))
    return true;

  // Off the Record preferences are inherited from the parent profile, which
  // has already been culled.
  if (off_the_record)
    return false;

  // Clear non-restorable user session settings, or non-Durable settings when no
  // restoring a previous session.
  switch (session_model) {
    case content_settings::SessionModel::Durable:
      return false;
    case content_settings::SessionModel::NonRestorableUserSession:
      return true;
    case content_settings::SessionModel::UserSession:
    case content_settings::SessionModel::OneTime:
      return !restore_session;
  }
}

}  // namespace

namespace content_settings {

ContentSettingsPref::ContentSettingsPref(
    ContentSettingsType content_type,
    PrefService* prefs,
    PrefChangeRegistrar* registrar,
    const std::string& pref_name,
    bool off_the_record,
    bool restore_session,
    NotifyObserversCallback notify_callback)
    : content_type_(content_type),
      prefs_(prefs),
      registrar_(registrar),
      pref_name_(pref_name),
      off_the_record_(off_the_record),
      restore_session_(restore_session),
      updating_preferences_(false),
      notify_callback_(notify_callback) {
  DCHECK(prefs_);

  ReadContentSettingsFromPref();

  registrar_->Add(*pref_name_,
                  base::BindRepeating(&ContentSettingsPref::OnPrefChanged,
                                      base::Unretained(this)));
}

ContentSettingsPref::~ContentSettingsPref() = default;

std::unique_ptr<RuleIterator> ContentSettingsPref::GetRuleIterator(
    bool off_the_record) const {
  if (off_the_record)
    return off_the_record_value_map_.GetRuleIterator(content_type_);
  return value_map_.GetRuleIterator(content_type_);
}

void ContentSettingsPref::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value value,
    const RuleMetaData& metadata) {
  DCHECK(value.is_none() || IsValueAllowedForType(value, content_type_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  DCHECK(primary_pattern != ContentSettingsPattern::Wildcard() ||
         secondary_pattern != ContentSettingsPattern::Wildcard());

  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &off_the_record_value_map_;
  if (!off_the_record_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(map_to_modify->GetLock());
    if (!value.is_none()) {
      map_to_modify->SetValue(primary_pattern, secondary_pattern, content_type_,
                              value.Clone(), metadata);
    } else {
      map_to_modify->DeleteValue(primary_pattern, secondary_pattern,
                                 content_type_);
    }
  }
  // Update the content settings preference.
  if (!off_the_record_) {
    UpdatePref(primary_pattern, secondary_pattern, std::move(value), metadata);
  }

  notify_callback_.Run(primary_pattern, secondary_pattern, content_type_);
}

void ContentSettingsPref::ClearPref() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  {
    base::AutoLock auto_lock(value_map_.GetLock());
    value_map_.clear();
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs::ScopedDictionaryPrefUpdate update(prefs_, *pref_name_);
    update->Clear();
  }
}

void ContentSettingsPref::ClearAllContentSettingsRules() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  if (off_the_record_) {
    base::AutoLock auto_lock(off_the_record_value_map_.GetLock());
    off_the_record_value_map_.clear();
  } else {
    ClearPref();
  }

  notify_callback_.Run(ContentSettingsPattern::Wildcard(),
                       ContentSettingsPattern::Wildcard(), content_type_);
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
  prefs::ScopedDictionaryPrefUpdate update(prefs_, *pref_name_);
  base::AutoLock auto_lock(value_map_.GetLock());

  value_map_.clear();

  // The returned value could be nullptr if the pref has never been set.
  const base::Value::Dict& all_settings_dictionary =
      prefs_->GetDict(*pref_name_);

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

  for (const auto i : all_settings_dictionary) {
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
      if (all_settings_dictionary.Find(canonicalized_pattern_str)) {
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
    SessionModel session_model = GetSessionModel(settings_dictionary);
    if (ShouldRemoveSetting(off_the_record_, expiration, restore_session_,
                            session_model)) {
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
            base::Time::Now() - last_used >= kLastUsedPermissionExpiration) {
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

      value_map_.SetValue(std::move(pattern_pair.first),
                          std::move(pattern_pair.second), content_type_,
                          value->Clone(), metadata);
    }
  }

  // Canonicalization and expiration are unnecessary when |off_the_record_|.
  // Both off_the_record and non off_the_record read from the same pref and non
  // off_the_record reads occur before off_the_record reads. Thus, by the time
  // the off_the_record call to ReadContentSettingsFromPref() occurs, the
  // regular profile will have canonicalized the stored pref data.
  if (!off_the_record_) {
    auto mutable_settings = update.Get();

    for (const auto& pattern : non_canonical_patterns_to_remove) {
      mutable_settings.get()->RemoveWithoutPathExpansion(pattern, nullptr);
    }

    for (const auto& pattern : expired_patterns_to_remove) {
      mutable_settings.get()->RemoveWithoutPathExpansion(pattern, nullptr);
    }

    for (const auto& pattern : expired_permission_usage_to_remove) {
      if (mutable_settings.get()->HasKey(pattern)) {
        std::unique_ptr<prefs::DictionaryValueUpdate> dict;
        mutable_settings.get()->GetDictionaryWithoutPathExpansion(pattern,
                                                                  &dict);
        dict->RemoveWithoutPathExpansion(kLastUsedKey, nullptr);
      }
    }

    for (const auto& old_to_new_pattern :
         non_canonical_patterns_to_canonical_pattern) {
      base::Value pattern_settings_dictionary;
      mutable_settings.get()->RemoveWithoutPathExpansion(
          old_to_new_pattern.first, &pattern_settings_dictionary);
      mutable_settings.get()->SetWithoutPathExpansion(
          old_to_new_pattern.second, std::move(pattern_settings_dictionary));
    }
  }
}

void ContentSettingsPref::OnPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (updating_preferences_)
    return;

  ReadContentSettingsFromPref();

  notify_callback_.Run(ContentSettingsPattern::Wildcard(),
                       ContentSettingsPattern::Wildcard(), content_type_);
}

void ContentSettingsPref::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value value,
    const RuleMetaData& metadata) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~ScopedDictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    prefs::ScopedDictionaryPrefUpdate update(prefs_, *pref_name_);
    std::unique_ptr<prefs::DictionaryValueUpdate> pattern_pairs_settings =
        update.Get();

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

    if (settings_dictionary) {
      // Update settings dictionary.
      if (value.is_none()) {
        settings_dictionary->RemoveWithoutPathExpansion(kSettingKey, nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kLastModifiedKey,
                                                        nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kLastVisitKey, nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kExpirationKey,
                                                        nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kSessionModelKey,
                                                        nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kLifetimeKey, nullptr);
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
        if (metadata.session_model() != SessionModel::Durable) {
          settings_dictionary->SetKey(
              kSessionModelKey,
              base::Value(static_cast<int>(metadata.session_model())));
        }
        if (metadata.last_used() != base::Time()) {
          settings_dictionary->SetKey(kLastUsedKey,
                                      base::TimeToValue(metadata.last_used()));
        }
        if (metadata.last_visited() != base::Time()) {
          settings_dictionary->SetKey(
              kLastVisitKey, base::TimeToValue(metadata.last_visited()));
        }
        if (!metadata.lifetime().is_zero()) {
          settings_dictionary->SetKey(
              kLifetimeKey, base::TimeDeltaToValue(metadata.lifetime()));
        }
      }

      // Remove the settings dictionary if it is empty.
      if (settings_dictionary->empty()) {
        pattern_pairs_settings->RemoveWithoutPathExpansion(pattern_str,
                                                           nullptr);
      }
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

}  // namespace content_settings

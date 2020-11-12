// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "url/gurl.h"

namespace {

const char kExpirationPath[] = "expiration";
const char kSessionModelPath[] = "model";
const char kSettingPath[] = "setting";
const char kLastModifiedPath[] = "last_modified";

bool IsValueAllowedForType(const base::Value* value, ContentSettingsType type) {
  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  if (info) {
    int setting;
    if (!value->GetAsInteger(&setting))
      return false;
    if (setting == CONTENT_SETTING_DEFAULT)
      return false;
    return info->IsSettingValid(IntToContentSetting(setting));
  }

  // TODO(raymes): We should permit different types of base::Value for
  // website settings.
  return value->type() == base::Value::Type::DICTIONARY;
}

// Extract a timestamp from |dictionary[kLastModifiedPath]|.
// Will return base::Time() if no timestamp exists.
base::Time GetTimeStamp(const base::DictionaryValue* dictionary) {
  std::string timestamp_str;
  dictionary->GetStringWithoutPathExpansion(kLastModifiedPath, &timestamp_str);
  int64_t timestamp = 0;
  base::StringToInt64(timestamp_str, &timestamp);
  base::Time last_modified = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(timestamp));
  return last_modified;
}

// Extract a timestamp from |dictionary[kExpirationPath]|. Will return
// base::Time() if no timestamp exists.
base::Time GetExpiration(const base::DictionaryValue* dictionary) {
  std::string expiration_timestamp_str;
  dictionary->GetStringWithoutPathExpansion(kExpirationPath,
                                            &expiration_timestamp_str);
  int64_t expiration_timestamp = 0;
  base::StringToInt64(expiration_timestamp_str, &expiration_timestamp);
  base::Time expiration = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(expiration_timestamp));
  return expiration;
}

// Extract a SessionModel from |dictionary[kSessionModelPath]|. Will return
// SessionModel::Durable if no model exists.
content_settings::SessionModel GetSessionModel(
    const base::DictionaryValue* dictionary) {
  int model_int = 0;
  dictionary->GetIntegerWithoutPathExpansion(kSessionModelPath, &model_int);
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
  // Delete if an expriation date is set and in the past.
  if (!expiration.is_null() && (expiration < base::Time::Now()))
    return true;

  // Off the Record preferences are inherited from the parent profile, which
  // has already been culled.
  if (off_the_record)
    return false;

  // Clear non-Durable settings when no restoring a previous session.
  return ((session_model != content_settings::SessionModel::Durable) &&
          !restore_session);
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

  registrar_->Add(pref_name_,
                  base::BindRepeating(&ContentSettingsPref::OnPrefChanged,
                                      base::Unretained(this)));
}

ContentSettingsPref::~ContentSettingsPref() {}

std::unique_ptr<RuleIterator> ContentSettingsPref::GetRuleIterator(
    bool off_the_record) const {
  if (off_the_record)
    return off_the_record_value_map_.GetRuleIterator(content_type_, &lock_);
  return value_map_.GetRuleIterator(content_type_, &lock_);
}

bool ContentSettingsPref::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Time modified_time,
    std::unique_ptr<base::Value>&& in_value,
    const ContentSettingConstraints& constraints) {
  DCHECK(!in_value || IsValueAllowedForType(in_value.get(), content_type_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  DCHECK(primary_pattern != ContentSettingsPattern::Wildcard() ||
         secondary_pattern != ContentSettingsPattern::Wildcard());

  // At this point take the ownership of the |in_value|.
  std::unique_ptr<base::Value> value(std::move(in_value));

  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &off_the_record_value_map_;
  if (!off_the_record_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (value) {
      map_to_modify->SetValue(primary_pattern, secondary_pattern, content_type_,
                              modified_time, value->Clone(), constraints);
    } else {
      map_to_modify->DeleteValue(primary_pattern, secondary_pattern,
                                 content_type_);
    }
  }
  // Update the content settings preference.
  if (!off_the_record_) {
    UpdatePref(primary_pattern, secondary_pattern, modified_time, value.get(),
               constraints);
  }

  notify_callback_.Run(primary_pattern, secondary_pattern, content_type_);

  return true;
}

base::Time ContentSettingsPref::GetWebsiteSettingLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  OriginIdentifierValueMap* map_to_modify = &off_the_record_value_map_;
  if (!off_the_record_)
    map_to_modify = &value_map_;

  base::Time last_modified = map_to_modify->GetLastModified(
      primary_pattern, secondary_pattern, content_type_);
  return last_modified;
}

void ContentSettingsPref::ClearPref() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  {
    base::AutoLock auto_lock(lock_);
    value_map_.clear();
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_name_);
    update->Clear();
  }
}

void ContentSettingsPref::ClearAllContentSettingsRules() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);

  if (off_the_record_) {
    base::AutoLock auto_lock(lock_);
    off_the_record_value_map_.clear();
  } else {
    ClearPref();
  }

  notify_callback_.Run(ContentSettingsPattern(), ContentSettingsPattern(),
                       content_type_);
}

size_t ContentSettingsPref::GetNumExceptions() {
  return value_map_.size();
}

bool ContentSettingsPref::TryLockForTesting() const {
  if (!lock_.Try())
    return false;
  lock_.Release();
  return true;
}

void ContentSettingsPref::ReadContentSettingsFromPref() {
  // |ScopedDictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |lock_| is
  // not held when the notifications are sent. Also, |auto_reset| must be still
  // valid when the notifications are sent, so that |Observe| skips the
  // notification.
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_name_);
  base::AutoLock auto_lock(lock_);

  value_map_.clear();

  // The returned value could be nullptr if the pref has never been set.
  const base::DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(pref_name_);
  if (!all_settings_dictionary)
    return;

  // Accumulates non-canonical pattern strings found in Prefs for which the
  // corresponding canonical pattern is also in Prefs. In these cases the
  // canonical version takes priority, and the non-canonical pattern is removed.
  std::vector<std::string> non_canonical_patterns_to_remove;

  // Keeps track of expired pattern strings found in Prefs, in these cases we
  // will remove the expired entries.
  std::vector<std::string> expired_patterns_to_remove;

  // Accumulates non-canonical pattern strings found in Prefs for which the
  // canonical pattern is not found in Prefs. The exception data for these
  // patterns is to be re-keyed under the canonical pattern.
  base::StringPairs non_canonical_patterns_to_canonical_pattern;

  for (base::DictionaryValue::Iterator i(*all_settings_dictionary);
       !i.IsAtEnd(); i.Advance()) {
    const std::string& pattern_str(i.key());
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
      if (all_settings_dictionary->HasKey(canonicalized_pattern_str)) {
        non_canonical_patterns_to_remove.push_back(pattern_str);
        continue;
      } else {
        // Need a container that preserves ordering of insertions, so that if
        // multiple non-canonical patterns map to the same canonical pattern,
        // the Preferences updating logic after this loop will preserve the same
        // value in Prefs that this loop ultimately leaves in |value_map_|.
        non_canonical_patterns_to_canonical_pattern.push_back(
            {pattern_str, canonicalized_pattern_str});
      }
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    const base::DictionaryValue* settings_dictionary = nullptr;
    bool is_dictionary = i.value().GetAsDictionary(&settings_dictionary);
    DCHECK(is_dictionary);

    // Check to see if the setting is expired or not. This may be due to a past
    // expiration date or a SessionModel of UserSession.
    base::Time expiration = GetExpiration(settings_dictionary);
    SessionModel session_model = GetSessionModel(settings_dictionary);
    if (ShouldRemoveSetting(off_the_record_, expiration, restore_session_,
                            session_model)) {
      expired_patterns_to_remove.push_back(pattern_str);
      continue;
    }

    const base::Value* value = nullptr;
    settings_dictionary->GetWithoutPathExpansion(kSettingPath, &value);
    if (value) {
      base::Time last_modified = GetTimeStamp(settings_dictionary);
      DCHECK(IsValueAllowedForType(value, content_type_));
      value_map_.SetValue(std::move(pattern_pair.first),
                          std::move(pattern_pair.second), content_type_,
                          last_modified, value->Clone(),
                          {expiration, session_model});
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

    for (const auto& old_to_new_pattern :
         non_canonical_patterns_to_canonical_pattern) {
      std::unique_ptr<base::Value> pattern_settings_dictionary;
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

  notify_callback_.Run(ContentSettingsPattern(), ContentSettingsPattern(),
                       content_type_);
}

void ContentSettingsPref::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const base::Time last_modified,
    const base::Value* value,
    const ContentSettingConstraints& constraints) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~ScopedDictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_name_);
    std::unique_ptr<prefs::DictionaryValueUpdate> pattern_pairs_settings =
        update.Get();

    // Get settings dictionary for the given patterns.
    std::string pattern_str(
        CreatePatternString(primary_pattern, secondary_pattern));
    std::unique_ptr<prefs::DictionaryValueUpdate> settings_dictionary;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found && value) {
      settings_dictionary =
          pattern_pairs_settings->SetDictionaryWithoutPathExpansion(
              pattern_str, std::make_unique<base::DictionaryValue>());
    }

    if (settings_dictionary) {
      // Update settings dictionary.
      if (value == nullptr) {
        settings_dictionary->RemoveWithoutPathExpansion(kSettingPath, nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kLastModifiedPath,
                                                        nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kExpirationPath,
                                                        nullptr);
        settings_dictionary->RemoveWithoutPathExpansion(kSessionModelPath,
                                                        nullptr);
      } else {
        settings_dictionary->SetWithoutPathExpansion(kSettingPath,
                                                     value->CreateDeepCopy());
        settings_dictionary->SetKey(
            kLastModifiedPath,
            base::Value(base::NumberToString(
                last_modified.ToDeltaSinceWindowsEpoch().InMicroseconds())));
        settings_dictionary->SetKey(
            kExpirationPath,
            base::Value(base::NumberToString(
                constraints.expiration.ToDeltaSinceWindowsEpoch()
                    .InMicroseconds())));
        settings_dictionary->SetKey(
            kSessionModelPath,
            base::Value(static_cast<int>(constraints.session_model)));
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
  lock_.Acquire();
  lock_.Release();
#endif
}

}  // namespace content_settings

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
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

const char kSettingPath[] = "setting";
const char kLastModifiedPath[] = "last_modified";
const char kPerResourceIdentifierPrefName[] = "per_resource";

// If the given content type supports resource identifiers in user preferences,
// returns true and sets |pref_key| to the key in the content settings
// dictionary under which per-resource content settings are stored.
// Otherwise, returns false.
bool SupportsResourceIdentifiers(ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_PLUGINS;
}

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
  base::Time last_modified = base::Time::FromInternalValue(timestamp);
  return last_modified;
}

}  // namespace

namespace content_settings {

ContentSettingsPref::ContentSettingsPref(
    ContentSettingsType content_type,
    PrefService* prefs,
    PrefChangeRegistrar* registrar,
    const std::string& pref_name,
    bool incognito,
    NotifyObserversCallback notify_callback)
    : content_type_(content_type),
      prefs_(prefs),
      registrar_(registrar),
      pref_name_(pref_name),
      is_incognito_(incognito),
      updating_preferences_(false),
      notify_callback_(notify_callback) {
  DCHECK(prefs_);

  ReadContentSettingsFromPref();

  registrar_->Add(
      pref_name_,
      base::Bind(&ContentSettingsPref::OnPrefChanged, base::Unretained(this)));
}

ContentSettingsPref::~ContentSettingsPref() {
}

std::unique_ptr<RuleIterator> ContentSettingsPref::GetRuleIterator(
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (incognito)
    return incognito_value_map_.GetRuleIterator(content_type_,
                                                resource_identifier,
                                                &lock_);
  return value_map_.GetRuleIterator(content_type_, resource_identifier, &lock_);
}

bool ContentSettingsPref::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier,
    base::Time modified_time,
    base::Value* in_value) {
  DCHECK(!in_value || IsValueAllowedForType(in_value, content_type_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  DCHECK(primary_pattern != ContentSettingsPattern::Wildcard() ||
         secondary_pattern != ContentSettingsPattern::Wildcard() ||
         !resource_identifier.empty());

  // At this point take the ownership of the |in_value|.
  std::unique_ptr<base::Value> value(in_value);

  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (value) {
      map_to_modify->SetValue(primary_pattern, secondary_pattern, content_type_,
                              resource_identifier, modified_time,
                              value->DeepCopy());
    } else {
      map_to_modify->DeleteValue(
          primary_pattern,
          secondary_pattern,
          content_type_,
          resource_identifier);
    }
  }
  // Update the content settings preference.
  if (!is_incognito_) {
    UpdatePref(primary_pattern, secondary_pattern, resource_identifier,
               modified_time, value.get());
  }

  notify_callback_.Run(
      primary_pattern, secondary_pattern, content_type_, resource_identifier);

  return true;
}

base::Time ContentSettingsPref::GetWebsiteSettingLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier) {
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  base::Time last_modified = map_to_modify->GetLastModified(
      primary_pattern, secondary_pattern, content_type_, resource_identifier);
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

  if (is_incognito_) {
    base::AutoLock auto_lock(lock_);
    incognito_value_map_.clear();
  } else {
    ClearPref();
  }

  notify_callback_.Run(ContentSettingsPattern(),
                       ContentSettingsPattern(),
                       content_type_,
                       ResourceIdentifier());
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

  const base::DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(pref_name_);

  value_map_.clear();

  // Careful: The returned value could be nullptr if the pref has never been
  // set.
  if (!all_settings_dictionary)
    return;

  const base::DictionaryValue* settings;

  if (!is_incognito_) {
    // Convert all Unicode patterns into punycode form, then read.
    auto mutable_settings = update.Get();
    CanonicalizeContentSettingsExceptions(mutable_settings.get());
    settings = mutable_settings->AsConstDictionary();
  } else {
    // Canonicalization is unnecessary when |is_incognito_|. Both incognito and
    // non-incognito read from the same pref and non-incognito reads occur
    // before incognito reads. Thus, by the time the incognito call to
    // ReadContentSettingsFromPref() occurs, the non-incognito call will have
    // canonicalized the stored pref data.
    settings = all_settings_dictionary;
  }

  for (base::DictionaryValue::Iterator i(*settings); !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      // TODO: Change this to DFATAL when crbug.com/132659 is fixed.
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    const base::DictionaryValue* settings_dictionary = nullptr;
    bool is_dictionary = i.value().GetAsDictionary(&settings_dictionary);
    DCHECK(is_dictionary);

    if (SupportsResourceIdentifiers(content_type_)) {
      const base::DictionaryValue* resource_dictionary = nullptr;
      if (settings_dictionary->GetDictionary(
              kPerResourceIdentifierPrefName, &resource_dictionary)) {
        base::Time last_modified = GetTimeStamp(settings_dictionary);
        for (base::DictionaryValue::Iterator j(*resource_dictionary);
             !j.IsAtEnd();
             j.Advance()) {
          const std::string& resource_identifier(j.key());
          int setting = CONTENT_SETTING_DEFAULT;
          bool is_integer = j.value().GetAsInteger(&setting);
          DCHECK(is_integer);
          DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
          std::unique_ptr<base::Value> setting_ptr(new base::Value(setting));
          DCHECK(IsValueAllowedForType(setting_ptr.get(), content_type_));
          // Per resource settings store a single timestamps for all resources.
          value_map_.SetValue(pattern_pair.first, pattern_pair.second,
                              content_type_, resource_identifier, last_modified,
                              setting_ptr->DeepCopy());
        }
      }
    }

    const base::Value* value = nullptr;
    settings_dictionary->GetWithoutPathExpansion(kSettingPath, &value);
    if (value) {
      base::Time last_modified = GetTimeStamp(settings_dictionary);
      DCHECK(IsValueAllowedForType(value, content_type_));
      value_map_.SetValue(pattern_pair.first, pattern_pair.second,
                          content_type_, ResourceIdentifier(), last_modified,
                          value->DeepCopy());
    }
  }
}

void ContentSettingsPref::OnPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (updating_preferences_)
    return;

  ReadContentSettingsFromPref();

  notify_callback_.Run(ContentSettingsPattern(),
                       ContentSettingsPattern(),
                       content_type_,
                       ResourceIdentifier());
}

void ContentSettingsPref::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier,
    const base::Time last_modified,
    const base::Value* value) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~ScopedDictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_name_);
    std::unique_ptr<prefs::DictionaryValueUpdate> pattern_pairs_settings =
        update.Get();

    // Get settings dictionary for the given patterns.
    std::string pattern_str(CreatePatternString(primary_pattern,
                                                secondary_pattern));
    std::unique_ptr<prefs::DictionaryValueUpdate> settings_dictionary;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found && value) {
      settings_dictionary =
          pattern_pairs_settings->SetDictionaryWithoutPathExpansion(
              pattern_str, std::make_unique<base::DictionaryValue>());
    }

    if (settings_dictionary) {
      if (SupportsResourceIdentifiers(content_type_) &&
          !resource_identifier.empty()) {
        std::unique_ptr<prefs::DictionaryValueUpdate> resource_dictionary;
        found = settings_dictionary->GetDictionary(
            kPerResourceIdentifierPrefName, &resource_dictionary);
        if (!found) {
          if (value == nullptr)
            return;  // Nothing to remove. Exit early.
          resource_dictionary =
              settings_dictionary->SetDictionaryWithoutPathExpansion(
                  kPerResourceIdentifierPrefName,
                  std::make_unique<base::DictionaryValue>());
        }
        // Update resource dictionary.
        if (value == nullptr) {
          resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                          nullptr);
          if (resource_dictionary->empty()) {
            settings_dictionary->RemoveWithoutPathExpansion(
                kPerResourceIdentifierPrefName, nullptr);
            settings_dictionary->RemoveWithoutPathExpansion(kLastModifiedPath,
                                                            nullptr);
          }
        } else {
          resource_dictionary->SetWithoutPathExpansion(resource_identifier,
                                                       value->CreateDeepCopy());
          // Update timestamp for whole resource dictionary.
          settings_dictionary->SetKey(kLastModifiedPath,
                                      base::Value(base::Int64ToString(
                                          last_modified.ToInternalValue())));
        }
      } else {
        // Update settings dictionary.
        if (value == nullptr) {
          settings_dictionary->RemoveWithoutPathExpansion(kSettingPath,
                                                          nullptr);
          settings_dictionary->RemoveWithoutPathExpansion(kLastModifiedPath,
                                                          nullptr);
        } else {
          settings_dictionary->SetWithoutPathExpansion(kSettingPath,
                                                       value->CreateDeepCopy());
          settings_dictionary->SetKey(kLastModifiedPath,
                                      base::Value(base::Int64ToString(
                                          last_modified.ToInternalValue())));
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

// static
void ContentSettingsPref::CanonicalizeContentSettingsExceptions(
    prefs::DictionaryValueUpdate* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  base::StringPairs move_items;
  for (base::DictionaryValue::Iterator i(
           *all_settings_dictionary->AsConstDictionary());
       !i.IsAtEnd(); i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
         ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    const std::string canonicalized_pattern_str = CreatePatternString(
        pattern_pair.first, pattern_pair.second);

    if (canonicalized_pattern_str.empty() ||
        canonicalized_pattern_str == pattern_str) {
      continue;
    }

    // Clear old pattern if prefs already have canonicalized pattern.
    const base::DictionaryValue* new_pattern_settings_dictionary = nullptr;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern_str, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern_str);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    const base::DictionaryValue* old_pattern_settings_dictionary = nullptr;
    if (i.value().GetAsDictionary(&old_pattern_settings_dictionary)) {
      move_items.push_back(
          std::make_pair(pattern_str, canonicalized_pattern_str));
    }
  }

  for (size_t i = 0; i < remove_items.size(); ++i) {
    all_settings_dictionary->RemoveWithoutPathExpansion(remove_items[i],
                                                        nullptr);
  }

  for (size_t i = 0; i < move_items.size(); ++i) {
    std::unique_ptr<base::Value> pattern_settings_dictionary;
    all_settings_dictionary->RemoveWithoutPathExpansion(
        move_items[i].first, &pattern_settings_dictionary);
    all_settings_dictionary->SetWithoutPathExpansion(
        move_items[i].second, std::move(pattern_settings_dictionary));
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

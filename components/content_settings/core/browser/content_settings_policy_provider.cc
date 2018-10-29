// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

struct PrefsForManagedContentSettingsMapEntry {
  const char* pref_name;
  ContentSettingsType content_type;
  ContentSetting setting;
};

const PrefsForManagedContentSettingsMapEntry
    kPrefsForManagedContentSettingsMap[] = {
        {prefs::kManagedCookiesAllowedForUrls, CONTENT_SETTINGS_TYPE_COOKIES,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedCookiesBlockedForUrls, CONTENT_SETTINGS_TYPE_COOKIES,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedCookiesSessionOnlyForUrls,
         CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_SESSION_ONLY},
        {prefs::kManagedImagesAllowedForUrls, CONTENT_SETTINGS_TYPE_IMAGES,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedImagesBlockedForUrls, CONTENT_SETTINGS_TYPE_IMAGES,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedJavaScriptAllowedForUrls,
         CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_ALLOW},
        {prefs::kManagedJavaScriptBlockedForUrls,
         CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK},
        {prefs::kManagedNotificationsAllowedForUrls,
         CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW},
        {prefs::kManagedNotificationsBlockedForUrls,
         CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_BLOCK},
        {prefs::kManagedPluginsAllowedForUrls, CONTENT_SETTINGS_TYPE_PLUGINS,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedPluginsBlockedForUrls, CONTENT_SETTINGS_TYPE_PLUGINS,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedPopupsAllowedForUrls, CONTENT_SETTINGS_TYPE_POPUPS,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedPopupsBlockedForUrls, CONTENT_SETTINGS_TYPE_POPUPS,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedWebUsbAskForUrls, CONTENT_SETTINGS_TYPE_USB_GUARD,
         CONTENT_SETTING_ASK},
        {prefs::kManagedWebUsbBlockedForUrls, CONTENT_SETTINGS_TYPE_USB_GUARD,
         CONTENT_SETTING_BLOCK}};

}  // namespace

namespace content_settings {

// The preferences used to manage the default policy value for
// ContentSettingsTypes.
struct PolicyProvider::PrefsForManagedDefaultMapEntry {
  ContentSettingsType content_type;
  const char* pref_name;
};

// static
const PolicyProvider::PrefsForManagedDefaultMapEntry
    PolicyProvider::kPrefsForManagedDefault[] = {
        {CONTENT_SETTINGS_TYPE_ADS, prefs::kManagedDefaultAdsSetting},
        {CONTENT_SETTINGS_TYPE_COOKIES, prefs::kManagedDefaultCookiesSetting},
        {CONTENT_SETTINGS_TYPE_IMAGES, prefs::kManagedDefaultImagesSetting},
        {CONTENT_SETTINGS_TYPE_GEOLOCATION,
         prefs::kManagedDefaultGeolocationSetting},
        {CONTENT_SETTINGS_TYPE_JAVASCRIPT,
         prefs::kManagedDefaultJavaScriptSetting},
        {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
         prefs::kManagedDefaultMediaStreamSetting},
        {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
         prefs::kManagedDefaultMediaStreamSetting},
        {CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
         prefs::kManagedDefaultNotificationsSetting},
        {CONTENT_SETTINGS_TYPE_PLUGINS, prefs::kManagedDefaultPluginsSetting},
        {CONTENT_SETTINGS_TYPE_POPUPS, prefs::kManagedDefaultPopupsSetting},
        {CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD,
         prefs::kManagedDefaultWebBluetoothGuardSetting},
        {CONTENT_SETTINGS_TYPE_USB_GUARD,
         prefs::kManagedDefaultWebUsbGuardSetting},
};

// static
void PolicyProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls);
  registry->RegisterListPref(prefs::kManagedCookiesAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedCookiesBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedCookiesSessionOnlyForUrls);
  registry->RegisterListPref(prefs::kManagedImagesAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedImagesBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedJavaScriptAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedJavaScriptBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedNotificationsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedNotificationsBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedPluginsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedPluginsBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedPopupsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedPopupsBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbAllowDevicesForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbAskForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbBlockedForUrls);
  // Preferences for default content setting policies. If a policy is not set of
  // the corresponding preferences below is set to CONTENT_SETTING_DEFAULT.
  registry->RegisterIntegerPref(prefs::kManagedDefaultAdsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultCookiesSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultGeolocationSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultImagesSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultJavaScriptSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultNotificationsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultMediaStreamSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultPluginsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultPopupsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultWebBluetoothGuardSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultWebUsbGuardSetting,
                                CONTENT_SETTING_DEFAULT);
}

PolicyProvider::PolicyProvider(PrefService* prefs) : prefs_(prefs) {
  ReadManagedDefaultSettings();
  ReadManagedContentSettings(false);

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback =
      base::Bind(&PolicyProvider::OnPreferenceChanged, base::Unretained(this));
  pref_change_registrar_.Add(
      prefs::kManagedAutoSelectCertificateForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedCookiesAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedCookiesBlockedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedCookiesSessionOnlyForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedImagesAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedImagesBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptBlockedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedNotificationsAllowedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedNotificationsBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPluginsAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPluginsBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPopupsAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPopupsBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedWebUsbAskForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedWebUsbBlockedForUrls, callback);
  // The following preferences are only used to indicate if a default content
  // setting is managed and to hold the managed default setting value. If the
  // value for any of the following preferences is set then the corresponding
  // default content setting is managed. These preferences exist in parallel to
  // the preference default content settings. If a default content settings type
  // is managed any user defined exceptions (patterns) for this type are
  // ignored.
  pref_change_registrar_.Add(prefs::kManagedDefaultAdsSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultCookiesSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultGeolocationSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultImagesSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultJavaScriptSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultNotificationsSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultMediaStreamSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultPluginsSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultPopupsSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultWebBluetoothGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultWebUsbGuardSetting,
                             callback);
}

PolicyProvider::~PolicyProvider() {
  DCHECK(!prefs_);
}

std::unique_ptr<RuleIterator> PolicyProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

void PolicyProvider::GetContentSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  for (size_t i = 0; i < arraysize(kPrefsForManagedContentSettingsMap); ++i) {
    const char* pref_name = kPrefsForManagedContentSettingsMap[i].pref_name;
    // Skip unset policies.
    if (!prefs_->HasPrefPath(pref_name)) {
      VLOG(2) << "Skipping unset preference: " << pref_name;
      continue;
    }

    const PrefService::Preference* pref = prefs_->FindPreference(pref_name);
    DCHECK(pref);
    DCHECK(!pref->HasUserSetting() && !pref->HasExtensionSetting());

    const base::ListValue* pattern_str_list = nullptr;
    if (!pref->GetValue()->GetAsList(&pattern_str_list)) {
      NOTREACHED() << "Could not read patterns from " << pref_name;
      return;
    }

    for (size_t j = 0; j < pattern_str_list->GetSize(); ++j) {
      std::string original_pattern_str;
      if (!pattern_str_list->GetString(j, &original_pattern_str)) {
        NOTREACHED() << "Could not read content settings pattern #" << j
                     << " from " << pref_name;
        continue;
      }

      VLOG(2) << "Reading content settings pattern " << original_pattern_str
              << " from " << pref_name;

      PatternPair pattern_pair = ParsePatternString(original_pattern_str);
      // Ignore invalid patterns.
      if (!pattern_pair.first.IsValid()) {
        VLOG(1) << "Ignoring invalid content settings pattern "
                << original_pattern_str;
        continue;
      }

      ContentSettingsType content_type =
          kPrefsForManagedContentSettingsMap[i].content_type;
      DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
      // If only one pattern was defined auto expand it to a pattern pair.
      ContentSettingsPattern secondary_pattern =
          !pattern_pair.second.IsValid() ? ContentSettingsPattern::Wildcard()
                                         : pattern_pair.second;
      VLOG_IF(2, !pattern_pair.second.IsValid())
          << "Replacing invalid secondary pattern '"
          << pattern_pair.second.ToString() << "' with wildcard";

      // Currently all settings that can set pattern pairs support embedded
      // exceptions. However if a new content setting is added that doesn't,
      // this DCHECK should be changed to an actual check which ignores such
      // patterns for that type.
      DCHECK(pattern_pair.first == pattern_pair.second ||
             pattern_pair.second == ContentSettingsPattern::Wildcard() ||
             content_settings::WebsiteSettingsRegistry::GetInstance()
                 ->Get(content_type)
                 ->SupportsEmbeddedExceptions());

      // Don't set a timestamp for policy settings.
      value_map->SetValue(
          pattern_pair.first, secondary_pattern, content_type,
          ResourceIdentifier(), base::Time(),
          new base::Value(kPrefsForManagedContentSettingsMap[i].setting));
    }
  }
}

void PolicyProvider::GetAutoSelectCertificateSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  const char* pref_name = prefs::kManagedAutoSelectCertificateForUrls;

  if (!prefs_->HasPrefPath(pref_name)) {
    VLOG(2) << "Skipping unset preference: " << pref_name;
    return;
  }

  const PrefService::Preference* pref = prefs_->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(!pref->HasUserSetting() && !pref->HasExtensionSetting());

  const base::ListValue* pattern_filter_str_list = nullptr;
  if (!pref->GetValue()->GetAsList(&pattern_filter_str_list)) {
    NOTREACHED();
    return;
  }

  // Parse the list of pattern filter strings. A pattern filter string has
  // the following JSON format:
  //
  // {
  //   "pattern": <content settings pattern string>,
  //   "filter" : <certificate filter in JSON format>
  // }
  //
  // e.g.
  // {
  //   "pattern": "[*.]example.com",
  //   "filter": {
  //      "ISSUER": {
  //        "CN": "some name"
  //      }
  //   }
  // }
  std::unordered_map<std::string, base::DictionaryValue> filters_map;
  for (size_t j = 0; j < pattern_filter_str_list->GetSize(); ++j) {
    std::string pattern_filter_json;
    if (!pattern_filter_str_list->GetString(j, &pattern_filter_json)) {
      NOTREACHED();
      continue;
    }

    std::unique_ptr<base::Value> value = base::JSONReader::Read(
        pattern_filter_json, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!value || !value->is_dict()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
                 " Invalid JSON object: " << pattern_filter_json;
      continue;
    }

    std::unique_ptr<base::DictionaryValue> pattern_filter_pair =
        base::DictionaryValue::From(std::move(value));
    base::Value* pattern = pattern_filter_pair->FindKey("pattern");
    base::Value* filter = pattern_filter_pair->FindKey("filter");
    if (!pattern || !filter) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
                 " Missing pattern or filter.";
      continue;
    }
    std::string pattern_str = pattern->GetString();

    if (filters_map.find(pattern_str) == filters_map.end())
      filters_map[pattern_str].SetKey("filters", base::ListValue());

    // Don't pass removed values from |value|, because base::Values read with
    // JSONReader use a shared string buffer. Instead, Clone() here.
    filters_map[pattern_str].FindKey("filters")->GetList().push_back(
        filter->Clone());
  }

  for (const auto& it : filters_map) {
    const std::string& pattern_str = it.first;
    const base::DictionaryValue& setting = it.second;

    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(pattern_str);
    // Ignore invalid patterns.
    if (!pattern.IsValid()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting:"
                 " Invalid content settings pattern: "
              << pattern.ToString();
      continue;
    }

    value_map->SetValue(pattern, ContentSettingsPattern::Wildcard(),
                        CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                        std::string(), base::Time(), setting.DeepCopy());
  }
}

void PolicyProvider::ReadManagedDefaultSettings() {
  for (const PrefsForManagedDefaultMapEntry& entry : kPrefsForManagedDefault)
    UpdateManagedDefaultSetting(entry);
}

void PolicyProvider::UpdateManagedDefaultSetting(
    const PrefsForManagedDefaultMapEntry& entry) {
  // Not all managed default types are registered on every platform. If they're
  // not registered, don't update them.
  const ContentSettingsInfo* info =
      ContentSettingsRegistry::GetInstance()->Get(entry.content_type);
  if (!info)
    return;

  // If a pref to manage a default-content-setting was not set (NOTICE:
  // "HasPrefPath" returns false if no value was set for a registered pref) then
  // the default value of the preference is used. The default value of a
  // preference to manage a default-content-settings is CONTENT_SETTING_DEFAULT.
  // This indicates that no managed value is set. If a pref was set, than it
  // MUST be managed.
  DCHECK(!prefs_->HasPrefPath(entry.pref_name) ||
         prefs_->IsManagedPreference(entry.pref_name));
  base::AutoLock auto_lock(lock_);
  int setting = prefs_->GetInteger(entry.pref_name);
  // TODO(wfh): Remove once HDB is enabled by default.
  if (entry.pref_name == prefs::kManagedDefaultPluginsSetting) {
    static constexpr base::Feature kIgnoreDefaultPluginsSetting = {
        "IgnoreDefaultPluginsSetting", base::FEATURE_DISABLED_BY_DEFAULT};
    if (base::FeatureList::IsEnabled(kIgnoreDefaultPluginsSetting))
      setting = CONTENT_SETTING_DEFAULT;
  }
  if (setting == CONTENT_SETTING_DEFAULT) {
    value_map_.DeleteValue(ContentSettingsPattern::Wildcard(),
                           ContentSettingsPattern::Wildcard(),
                           entry.content_type, std::string());
  } else if (info->IsSettingValid(IntToContentSetting(setting))) {
    // Don't set a timestamp for policy settings.
    value_map_.SetValue(ContentSettingsPattern::Wildcard(),
                        ContentSettingsPattern::Wildcard(), entry.content_type,
                        std::string(), base::Time(), new base::Value(setting));
  }
}


void PolicyProvider::ReadManagedContentSettings(bool overwrite) {
  base::AutoLock auto_lock(lock_);
  if (overwrite)
    value_map_.clear();
  GetContentSettingsFromPreferences(&value_map_);
  GetAutoSelectCertificateSettingsFromPreferences(&value_map_);
}

// Since the PolicyProvider is a read only content settings provider, all
// methodes of the ProviderInterface that set or delete any settings do nothing.
bool PolicyProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* value) {
  return false;
}

void PolicyProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

void PolicyProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  if (!prefs_)
    return;
  pref_change_registrar_.RemoveAll();
  prefs_ = nullptr;
}

void PolicyProvider::OnPreferenceChanged(const std::string& name) {
  DCHECK(CalledOnValidThread());

  for (const PrefsForManagedDefaultMapEntry& entry : kPrefsForManagedDefault) {
    if (entry.pref_name == name)
      UpdateManagedDefaultSetting(entry);
  }

  if (name == prefs::kManagedAutoSelectCertificateForUrls ||
      name == prefs::kManagedCookiesAllowedForUrls ||
      name == prefs::kManagedCookiesBlockedForUrls ||
      name == prefs::kManagedCookiesSessionOnlyForUrls ||
      name == prefs::kManagedImagesAllowedForUrls ||
      name == prefs::kManagedImagesBlockedForUrls ||
      name == prefs::kManagedJavaScriptAllowedForUrls ||
      name == prefs::kManagedJavaScriptBlockedForUrls ||
      name == prefs::kManagedNotificationsAllowedForUrls ||
      name == prefs::kManagedNotificationsBlockedForUrls ||
      name == prefs::kManagedPluginsAllowedForUrls ||
      name == prefs::kManagedPluginsBlockedForUrls ||
      name == prefs::kManagedPopupsAllowedForUrls ||
      name == prefs::kManagedPopupsBlockedForUrls) {
    ReadManagedContentSettings(true);
    ReadManagedDefaultSettings();
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_DEFAULT,
                  std::string());
}

}  // namespace content_settings

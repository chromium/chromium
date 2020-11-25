// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
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
        {prefs::kManagedCookiesAllowedForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedCookiesBlockedForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedCookiesSessionOnlyForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_SESSION_ONLY},
        {prefs::kManagedImagesAllowedForUrls, ContentSettingsType::IMAGES,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedImagesBlockedForUrls, ContentSettingsType::IMAGES,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedInsecureContentAllowedForUrls,
         ContentSettingsType::MIXEDSCRIPT, CONTENT_SETTING_ALLOW},
        {prefs::kManagedInsecureContentBlockedForUrls,
         ContentSettingsType::MIXEDSCRIPT, CONTENT_SETTING_BLOCK},
        {prefs::kManagedJavaScriptAllowedForUrls,
         ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_ALLOW},
        {prefs::kManagedJavaScriptBlockedForUrls,
         ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK},
        {prefs::kManagedNotificationsAllowedForUrls,
         ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW},
        {prefs::kManagedNotificationsBlockedForUrls,
         ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK},
        {prefs::kManagedPopupsAllowedForUrls, ContentSettingsType::POPUPS,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedPopupsBlockedForUrls, ContentSettingsType::POPUPS,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedWebUsbAskForUrls, ContentSettingsType::USB_GUARD,
         CONTENT_SETTING_ASK},
        {prefs::kManagedWebUsbBlockedForUrls, ContentSettingsType::USB_GUARD,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedFileSystemReadAskForUrls,
         ContentSettingsType::FILE_SYSTEM_READ_GUARD, CONTENT_SETTING_ASK},
        {prefs::kManagedFileSystemReadBlockedForUrls,
         ContentSettingsType::FILE_SYSTEM_READ_GUARD, CONTENT_SETTING_BLOCK},
        {prefs::kManagedFileSystemWriteAskForUrls,
         ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, CONTENT_SETTING_ASK},
        {prefs::kManagedFileSystemWriteBlockedForUrls,
         ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, CONTENT_SETTING_BLOCK},
        {prefs::kManagedLegacyCookieAccessAllowedForDomains,
         ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW},
        {prefs::kManagedSerialAskForUrls, ContentSettingsType::SERIAL_GUARD,
         CONTENT_SETTING_ASK},
        {prefs::kManagedSerialBlockedForUrls, ContentSettingsType::SERIAL_GUARD,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedSensorsAllowedForUrls, ContentSettingsType::SENSORS,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedSensorsBlockedForUrls, ContentSettingsType::SENSORS,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedInsecurePrivateNetworkAllowedForUrls,
         ContentSettingsType::INSECURE_PRIVATE_NETWORK, CONTENT_SETTING_ALLOW},
};

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
        {ContentSettingsType::ADS, prefs::kManagedDefaultAdsSetting},
        {ContentSettingsType::COOKIES, prefs::kManagedDefaultCookiesSetting},
        {ContentSettingsType::IMAGES, prefs::kManagedDefaultImagesSetting},
        {ContentSettingsType::GEOLOCATION,
         prefs::kManagedDefaultGeolocationSetting},
        {ContentSettingsType::JAVASCRIPT,
         prefs::kManagedDefaultJavaScriptSetting},
        {ContentSettingsType::MEDIASTREAM_CAMERA,
         prefs::kManagedDefaultMediaStreamSetting},
        {ContentSettingsType::MEDIASTREAM_MIC,
         prefs::kManagedDefaultMediaStreamSetting},
        {ContentSettingsType::MIXEDSCRIPT,
         prefs::kManagedDefaultInsecureContentSetting},
        {ContentSettingsType::NOTIFICATIONS,
         prefs::kManagedDefaultNotificationsSetting},
        {ContentSettingsType::POPUPS, prefs::kManagedDefaultPopupsSetting},
        {ContentSettingsType::BLUETOOTH_GUARD,
         prefs::kManagedDefaultWebBluetoothGuardSetting},
        {ContentSettingsType::USB_GUARD,
         prefs::kManagedDefaultWebUsbGuardSetting},
        {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
         prefs::kManagedDefaultFileSystemReadGuardSetting},
        {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
         prefs::kManagedDefaultFileSystemWriteGuardSetting},
        {ContentSettingsType::LEGACY_COOKIE_ACCESS,
         prefs::kManagedDefaultLegacyCookieAccessSetting},
        {ContentSettingsType::SERIAL_GUARD,
         prefs::kManagedDefaultSerialGuardSetting},
        {ContentSettingsType::SENSORS, prefs::kManagedDefaultSensorsSetting},
        {ContentSettingsType::INSECURE_PRIVATE_NETWORK,
         prefs::kManagedDefaultInsecurePrivateNetworkSetting},
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
  registry->RegisterListPref(prefs::kManagedInsecureContentAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedInsecureContentBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedJavaScriptAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedJavaScriptBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedNotificationsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedNotificationsBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedPopupsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedPopupsBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbAllowDevicesForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbAskForUrls);
  registry->RegisterListPref(prefs::kManagedWebUsbBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedFileSystemReadAskForUrls);
  registry->RegisterListPref(prefs::kManagedFileSystemReadBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedFileSystemWriteAskForUrls);
  registry->RegisterListPref(prefs::kManagedFileSystemWriteBlockedForUrls);
  registry->RegisterListPref(
      prefs::kManagedLegacyCookieAccessAllowedForDomains);
  registry->RegisterListPref(prefs::kManagedSerialAskForUrls);
  registry->RegisterListPref(prefs::kManagedSerialBlockedForUrls);
  registry->RegisterListPref(prefs::kManagedSensorsAllowedForUrls);
  registry->RegisterListPref(prefs::kManagedSensorsBlockedForUrls);
  registry->RegisterListPref(
      prefs::kManagedInsecurePrivateNetworkAllowedForUrls);

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
  registry->RegisterIntegerPref(prefs::kManagedDefaultInsecureContentSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultJavaScriptSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultNotificationsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultMediaStreamSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultPopupsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultWebBluetoothGuardSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultWebUsbGuardSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(
      prefs::kManagedDefaultFileSystemReadGuardSetting,
      CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(
      prefs::kManagedDefaultFileSystemWriteGuardSetting,
      CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultLegacyCookieAccessSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultSerialGuardSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(prefs::kManagedDefaultSensorsSetting,
                                CONTENT_SETTING_DEFAULT);
  registry->RegisterIntegerPref(
      prefs::kManagedDefaultInsecurePrivateNetworkSetting,
      CONTENT_SETTING_DEFAULT);
}

PolicyProvider::PolicyProvider(PrefService* prefs) : prefs_(prefs) {
  ReadManagedDefaultSettings();
  ReadManagedContentSettings(false);

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &PolicyProvider::OnPreferenceChanged, base::Unretained(this));
  pref_change_registrar_.Add(
      prefs::kManagedAutoSelectCertificateForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedCookiesAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedCookiesBlockedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedCookiesSessionOnlyForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedImagesAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedImagesBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedInsecureContentAllowedForUrls,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedInsecureContentBlockedForUrls,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedJavaScriptBlockedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedNotificationsAllowedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedNotificationsBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPopupsAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedPopupsBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedWebUsbAskForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedWebUsbBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedFileSystemReadAskForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedFileSystemReadBlockedForUrls,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedFileSystemWriteAskForUrls,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedFileSystemWriteBlockedForUrls,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedLegacyCookieAccessAllowedForDomains,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedSerialAskForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedSerialBlockedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedSensorsAllowedForUrls, callback);
  pref_change_registrar_.Add(prefs::kManagedSensorsBlockedForUrls, callback);
  pref_change_registrar_.Add(
      prefs::kManagedInsecurePrivateNetworkAllowedForUrls, callback);

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
  pref_change_registrar_.Add(prefs::kManagedDefaultInsecureContentSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultJavaScriptSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultNotificationsSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultMediaStreamSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultPopupsSetting, callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultWebBluetoothGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultWebUsbGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultFileSystemReadGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultLegacyCookieAccessSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultSerialGuardSetting,
                             callback);
  pref_change_registrar_.Add(prefs::kManagedDefaultSensorsSetting, callback);
  pref_change_registrar_.Add(
      prefs::kManagedDefaultInsecurePrivateNetworkSetting, callback);
}

PolicyProvider::~PolicyProvider() {
  DCHECK(!prefs_);
}

std::unique_ptr<RuleIterator> PolicyProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, &lock_);
}

void PolicyProvider::GetContentSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  for (size_t i = 0; i < base::size(kPrefsForManagedContentSettingsMap); ++i) {
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
      DCHECK_NE(content_type, ContentSettingsType::AUTO_SELECT_CERTIFICATE);
      // If only one pattern was defined auto expand it to a pattern pair.
      ContentSettingsPattern secondary_pattern =
          !pattern_pair.second.IsValid() ? ContentSettingsPattern::Wildcard()
                                         : pattern_pair.second;
      VLOG_IF(2, !pattern_pair.second.IsValid())
          << "Replacing invalid secondary pattern '"
          << pattern_pair.second.ToString() << "' with wildcard";

      // All settings that can set pattern pairs support embedded exceptions.
      if (pattern_pair.first != pattern_pair.second &&
          pattern_pair.second != ContentSettingsPattern::Wildcard() &&
          !content_settings::WebsiteSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->SupportsSecondaryPattern()) {
        continue;
      }

      // Don't set a timestamp for policy settings.
      value_map->SetValue(
          pattern_pair.first, secondary_pattern, content_type, base::Time(),
          base::Value(kPrefsForManagedContentSettingsMap[i].setting), {});
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

    std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
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
    filters_map[pattern_str].FindKey("filters")->Append(filter->Clone());
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
                        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
                        base::Time(), setting.Clone(), {});
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
  if (setting == CONTENT_SETTING_DEFAULT) {
    value_map_.DeleteValue(ContentSettingsPattern::Wildcard(),
                           ContentSettingsPattern::Wildcard(),
                           entry.content_type);
  } else if (info->IsSettingValid(IntToContentSetting(setting))) {
    // Don't set a timestamp for policy settings.
    value_map_.SetValue(ContentSettingsPattern::Wildcard(),
                        ContentSettingsPattern::Wildcard(), entry.content_type,
                        base::Time(), base::Value(setting), {});
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
    std::unique_ptr<base::Value>&& value,
    const ContentSettingConstraints& constraints) {
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
      name == prefs::kManagedFileSystemReadAskForUrls ||
      name == prefs::kManagedFileSystemReadBlockedForUrls ||
      name == prefs::kManagedFileSystemWriteAskForUrls ||
      name == prefs::kManagedFileSystemWriteBlockedForUrls ||
      name == prefs::kManagedImagesAllowedForUrls ||
      name == prefs::kManagedImagesBlockedForUrls ||
      name == prefs::kManagedInsecureContentAllowedForUrls ||
      name == prefs::kManagedInsecureContentBlockedForUrls ||
      name == prefs::kManagedJavaScriptAllowedForUrls ||
      name == prefs::kManagedJavaScriptBlockedForUrls ||
      name == prefs::kManagedNotificationsAllowedForUrls ||
      name == prefs::kManagedNotificationsBlockedForUrls ||
      name == prefs::kManagedPopupsAllowedForUrls ||
      name == prefs::kManagedPopupsBlockedForUrls ||
      name == prefs::kManagedWebUsbAskForUrls ||
      name == prefs::kManagedWebUsbBlockedForUrls ||
      name == prefs::kManagedLegacyCookieAccessAllowedForDomains ||
      name == prefs::kManagedSerialAskForUrls ||
      name == prefs::kManagedSerialBlockedForUrls ||
      name == prefs::kManagedSensorsAllowedForUrls ||
      name == prefs::kManagedSensorsBlockedForUrls ||
      name == prefs::kManagedInsecurePrivateNetworkAllowedForUrls) {
    ReadManagedContentSettings(true);
    ReadManagedDefaultSettings();
  }

  NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                  ContentSettingsType::DEFAULT);
}

}  // namespace content_settings

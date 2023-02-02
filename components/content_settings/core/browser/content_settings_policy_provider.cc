// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <stddef.h>

#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
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

constexpr PrefsForManagedContentSettingsMapEntry
    kPrefsForManagedContentSettingsMap[] = {
        {prefs::kManagedCookiesAllowedForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_ALLOW},
        {prefs::kManagedCookiesBlockedForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedCookiesSessionOnlyForUrls, ContentSettingsType::COOKIES,
         CONTENT_SETTING_SESSION_ONLY},
        {prefs::kManagedGetDisplayMediaSetSelectAllScreensAllowedForUrls,
         ContentSettingsType::GET_DISPLAY_MEDIA_SET_SELECT_ALL_SCREENS,
         CONTENT_SETTING_ALLOW},
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
        {prefs::kManagedClipboardAllowedForUrls,
         ContentSettingsType::CLIPBOARD_READ_WRITE, CONTENT_SETTING_ALLOW},
        {prefs::kManagedClipboardBlockedForUrls,
         ContentSettingsType::CLIPBOARD_READ_WRITE, CONTENT_SETTING_BLOCK},
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
        {prefs::kManagedJavaScriptJitAllowedForSites,
         ContentSettingsType::JAVASCRIPT_JIT, CONTENT_SETTING_ALLOW},
        {prefs::kManagedJavaScriptJitBlockedForSites,
         ContentSettingsType::JAVASCRIPT_JIT, CONTENT_SETTING_BLOCK},
        {prefs::kManagedWebHidAskForUrls, ContentSettingsType::HID_GUARD,
         CONTENT_SETTING_ASK},
        {prefs::kManagedWebHidBlockedForUrls, ContentSettingsType::HID_GUARD,
         CONTENT_SETTING_BLOCK},
        {prefs::kManagedWindowManagementAllowedForUrls,
         ContentSettingsType::WINDOW_MANAGEMENT, CONTENT_SETTING_ALLOW},
        {prefs::kManagedWindowManagementBlockedForUrls,
         ContentSettingsType::WINDOW_MANAGEMENT, CONTENT_SETTING_BLOCK},
        {prefs::kManagedLocalFontsAllowedForUrls,
         ContentSettingsType::LOCAL_FONTS, CONTENT_SETTING_ALLOW},
        {prefs::kManagedLocalFontsBlockedForUrls,
         ContentSettingsType::LOCAL_FONTS, CONTENT_SETTING_BLOCK},
};

constexpr const char* kManagedPrefs[] = {
    prefs::kManagedAutoSelectCertificateForUrls,
    prefs::kManagedClipboardAllowedForUrls,
    prefs::kManagedClipboardBlockedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    prefs::kManagedFileSystemReadAskForUrls,
    prefs::kManagedFileSystemReadBlockedForUrls,
    prefs::kManagedFileSystemWriteAskForUrls,
    prefs::kManagedFileSystemWriteBlockedForUrls,
    prefs::kManagedGetDisplayMediaSetSelectAllScreensAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    prefs::kManagedInsecureContentAllowedForUrls,
    prefs::kManagedInsecureContentBlockedForUrls,
    prefs::kManagedInsecurePrivateNetworkAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptJitAllowedForSites,
    prefs::kManagedJavaScriptJitBlockedForSites,
    prefs::kManagedLegacyCookieAccessAllowedForDomains,
    prefs::kManagedNotificationsAllowedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    prefs::kManagedSensorsAllowedForUrls,
    prefs::kManagedSensorsBlockedForUrls,
    prefs::kManagedSerialAskForUrls,
    prefs::kManagedSerialBlockedForUrls,
    prefs::kManagedWebHidAskForUrls,
    prefs::kManagedWebHidBlockedForUrls,
    prefs::kManagedWebUsbAllowDevicesForUrls,
    prefs::kManagedWebUsbAskForUrls,
    prefs::kManagedWebUsbBlockedForUrls,
    prefs::kManagedWindowManagementAllowedForUrls,
    prefs::kManagedWindowManagementBlockedForUrls,
    prefs::kManagedLocalFontsAllowedForUrls,
    prefs::kManagedLocalFontsBlockedForUrls,
};

// The following preferences are only used to indicate if a default content
// setting is managed and to hold the managed default setting value. If the
// value for any of the following preferences is set then the corresponding
// default content setting is managed. These preferences exist in parallel to
// the preference default content settings. If a default content settings type
// is managed any user defined exceptions (patterns) for this type are ignored.
constexpr const char* kManagedDefaultPrefs[] = {
    prefs::kManagedDefaultAdsSetting,
    prefs::kManagedDefaultClipboardSetting,
    prefs::kManagedDefaultCookiesSetting,
    prefs::kManagedDefaultFileSystemReadGuardSetting,
    prefs::kManagedDefaultFileSystemWriteGuardSetting,
    prefs::kManagedDefaultGeolocationSetting,
    prefs::kManagedDefaultImagesSetting,
    prefs::kManagedDefaultInsecureContentSetting,
    prefs::kManagedDefaultInsecurePrivateNetworkSetting,
    prefs::kManagedDefaultJavaScriptSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    prefs::kManagedDefaultNotificationsSetting,
    prefs::kManagedDefaultPopupsSetting,
    prefs::kManagedDefaultSensorsSetting,
    prefs::kManagedDefaultSerialGuardSetting,
    prefs::kManagedDefaultWebBluetoothGuardSetting,
    prefs::kManagedDefaultWebUsbGuardSetting,
    prefs::kManagedDefaultJavaScriptJitSetting,
    prefs::kManagedDefaultWebHidGuardSetting,
    prefs::kManagedDefaultWindowManagementSetting,
    prefs::kManagedDefaultLocalFontsSetting,
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
        {ContentSettingsType::CLIPBOARD_READ_WRITE,
         prefs::kManagedDefaultClipboardSetting},
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
        {ContentSettingsType::SERIAL_GUARD,
         prefs::kManagedDefaultSerialGuardSetting},
        {ContentSettingsType::SENSORS, prefs::kManagedDefaultSensorsSetting},
        {ContentSettingsType::INSECURE_PRIVATE_NETWORK,
         prefs::kManagedDefaultInsecurePrivateNetworkSetting},
        {ContentSettingsType::JAVASCRIPT_JIT,
         prefs::kManagedDefaultJavaScriptJitSetting},
        {ContentSettingsType::HID_GUARD,
         prefs::kManagedDefaultWebHidGuardSetting},
        {ContentSettingsType::WINDOW_MANAGEMENT,
         prefs::kManagedDefaultWindowManagementSetting},
        {ContentSettingsType::LOCAL_FONTS,
         prefs::kManagedDefaultLocalFontsSetting},
};

// static
void PolicyProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  for (const char* pref : kManagedPrefs)
    registry->RegisterListPref(pref);

  // Preferences for default content setting policies. If a policy is not set of
  // the corresponding preferences below is set to CONTENT_SETTING_DEFAULT.
  for (const char* pref : kManagedDefaultPrefs)
    registry->RegisterIntegerPref(pref, CONTENT_SETTING_DEFAULT);
}

PolicyProvider::PolicyProvider(PrefService* prefs) : prefs_(prefs) {
  ReadManagedDefaultSettings();
  ReadManagedContentSettings(false);

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &PolicyProvider::OnPreferenceChanged, base::Unretained(this));
  for (const char* pref : kManagedPrefs)
    pref_change_registrar_.Add(pref, callback);

  for (const char* pref : kManagedDefaultPrefs)
    pref_change_registrar_.Add(pref, callback);
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
  for (const auto& entry : kPrefsForManagedContentSettingsMap) {
    // Skip unset policies.
    if (!prefs_->HasPrefPath(entry.pref_name)) {
      VLOG(2) << "Skipping unset preference: " << entry.pref_name;
      continue;
    }

    const PrefService::Preference* pref =
        prefs_->FindPreference(entry.pref_name);
    DCHECK(pref);
    DCHECK(!pref->HasUserSetting());
    DCHECK(!pref->HasExtensionSetting());

    if (!pref->GetValue()->is_list()) {
      NOTREACHED() << "Could not read patterns from " << entry.pref_name;
      return;
    }

    const base::Value::List& pattern_str_list = pref->GetValue()->GetList();
    for (size_t i = 0; i < pattern_str_list.size(); ++i) {
      if (!pattern_str_list[i].is_string()) {
        NOTREACHED() << "Could not read content settings pattern #" << i
                     << " from " << entry.pref_name;
        continue;
      }

      const std::string& original_pattern_str = pattern_str_list[i].GetString();
      VLOG(2) << "Reading content settings pattern " << original_pattern_str
              << " from " << entry.pref_name;

      PatternPair pattern_pair = ParsePatternString(original_pattern_str);
      // Ignore invalid patterns.
      if (!pattern_pair.first.IsValid()) {
        VLOG(1) << "Ignoring invalid content settings pattern "
                << original_pattern_str;
        continue;
      }

      DCHECK_NE(entry.content_type,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE);
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
          !WebsiteSettingsRegistry::GetInstance()
               ->Get(entry.content_type)
               ->SupportsSecondaryPattern()) {
        continue;
      }

      // Don't set a timestamp for policy settings.
      value_map->SetValue(pattern_pair.first, secondary_pattern,
                          entry.content_type, base::Value(entry.setting), {});
    }
  }
}

void PolicyProvider::GetAutoSelectCertificateSettingsFromPreferences(
    OriginIdentifierValueMap* value_map) {
  constexpr const char* pref_name = prefs::kManagedAutoSelectCertificateForUrls;
  if (!prefs_->HasPrefPath(pref_name)) {
    VLOG(2) << "Skipping unset preference: " << pref_name;
    return;
  }

  const PrefService::Preference* pref = prefs_->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(!pref->HasUserSetting());
  DCHECK(!pref->HasExtensionSetting());

  if (!pref->GetValue()->is_list()) {
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
  std::unordered_map<std::string, base::Value::Dict> filters_map;
  for (const auto& pattern_filter_str : pref->GetValue()->GetList()) {
    if (!pattern_filter_str.is_string()) {
      NOTREACHED();
      continue;
    }

    absl::optional<base::Value> pattern_filter = base::JSONReader::Read(
        pattern_filter_str.GetString(), base::JSON_ALLOW_TRAILING_COMMAS);
    if (!pattern_filter || !pattern_filter->is_dict()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
              << " Invalid JSON object: " << pattern_filter_str.GetString();
      continue;
    }

    const base::Value::Dict& pattern_filter_dict = pattern_filter->GetDict();
    const std::string* pattern = pattern_filter_dict.FindString("pattern");
    const base::Value* filter = pattern_filter_dict.Find("filter");
    if (!pattern || !filter) {
      VLOG(1) << "Ignoring invalid certificate auto select setting. Reason:"
              << " Missing pattern or filter.";
      continue;
    }

    const std::string& pattern_str = *pattern;
    // This adds a `pattern_str` entry to `filters_map` if not already present,
    // and gets a pointer to its `filters` list, inserting an entry into the
    // dictionary if needed.
    base::Value::List* filter_list =
        filters_map[pattern_str].EnsureList("filters");

    // Don't pass removed values from `pattern_filter`, because base::Values
    // read with JSONReader use a shared string buffer. Instead, Clone() here.
    filter_list->Append(filter->Clone());
  }

  for (const auto& it : filters_map) {
    const std::string& pattern_str = it.first;
    const base::Value::Dict& setting = it.second;

    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(pattern_str);
    // Ignore invalid patterns.
    if (!pattern.IsValid()) {
      VLOG(1) << "Ignoring invalid certificate auto select setting:"
              << " Invalid content settings pattern: " << pattern.ToString();
      continue;
    }

    value_map->SetValue(pattern, ContentSettingsPattern::Wildcard(),
                        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
                        base::Value(setting.Clone()), {});
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
                        base::Value(setting), {});
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
// methods of the ProviderInterface that set or delete any settings do nothing.
bool PolicyProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const ContentSettingConstraints& constraints) {
  return false;
}

void PolicyProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {}

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

  if (base::Contains(kManagedPrefs, name)) {
    ReadManagedContentSettings(true);
    ReadManagedDefaultSettings();
  }

  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsType::DEFAULT);
}

}  // namespace content_settings

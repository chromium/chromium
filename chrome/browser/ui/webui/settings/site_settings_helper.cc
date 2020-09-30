// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/chooser_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace site_settings {

constexpr char kAppName[] = "appName";
constexpr char kAppId[] = "appId";

namespace {

// Maps from the UI string to the object it represents (for sorting purposes).
typedef std::multimap<std::string, const base::DictionaryValue*> SortedObjects;

// Maps from a secondary URL to the set of objects it has permission to access.
typedef std::map<GURL, SortedObjects> OneOriginObjects;

// Maps from a primary URL/source pair to a OneOriginObjects. All the mappings
// in OneOriginObjects share the given primary URL and source.
typedef std::map<std::pair<GURL, std::string>, OneOriginObjects>
    AllOriginObjects;

// Chooser data group names.
const char kUsbChooserDataGroupType[] = "usb-devices-data";
const char kSerialChooserDataGroupType[] = "serial-ports-data";
const char kHidChooserDataGroupType[] = "hid-devices-data";
const char kBluetoothChooserDataGroupType[] = "bluetooth-devices-data";

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
    // The following ContentSettingsTypes have UI in Content Settings
    // and require a mapping from their Javascript string representation in
    // chrome/browser/resources/settings/site_settings/constants.js to their C++
    // ContentSettingsType provided here.
    {ContentSettingsType::COOKIES, "cookies"},
    {ContentSettingsType::IMAGES, "images"},
    {ContentSettingsType::JAVASCRIPT, "javascript"},
    {ContentSettingsType::PLUGINS, "plugins"},
    {ContentSettingsType::POPUPS, "popups"},
    {ContentSettingsType::GEOLOCATION, "location"},
    {ContentSettingsType::NOTIFICATIONS, "notifications"},
    {ContentSettingsType::MEDIASTREAM_MIC, "media-stream-mic"},
    {ContentSettingsType::MEDIASTREAM_CAMERA, "media-stream-camera"},
    {ContentSettingsType::PROTOCOL_HANDLERS, "register-protocol-handler"},
    {ContentSettingsType::PPAPI_BROKER, "ppapi-broker"},
    {ContentSettingsType::AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
    {ContentSettingsType::MIDI_SYSEX, "midi-sysex"},
    {ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER, "protected-content"},
    {ContentSettingsType::BACKGROUND_SYNC, "background-sync"},
    {ContentSettingsType::ADS, "ads"},
    {ContentSettingsType::SOUND, "sound"},
    {ContentSettingsType::CLIPBOARD_READ_WRITE, "clipboard"},
    {ContentSettingsType::SENSORS, "sensors"},
    {ContentSettingsType::PAYMENT_HANDLER, "payment-handler"},
    {ContentSettingsType::USB_GUARD, "usb-devices"},
    {ContentSettingsType::USB_CHOOSER_DATA, kUsbChooserDataGroupType},
    {ContentSettingsType::IDLE_DETECTION, "idle-detection"},
    {ContentSettingsType::SERIAL_GUARD, "serial-ports"},
    {ContentSettingsType::SERIAL_CHOOSER_DATA, kSerialChooserDataGroupType},
    {ContentSettingsType::BLUETOOTH_SCANNING, "bluetooth-scanning"},
    {ContentSettingsType::HID_GUARD, "hid-devices"},
    {ContentSettingsType::HID_CHOOSER_DATA, kHidChooserDataGroupType},
    {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, "file-system-write"},
    {ContentSettingsType::MIXEDSCRIPT, "mixed-script"},
    {ContentSettingsType::VR, "vr"},
    {ContentSettingsType::AR, "ar"},
    {ContentSettingsType::BLUETOOTH_GUARD, "bluetooth-devices"},
    {ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
     kBluetoothChooserDataGroupType},
    {ContentSettingsType::WINDOW_PLACEMENT, "window-placement"},
    {ContentSettingsType::FONT_ACCESS, "font-access"},

    // Add new content settings here if a corresponding Javascript string
    // representation for it is not required. Note some exceptions do have UI in
    // Content Settings but do not require a separate string.
    {ContentSettingsType::DEFAULT, nullptr},
    {ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr},
    {ContentSettingsType::SSL_CERT_DECISIONS, nullptr},
    {ContentSettingsType::APP_BANNER, nullptr},
    {ContentSettingsType::SITE_ENGAGEMENT, nullptr},
    {ContentSettingsType::DURABLE_STORAGE, nullptr},
    {ContentSettingsType::AUTOPLAY, nullptr},
    {ContentSettingsType::IMPORTANT_SITE_INFO, nullptr},
    {ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA, nullptr},
    {ContentSettingsType::ADS_DATA, nullptr},
    {ContentSettingsType::MIDI, nullptr},
    {ContentSettingsType::PASSWORD_PROTECTION, nullptr},
    {ContentSettingsType::MEDIA_ENGAGEMENT, nullptr},
    {ContentSettingsType::CLIENT_HINTS, nullptr},
    {ContentSettingsType::ACCESSIBILITY_EVENTS, nullptr},
    {ContentSettingsType::CLIPBOARD_SANITIZED_WRITE, nullptr},
    {ContentSettingsType::PLUGINS_DATA, nullptr},
    {ContentSettingsType::BACKGROUND_FETCH, nullptr},
    {ContentSettingsType::INTENT_PICKER_DISPLAY, nullptr},
    {ContentSettingsType::PERIODIC_BACKGROUND_SYNC, nullptr},
    {ContentSettingsType::WAKE_LOCK_SCREEN, nullptr},
    {ContentSettingsType::WAKE_LOCK_SYSTEM, nullptr},
    {ContentSettingsType::LEGACY_COOKIE_ACCESS, nullptr},
    {ContentSettingsType::INSTALLED_WEB_APP_METADATA, nullptr},
    {ContentSettingsType::NFC, nullptr},
    {ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA, nullptr},
    {ContentSettingsType::FILE_SYSTEM_READ_GUARD, nullptr},
    {ContentSettingsType::STORAGE_ACCESS, nullptr},
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM, nullptr},
    {ContentSettingsType::INSECURE_PRIVATE_NETWORK, nullptr},
    {ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA, nullptr},
};
static_assert(base::size(kContentSettingsTypeGroupNames) ==
                  // ContentSettingsType starts at -1, so add 1 here.
                  static_cast<int32_t>(ContentSettingsType::NUM_TYPES) + 1,
              "kContentSettingsTypeGroupNames should have "
              "CONTENT_SETTINGS_NUM_TYPES elements");

struct SiteSettingSourceStringMapping {
  SiteSettingSource source;
  const char* source_str;
};

const SiteSettingSourceStringMapping kSiteSettingSourceStringMapping[] = {
    {SiteSettingSource::kAllowlist, "allowlist"},
    {SiteSettingSource::kAdsFilterBlocklist, "ads-filter-blacklist"},
    {SiteSettingSource::kDefault, "default"},
    {SiteSettingSource::kDrmDisabled, "drm-disabled"},
    {SiteSettingSource::kEmbargo, "embargo"},
    {SiteSettingSource::kExtension, "extension"},
    {SiteSettingSource::kInsecureOrigin, "insecure-origin"},
    {SiteSettingSource::kKillSwitch, "kill-switch"},
    {SiteSettingSource::kPolicy, "policy"},
    {SiteSettingSource::kPreference, "preference"},
};
static_assert(base::size(kSiteSettingSourceStringMapping) ==
                  static_cast<int>(SiteSettingSource::kNumSources),
              "kSiteSettingSourceStringMapping should have "
              "SiteSettingSource::kNumSources elements");

struct PolicyIndicatorTypeStringMapping {
  PolicyIndicatorType source;
  const char* indicator_str;
};

// Converts a policy indicator type to its JS usable string representation.
const PolicyIndicatorTypeStringMapping kPolicyIndicatorTypeStringMapping[] = {
    {PolicyIndicatorType::kDevicePolicy, "devicePolicy"},
    {PolicyIndicatorType::kExtension, "extension"},
    {PolicyIndicatorType::kNone, "none"},
    {PolicyIndicatorType::kOwner, "owner"},
    {PolicyIndicatorType::kPrimaryUser, "primary_user"},
    {PolicyIndicatorType::kRecommended, "recommended"},
    {PolicyIndicatorType::kUserPolicy, "userPolicy"},
    {PolicyIndicatorType::kParent, "parent"},
    {PolicyIndicatorType::kChildRestriction, "childRestriction"},
};
static_assert(base::size(kPolicyIndicatorTypeStringMapping) ==
                  static_cast<int>(PolicyIndicatorType::kNumIndicators),
              "kPolicyIndicatorStringMapping should have "
              "PolicyIndicatorType::kNumIndicators elements");

// Retrieves the corresponding string, according to the following precedence
// order from highest to lowest priority:
//    1. Allowlisted WebUI content setting.
//    2. Kill-switch.
//    3. Insecure origins (some permissions are denied to insecure origins).
//    4. Enterprise policy.
//    5. Extensions.
//    6. Activated for ads filtering (for Ads ContentSettingsType only).
//    7. DRM disabled (for CrOS's Protected Content ContentSettingsType only).
//    8. User-set per-origin setting.
//    9. Embargo.
//   10. User-set patterns.
//   11. User-set global default for a ContentSettingsType.
//   12. Chrome's built-in default.
SiteSettingSource CalculateSiteSettingSource(
    Profile* profile,
    const ContentSettingsType content_type,
    const GURL& origin,
    const content_settings::SettingInfo& info,
    const permissions::PermissionResult result) {
  if (info.source == content_settings::SETTING_SOURCE_ALLOWLIST)
    return SiteSettingSource::kAllowlist;  // Source #1.

  if (result.source == permissions::PermissionStatusSource::KILL_SWITCH)
    return SiteSettingSource::kKillSwitch;  // Source #2.

  if (result.source == permissions::PermissionStatusSource::INSECURE_ORIGIN)
    return SiteSettingSource::kInsecureOrigin;  // Source #3.

  if (info.source == content_settings::SETTING_SOURCE_POLICY ||
      info.source == content_settings::SETTING_SOURCE_SUPERVISED) {
    return SiteSettingSource::kPolicy;  // Source #4.
  }

  if (info.source == content_settings::SETTING_SOURCE_EXTENSION)
    return SiteSettingSource::kExtension;  // Source #5.

  if (content_type == ContentSettingsType::ADS &&
      base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter)) {
    SubresourceFilterContentSettingsManager* settings_manager =
        SubresourceFilterProfileContextFactory::GetForProfile(profile)
            ->settings_manager();

    if (settings_manager->GetSiteActivationFromMetadata(origin)) {
      return SiteSettingSource::kAdsFilterBlocklist;  // Source #6.
    }
  }

  // Protected Content will be blocked if the |kEnableDRM| pref is off.
  if (content_type == ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER &&
      !profile->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    return SiteSettingSource::kDrmDisabled;  // Source #7.
  }

  DCHECK_NE(content_settings::SETTING_SOURCE_NONE, info.source);
  if (info.source == content_settings::SETTING_SOURCE_USER) {
    if (result.source ==
            permissions::PermissionStatusSource::MULTIPLE_DISMISSALS ||
        result.source ==
            permissions::PermissionStatusSource::MULTIPLE_IGNORES) {
      return SiteSettingSource::kEmbargo;  // Source #9.
    }
    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return SiteSettingSource::kDefault;  // Source #11, #12.
    }

    // Source #8, #10. When #8 is the source, |result.source| won't be set to
    // any of the source #8 enum values, as PermissionManager is aware of the
    // difference between these two sources internally. The subtlety here should
    // go away when PermissionManager can handle all content settings and all
    // possible sources.
    return SiteSettingSource::kPreference;
  }

  NOTREACHED();
  return SiteSettingSource::kPreference;
}

// Whether |pattern| applies to a single origin.
bool PatternAppliesToSingleOrigin(const ContentSettingPatternSource& pattern) {
  const GURL url(pattern.primary_pattern.ToString());
  // Default settings and other patterns apply to multiple origins.
  if (url::Origin::Create(url).opaque())
    return false;
  // Embedded content settings only match when |url| is embedded in another
  // origin, so ignore non-wildcard secondary patterns.
  if (pattern.secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return false;
  }
  return true;
}

bool PatternAppliesToWebUISchemes(const ContentSettingPatternSource& pattern) {
  return pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_CHROME ||
         pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_CHROMEUNTRUSTED ||
         pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_DEVTOOLS;
}

// Retrieves the source of a chooser exception as a string. This method uses the
// CalculateSiteSettingSource method above to calculate the correct string to
// use.
std::string GetSourceStringForChooserException(
    Profile* profile,
    ContentSettingsType content_type,
    content_settings::SettingSource source) {
  // Prepare the parameters needed by CalculateSiteSettingSource
  content_settings::SettingInfo info;
  info.source = source;

  // Chooser exceptions do not use a PermissionContextBase for their
  // permissions.
  permissions::PermissionResult permission_result(
      CONTENT_SETTING_DEFAULT,
      permissions::PermissionStatusSource::UNSPECIFIED);

  // The |origin| parameter is only used for |ContentSettingsType::ADS| with
  // the |kSafeBrowsingSubresourceFilter| feature flag enabled, so an empty GURL
  // is used.
  SiteSettingSource calculated_source = CalculateSiteSettingSource(
      profile, content_type, /*origin=*/GURL::EmptyGURL(), info,
      permission_result);
  DCHECK(calculated_source == SiteSettingSource::kPolicy ||
         calculated_source == SiteSettingSource::kPreference);
  return SiteSettingSourceToString(calculated_source);
}

permissions::ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

permissions::ChooserContextBase* GetSerialChooserContext(Profile* profile) {
  return SerialChooserContextFactory::GetForProfile(profile);
}

permissions::ChooserContextBase* GetHidChooserContext(Profile* profile) {
  return HidChooserContextFactory::GetForProfile(profile);
}

// The BluetoothChooserContext is only available when the
// WebBluetoothNewPermissionsBackend flag is enabled.
// TODO(https://crbug.com/589228): Remove the feature check when it is enabled
// by default.
permissions::ChooserContextBase* GetBluetoothChooserContext(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    return BluetoothChooserContextFactory::GetForProfile(profile);
  }
  return nullptr;
}

const ChooserTypeNameEntry kChooserTypeGroupNames[] = {
    {&GetUsbChooserContext, kUsbChooserDataGroupType},
    {&GetSerialChooserContext, kSerialChooserDataGroupType},
    {&GetHidChooserContext, kHidChooserDataGroupType},
    {&GetBluetoothChooserContext, kBluetoothChooserDataGroupType}};

}  // namespace

bool HasRegisteredGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < base::size(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type &&
        kContentSettingsTypeGroupNames[i].name) {
      return true;
    }
  }
  return false;
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < base::size(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return ContentSettingsType::DEFAULT;
}

std::string ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < base::size(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type) {
      const char* name = kContentSettingsTypeGroupNames[i].name;
      if (name)
        return name;
      break;
    }
  }

  NOTREACHED() << static_cast<int32_t>(type)
               << " is not a recognized content settings type.";
  return std::string();
}

std::vector<ContentSettingsType> ContentSettingsTypesFromGroupNames(
    const base::Value::ConstListView types) {
  std::vector<ContentSettingsType> content_types;
  content_types.reserve(types.size());
  for (const auto& value : types) {
    const auto& type = value.GetString();
    content_types.push_back(
        site_settings::ContentSettingsTypeFromGroupName(type));
  }
  return content_types;
}

std::string SiteSettingSourceToString(const SiteSettingSource source) {
  return kSiteSettingSourceStringMapping[static_cast<int>(source)].source_str;
}

base::Value GetValueForManagedState(const site_settings::ManagedState& state) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(site_settings::kDisabled, base::Value(state.disabled));
  value.SetKey(
      site_settings::kPolicyIndicator,
      base::Value(site_settings::PolicyIndicatorTypeToString(state.indicator)));
  return value;
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
                              const extensions::Extension& app,
                              base::ListValue* exceptions) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, url_pattern);
  exception->SetString(kDisplayName, url_pattern);
  exception->SetString(kEmbeddingOrigin, url_pattern);
  exception->SetString(kSource, "HostedApp");
  exception->SetBoolean(kIncognito, false);
  exception->SetString(kAppName, app.name());
  exception->SetString(kAppId, app.id());
  exceptions->Append(std::move(exception));
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
std::unique_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito,
    bool is_embargoed,
    bool is_discarded) {
  auto exception = std::make_unique<base::DictionaryValue>();
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kDisplayName, display_name);
  exception->SetString(kEmbeddingOrigin,
                       secondary_pattern == ContentSettingsPattern::Wildcard()
                           ? std::string()
                           : secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kSource, provider_name);
  exception->SetBoolean(kIncognito, incognito);
  exception->SetBoolean(kIsEmbargoed, is_embargoed);
  exception->SetBoolean(kIsDiscarded, is_discarded);
  return exception;
}

std::string GetDisplayNameForExtension(
    const GURL& url,
    const extensions::ExtensionRegistry* extension_registry) {
  if (extension_registry && url.SchemeIs(extensions::kExtensionScheme)) {
    // For the extension scheme, the pattern must be a valid URL.
    DCHECK(url.is_valid());
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            url.host(), extensions::ExtensionRegistry::EVERYTHING);
    if (extension)
      return extension->name();
  }
  return std::string();
}

// Takes |url| and converts it into an individual origin string or retrieves
// name of the extension it belongs to.
std::string GetDisplayNameForGURL(
    const GURL& url,
    const extensions::ExtensionRegistry* extension_registry) {
  const url::Origin origin = url::Origin::Create(url);
  if (origin.opaque())
    return url.spec();

  std::string display_name =
      GetDisplayNameForExtension(url, extension_registry);
  if (!display_name.empty())
    return display_name;

  auto url_16 = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
      net::UnescapeRule::NONE, nullptr, nullptr, nullptr);
  auto url_string = base::UTF16ToUTF8(url_16);
  return url_string;
}

// If the given |pattern| represents an individual origin or extension, retrieve
// a string to display it as such. If not, return the pattern as a string.
std::string GetDisplayNameForPattern(
    const ContentSettingsPattern& pattern,
    const extensions::ExtensionRegistry* extension_registry) {
  const GURL url(pattern.ToString());
  const std::string extension_display_name =
      GetDisplayNameForExtension(url, extension_registry);
  if (!extension_display_name.empty())
    return extension_display_name;
  return pattern.ToString();
}

void GetExceptionsForContentType(
    ContentSettingsType type,
    Profile* profile,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito,
    base::ListValue* exceptions) {
  ContentSettingsForOneType all_settings;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  map->GetSettingsForOneType(type, std::string(), &all_settings);

  // Will return only regular settings for a regular profile and only incognito
  // settings for an incognito Profile.
  ContentSettingsForOneType discarded_settings;
  map->GetDiscardedSettingsForOneType(type, std::string(), &discarded_settings);

  // Group settings by primary_pattern.
  AllPatternsSettings all_patterns_settings;
  for (const auto& setting : all_settings) {
    // Don't add default settings.
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.source !=
            SiteSettingSourceToString(SiteSettingSource::kPreference)) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incognito settings
    // only.
    if (map->IsOffTheRecord() && !setting.incognito)
      continue;

    // Don't add WebUI settings.
    if (PatternAppliesToWebUISchemes(setting)) {
      continue;
    }

    all_patterns_settings[std::make_pair(
        setting.primary_pattern, setting.source)][setting.secondary_pattern] =
        setting.GetContentSetting();
  }

  ContentSettingsForOneType embargo_settings;
  map->GetSettingsForOneType(ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
                             std::string(), &embargo_settings);

  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          profile);

  std::set<ContentSettingsPattern> origins_under_embargo;

  for (const auto& setting : embargo_settings) {
    // Off-the-record HostContentSettingsMap contains incognito content
    // settings as well as normal content settings. Here, we use the
    // incognito settings only.
    if (map->IsOffTheRecord() && !setting.incognito)
      continue;

    if (!permissions::PermissionUtil::IsPermission(type))
      continue;

    if (auto_blocker
            ->GetEmbargoResult(GURL(setting.primary_pattern.ToString()), type)
            .content_setting == CONTENT_SETTING_BLOCK) {
      origins_under_embargo.insert(setting.primary_pattern);
      all_patterns_settings[std::make_pair(
          setting.primary_pattern, setting.source)][setting.secondary_pattern] =
          CONTENT_SETTING_BLOCK;
    }
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  // |all_patterns_settings| is sorted from the lowest precedence pattern to
  // the highest (see operator< in ContentSettingsPattern), so traverse it in
  // reverse to show the patterns with the highest precedence (the more specific
  // ones) on the top.
  for (auto i = all_patterns_settings.rbegin();
       i != all_patterns_settings.rend(); ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const OnePatternSettings& one_settings = i->second;
    const std::string display_name =
        GetDisplayNameForPattern(primary_pattern, extension_registry);

    // The "parent" entry either has an identical primary and secondary pattern,
    // or has a wildcard secondary. The two cases are indistinguishable in the
    // UI.
    auto parent = one_settings.find(primary_pattern);
    if (parent == one_settings.end())
      parent = one_settings.find(ContentSettingsPattern::Wildcard());

    const std::string& source = i->first.second;
    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    const ContentSettingsPattern& secondary_pattern =
        parent == one_settings.end() ? primary_pattern : parent->first;
    this_provider_exceptions.push_back(GetExceptionForPage(
        primary_pattern, secondary_pattern, display_name, parent_setting,
        source, incognito,
        base::Contains(origins_under_embargo, primary_pattern)));

    // Add the "children" for any embedded settings.
    for (auto j = one_settings.begin(); j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(GetExceptionForPage(
          primary_pattern, j->first, display_name, content_setting, source,
          incognito, base::Contains(origins_under_embargo, primary_pattern)));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == ContentSettingsType::MEDIASTREAM_MIC ||
      type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    auto& policy_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(
            SiteSettingSourceToString(SiteSettingSource::kPolicy))];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions, extension_registry, web_ui,
                         incognito);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }

  for (auto& discarded_rule : discarded_settings) {
    exceptions->Append(GetExceptionForPage(
        discarded_rule.primary_pattern, discarded_rule.secondary_pattern,
        GetDisplayNameForPattern(discarded_rule.primary_pattern,
                                 extension_registry),
        discarded_rule.GetContentSetting(), discarded_rule.source, incognito,
        false /*is_embargoed*/, true /*is_discarded*/));
  }
}

void GetContentCategorySetting(const HostContentSettingsMap* map,
                               ContentSettingsType content_type,
                               base::DictionaryValue* object) {
  std::string provider;
  std::string setting = content_settings::ContentSettingToString(
      map->GetDefaultContentSetting(content_type, &provider));
  DCHECK(!setting.empty());

  object->SetString(kSetting, setting);
  if (provider != SiteSettingSourceToString(SiteSettingSource::kDefault))
    object->SetString(kSource, provider);
}

ContentSetting GetContentSettingForOrigin(
    Profile* profile,
    const HostContentSettingsMap* map,
    const GURL& origin,
    ContentSettingsType content_type,
    std::string* source_string,
    const extensions::ExtensionRegistry* extension_registry,
    std::string* display_name) {
  // TODO(patricialor): In future, PermissionManager should know about all
  // content settings, not just the permissions, plus all the possible sources,
  // and the calls to HostContentSettingsMap should be removed.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> value = map->GetWebsiteSetting(
      origin, origin, content_type, std::string(), &info);

  // Retrieve the content setting.
  permissions::PermissionResult result(
      CONTENT_SETTING_DEFAULT,
      permissions::PermissionStatusSource::UNSPECIFIED);
  if (permissions::PermissionUtil::IsPermission(content_type)) {
    result =
        PermissionManagerFactory::GetForProfile(profile)->GetPermissionStatus(
            content_type, origin, origin);
  } else {
    DCHECK(value.get());
    DCHECK_EQ(base::Value::Type::INTEGER, value->type());
    result.content_setting =
        content_settings::ValueToContentSetting(value.get());
  }

  // Retrieve the source of the content setting.
  *source_string = SiteSettingSourceToString(
      CalculateSiteSettingSource(profile, content_type, origin, info, result));
  *display_name = GetDisplayNameForGURL(origin, extension_registry);

  return result.content_setting;
}

std::vector<ContentSettingPatternSource> GetSiteExceptionsForContentType(
    HostContentSettingsMap* map,
    ContentSettingsType content_type) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(content_type, std::string(), &entries);
  entries.erase(std::remove_if(entries.begin(), entries.end(),
                               [](const ContentSettingPatternSource& e) {
                                 return !PatternAppliesToSingleOrigin(e) ||
                                        PatternAppliesToWebUISchemes(e);
                               }),
                entries.end());
  return entries;
}

void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito) {
  DCHECK(type == ContentSettingsType::MEDIASTREAM_MIC ||
         type == ContentSettingsType::MEDIASTREAM_CAMERA);

  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();
  const base::ListValue* policy_urls =
      prefs->GetList(type == ContentSettingsType::MEDIASTREAM_MIC
                         ? prefs::kAudioCaptureAllowedUrls
                         : prefs::kVideoCaptureAllowedUrls);

  // Convert the URLs to |ContentSettingsPattern|s. Ignore any invalid ones.
  std::vector<ContentSettingsPattern> patterns;
  for (const auto& entry : *policy_urls) {
    std::string url;
    bool valid_string = entry.GetAsString(&url);
    if (!valid_string)
      continue;

    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(url);
    if (!pattern.IsValid())
      continue;

    patterns.push_back(pattern);
  }

  // The patterns are shown in the UI in a reverse order defined by
  // |ContentSettingsPattern::operator<|.
  std::sort(patterns.begin(), patterns.end(),
            std::greater<ContentSettingsPattern>());

  for (const ContentSettingsPattern& pattern : patterns) {
    std::string display_name =
        GetDisplayNameForPattern(pattern, extension_registry);
    exceptions->push_back(GetExceptionForPage(
        pattern, ContentSettingsPattern(), display_name, CONTENT_SETTING_ALLOW,
        SiteSettingSourceToString(SiteSettingSource::kPolicy), incognito));
  }
}

const ChooserTypeNameEntry* ChooserTypeFromGroupName(const std::string& name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name)
      return &chooser_type;
  }
  return nullptr;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a chooser permission exceptions table. The chooser permission will contain
// a list of site exceptions that correspond to the exception.
base::Value CreateChooserExceptionObject(
    const base::string16& display_name,
    const base::Value& object,
    const std::string& chooser_type,
    const ChooserExceptionDetails& chooser_exception_details) {
  base::Value exception(base::Value::Type::DICTIONARY);

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception.SetStringKey(kDisplayName, display_name);
  exception.SetKey(kObject, object.Clone());
  exception.SetStringKey(kChooserType, chooser_type);

  // Order the sites by the provider precedence order.
  std::vector<base::Value>
      all_provider_sites[HostContentSettingsMap::NUM_PROVIDER_TYPES];
  for (const auto& details : chooser_exception_details) {
    const GURL& requesting_origin = details.first.first;
    const std::string& source = details.first.second;

    auto& this_provider_sites =
        all_provider_sites[HostContentSettingsMap::GetProviderTypeFromSource(
            source)];

    for (const auto& embedding_origin_incognito_pair : details.second) {
      const GURL& embedding_origin = embedding_origin_incognito_pair.first;
      const bool incognito = embedding_origin_incognito_pair.second;
      base::Value site(base::Value::Type::DICTIONARY);

      site.SetStringKey(kOrigin, requesting_origin.spec());
      site.SetStringKey(kDisplayName, requesting_origin.spec());
      site.SetStringKey(kEmbeddingOrigin, embedding_origin.is_empty()
                                              ? std::string()
                                              : embedding_origin.spec());
      site.SetStringKey(kSetting, setting_string);
      site.SetStringKey(kSource, source);
      site.SetBoolKey(kIncognito, incognito);
      this_provider_sites.push_back(std::move(site));
    }
  }

  base::Value sites(base::Value::Type::LIST);
  for (auto& one_provider_sites : all_provider_sites) {
    for (auto& site : one_provider_sites) {
      sites.Append(std::move(site));
    }
  }

  exception.SetKey(kSites, std::move(sites));
  return exception;
}

base::Value GetChooserExceptionListFromProfile(
    Profile* profile,
    const ChooserTypeNameEntry& chooser_type) {
  base::Value exceptions(base::Value::Type::LIST);
  ContentSettingsType content_type =
      ContentSettingsTypeFromGroupName(std::string(chooser_type.name));

  // The BluetoothChooserContext is only available when the
  // WebBluetoothNewPermissionsBackend flag is enabled.
  // TODO(https://crbug.com/589228): Remove the nullptr check when it is enabled
  // by default.
  permissions::ChooserContextBase* chooser_context =
      chooser_type.get_context(profile);
  if (!chooser_context)
    return exceptions;

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = chooser_context->GetAllGrantedObjects();

  if (profile->HasPrimaryOTRProfile()) {
    Profile* incognito_profile = profile->GetPrimaryOTRProfile();
    permissions::ChooserContextBase* incognito_chooser_context =
        chooser_type.get_context(incognito_profile);
    std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
        incognito_objects = incognito_chooser_context->GetAllGrantedObjects();
    objects.insert(objects.end(),
                   std::make_move_iterator(incognito_objects.begin()),
                   std::make_move_iterator(incognito_objects.end()));
  }

  // Maps from a chooser exception name/object pair to a
  // ChooserExceptionDetails. This will group and sort the exceptions by the UI
  // string and object for display.
  std::map<std::pair<base::string16, base::Value>, ChooserExceptionDetails>
      all_chooser_objects;
  for (const auto& object : objects) {
    // Don't include WebUI settings.
    if (content::HasWebUIScheme(object->requesting_origin))
      continue;

    base::string16 name = chooser_context->GetObjectDisplayName(object->value);
    auto& chooser_exception_details =
        all_chooser_objects[std::make_pair(name, object->value.Clone())];

    std::string source = GetSourceStringForChooserException(
        profile, content_type, object->source);

    const auto requesting_origin_source_pair =
        std::make_pair(object->requesting_origin, source);
    auto& embedding_origin_incognito_pair_set =
        chooser_exception_details[requesting_origin_source_pair];

    const auto embedding_origin_incognito_pair =
        std::make_pair(object->embedding_origin, object->incognito);
    embedding_origin_incognito_pair_set.insert(embedding_origin_incognito_pair);
  }

  for (const auto& all_chooser_objects_entry : all_chooser_objects) {
    const base::string16& name = all_chooser_objects_entry.first.first;
    const base::Value& object = all_chooser_objects_entry.first.second;
    const ChooserExceptionDetails& chooser_exception_details =
        all_chooser_objects_entry.second;
    exceptions.Append(CreateChooserExceptionObject(
        name, object, chooser_type.name, chooser_exception_details));
  }

  return exceptions;
}

std::string PolicyIndicatorTypeToString(const PolicyIndicatorType type) {
  return kPolicyIndicatorTypeStringMapping[static_cast<int>(type)]
      .indicator_str;
}

PolicyIndicatorType GetPolicyIndicatorFromPref(
    const PrefService::Preference* pref) {
  if (!pref) {
    return PolicyIndicatorType::kNone;
  }
  if (pref->IsExtensionControlled()) {
    return PolicyIndicatorType::kExtension;
  }
  if (pref->IsManagedByCustodian()) {
    return PolicyIndicatorType::kParent;
  }
  if (pref->IsManaged()) {
    return PolicyIndicatorType::kDevicePolicy;
  }
  if (pref->GetRecommendedValue()) {
    return PolicyIndicatorType::kRecommended;
  }
  return PolicyIndicatorType::kNone;
}

}  // namespace site_settings

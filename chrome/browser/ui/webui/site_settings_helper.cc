// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/site_settings_helper.h"

#include <functional>
#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace site_settings {

constexpr char kAppName[] = "appName";
constexpr char kAppId[] = "appId";
constexpr char kObject[] = "object";
constexpr char kObjectName[] = "objectName";

namespace {

// Maps from the UI string to the object it represents (for sorting purposes).
typedef std::multimap<std::string, const base::DictionaryValue*> SortedObjects;

// Maps from a secondary URL to the set of objects it has permission to access.
typedef std::map<GURL, SortedObjects> OneOriginObjects;

// Maps from a primary URL/source pair to a OneOriginObjects. All the mappings
// in OneOriginObjects share the given primary URL and source.
typedef std::map<std::pair<GURL, std::string>, OneOriginObjects>
    AllOriginObjects;

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
    // The following ContentSettingsTypes have UI in Content Settings
    // and require a mapping from their Javascript string representation in
    // chrome/browser/resources/settings/site_settings/constants.js to their C++
    // ContentSettingsType provided here.
    {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
    {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
    {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
    {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
    {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic"},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera"},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex"},
    {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, "protected-content"},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, "background-sync"},
    {CONTENT_SETTINGS_TYPE_ADS, "ads"},
    {CONTENT_SETTINGS_TYPE_SOUND, "sound"},
    {CONTENT_SETTINGS_TYPE_CLIPBOARD_READ, "clipboard"},
    {CONTENT_SETTINGS_TYPE_SENSORS, "sensors"},
    {CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER, "payment-handler"},
    {CONTENT_SETTINGS_TYPE_USB_GUARD, "usb-devices"},

    // Add new content settings here if a corresponding Javascript string
    // representation for it is not required. Note some exceptions, such as
    // USB_CHOOSER_DATA, do have UI in Content Settings but do not require a
    // separate string.
    {CONTENT_SETTINGS_TYPE_DEFAULT, nullptr},
    {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, nullptr},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, nullptr},
    {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, nullptr},
    {CONTENT_SETTINGS_TYPE_APP_BANNER, nullptr},
    {CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, nullptr},
    {CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, nullptr},
    {CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, nullptr},
    {CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD, nullptr},
    {CONTENT_SETTINGS_TYPE_AUTOPLAY, nullptr},
    {CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, nullptr},
    {CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA, nullptr},
    {CONTENT_SETTINGS_TYPE_ADS_DATA, nullptr},
    {CONTENT_SETTINGS_TYPE_MIDI, nullptr},
    {CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, nullptr},
    {CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, nullptr},
    {CONTENT_SETTINGS_TYPE_CLIENT_HINTS, nullptr},
    {CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS, nullptr},
    {CONTENT_SETTINGS_TYPE_CLIPBOARD_WRITE, nullptr},
    {CONTENT_SETTINGS_TYPE_PLUGINS_DATA, nullptr},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_FETCH, nullptr},
};
static_assert(arraysize(kContentSettingsTypeGroupNames) ==
                  // ContentSettingsType starts at -1, so add 1 here.
                  static_cast<int>(CONTENT_SETTINGS_NUM_TYPES) + 1,
              "kContentSettingsTypeGroupNames should have "
              "CONTENT_SETTINGS_NUM_TYPES elements");

struct SiteSettingSourceStringMapping {
  SiteSettingSource source;
  const char* source_str;
};

const SiteSettingSourceStringMapping kSiteSettingSourceStringMapping[] = {
    {SiteSettingSource::kAdsFilterBlacklist, "ads-filter-blacklist"},
    {SiteSettingSource::kDefault, "default"},
    {SiteSettingSource::kDrmDisabled, "drm-disabled"},
    {SiteSettingSource::kEmbargo, "embargo"},
    {SiteSettingSource::kExtension, "extension"},
    {SiteSettingSource::kInsecureOrigin, "insecure-origin"},
    {SiteSettingSource::kKillSwitch, "kill-switch"},
    {SiteSettingSource::kPolicy, "policy"},
    {SiteSettingSource::kPreference, "preference"},
};
static_assert(arraysize(kSiteSettingSourceStringMapping) ==
                  static_cast<int>(SiteSettingSource::kNumSources),
              "kSiteSettingSourceStringMapping should have "
              "SiteSettingSource::kNumSources elements");

// Retrieves the corresponding string, according to the following precedence
// order from highest to lowest priority:
//    1. Kill-switch.
//    2. Insecure origins (some permissions are denied to insecure origins).
//    3. Enterprise policy.
//    4. Extensions.
//    5. Activated for ads filtering (for Ads ContentSettingsType only).
//    6. DRM disabled (for CrOS's Protected Content ContentSettingsType only).
//    7. User-set per-origin setting.
//    8. Embargo.
//    9. User-set patterns.
//   10. User-set global default for a ContentSettingsType.
//   11. Chrome's built-in default.
SiteSettingSource CalculateSiteSettingSource(
    Profile* profile,
    const ContentSettingsType content_type,
    const GURL& origin,
    const content_settings::SettingInfo& info,
    const PermissionResult result) {
  if (result.source == PermissionStatusSource::KILL_SWITCH)
    return SiteSettingSource::kKillSwitch;  // Source #1.

  if (result.source == PermissionStatusSource::INSECURE_ORIGIN)
    return SiteSettingSource::kInsecureOrigin;  // Source #2.

  if (info.source == content_settings::SETTING_SOURCE_POLICY ||
      info.source == content_settings::SETTING_SOURCE_SUPERVISED) {
    return SiteSettingSource::kPolicy;  // Source #3.
  }

  if (info.source == content_settings::SETTING_SOURCE_EXTENSION)
    return SiteSettingSource::kExtension;  // Source #4.

  if (content_type == CONTENT_SETTINGS_TYPE_ADS &&
      base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter)) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    if (map->GetWebsiteSetting(origin, GURL(), CONTENT_SETTINGS_TYPE_ADS_DATA,
                               /*resource_identifier=*/std::string(),
                               /*setting_info=*/nullptr) != nullptr) {
      return SiteSettingSource::kAdsFilterBlacklist;  // Source #5.
    }
  }

  // Protected Content will be blocked if the |kEnableDRM| pref is off.
  if (content_type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER &&
      !profile->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    return SiteSettingSource::kDrmDisabled;  // Source #6.
  }

  DCHECK_NE(content_settings::SETTING_SOURCE_NONE, info.source);
  if (info.source == content_settings::SETTING_SOURCE_USER) {
    if (result.source == PermissionStatusSource::MULTIPLE_DISMISSALS ||
        result.source == PermissionStatusSource::MULTIPLE_IGNORES) {
      return SiteSettingSource::kEmbargo;  // Source #8.
    }
    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return SiteSettingSource::kDefault;  // Source #10, #11.
    }

    // Source #7, #9. When #7 is the source, |result.source| won't
    // be set to any of the source #7 enum values, as PermissionManager is
    // aware of the difference between these two sources internally. The
    // subtlety here should go away when PermissionManager can handle all
    // content settings and all possible sources.
    return SiteSettingSource::kPreference;
  }

  NOTREACHED();
  return SiteSettingSource::kPreference;
}

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

const ChooserTypeNameEntry kChooserTypeGroupNames[] = {
    {&GetUsbChooserContext, kGroupTypeUsb},
};

}  // namespace

bool HasRegisteredGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type &&
        kContentSettingsTypeGroupNames[i].name != nullptr) {
      return true;
    }
  }
  return false;
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

std::string ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type) {
      const char* name = kContentSettingsTypeGroupNames[i].name;
      if (name != nullptr)
        return name;
      break;
    }
  }

  NOTREACHED() << type << " is not a recognized content settings type.";
  return std::string();
}

std::string SiteSettingSourceToString(const SiteSettingSource source) {
  return kSiteSettingSourceStringMapping[static_cast<int>(source)].source_str;
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
    bool incognito) {
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

  // Note that using Serialize() here will chop off default port numbers and
  // percent encode the origin.
  return origin.Serialize();
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

void GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito,
    const std::string* filter,
    base::ListValue* exceptions) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(type, std::string(), &entries);
  // Group settings by primary_pattern.
  AllPatternsSettings all_patterns_settings;
  for (auto i = entries.begin(); i != entries.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source !=
            SiteSettingSourceToString(SiteSettingSource::kPreference)) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incongnito settings
    // only.
    if (map->is_incognito() && !i->incognito)
      continue;

    if (filter && i->primary_pattern.ToString() != *filter)
      continue;

    all_patterns_settings[std::make_pair(i->primary_pattern, i->source)]
                         [i->secondary_pattern] = i->GetContentSetting();
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
    this_provider_exceptions.push_back(
        GetExceptionForPage(primary_pattern, secondary_pattern, display_name,
                            parent_setting, source, incognito));

    // Add the "children" for any embedded settings.
    for (auto j = one_settings.begin(); j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(
          GetExceptionForPage(primary_pattern, j->first, display_name,
                              content_setting, source, incognito));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
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
  PermissionResult result(CONTENT_SETTING_DEFAULT,
                          PermissionStatusSource::UNSPECIFIED);
  if (PermissionUtil::IsPermission(content_type)) {
    result = PermissionManager::Get(profile)->GetPermissionStatus(
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

void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito) {
  DCHECK(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();
  const base::ListValue* policy_urls =
      prefs->GetList(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
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
// in a chooser permission exceptions table.
std::unique_ptr<base::DictionaryValue> GetChooserExceptionForPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const std::string& provider_name,
    bool incognito,
    const std::string& name,
    const base::DictionaryValue* object) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, requesting_origin.spec());
  exception->SetString(kDisplayName, requesting_origin.spec());
  exception->SetString(kEmbeddingOrigin, embedding_origin.spec());
  exception->SetString(kSource, provider_name);
  exception->SetBoolean(kIncognito, incognito);
  if (object) {
    exception->SetString(kObjectName, name);
    exception->Set(kObject, object->CreateDeepCopy());
  }
  return exception;
}

void GetChooserExceptionsFromProfile(Profile* profile,
                                     bool incognito,
                                     const ChooserTypeNameEntry& chooser_type,
                                     base::ListValue* exceptions) {
  if (incognito) {
    if (!profile->HasOffTheRecordProfile())
      return;
    profile = profile->GetOffTheRecordProfile();
  }

  ChooserContextBase* chooser_context = chooser_type.get_context(profile);
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      chooser_context->GetAllGrantedObjects();
  AllOriginObjects all_origin_objects;
  for (const auto& object : objects) {
    std::string name = chooser_context->GetObjectName(object->object);
    // It is safe for this structure to hold references into |objects| because
    // they are both destroyed at the end of this function.
    all_origin_objects[make_pair(object->requesting_origin, object->source)]
                      [object->embedding_origin]
                          .insert(make_pair(name, &object->object));
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  for (const auto& all_origin_objects_entry : all_origin_objects) {
    const GURL& requesting_origin = all_origin_objects_entry.first.first;
    const std::string& source = all_origin_objects_entry.first.second;
    const OneOriginObjects& one_origin_objects =
        all_origin_objects_entry.second;

    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add entries for any non-embedded origins.
    bool has_embedded_entries = false;
    for (const auto& one_origin_objects_entry : one_origin_objects) {
      const GURL& embedding_origin = one_origin_objects_entry.first;
      const SortedObjects& sorted_objects = one_origin_objects_entry.second;

      // Skip the embedded settings which will be added below.
      if (requesting_origin != embedding_origin) {
        has_embedded_entries = true;
        continue;
      }

      for (const auto& sorted_objects_entry : sorted_objects) {
        this_provider_exceptions.push_back(GetChooserExceptionForPage(
            requesting_origin, embedding_origin, source, incognito,
            sorted_objects_entry.first, sorted_objects_entry.second));
      }
    }

    if (has_embedded_entries) {
      // Add a "parent" entry that simply acts as a heading for all entries
      // where |requesting_origin| has been embedded.
      this_provider_exceptions.push_back(GetChooserExceptionForPage(
          requesting_origin, requesting_origin, source, incognito,
          std::string(), nullptr));

      // Add the "children" for any embedded settings.
      for (const auto& one_origin_objects_entry : one_origin_objects) {
        const GURL& embedding_origin = one_origin_objects_entry.first;
        const SortedObjects& sorted_objects = one_origin_objects_entry.second;

        // Skip the non-embedded setting which we already added above.
        if (requesting_origin == embedding_origin)
          continue;

        for (const auto& sorted_objects_entry : sorted_objects) {
          this_provider_exceptions.push_back(GetChooserExceptionForPage(
              requesting_origin, embedding_origin, source, incognito,
              sorted_objects_entry.first, sorted_objects_entry.second));
        }
      }
    }
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

}  // namespace site_settings

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

namespace site_settings {

constexpr char kAppName[] = "appName";
constexpr char kAppId[] = "appId";

namespace {

// Chooser data group names.
const char kUsbChooserDataGroupType[] = "usb-devices-data";
const char kSerialChooserDataGroupType[] = "serial-ports-data";
const char kHidChooserDataGroupType[] = "hid-devices-data";
const char kBluetoothChooserDataGroupType[] = "bluetooth-devices-data";

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
    // The following ContentSettingsTypes have UI in Content Settings
    // and require a mapping from their Javascript string representation in
    // chrome/browser/resources/settings/site_settings/constants.js to their C++
    // ContentSettingsType provided here. These group names are only used by
    // desktop webui.
    {ContentSettingsType::COOKIES, "cookies"},
    {ContentSettingsType::IMAGES, "images"},
    {ContentSettingsType::JAVASCRIPT, "javascript"},
    {ContentSettingsType::POPUPS, "popups"},
    {ContentSettingsType::GEOLOCATION, "location"},
    {ContentSettingsType::NOTIFICATIONS, "notifications"},
    {ContentSettingsType::MEDIASTREAM_MIC, "media-stream-mic"},
    {ContentSettingsType::MEDIASTREAM_CAMERA, "media-stream-camera"},
    {ContentSettingsType::PROTOCOL_HANDLERS, "register-protocol-handler"},
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
    {ContentSettingsType::WINDOW_MANAGEMENT, "window-placement"},
    {ContentSettingsType::LOCAL_FONTS, "local-fonts"},
    {ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
     "file-system-access-handles-data"},
    {ContentSettingsType::FEDERATED_IDENTITY_API, "federated-identity-api"},
    {ContentSettingsType::PRIVATE_NETWORK_GUARD, "private-network-devices"},
    {ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA,
     "private-network-devices-data"},
    {ContentSettingsType::ANTI_ABUSE, "anti-abuse"},

    // Add new content settings here if a corresponding Javascript string
    // representation for it is not required, for example if the content setting
    // is not used for desktop. Note some exceptions do have UI in Content
    // Settings but do not require a separate string.
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
    {ContentSettingsType::BACKGROUND_FETCH, nullptr},
    {ContentSettingsType::INTENT_PICKER_DISPLAY, nullptr},
    {ContentSettingsType::PERIODIC_BACKGROUND_SYNC, nullptr},
    {ContentSettingsType::WAKE_LOCK_SCREEN, nullptr},
    {ContentSettingsType::WAKE_LOCK_SYSTEM, nullptr},
    {ContentSettingsType::LEGACY_COOKIE_ACCESS, nullptr},
    {ContentSettingsType::NFC, nullptr},
    {ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA, nullptr},
    {ContentSettingsType::FILE_SYSTEM_READ_GUARD, nullptr},
    {ContentSettingsType::STORAGE_ACCESS, nullptr},
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM, nullptr},
    {ContentSettingsType::INSECURE_LOCAL_NETWORK, nullptr},
    {ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA, nullptr},
    {ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, nullptr},
    {ContentSettingsType::DISPLAY_CAPTURE, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_SHARING, nullptr},
    {ContentSettingsType::JAVASCRIPT_JIT, nullptr},
    {ContentSettingsType::HTTP_ALLOWED, nullptr},
    {ContentSettingsType::HTTPS_ENFORCED, nullptr},
    {ContentSettingsType::FORMFILL_METADATA, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_ACTIVE_SESSION, nullptr},
    {ContentSettingsType::AUTO_DARK_WEB_CONTENT, nullptr},
    {ContentSettingsType::REQUEST_DESKTOP_SITE, nullptr},
    {ContentSettingsType::GET_DISPLAY_MEDIA_SET_SELECT_ALL_SCREENS, nullptr},
    {ContentSettingsType::NOTIFICATION_INTERACTIONS, nullptr},
    {ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, nullptr},
    {ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
     nullptr},
    // PPAPI_BROKER has been deprecated. The content setting is not used or
    // called from UI, so we don't need a representation JS string.
    {ContentSettingsType::DEPRECATED_PPAPI_BROKER, nullptr},
    {ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, nullptr},
    {ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, nullptr},
    // TODO(crbug.com/1408520): Update JavaScript string representation when
    // desktop UI is implemented.
    {ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
     nullptr},
    {ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING, nullptr},
};

static_assert(std::size(kContentSettingsTypeGroupNames) ==
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
    {SiteSettingSource::kEmbargo, "embargo"},
    {SiteSettingSource::kExtension, "extension"},
    {SiteSettingSource::kHostedApp, "HostedApp"},
    {SiteSettingSource::kInsecureOrigin, "insecure-origin"},
    {SiteSettingSource::kKillSwitch, "kill-switch"},
    {SiteSettingSource::kPolicy, "policy"},
    {SiteSettingSource::kPreference, "preference"},
};
static_assert(std::size(kSiteSettingSourceStringMapping) ==
                  static_cast<int>(SiteSettingSource::kNumSources),
              "kSiteSettingSourceStringMapping should have "
              "SiteSettingSource::kNumSources elements");

// Retrieves the corresponding string, according to the following precedence
// order from highest to lowest priority:
//    1. Allowlisted WebUI content setting.
//    2. Kill-switch.
//    3. Insecure origins (some permissions are denied to insecure origins).
//    4. Enterprise policy.
//    5. Extensions.
//    6. Activated for ads filtering (for Ads ContentSettingsType only).
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
    subresource_filter::SubresourceFilterContentSettingsManager*
        settings_manager =
            SubresourceFilterProfileContextFactory::GetForProfile(profile)
                ->settings_manager();

    if (settings_manager->GetSiteActivationFromMetadata(origin)) {
      return SiteSettingSource::kAdsFilterBlocklist;  // Source #6.
    }
  }

  DCHECK_NE(content_settings::SETTING_SOURCE_NONE, info.source);
  if (info.source == content_settings::SETTING_SOURCE_USER) {
    if (result.source ==
            permissions::PermissionStatusSource::MULTIPLE_DISMISSALS ||
        result.source ==
            permissions::PermissionStatusSource::MULTIPLE_IGNORES) {
      return SiteSettingSource::kEmbargo;  // Source #8.
    }
    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return SiteSettingSource::kDefault;  // Source #10, #11.
    }

    // Source #7, #9. When #7 is the source, |result.source| won't be set to
    // any of the source #7 enum values, as PermissionManager is aware of the
    // difference between these two sources internally. The subtlety here should
    // go away when PermissionManager can handle all content settings and all
    // possible sources.
    return SiteSettingSource::kPreference;
  }

  NOTREACHED();
  return SiteSettingSource::kPreference;
}

bool PatternAppliesToWebUISchemes(const ContentSettingPatternSource& pattern) {
  return pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_CHROME ||
         pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_CHROMEUNTRUSTED ||
         pattern.primary_pattern.GetScheme() ==
             ContentSettingsPattern::SchemeType::SCHEME_DEVTOOLS;
}

// If the given |pattern| represents an individual origin, Isolated Web App, or
// extension, retrieve a string to display it as such. If not, return the
// pattern as a string.
std::string GetDisplayNameForPattern(Profile* profile,
                                     const ContentSettingsPattern& pattern) {
  GURL url(pattern.ToString());
  if (url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(chrome::kIsolatedAppScheme)) {
    return GetDisplayNameForGURL(profile, url, /*hostname_only=*/false);
  }
  return pattern.ToString();
}

// Returns exceptions constructed from the policy-set allowed URLs
// for the content settings |type| mic or camera.
void GetPolicyAllowedUrls(ContentSettingsType type,
                          std::vector<base::Value::Dict>* exceptions,
                          content::WebUI* web_ui,
                          bool incognito) {
  DCHECK(type == ContentSettingsType::MEDIASTREAM_MIC ||
         type == ContentSettingsType::MEDIASTREAM_CAMERA);

  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  const base::Value::List& policy_urls =
      prefs->GetList(type == ContentSettingsType::MEDIASTREAM_MIC
                         ? prefs::kAudioCaptureAllowedUrls
                         : prefs::kVideoCaptureAllowedUrls);

  // Convert the URLs to |ContentSettingsPattern|s. Ignore any invalid ones.
  std::vector<ContentSettingsPattern> patterns;
  for (const auto& entry : policy_urls) {
    const std::string* url = entry.GetIfString();
    if (!url) {
      continue;
    }

    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(*url);
    if (!pattern.IsValid()) {
      continue;
    }

    patterns.push_back(pattern);
  }

  // The patterns are shown in the UI in a reverse order defined by
  // |ContentSettingsPattern::operator<|.
  std::sort(patterns.begin(), patterns.end(),
            std::greater<ContentSettingsPattern>());

  for (const ContentSettingsPattern& pattern : patterns) {
    std::string display_name = GetDisplayNameForPattern(profile, pattern);
    exceptions->push_back(GetExceptionForPage(
        type, profile, pattern, ContentSettingsPattern(), display_name,
        CONTENT_SETTING_ALLOW,
        SiteSettingSourceToString(SiteSettingSource::kPolicy), incognito));
  }
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

permissions::ObjectPermissionContextBase* GetUsbChooserContext(
    Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

permissions::ObjectPermissionContextBase* GetSerialChooserContext(
    Profile* profile) {
  return SerialChooserContextFactory::GetForProfile(profile);
}

permissions::ObjectPermissionContextBase* GetHidChooserContext(
    Profile* profile) {
  return HidChooserContextFactory::GetForProfile(profile);
}

// The BluetoothChooserContext is only available when the
// WebBluetoothNewPermissionsBackend flag is enabled.
// TODO(https://crbug.com/589228): Remove the feature check when it is enabled
// by default.
permissions::ObjectPermissionContextBase* GetBluetoothChooserContext(
    Profile* profile) {
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

// There are two FormatOptions to support both hostname-only and schemeful URL
// formatting, both of which are used in Site Settings.
constexpr UrlIdentity::FormatOptions kUrlIdentityOptionsWithScheme = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptionsHostnameOnly = {
    .default_options = {UrlIdentity::DefaultFormatOptions::kHostname}};
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};

}  // namespace

bool HasRegisteredGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < std::size(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type &&
        kContentSettingsTypeGroupNames[i].name) {
      return true;
    }
  }
  return false;
}

ContentSettingsType ContentSettingsTypeFromGroupName(base::StringPiece name) {
  for (const auto& entry : kContentSettingsTypeGroupNames) {
    // Content setting types that aren't represented in the settings UI
    // will have `nullptr` as their `name`. However, converting `nullptr`
    // to a StringPiece will crash, so we have to handle it explicitly
    // before comparing.
    if (entry.name != nullptr && entry.name == name) {
      return entry.type;
    }
  }

  return ContentSettingsType::DEFAULT;
}

base::StringPiece ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < std::size(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type) {
      const char* name = kContentSettingsTypeGroupNames[i].name;
      if (name)
        return name;
      break;
    }
  }

  NOTREACHED() << static_cast<int32_t>(type)
               << " is not a recognized content settings type.";
  return base::StringPiece();
}

const std::vector<ContentSettingsType>& GetVisiblePermissionCategories() {
  // First build the list of permissions that will be shown regardless of
  // `origin`. Some categories such as COOKIES store their data in a custom way,
  // so are not included here.
  static base::NoDestructor<std::vector<ContentSettingsType>> base_types{{
      ContentSettingsType::AR,
      ContentSettingsType::AUTOMATIC_DOWNLOADS,
      ContentSettingsType::BACKGROUND_SYNC,
      ContentSettingsType::CLIPBOARD_READ_WRITE,
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::HID_GUARD,
      ContentSettingsType::IDLE_DETECTION,
      ContentSettingsType::IMAGES,
      ContentSettingsType::JAVASCRIPT,
      ContentSettingsType::LOCAL_FONTS,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MIDI_SYSEX,
      ContentSettingsType::MIXEDSCRIPT,
      ContentSettingsType::NOTIFICATIONS,
      ContentSettingsType::POPUPS,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
#endif
      ContentSettingsType::SENSORS,
      ContentSettingsType::SERIAL_GUARD,
      ContentSettingsType::SOUND,
      ContentSettingsType::USB_GUARD,
      ContentSettingsType::VR,
      ContentSettingsType::WINDOW_MANAGEMENT,
  }};
  static bool initialized = false;
  if (!initialized) {
    // The permission categories in this block are only shown when running with
    // certain flags/switches.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kEnableExperimentalWebPlatformFeatures)) {
      base_types->push_back(ContentSettingsType::BLUETOOTH_SCANNING);
    }

    if (base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps))
      base_types->push_back(ContentSettingsType::PAYMENT_HANDLER);

    if (base::FeatureList::IsEnabled(features::kFedCm)) {
      base_types->push_back(ContentSettingsType::FEDERATED_IDENTITY_API);
    }

    if (base::FeatureList::IsEnabled(
            features::kWebBluetoothNewPermissionsBackend)) {
      base_types->push_back(ContentSettingsType::BLUETOOTH_GUARD);
    }

    if (base::FeatureList::IsEnabled(
            subresource_filter::kSafeBrowsingSubresourceFilter)) {
      base_types->push_back(ContentSettingsType::ADS);
    }

    if (base::FeatureList::IsEnabled(
            blink::features::kPrivateNetworkAccessPermissionPrompt)) {
      base_types->push_back(ContentSettingsType::PRIVATE_NETWORK_GUARD);
    }

    initialized = true;
  }

  return *base_types;
}

std::string SiteSettingSourceToString(const SiteSettingSource source) {
  return kSiteSettingSourceStringMapping[static_cast<int>(source)].source_str;
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
                              const extensions::Extension& app,
                              base::Value::List* exceptions) {
  base::Value::Dict exception;

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception.Set(kSetting, setting_string);
  exception.Set(kOrigin, url_pattern);
  exception.Set(kDisplayName, url_pattern);
  exception.Set(kEmbeddingOrigin, url_pattern);
  exception.Set(kSource,
                SiteSettingSourceToString(SiteSettingSource::kHostedApp));
  exception.Set(kIncognito, false);
  exception.Set(kAppName, app.name());
  exception.Set(kAppId, app.id());
  exceptions->Append(std::move(exception));
}

// Create a base::Value::Dict that will act as a data source for a single row
// for a File System Access permission grant.
base::Value::Dict GetFileSystemExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const std::string& origin,
    const base::FilePath& file_path,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito,
    bool is_embargoed) {
  base::Value::Dict exception;
  exception.Set(kOrigin, origin);
  // TODO(crbug.com/1373962): Replace `LossyDisplayName` method with a
  // new method that returns the full file path in a human-readable format.
  exception.Set(kDisplayName, file_path.LossyDisplayName());
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  exception.Set(kSetting, setting_string);

  exception.Set(kSource, provider_name);
  exception.Set(kIncognito, incognito);
  exception.Set(kIsEmbargoed, is_embargoed);
  return exception;
}

// Create a base::Value::Dict that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
base::Value::Dict GetExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito,
    bool is_embargoed) {
  base::Value::Dict exception;
  exception.Set(kOrigin, pattern.ToString());
  exception.Set(kDisplayName, display_name);
  exception.Set(kEmbeddingOrigin,
                secondary_pattern == ContentSettingsPattern::Wildcard()
                    ? std::string()
                    : secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  exception.Set(kSetting, setting_string);

  exception.Set(kSource, provider_name);
  exception.Set(kIncognito, incognito);
  exception.Set(kIsEmbargoed, is_embargoed);
  return exception;
}

std::string GetDisplayNameForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only) {
  auto origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return url.spec();
  }

  auto url_identity = UrlIdentity::CreateFromUrl(
      profile, origin.GetURL(), kUrlIdentityAllowedTypes,
      hostname_only ? kUrlIdentityOptionsHostnameOnly
                    : kUrlIdentityOptionsWithScheme);
  return base::UTF16ToUTF8(url_identity.name);
}

void GetExceptionsForContentType(ContentSettingsType type,
                                 Profile* profile,
                                 content::WebUI* web_ui,
                                 bool incognito,
                                 base::Value::List* exceptions) {
  ContentSettingsForOneType all_settings;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  map->GetSettingsForOneType(type, &all_settings);

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

    auto content_setting = setting.GetContentSetting();

    if (type == ContentSettingsType::COOKIES &&
        base::FeatureList::IsEnabled(
            privacy_sandbox::kPrivacySandboxSettings4)) {
      // With the changes to settings introduced in PrivacySandboxSettings4,
      // there is no user-facing concept of SESSION_ONLY cookie exceptions that
      // use secondary patterns. These are instead presented as ALLOW.
      // TODO(crbug.com/1404436): Perform a one time migration of the actual
      // content settings when the extension API no-longer allows them to be
      // created.
      if (content_setting == ContentSetting::CONTENT_SETTING_SESSION_ONLY &&
          setting.secondary_pattern != ContentSettingsPattern::Wildcard()) {
        content_setting = ContentSetting::CONTENT_SETTING_ALLOW;
      }
    }

    all_patterns_settings[std::make_pair(
        setting.primary_pattern, setting.source)][setting.secondary_pattern] =
        content_setting;
  }

  ContentSettingsForOneType embargo_settings;
  map->GetSettingsForOneType(ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
                             &embargo_settings);

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

    if (!permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
            type)) {
      continue;
    }

    if (auto_blocker->IsEmbargoed(GURL(setting.primary_pattern.ToString()),
                                  type)) {
      origins_under_embargo.insert(setting.primary_pattern);
      all_patterns_settings[std::make_pair(
          setting.primary_pattern, setting.source)][setting.secondary_pattern] =
          CONTENT_SETTING_BLOCK;
    }
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<base::Value::Dict>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  // |all_patterns_settings| is sorted from the lowest precedence pattern to
  // the highest (see operator< in ContentSettingsPattern), so traverse it in
  // reverse to show the patterns with the highest precedence (the more specific
  // ones) on the top.
  for (const auto& [primary_pattern_and_source, one_settings] :
       base::Reversed(all_patterns_settings)) {
    const auto& [primary_pattern, source] = primary_pattern_and_source;
    const std::string display_name =
        GetDisplayNameForPattern(profile, primary_pattern);

    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    for (auto j = one_settings.begin(); j != one_settings.end(); ++j) {
      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(GetExceptionForPage(
          type, profile, primary_pattern, j->first, display_name,
          content_setting, source, incognito,
          base::Contains(origins_under_embargo, primary_pattern)));
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
    GetPolicyAllowedUrls(type, &policy_exceptions, web_ui, incognito);
  }

  // Display the URLs with File System entries that are granted
  // permissions via File System Access Persistent Permissions.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions) &&
      (type == ContentSettingsType::FILE_SYSTEM_READ_GUARD ||
       type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD)) {
    auto& urls_with_granted_entries = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(
            SiteSettingSourceToString(SiteSettingSource::kDefault))];
    GetFileSystemGrantedEntries(&urls_with_granted_entries, profile, incognito);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

void GetContentCategorySetting(const HostContentSettingsMap* map,
                               ContentSettingsType content_type,
                               base::Value::Dict* object) {
  std::string provider;
  std::string setting = content_settings::ContentSettingToString(
      map->GetDefaultContentSetting(content_type, &provider));
  DCHECK(!setting.empty());

  object->Set(kSetting, setting);
  if (provider != SiteSettingSourceToString(SiteSettingSource::kDefault))
    object->Set(kSource, provider);
}

ContentSetting GetContentSettingForOrigin(Profile* profile,
                                          const HostContentSettingsMap* map,
                                          const GURL& origin,
                                          ContentSettingsType content_type,
                                          std::string* source_string,
                                          std::string* display_name) {
  // TODO(patricialor): In future, PermissionManager should know about all
  // content settings, not just the permissions, plus all the possible sources,
  // and the calls to HostContentSettingsMap should be removed.
  content_settings::SettingInfo info;
  const base::Value value =
      map->GetWebsiteSetting(origin, origin, content_type, &info);

  // Retrieve the content setting.
  permissions::PermissionResult result(
      content_settings::ValueToContentSetting(value),
      permissions::PermissionStatusSource::UNSPECIFIED);
  if (permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
          content_type)) {
    if (permissions::PermissionUtil::IsPermission(content_type)) {
      content::PermissionResult permission_result =
          profile->GetPermissionController()
              ->GetPermissionResultForOriginWithoutContext(
                  permissions::PermissionUtil::
                      ContentSettingTypeToPermissionType(content_type),
                  url::Origin::Create(origin));
      result =
          permissions::PermissionUtil::ToPermissionResult(permission_result);
    } else {
      permissions::PermissionDecisionAutoBlocker* auto_blocker =
          permissions::PermissionsClient::Get()
              ->GetPermissionDecisionAutoBlocker(profile);
      absl::optional<permissions::PermissionResult> embargo_result =
          auto_blocker->GetEmbargoResult(origin, content_type);
      if (embargo_result)
        result = *embargo_result;
    }
  }

  // Retrieve the source of the content setting.
  *source_string = SiteSettingSourceToString(
      CalculateSiteSettingSource(profile, content_type, origin, info, result));
  *display_name =
      GetDisplayNameForGURL(profile, origin, /*hostname_only=*/false);

  if (info.metadata.session_model == content_settings::SessionModel::OneTime) {
    DCHECK(
        permissions::PermissionUtil::CanPermissionBeAllowedOnce(content_type));
    DCHECK_EQ(result.content_setting, CONTENT_SETTING_ALLOW);
    return CONTENT_SETTING_DEFAULT;
  }
  return result.content_setting;
}

std::vector<ContentSettingPatternSource>
GetSingleOriginExceptionsForContentType(HostContentSettingsMap* map,
                                        ContentSettingsType content_type) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(content_type, &entries);
  // Exclude any entries that don't represent a single webby top-frame origin.
  base::EraseIf(entries, [](const ContentSettingPatternSource& e) {
    return !content_settings::PatternAppliesToSingleOrigin(
               e.primary_pattern, e.secondary_pattern) ||
           PatternAppliesToWebUISchemes(e);
  });
  return entries;
}

void GetFileSystemGrantedEntries(std::vector<base::Value::Dict>* exceptions,
                                 Profile* profile,
                                 bool incognito) {
  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile);
  std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
      grants = permission_context->GetAllGrantedObjects();

  for (const auto& grant : grants) {
    const std::string url = grant->origin.spec();
    auto* const optional_path = grant->value.GetDict().Find(
        ChromeFileSystemAccessPermissionContext::kPermissionPathKey);

    // Ensure that the file path is found for the given kPermissionPathKey.
    if (optional_path) {
      const base::FilePath file_path =
          base::ValueToFilePath(optional_path).value();
      exceptions->push_back(GetFileSystemExceptionForPage(
          ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, profile, url, file_path,
          CONTENT_SETTING_ALLOW,
          SiteSettingSourceToString(SiteSettingSource::kDefault), incognito));
    }
  }
  // Sort exceptions by origin name, alphabetically.
  base::ranges::sort(*exceptions, [](const base::Value::Dict& lhs,
                                     const base::Value::Dict& rhs) {
    return lhs.Find(kOrigin)->GetString() < rhs.Find(kOrigin)->GetString();
  });
}

const ChooserTypeNameEntry* ChooserTypeFromGroupName(base::StringPiece name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name)
      return &chooser_type;
  }
  return nullptr;
}

// Create a base::Value::Dict that will act as a data source for a single row
// in a chooser permission exceptions table. The chooser permission will contain
// a list of site exceptions that correspond to the exception.
base::Value::Dict CreateChooserExceptionObject(
    const std::u16string& display_name,
    const base::Value& object,
    const std::string& chooser_type,
    const ChooserExceptionDetails& chooser_exception_details,
    Profile* profile) {
  base::Value::Dict exception;

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception.Set(kDisplayName, display_name);
  exception.Set(kObject, object.Clone());
  exception.Set(kChooserType, chooser_type);

  // Order the sites by the provider precedence order.
  std::vector<base::Value::Dict>
      all_provider_sites[HostContentSettingsMap::NUM_PROVIDER_TYPES];
  for (const auto& details : chooser_exception_details) {
    const GURL& origin = std::get<0>(details);
    const std::string& source = std::get<1>(details);
    const bool incognito = std::get<2>(details);

    std::string site_display_name = origin.spec();
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Set the |site_display_name| to the extension's name which is more clear
    // to the user if the |origin| is for an extension and the extension name
    // can be found in the |profile|.
    if (origin.SchemeIs(extensions::kExtensionScheme)) {
      DCHECK(profile);
      const auto* extension_registry =
          extensions::ExtensionRegistry::Get(profile);
      const extensions::Extension* extension =
          extension_registry->GetExtensionById(
              origin.host(), extensions::ExtensionRegistry::EVERYTHING);
      if (extension) {
        site_display_name = extension->name();
      }
    }
#endif

    auto& this_provider_sites =
        all_provider_sites[HostContentSettingsMap::GetProviderTypeFromSource(
            source)];
    base::Value::Dict site;
    site.Set(kOrigin, origin.spec());
    site.Set(kDisplayName, site_display_name);
    site.Set(kSetting, setting_string);
    site.Set(kSource, source);
    site.Set(kIncognito, incognito);
    this_provider_sites.push_back(std::move(site));
  }

  base::Value::List sites;
  for (auto& one_provider_sites : all_provider_sites) {
    for (auto& site : one_provider_sites) {
      sites.Append(std::move(site));
    }
  }

  exception.Set(kSites, std::move(sites));
  return exception;
}

base::Value::List GetChooserExceptionListFromProfile(
    Profile* profile,
    const ChooserTypeNameEntry& chooser_type) {
  base::Value::List exceptions;
  ContentSettingsType content_type =
      ContentSettingsTypeFromGroupName(std::string(chooser_type.name));
  DCHECK(content_type != ContentSettingsType::DEFAULT);

  // The BluetoothChooserContext is only available when the
  // WebBluetoothNewPermissionsBackend flag is enabled.
  // TODO(https://crbug.com/589228): Remove the nullptr check when it is enabled
  // by default.
  permissions::ObjectPermissionContextBase* chooser_context =
      chooser_type.get_context(profile);
  if (!chooser_context)
    return exceptions;

  std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
      objects = chooser_context->GetAllGrantedObjects();

  if (profile->HasPrimaryOTRProfile()) {
    Profile* incognito_profile =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    permissions::ObjectPermissionContextBase* incognito_chooser_context =
        chooser_type.get_context(incognito_profile);
    std::vector<
        std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
        incognito_objects = incognito_chooser_context->GetAllGrantedObjects();
    objects.insert(objects.end(),
                   std::make_move_iterator(incognito_objects.begin()),
                   std::make_move_iterator(incognito_objects.end()));
  }

  // Maps from a chooser exception name/object pair to a
  // ChooserExceptionDetails. This will group and sort the exceptions by the UI
  // string and object for display.
  std::map<std::pair<std::u16string, base::Value>, ChooserExceptionDetails>
      all_chooser_objects;
  for (const auto& object : objects) {
    // Don't include WebUI settings.
    if (content::HasWebUIScheme(object->origin))
      continue;

    std::u16string name = chooser_context->GetObjectDisplayName(object->value);
    auto& chooser_exception_details =
        all_chooser_objects[std::make_pair(name, object->value.Clone())];

    std::string source = GetSourceStringForChooserException(
        profile, content_type, object->source);

    chooser_exception_details.insert(
        {object->origin, source, object->incognito});
  }

  for (const auto& all_chooser_objects_entry : all_chooser_objects) {
    const std::u16string& name = all_chooser_objects_entry.first.first;
    const base::Value& object = all_chooser_objects_entry.first.second;
    const ChooserExceptionDetails& chooser_exception_details =
        all_chooser_objects_entry.second;
    exceptions.Append(CreateChooserExceptionObject(
        name, object, chooser_type.name, chooser_exception_details, profile));
  }

  return exceptions;
}

absl::optional<std::string> GetExtensionDisplayName(Profile* profile,
                                                    GURL url) {
  if (!url.SchemeIs(extensions::kExtensionScheme)) {
    return {};
  }
  auto* extension_registry = extensions::ExtensionRegistry::Get(profile);
  if (!extension_registry) {
    return {};
  }
  // For the extension scheme, the pattern must be a valid URL.
  DCHECK(url.is_valid());
  const extensions::Extension* extension = extension_registry->GetExtensionById(
      url.host(), extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return {};
  }
  return extension->name();
}

}  // namespace site_settings

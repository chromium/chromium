// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

namespace site_settings {

constexpr char kAppName[] = "appName";
constexpr char kAppId[] = "appId";

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;
using ::content_settings::ProviderType;
using ::content_settings::SettingSource;

// Chooser data group names.
const char kUsbChooserDataGroupType[] = "usb-devices-data";
const char kSerialChooserDataGroupType[] = "serial-ports-data";
const char kHidChooserDataGroupType[] = "hid-devices-data";
const char kBluetoothChooserDataGroupType[] = "bluetooth-devices-data";

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
    // The following ContentSettingsTypes have UI in Content Settings
    // and require a mapping from their Javascript string representation in
    // chrome/browser/resources/settings/site_settings/constants.ts to their C++
    // ContentSettingsType provided here. These group names are only used by
    // desktop webui.
    {ContentSettingsType::COOKIES, "cookies"},
    {ContentSettingsType::IMAGES, "images"},
    {ContentSettingsType::JAVASCRIPT, "javascript"},
    {ContentSettingsType::JAVASCRIPT_JIT, "javascript-jit"},
    {ContentSettingsType::JAVASCRIPT_OPTIMIZER, "javascript-optimizer"},
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
    {ContentSettingsType::HAND_TRACKING, "hand-tracking"},
    {ContentSettingsType::BLUETOOTH_GUARD, "bluetooth-devices"},
    {ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
     kBluetoothChooserDataGroupType},
    {ContentSettingsType::WINDOW_MANAGEMENT, "window-management"},
    {ContentSettingsType::LOCAL_FONTS, "local-fonts"},
    {ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
     "file-system-access-handles-data"},
    {ContentSettingsType::FEDERATED_IDENTITY_API, "federated-identity-api"},
    {ContentSettingsType::PRIVATE_NETWORK_GUARD, "private-network-devices"},
    {ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA,
     "private-network-devices-data"},
    {ContentSettingsType::ANTI_ABUSE, "anti-abuse"},
    {ContentSettingsType::STORAGE_ACCESS, "storage-access"},
    {ContentSettingsType::AUTO_PICTURE_IN_PICTURE, "auto-picture-in-picture"},
    {ContentSettingsType::CAPTURED_SURFACE_CONTROL, "captured-surface-control"},
    {ContentSettingsType::WEB_PRINTING, "web-printing"},
    {ContentSettingsType::SPEAKER_SELECTION, "speaker-selection"},
    {ContentSettingsType::AUTOMATIC_FULLSCREEN, "automatic-fullscreen"},
    {ContentSettingsType::KEYBOARD_LOCK, "keyboard-lock"},
    {ContentSettingsType::POINTER_LOCK, "pointer-lock"},
    {ContentSettingsType::TRACKING_PROTECTION, "tracking-protection"},
    {ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, "top-level-storage-access"},
    {ContentSettingsType::WEB_APP_INSTALLATION, "web-app-installation"},
    {ContentSettingsType::SMART_CARD_GUARD, "smart-card-readers"},
    {ContentSettingsType::SMART_CARD_DATA, "smart-card-readers-data"},

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
    {ContentSettingsType::DEPRECATED_ACCESSIBILITY_EVENTS, nullptr},
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
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM, nullptr},
    {ContentSettingsType::INSECURE_PRIVATE_NETWORK, nullptr},
    {ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA, nullptr},
    {ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, nullptr},
    {ContentSettingsType::DISPLAY_CAPTURE, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_SHARING, nullptr},
    {ContentSettingsType::HTTP_ALLOWED, nullptr},
    {ContentSettingsType::HTTPS_ENFORCED, nullptr},
    {ContentSettingsType::FORMFILL_METADATA, nullptr},
    {ContentSettingsType::DEPRECATED_FEDERATED_IDENTITY_ACTIVE_SESSION,
     nullptr},
    {ContentSettingsType::AUTO_DARK_WEB_CONTENT, nullptr},
    {ContentSettingsType::REQUEST_DESKTOP_SITE, nullptr},
    {ContentSettingsType::NOTIFICATION_INTERACTIONS, nullptr},
    {ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, nullptr},
    {ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
     nullptr},
    // PPAPI_BROKER has been deprecated. The content setting is not used or
    // called from UI, so we don't need a representation JS string.
    {ContentSettingsType::DEPRECATED_PPAPI_BROKER, nullptr},
    {ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, nullptr},
    // TODO(crbug.com/40253587): Update JavaScript string representation when
    // desktop UI is implemented.
    {ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION, nullptr},
    {ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
     nullptr},
    {ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING, nullptr},
    {ContentSettingsType::ALL_SCREEN_CAPTURE, nullptr},
    {ContentSettingsType::COOKIE_CONTROLS_METADATA, nullptr},
    {ContentSettingsType::TPCD_TRIAL, nullptr},
    {ContentSettingsType::TPCD_METADATA_GRANTS, nullptr},
    // TODO(crbug.com/40101962): Update the name once the design is finalized
    // for the integration with Safety Hub.
    {ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION, nullptr},
    {ContentSettingsType::TPCD_HEURISTICS_GRANTS, nullptr},
    {ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION, nullptr},
    {ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, nullptr},
    {ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS, nullptr},
    {ContentSettingsType::DIRECT_SOCKETS, nullptr},
    {ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS, nullptr},
    {ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL, nullptr},
    {ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO, nullptr},
    {ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr},
    // TODO(crbug.com/368266658): Implement the UI for Direct Sockets PNA.
    {ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS, nullptr},
};

static_assert(
    std::size(kContentSettingsTypeGroupNames) ==
        // Add one since the sequence is kMinValue = -1, 0, ..., kMaxValue
        1 + static_cast<int32_t>(ContentSettingsType::kMaxValue) -
            static_cast<int32_t>(ContentSettingsType::kMinValue),
    "kContentSettingsTypeGroupNames should have the correct number "
    "of elements");

struct SiteSettingSourceStringMapping {
  SiteSettingSource source;
  const char* source_str;
};

// Determines whether an IWA-specific `content_setting` should be shown for a
// particular `origin`.
bool ShouldShowIwaContentSettingForOrigin(Profile* profile,
                                          std::string_view origin,
                                          ContentSettingsType content_setting) {
  // Show for non-origin-specific lists, IWAs, and non-default values.
  if (origin.empty() || GURL(origin).SchemeIs(chrome::kIsolatedAppScheme)) {
    return true;
  }
  if (!profile) {
    return false;
  }
  SiteSettingSource source;
  GetContentSettingForOrigin(
      profile, HostContentSettingsMapFactory::GetForProfile(profile),
      GURL(origin), content_setting, &source);
  return source != SiteSettingSource::kDefault;
}

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
    const content::PermissionResult result) {
  if (info.source == SettingSource::kAllowList) {
    return SiteSettingSource::kAllowlist;  // Source #1.
  }

  if (result.source == content::PermissionStatusSource::KILL_SWITCH) {
    return SiteSettingSource::kKillSwitch;  // Source #2.
  }

  if (result.source == content::PermissionStatusSource::INSECURE_ORIGIN) {
    return SiteSettingSource::kInsecureOrigin;  // Source #3.
  }

  if (info.source == SettingSource::kPolicy ||
      info.source == SettingSource::kSupervised) {
    return SiteSettingSource::kPolicy;  // Source #4.
  }

  if (info.source == SettingSource::kExtension) {
    return SiteSettingSource::kExtension;  // Source #5.
  }

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

  DCHECK_NE(SettingSource::kNone, info.source);
  if (info.source == SettingSource::kUser) {
    if (result.source == content::PermissionStatusSource::MULTIPLE_DISMISSALS ||
        result.source == content::PermissionStatusSource::MULTIPLE_IGNORES) {
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

  NOTREACHED_IN_MIGRATION();
  return SiteSettingSource::kPreference;
}

bool IsFromWebUIAllowlistSource(const ContentSettingPatternSource& pattern) {
  return pattern.source == ProviderType::kWebuiAllowlistProvider;
}

// If the given |pattern| represents an individual origin, Isolated Web App, or
// extension, retrieve a string to display it as such. If not, return the
// pattern as a string.
std::string GetDisplayNameForPattern(Profile* profile,
                                     const ContentSettingsPattern& pattern) {
  GURL url(pattern.ToString());
  if (url.is_valid() && (url.SchemeIs(extensions::kExtensionScheme) ||
                         url.SchemeIs(chrome::kIsolatedAppScheme))) {
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
        CONTENT_SETTING_ALLOW, SiteSettingSource::kPolicy,
        // Pass base::Time() to indicate the exceptions do not expire.
        base::Time(), incognito));
  }
}

// Retrieves the source of a chooser exception as a string. This method uses the
// CalculateSiteSettingSource method above to calculate the correct string to
// use.
SiteSettingSource GetSourceForChooserException(Profile* profile,
                                               ContentSettingsType content_type,
                                               SettingSource source) {
  // Prepare the parameters needed by CalculateSiteSettingSource
  content_settings::SettingInfo info;
  info.source = source;

  // Chooser exceptions do not use a PermissionContextBase for their
  // permissions.
  content::PermissionResult permission_result(
      PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED);

  // The |origin| parameter is only used for |ContentSettingsType::ADS| with
  // the |kSafeBrowsingSubresourceFilter| feature flag enabled, so an empty GURL
  // is used.
  SiteSettingSource calculated_source = CalculateSiteSettingSource(
      profile, content_type, /*origin=*/GURL(), info, permission_result);
  DCHECK(calculated_source == SiteSettingSource::kPolicy ||
         calculated_source == SiteSettingSource::kPreference);
  return calculated_source;
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
// TODO(crbug.com/40458188): Remove the feature check when it is enabled
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

// These variables represent different formatting options for default (i.e. not
// extension or IWA) URLs as well as fallbacks for when the IWA/extension is not
// found in the registry.
constexpr UrlIdentity::FormatOptions kUrlIdentityOptionsOmitHttps = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptionsHostOnly = {
    .default_options = {UrlIdentity::DefaultFormatOptions::kHostname}};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptionsRawSpec = {
    .default_options = {UrlIdentity::DefaultFormatOptions::kRawSpec}};

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

ContentSettingsType ContentSettingsTypeFromGroupName(std::string_view name) {
  for (const auto& entry : kContentSettingsTypeGroupNames) {
    // Content setting types that aren't represented in the settings UI
    // will have `nullptr` as their `name`. However, converting `nullptr`
    // to a std::string_view will crash, so we have to handle it explicitly
    // before comparing.
    if (entry.name != nullptr && entry.name == name) {
      return entry.type;
    }
  }

  return ContentSettingsType::DEFAULT;
}

std::string_view ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (const auto& entry : kContentSettingsTypeGroupNames) {
    if (type == entry.type) {
      // Content setting types that aren't represented in the settings UI
      // will have `nullptr` as their `name`. Although they are valid content
      // settings types, they don't have a readable name.
      // TODO(crbug.com/40066645): Replace LOG with CHECK.
      if (!entry.name) {
        LOG(ERROR) << static_cast<int32_t>(type)
                   << " does not have a readable name.";
      }

      return entry.name ? entry.name : std::string_view();
    }
  }

  NOTREACHED_IN_MIGRATION() << static_cast<int32_t>(type)
                            << " is not a recognized content settings type.";
  return std::string_view();
}

std::vector<ContentSettingsType> GetVisiblePermissionCategories(
    const std::string& origin,
    Profile* profile) {
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
      ContentSettingsType::JAVASCRIPT_JIT,
      ContentSettingsType::NOTIFICATIONS,
      ContentSettingsType::POPUPS,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
#endif
      ContentSettingsType::SENSORS,
      ContentSettingsType::SERIAL_GUARD,
      ContentSettingsType::SOUND,
      ContentSettingsType::STORAGE_ACCESS,
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
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

    if (base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps)) {
      base_types->push_back(ContentSettingsType::PAYMENT_HANDLER);
    }

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
            network::features::kPrivateNetworkAccessPermissionPrompt)) {
      base_types->push_back(ContentSettingsType::PRIVATE_NETWORK_GUARD);
    }

    if (base::FeatureList::IsEnabled(
            blink::features::kMediaSessionEnterPictureInPicture)) {
      base_types->push_back(ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
    }

    if (base::FeatureList::IsEnabled(blink::features::kSpeakerSelection)) {
      base_types->push_back(ContentSettingsType::SPEAKER_SELECTION);
    }

    if (base::FeatureList::IsEnabled(
            features::kCapturedSurfaceControlKillswitch) &&
        base::FeatureList::IsEnabled(
            features::kCapturedSurfaceControlStickyPermissions)) {
      base_types->push_back(ContentSettingsType::CAPTURED_SURFACE_CONTROL);
    }

    if (base::FeatureList::IsEnabled(
            permissions::features::kKeyboardAndPointerLockPrompt)) {
      base_types->push_back(ContentSettingsType::KEYBOARD_LOCK);
      base_types->push_back(ContentSettingsType::POINTER_LOCK);
    }

#if BUILDFLAG(ENABLE_VR)
    if (device::features::IsHandTrackingEnabled()) {
      base_types->push_back(ContentSettingsType::HAND_TRACKING);
    }
#endif

    if (base::FeatureList::IsEnabled(blink::features::kWebAppInstallation)) {
      base_types->push_back(ContentSettingsType::WEB_APP_INSTALLATION);
    }

    initialized = true;
  }

  // The permission categories below are only shown for certain origins.
  std::vector<ContentSettingsType> types_for_origin = *base_types;
  if (base::FeatureList::IsEnabled(
          features::kAutomaticFullscreenContentSetting) &&
      ShouldShowIwaContentSettingForOrigin(
          profile, origin, ContentSettingsType::AUTOMATIC_FULLSCREEN)) {
    types_for_origin.push_back(ContentSettingsType::AUTOMATIC_FULLSCREEN);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(blink::features::kWebPrinting) &&
      ShouldShowIwaContentSettingForOrigin(profile, origin,
                                           ContentSettingsType::WEB_PRINTING)) {
    types_for_origin.push_back(ContentSettingsType::WEB_PRINTING);
  }
#endif

  return types_for_origin;
}

std::string SiteSettingSourceToString(const SiteSettingSource source) {
  switch (source) {
    case SiteSettingSource::kAllowlist:
      return "allowlist";
    case SiteSettingSource::kAdsFilterBlocklist:
      return "ads-filter-blacklist";
    case SiteSettingSource::kDefault:
      return "default";
    case SiteSettingSource::kEmbargo:
      return "embargo";
    case SiteSettingSource::kExtension:
      return "extension";
    case SiteSettingSource::kHostedApp:
      return "HostedApp";
    case SiteSettingSource::kInsecureOrigin:
      return "insecure-origin";
    case SiteSettingSource::kKillSwitch:
      return "kill-switch";
    case SiteSettingSource::kPolicy:
      return "policy";
    case SiteSettingSource::kPreference:
      return "preference";
    case SiteSettingSource::kNumSources:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

SiteSettingSource ProviderTypeToSiteSettingsSource(
    const ProviderType provider_type) {
  switch (provider_type) {
    case ProviderType::kWebuiAllowlistProvider:
      return SiteSettingSource::kAllowlist;
    case ProviderType::kPolicyProvider:
    case ProviderType::kSupervisedProvider:
      return SiteSettingSource::kPolicy;
    case ProviderType::kCustomExtensionProvider:
      return SiteSettingSource::kExtension;
    case ProviderType::kInstalledWebappProvider:
      return SiteSettingSource::kHostedApp;
    case ProviderType::kOneTimePermissionProvider:
    case ProviderType::kPrefProvider:
      return SiteSettingSource::kPreference;
    case ProviderType::kDefaultProvider:
      return SiteSettingSource::kDefault;

    case ProviderType::kNone:
    case ProviderType::kNotificationAndroidProvider:
    case ProviderType::kProviderForTests:
    case ProviderType::kOtherProviderForTests:
      NOTREACHED_IN_MIGRATION();
      return SiteSettingSource::kPreference;
  }
}

std::string ProviderToDefaultSettingSourceString(const ProviderType provider) {
  switch (provider) {
    case ProviderType::kPolicyProvider:
      return "policy";
    case ProviderType::kSupervisedProvider:
      return "supervised_user";
    case ProviderType::kCustomExtensionProvider:
      return "extension";
    case ProviderType::kOneTimePermissionProvider:
    case ProviderType::kPrefProvider:
      return "preference";
    case ProviderType::kInstalledWebappProvider:
    case ProviderType::kWebuiAllowlistProvider:
    case ProviderType::kDefaultProvider:
      return "default";

    case ProviderType::kNone:
    case ProviderType::kNotificationAndroidProvider:
    case ProviderType::kProviderForTests:
    case ProviderType::kOtherProviderForTests:
      NOTREACHED_IN_MIGRATION();
      return "preference";
  }
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
    SiteSettingSource source,
    bool incognito,
    bool is_embargoed) {
  base::Value::Dict exception;
  exception.Set(kOrigin, origin);
  // TODO(crbug.com/40101962): Replace `LossyDisplayName` method with a
  // new method that returns the full file path in a human-readable format.
  exception.Set(kDisplayName, file_path.LossyDisplayName());
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  exception.Set(kSetting, setting_string);

  exception.Set(kSource, SiteSettingSourceToString(source));
  exception.Set(kIncognito, incognito);
  exception.Set(kIsEmbargoed, is_embargoed);
  return exception;
}

std::u16string GetExpirationDescription(const base::Time& expiration) {
  CHECK(!expiration.is_null());

  const base::TimeDelta time_diff =
      expiration.LocalMidnight() - base::Time::Now().LocalMidnight();

  // Only exceptions that haven't expired should reach this function.
  // However, there is an edge case where an exception could expire between
  // being fetched and this calculation. So let's always return a valid
  // number, zero.
  int days = std::max(time_diff.InDays(), 0);

  return l10n_util::GetPluralStringFUTF16(IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL,
                                          days);
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
    const SiteSettingSource source,
    const base::Time& expiration,
    bool incognito,
    bool is_embargoed) {
  base::Value::Dict exception;
  exception.Set(kType, ContentSettingsTypeToGroupName(content_type));
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

  // Cookie exception types may have an expiration that should be shown.
  if ((content_type == ContentSettingsType::COOKIES ||
       content_type == ContentSettingsType::TRACKING_PROTECTION) &&
      !expiration.is_null() && !incognito) {
    exception.Set(kDescription, GetExpirationDescription(expiration));
  }

  exception.Set(kSource, SiteSettingSourceToString(source));
  exception.Set(kIncognito, incognito);
  exception.Set(kIsEmbargoed, is_embargoed);
  return exception;
}

std::u16string GetStorageAccessEmbeddingDescription(
    StorageAccessEmbeddingException embedding_sa_exception) {
  if (embedding_sa_exception.is_embargoed) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED);
  }

  if (embedding_sa_exception.expiration.is_null()) {
    return std::u16string();
  }

  return GetExpirationDescription(embedding_sa_exception.expiration);
}

// If the given `pattern` represents an individual origin, Isolated Web App, or
// extension, retrieve a string to display it as such. If not, return the
// pattern without wildcards as a string.
std::string GetStorageAccessDisplayNameForPattern(
    Profile* profile,
    ContentSettingsPattern pattern) {
  GURL url(pattern.ToString());
  if (url.is_valid() && (url.SchemeIs(extensions::kExtensionScheme) ||
                         url.SchemeIs(chrome::kIsolatedAppScheme))) {
    return GetDisplayNameForGURL(profile, url, /*hostname_only=*/false);
  }

  GURL url2 = pattern.ToRepresentativeUrl();
  if (url2.is_valid()) {
    return base::UTF16ToUTF8(FormatUrlForSecurityDisplay(
        url2, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }

  return pattern.ToString();
}

base::Value::Dict GetStorageAccessExceptionForPage(
    Profile* profile,
    const ContentSettingsPattern& pattern,
    const std::string& display_name,
    ContentSetting setting,
    const std::vector<StorageAccessEmbeddingException>& exceptions) {
  CHECK(!exceptions.empty());

  base::Value::Dict exception;
  exception.Set(kOrigin, pattern.ToString());
  exception.Set(kDisplayName, display_name);
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  exception.Set(kSetting, setting_string);

  // If there is only one exception and that exception applies everywhere,
  // i.e. `secondary_pattern` is empty, then don't return exceptions and a
  // static row should be displayed. In practice, this only applies to embargoed
  // sites.
  if (exceptions.size() == 1 &&
      exceptions[0].secondary_pattern == ContentSettingsPattern::Wildcard()) {
    auto& embedding_sa_exception = exceptions[0];

    std::u16string description =
        GetStorageAccessEmbeddingDescription(embedding_sa_exception);
    if (!description.empty()) {
      exception.Set(kDescription, description);
    }

    exception.Set(kIncognito, embedding_sa_exception.is_incognito);
    exception.Set(kExceptions, base::Value::List());
    return exception;
  }

  exception.Set(kCloseDescription,
                l10n_util::GetPluralStringFUTF16(IDS_DEL_SITE_SETTINGS_COUNTER,
                                                 exceptions.size()));
  const int open_description_id =
      (setting == ContentSetting::CONTENT_SETTING_ALLOW)
          ? IDS_SETTINGS_STORAGE_ACCESS_ALLOWED_SITE_LABEL
          : IDS_SETTINGS_STORAGE_ACCESS_BLOCKED_SITE_LABEL;
  exception.Set(kOpenDescription,
                l10n_util::GetStringUTF16(open_description_id));

  base::Value::List embedding_origins;
  for (auto& embedding_sa_exception : exceptions) {
    ContentSettingsPattern secondary_pattern =
        embedding_sa_exception.secondary_pattern;
    base::Value::Dict embedding_exception;
    embedding_exception.Set(
        kEmbeddingOrigin,
        secondary_pattern == ContentSettingsPattern::Wildcard()
            ? std::string()
            : secondary_pattern.ToString());
    embedding_exception.Set(
        kEmbeddingDisplayName,
        GetStorageAccessDisplayNameForPattern(profile, secondary_pattern));

    std::u16string description =
        GetStorageAccessEmbeddingDescription(embedding_sa_exception);
    if (!description.empty()) {
      embedding_exception.Set(kDescription, description);
    }
    embedding_exception.Set(kIncognito, embedding_sa_exception.is_incognito);
    embedding_origins.Append(std::move(embedding_exception));
  }

  exception.Set(kExceptions, std::move(embedding_origins));

  return exception;
}

UrlIdentity GetUrlIdentityForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only) {
  auto origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return {.type = UrlIdentity::Type::kDefault,
            .name = base::UTF8ToUTF16(url.spec())};
  }

  return UrlIdentity::CreateFromUrl(
      profile, origin.GetURL(), kUrlIdentityAllowedTypes,
      hostname_only ? kUrlIdentityOptionsHostOnly
                    : kUrlIdentityOptionsOmitHttps);
}

std::string GetDisplayNameForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only) {
  return base::UTF16ToUTF8(
      GetUrlIdentityForGURL(profile, url, hostname_only).name);
}

using RawPatternSettings =
    std::map<std::pair<ContentSettingsPattern, ProviderType>,
             OnePatternSettings,
             std::greater<>>;

// Fills in `all_patterns_settings` with site exceptions information for the
// given `type` from `profile`.
void GetRawExceptionsForContentSettingsType(
    ContentSettingsType type,
    Profile* profile,
    content::WebUI* web_ui,
    RawPatternSettings& all_patterns_settings) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  for (const auto& setting : map->GetSettingsForOneType(type)) {
    // Don't add default settings.
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.source != ProviderType::kPrefProvider) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incognito settings
    // only, excluding policy-source exceptions as policies cannot specify
    // incognito-only exceptions, meaning these are necesssarily duplicates.
    if (map->IsOffTheRecord() &&
        (!setting.incognito ||
         setting.source == ProviderType::kPolicyProvider)) {
      continue;
    }

    // Don't add allowlisted settings.
    if (IsFromWebUIAllowlistSource(setting)) {
      continue;
    }

    // Don't add auto-granted permissions for storage access exceptions.
    if (IsGrantedByRelatedWebsiteSets(type, setting.metadata) &&
        !base::FeatureList::IsEnabled(
            permissions::features::kShowRelatedWebsiteSetsPermissionGrants)) {
      continue;
    }

    auto content_setting = setting.GetContentSetting();
    // There is no user-facing concept of SESSION_ONLY cookie exceptions that
    // use secondary patterns. These are instead presented as ALLOW.
    // TODO(crbug.com/40251893): Perform a one time migration of the actual
    // content settings when the extension API no-longer allows them to be
    // created.
    if (type == ContentSettingsType::COOKIES &&
        content_setting == ContentSetting::CONTENT_SETTING_SESSION_ONLY &&
        setting.secondary_pattern != ContentSettingsPattern::Wildcard()) {
      content_setting = ContentSetting::CONTENT_SETTING_ALLOW;
    }

    all_patterns_settings[{setting.primary_pattern, setting.source}][{
        setting.secondary_pattern, setting.incognito}] = {
        content_setting, /*is_embargoed=*/false, setting.metadata.expiration()};
  }

  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          profile);

  for (const auto& setting : map->GetSettingsForOneType(
           ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA)) {
    // Off-the-record HostContentSettingsMap contains incognito content
    // settings as well as normal content settings. Here, we use the
    // incognito settings only.
    if (map->IsOffTheRecord() && !setting.incognito) {
      continue;
    }

    if (!permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
            type)) {
      continue;
    }

    if (auto_blocker->IsEmbargoed(GURL(setting.primary_pattern.ToString()),
                                  type)) {
      all_patterns_settings[{setting.primary_pattern, setting.source}]
                           [{setting.secondary_pattern, setting.incognito}] = {
                               CONTENT_SETTING_BLOCK, /*is_embargoed=*/true,
                               setting.metadata.expiration()};
    }
  }
}

void Append3pcExceptions(Profile* profile,
                         content::WebUI* web_ui,
                         base::Value::List* exceptions) {
  base::Value::List cookie_exceptions;
  GetExceptionsForContentType(ContentSettingsType::COOKIES, profile, web_ui,
                              /*incognito=*/false, &cookie_exceptions);
  for (auto& cookie_exception : cookie_exceptions) {
    auto& dict = cookie_exception.GetDict();
    if (dict.contains(kOrigin) && *dict.FindString(kOrigin) == "*") {
      dict.Set(kDescription,
               l10n_util::GetStringUTF16(
                   IDS_SETTINGS_THIRD_PARTY_COOKIES_ONLY_EXCEPTION_LABEL));
      exceptions->Append(std::move(cookie_exception));
    }
  }
}

void GetExceptionsForContentType(ContentSettingsType type,
                                 Profile* profile,
                                 content::WebUI* web_ui,
                                 bool incognito,
                                 base::Value::List* exceptions) {
  // Group settings by primary_pattern.
  RawPatternSettings all_patterns_settings;

  GetRawExceptionsForContentSettingsType(type, profile, web_ui,
                                         all_patterns_settings);

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::map<ProviderType, std::vector<base::Value::Dict>>
      all_provider_exceptions;

  for (const auto& [primary_pattern_and_source, one_settings] :
       all_patterns_settings) {
    const auto& [primary_pattern, source] = primary_pattern_and_source;
    const std::string display_name =
        GetDisplayNameForPattern(profile, primary_pattern);

    auto& this_provider_exceptions = all_provider_exceptions[source];

    for (const auto& secondary_setting : one_settings) {
      const SiteExceptionInfo& site_exception_info = secondary_setting.second;
      const auto& [secondary_pattern, is_incognito] = secondary_setting.first;
      this_provider_exceptions.push_back(GetExceptionForPage(
          type, profile, primary_pattern, secondary_pattern,
          std::move(display_name), site_exception_info.content_setting,
          ProviderTypeToSiteSettingsSource(source),
          site_exception_info.expiration, is_incognito,
          site_exception_info.is_embargoed));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == ContentSettingsType::MEDIASTREAM_MIC ||
      type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    auto& policy_exceptions =
        all_provider_exceptions[ProviderType::kPolicyProvider];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions, web_ui, incognito);
  }

  // Display the URLs with File System entries that are granted
  // permissions via File System Access Persistent Permissions.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions) &&
      (type == ContentSettingsType::FILE_SYSTEM_READ_GUARD ||
       type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD)) {
    auto& urls_with_granted_entries =
        all_provider_exceptions[ProviderType::kDefaultProvider];
    GetFileSystemGrantedEntries(&urls_with_granted_entries, profile, incognito);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions.second) {
      exceptions->Append(std::move(exception));
    }
  }

  // The TP exceptions list should also contain 3PC exceptions.
  if (type == ContentSettingsType::TRACKING_PROTECTION) {
    Append3pcExceptions(profile, web_ui, exceptions);
  }
}

void GetStorageAccessExceptions(ContentSetting content_setting,
                                Profile* profile,
                                Profile* incognito_profile,
                                content::WebUI* web_ui,
                                base::Value::List* exceptions) {
  ContentSettingsType type = ContentSettingsType::STORAGE_ACCESS;

  // Group settings by primary_pattern.
  RawPatternSettings all_patterns_settings;

  GetRawExceptionsForContentSettingsType(type, profile, web_ui,
                                         all_patterns_settings);

  if (incognito_profile) {
    GetRawExceptionsForContentSettingsType(type, incognito_profile, web_ui,
                                           all_patterns_settings);
  }

  for (const auto& [primary_pattern_and_source, one_settings] :
       all_patterns_settings) {
    const auto& [primary_pattern, source] = primary_pattern_and_source;

    std::vector<StorageAccessEmbeddingException> sa_exceptions;

    for (const auto& secondary_setting : one_settings) {
      const SiteExceptionInfo& site_exception_info = secondary_setting.second;
      const auto& [secondary_pattern, is_incognito] = secondary_setting.first;

      if (site_exception_info.content_setting != content_setting) {
        continue;
      }

      sa_exceptions.push_back({secondary_pattern, is_incognito,
                               site_exception_info.is_embargoed,
                               site_exception_info.expiration});
    }

    if (sa_exceptions.empty()) {
      continue;
    }

    // TODO(http://b/289788055): Remove wildcards.
    const std::string display_name =
        GetStorageAccessDisplayNameForPattern(profile, primary_pattern);

    exceptions->Append(GetStorageAccessExceptionForPage(
        profile, primary_pattern, std::move(display_name), content_setting,
        sa_exceptions));
  }
}

void GetContentCategorySetting(const HostContentSettingsMap* map,
                               ContentSettingsType content_type,
                               base::Value::Dict* object) {
  auto provider = ProviderType::kDefaultProvider;
  std::string setting = content_settings::ContentSettingToString(
      map->GetDefaultContentSetting(content_type, &provider));
  DCHECK(!setting.empty());

  object->Set(kSetting, setting);
  if (provider != ProviderType::kDefaultProvider) {
    object->Set(kSource, ProviderToDefaultSettingSourceString(provider));
  }
}

ContentSetting GetContentSettingForOrigin(Profile* profile,
                                          const HostContentSettingsMap* map,
                                          const GURL& origin,
                                          ContentSettingsType content_type,
                                          SiteSettingSource* source) {
  // TODO(patricialor): In future, PermissionManager should know about all
  // content settings, not just the permissions, plus all the possible sources,
  // and the calls to HostContentSettingsMap should be removed.
  content_settings::SettingInfo info;
  ContentSetting setting =
      map->GetContentSetting(origin, origin, content_type, &info);

  // Retrieve the content setting.
  content::PermissionResult result(
      permissions::PermissionUtil::ContentSettingToPermissionStatus(setting),
      content::PermissionStatusSource::UNSPECIFIED);
  if (permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
          content_type)) {
    if (permissions::PermissionUtil::IsPermission(content_type)) {
      result = profile->GetPermissionController()
                   ->GetPermissionResultForOriginWithoutContext(
                       permissions::PermissionUtil::
                           ContentSettingTypeToPermissionType(content_type),
                       url::Origin::Create(origin));
    } else {
      permissions::PermissionDecisionAutoBlocker* auto_blocker =
          permissions::PermissionsClient::Get()
              ->GetPermissionDecisionAutoBlocker(profile);
      std::optional<content::PermissionResult> embargo_result =
          auto_blocker->GetEmbargoResult(origin, content_type);
      if (embargo_result) {
        result = embargo_result.value();
      }
    }
  }

  // Retrieve the source of the content setting.
  *source =
      CalculateSiteSettingSource(profile, content_type, origin, info, result);

  if (info.metadata.session_model() ==
      content_settings::mojom::SessionModel::ONE_TIME) {
    DCHECK(
        permissions::PermissionUtil::DoesSupportTemporaryGrants(content_type));
    DCHECK_EQ(result.status, PermissionStatus::GRANTED);
    return CONTENT_SETTING_DEFAULT;
  }
  return permissions::PermissionUtil::PermissionStatusToContentSetting(
      result.status);
}

std::vector<ContentSettingPatternSource>
GetSingleOriginExceptionsForContentType(HostContentSettingsMap* map,
                                        ContentSettingsType content_type) {
  ContentSettingsForOneType entries = map->GetSettingsForOneType(content_type);
  // Exclude any entries that are allowlisted or don't represent a single
  // top-frame origin.
  std::erase_if(entries, [](const ContentSettingPatternSource& e) {
    return !content_settings::PatternAppliesToSingleOrigin(
               e.primary_pattern, e.secondary_pattern) ||
           IsFromWebUIAllowlistSource(e);
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
    auto* const optional_path = grant->value.Find(
        ChromeFileSystemAccessPermissionContext::kPermissionPathKey);

    // Ensure that the file path is found for the given kPermissionPathKey.
    if (optional_path) {
      const base::FilePath file_path =
          base::ValueToFilePath(optional_path).value();
      exceptions->push_back(GetFileSystemExceptionForPage(
          ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, profile, url, file_path,
          CONTENT_SETTING_ALLOW, SiteSettingSource::kDefault, incognito));
    }
  }
  // Sort exceptions by origin name, alphabetically.
  base::ranges::sort(*exceptions, [](const base::Value::Dict& lhs,
                                     const base::Value::Dict& rhs) {
    return lhs.Find(kOrigin)->GetString() < rhs.Find(kOrigin)->GetString();
  });
}

const ChooserTypeNameEntry* ChooserTypeFromGroupName(std::string_view name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name) {
      return &chooser_type;
    }
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
  std::map<SiteSettingSource, std::vector<base::Value::Dict>>
      all_provider_sites;
  for (const auto& details : chooser_exception_details) {
    const GURL& origin = std::get<0>(details);
    const SiteSettingSource source = std::get<1>(details);
    const bool incognito = std::get<2>(details);

    std::string site_display_name = base::UTF16ToUTF8(
        UrlIdentity::CreateFromUrl(profile, origin, kUrlIdentityAllowedTypes,
                                   kUrlIdentityOptionsRawSpec)
            .name);

    auto& this_provider_sites = all_provider_sites[source];
    base::Value::Dict site;
    site.Set(kOrigin, origin.spec());
    site.Set(kDisplayName, site_display_name);
    site.Set(kSetting, setting_string);
    site.Set(kSource, SiteSettingSourceToString(source));
    site.Set(kIncognito, incognito);
    this_provider_sites.push_back(std::move(site));
  }

  base::Value::List sites;
  for (auto& one_provider_sites : all_provider_sites) {
    for (auto& site : one_provider_sites.second) {
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
  // TODO(crbug.com/40458188): Remove the nullptr check when it is enabled
  // by default.
  permissions::ObjectPermissionContextBase* chooser_context =
      chooser_type.get_context(profile);
  if (!chooser_context) {
    return exceptions;
  }

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
    if (content::HasWebUIScheme(object->origin)) {
      continue;
    }

    std::u16string name = chooser_context->GetObjectDisplayName(object->value);
    auto& chooser_exception_details = all_chooser_objects[std::make_pair(
        name, base::Value(object->value.Clone()))];

    SiteSettingSource source =
        GetSourceForChooserException(profile, content_type, object->source);

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

std::vector<web_app::IsolatedWebAppUrlInfo> GetInstalledIsolatedWebApps(
    Profile* profile) {
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!web_app_provider) {
    return {};
  }

  std::vector<web_app::IsolatedWebAppUrlInfo> iwas;
  web_app::WebAppRegistrar& registrar = web_app_provider->registrar_unsafe();
  for (const web_app::WebApp& web_app : registrar.GetApps()) {
    if (!registrar.IsIsolated(web_app.app_id())) {
      continue;
    }
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(web_app.scope());
    if (url_info.has_value()) {
      iwas.push_back(*url_info);
    }
  }
  return iwas;
}

}  // namespace site_settings

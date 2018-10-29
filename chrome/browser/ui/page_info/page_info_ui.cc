// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/password_protection/password_protection_service.h"
#endif

namespace {

const int kInvalidResourceID = -1;

#if !defined(OS_ANDROID)
// The icon size is actually 16, but the vector icons being used generally all
// have additional internal padding. Account for this difference by asking for
// the vectors in 18x18dip sizes.
constexpr int kVectorIconSize = 18;
#endif

// The resource IDs for the strings that are displayed on the permissions
// button if the permission setting is managed by policy.
const int kPermissionButtonTextIDPolicyManaged[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_POLICY,
    IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_POLICY,
    IDS_PAGE_INFO_PERMISSION_ASK_BY_POLICY,
    kInvalidResourceID,
    kInvalidResourceID};
static_assert(arraysize(kPermissionButtonTextIDPolicyManaged) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kPermissionButtonTextIDPolicyManaged array size is incorrect");

// The resource IDs for the strings that are displayed on the permissions
// button if the permission setting is managed by an extension.
const int kPermissionButtonTextIDExtensionManaged[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_EXTENSION,
    IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_EXTENSION,
    IDS_PAGE_INFO_PERMISSION_ASK_BY_EXTENSION,
    kInvalidResourceID,
    kInvalidResourceID};
static_assert(arraysize(kPermissionButtonTextIDExtensionManaged) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kPermissionButtonTextIDExtensionManaged array size is "
              "incorrect");

// The resource IDs for the strings that are displayed on the permissions
// button if the permission setting is managed by the user.
const int kPermissionButtonTextIDUserManaged[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER,
    IDS_PAGE_INFO_BUTTON_TEXT_BLOCKED_BY_USER,
    IDS_PAGE_INFO_BUTTON_TEXT_ASK_BY_USER,
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_DETECT_IMPORTANT_CONTENT_BY_USER};
static_assert(arraysize(kPermissionButtonTextIDUserManaged) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kPermissionButtonTextIDUserManaged array size is incorrect");

// The resource IDs for the strings that are displayed on the permissions
// button if the permission setting is the global default setting.
const int kPermissionButtonTextIDDefaultSetting[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_DEFAULT,
    IDS_PAGE_INFO_BUTTON_TEXT_BLOCKED_BY_DEFAULT,
    IDS_PAGE_INFO_BUTTON_TEXT_ASK_BY_DEFAULT,
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_DETECT_IMPORTANT_CONTENT_BY_DEFAULT};
static_assert(base::size(kPermissionButtonTextIDDefaultSetting) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kPermissionButtonTextIDDefaultSetting array size is incorrect");

#if !defined(OS_ANDROID)
// The resource IDs for the strings that are displayed on the sound permission
// button if the sound permission setting is managed by the user.
const int kSoundPermissionButtonTextIDUserManaged[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER,
    IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_USER,
    kInvalidResourceID,
    kInvalidResourceID,
    kInvalidResourceID};
static_assert(
    base::size(kSoundPermissionButtonTextIDUserManaged) ==
        CONTENT_SETTING_NUM_SETTINGS,
    "kSoundPermissionButtonTextIDUserManaged array size is incorrect");

// The resource IDs for the strings that are displayed on the sound permission
// button if the permission setting is the global default setting and the
// block autoplay preference is disabled.
const int kSoundPermissionButtonTextIDDefaultSetting[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_DEFAULT,
    IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_DEFAULT,
    kInvalidResourceID,
    kInvalidResourceID,
    kInvalidResourceID};
static_assert(
    base::size(kSoundPermissionButtonTextIDDefaultSetting) ==
        CONTENT_SETTING_NUM_SETTINGS,
    "kSoundPermissionButtonTextIDDefaultSetting array size is incorrect");
#endif

struct PermissionsUIInfo {
  ContentSettingsType type;
  int string_id;
};

const PermissionsUIInfo kPermissionsUIInfo[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, 0},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_PAGE_INFO_TYPE_IMAGES},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_PAGE_INFO_TYPE_JAVASCRIPT},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_PAGE_INFO_TYPE_POPUPS_REDIRECTS},
#if BUILDFLAG(ENABLE_PLUGINS)
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_PAGE_INFO_TYPE_FLASH},
#endif
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, IDS_PAGE_INFO_TYPE_LOCATION},
    {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, IDS_PAGE_INFO_TYPE_NOTIFICATIONS},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, IDS_PAGE_INFO_TYPE_MIC},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, IDS_PAGE_INFO_TYPE_CAMERA},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
     IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, IDS_PAGE_INFO_TYPE_MIDI_SYSEX},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, IDS_PAGE_INFO_TYPE_BACKGROUND_SYNC},
    {CONTENT_SETTINGS_TYPE_AUTOPLAY, IDS_PAGE_INFO_TYPE_AUTOPLAY},
    {CONTENT_SETTINGS_TYPE_ADS, IDS_PAGE_INFO_TYPE_ADS},
    {CONTENT_SETTINGS_TYPE_SOUND, IDS_PAGE_INFO_TYPE_SOUND},
    {CONTENT_SETTINGS_TYPE_CLIPBOARD_READ, IDS_PAGE_INFO_TYPE_CLIPBOARD},
    {CONTENT_SETTINGS_TYPE_SENSORS, IDS_PAGE_INFO_TYPE_SENSORS},
    {CONTENT_SETTINGS_TYPE_USB_GUARD, IDS_PAGE_INFO_TYPE_USB},
};

std::unique_ptr<PageInfoUI::SecurityDescription> CreateSecurityDescription(
    PageInfoUI::SecuritySummaryColor style,
    int summary_id,
    int details_id) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description(
      new PageInfoUI::SecurityDescription());
  security_description->summary_style = style;
  security_description->summary = l10n_util::GetStringUTF16(summary_id);
  security_description->details = l10n_util::GetStringUTF16(details_id);
  return security_description;
}

// Gets the actual setting for a ContentSettingType, taking into account what
// the default setting value is and whether Html5ByDefault is enabled.
ContentSetting GetEffectiveSetting(Profile* profile,
                                   ContentSettingsType type,
                                   ContentSetting setting,
                                   ContentSetting default_setting) {
  ContentSetting effective_setting = setting;
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = default_setting;

#if BUILDFLAG(ENABLE_PLUGINS)
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  effective_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map, type, effective_setting);

  // Display the UI string for ASK instead of DETECT for HTML5 by Default.
  // TODO(tommycli): Once HTML5 by Default is shipped and the feature flag
  // is removed, just migrate the actual content setting to ASK.
  if (PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map) &&
      effective_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT) {
    effective_setting = CONTENT_SETTING_ASK;
  }
#endif
  return effective_setting;
}

}  // namespace

PageInfoUI::CookieInfo::CookieInfo() : allowed(-1), blocked(-1) {}

PageInfoUI::PermissionInfo::PermissionInfo()
    : type(CONTENT_SETTINGS_TYPE_DEFAULT),
      setting(CONTENT_SETTING_DEFAULT),
      default_setting(CONTENT_SETTING_DEFAULT),
      source(content_settings::SETTING_SOURCE_NONE),
      is_incognito(false) {}

PageInfoUI::ChosenObjectInfo::ChosenObjectInfo(
    const PageInfo::ChooserUIInfo& ui_info,
    std::unique_ptr<base::DictionaryValue> object)
    : ui_info(ui_info), object(std::move(object)) {}

PageInfoUI::ChosenObjectInfo::~ChosenObjectInfo() {}

PageInfoUI::IdentityInfo::IdentityInfo()
    : identity_status(PageInfo::SITE_IDENTITY_STATUS_UNKNOWN),
      connection_status(PageInfo::SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button(false),
      show_change_password_buttons(false) {}

PageInfoUI::IdentityInfo::~IdentityInfo() {}

std::unique_ptr<PageInfoUI::SecurityDescription>
PageInfoUI::GetSecurityDescription(const IdentityInfo& identity_info) const {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description(
      new PageInfoUI::SecurityDescription());

  switch (identity_info.identity_status) {
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
#if defined(OS_ANDROID)
      // We provide identical summary and detail strings for Android, which
      // deduplicates them in the UI code.
      return CreateSecurityDescription(SecuritySummaryColor::GREEN,
                                       IDS_PAGE_INFO_INTERNAL_PAGE,
                                       IDS_PAGE_INFO_INTERNAL_PAGE);
#else
      // Internal pages on desktop have their own UI implementations which
      // should never call this function.
      NOTREACHED();
      FALLTHROUGH;
#endif
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
      switch (identity_info.connection_status) {
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_NOT_SECURE_SUMMARY,
                                           IDS_PAGE_INFO_NOT_SECURE_DETAILS);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY,
                                           IDS_PAGE_INFO_NOT_SECURE_DETAILS);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY,
                                           IDS_PAGE_INFO_MIXED_CONTENT_DETAILS);
        default:
          return CreateSecurityDescription(SecuritySummaryColor::GREEN,
                                           IDS_PAGE_INFO_SECURE_SUMMARY,
                                           IDS_PAGE_INFO_SECURE_DETAILS);
      }
    case PageInfo::SITE_IDENTITY_STATUS_MALWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_MALWARE_SUMMARY,
                                       IDS_PAGE_INFO_MALWARE_DETAILS);
    case PageInfo::SITE_IDENTITY_STATUS_SOCIAL_ENGINEERING:
      return CreateSecurityDescription(
          SecuritySummaryColor::RED, IDS_PAGE_INFO_SOCIAL_ENGINEERING_SUMMARY,
          IDS_PAGE_INFO_SOCIAL_ENGINEERING_DETAILS);
    case PageInfo::SITE_IDENTITY_STATUS_UNWANTED_SOFTWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_UNWANTED_SOFTWARE_SUMMARY,
                                       IDS_PAGE_INFO_UNWANTED_SOFTWARE_DETAILS);
    case PageInfo::SITE_IDENTITY_STATUS_SIGN_IN_PASSWORD_REUSE:
#if defined(SAFE_BROWSING_DB_LOCAL)
      return CreateSecurityDescriptionForPasswordReuse(
          /*is_enterprise_password=*/false);
#endif
    case PageInfo::SITE_IDENTITY_STATUS_ENTERPRISE_PASSWORD_REUSE:
#if defined(SAFE_BROWSING_DB_LOCAL)
      return CreateSecurityDescriptionForPasswordReuse(
          /*is_enterprise_password=*/true);
#endif
    case PageInfo::SITE_IDENTITY_STATUS_BILLING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_BILLING_SUMMARY,
                                       IDS_PAGE_INFO_BILLING_DETAILS);
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    default:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_NOT_SECURE_SUMMARY,
                                       IDS_PAGE_INFO_NOT_SECURE_DETAILS);
  }
}

PageInfoUI::~PageInfoUI() {}

// static
base::string16 PageInfoUI::PermissionTypeToUIString(ContentSettingsType type) {
  for (const PermissionsUIInfo& info : kPermissionsUIInfo) {
    if (info.type == type)
      return l10n_util::GetStringUTF16(info.string_id);
  }
  NOTREACHED();
  return base::string16();
}

// static
base::string16 PageInfoUI::PermissionActionToUIString(
    Profile* profile,
    ContentSettingsType type,
    ContentSetting setting,
    ContentSetting default_setting,
    content_settings::SettingSource source) {
  ContentSetting effective_setting =
      GetEffectiveSetting(profile, type, setting, default_setting);
  const int* button_text_ids = NULL;
  switch (source) {
    case content_settings::SETTING_SOURCE_USER:
      if (setting == CONTENT_SETTING_DEFAULT) {
#if !defined(OS_ANDROID)
        if (type == CONTENT_SETTINGS_TYPE_SOUND &&
            base::FeatureList::IsEnabled(media::kAutoplayDisableSettings)) {
          // If the block autoplay enabled preference is enabled and the
          // sound default setting is ALLOW, we will return a custom string
          // indicating that Chrome is controlling autoplay and sound
          // automatically.
          if (profile->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled) &&
              effective_setting == ContentSetting::CONTENT_SETTING_ALLOW) {
            return l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_BUTTON_TEXT_AUTOMATIC_BY_DEFAULT);
          }

          button_text_ids = kSoundPermissionButtonTextIDDefaultSetting;
          break;
        }
#endif

        button_text_ids = kPermissionButtonTextIDDefaultSetting;
        break;
      }
      FALLTHROUGH;
    case content_settings::SETTING_SOURCE_POLICY:
    case content_settings::SETTING_SOURCE_EXTENSION:
#if !defined(OS_ANDROID)
      if (type == CONTENT_SETTINGS_TYPE_SOUND &&
          base::FeatureList::IsEnabled(media::kAutoplayDisableSettings)) {
        button_text_ids = kSoundPermissionButtonTextIDUserManaged;
        break;
      }
#endif

      button_text_ids = kPermissionButtonTextIDUserManaged;
      break;
    case content_settings::SETTING_SOURCE_WHITELIST:
    case content_settings::SETTING_SOURCE_NONE:
    default:
      NOTREACHED();
      return base::string16();
  }
  int button_text_id = button_text_ids[effective_setting];
  DCHECK_NE(button_text_id, kInvalidResourceID);
  return l10n_util::GetStringUTF16(button_text_id);
}

// static
base::string16 PageInfoUI::PermissionDecisionReasonToUIString(
    Profile* profile,
    const PageInfoUI::PermissionInfo& permission,
    const GURL& url) {
  ContentSetting effective_setting = GetEffectiveSetting(
      profile, permission.type, permission.setting, permission.default_setting);
  int message_id = kInvalidResourceID;
  switch (permission.source) {
    case content_settings::SettingSource::SETTING_SOURCE_POLICY:
      message_id = kPermissionButtonTextIDPolicyManaged[effective_setting];
      break;
    case content_settings::SettingSource::SETTING_SOURCE_EXTENSION:
      message_id = kPermissionButtonTextIDExtensionManaged[effective_setting];
      break;
    default:
      break;
  }

  if (permission.setting == CONTENT_SETTING_BLOCK &&
      PermissionUtil::IsPermission(permission.type)) {
    PermissionResult permission_result =
        PermissionManager::Get(profile)->GetPermissionStatus(permission.type,
                                                             url, url);
    switch (permission_result.source) {
      case PermissionStatusSource::MULTIPLE_DISMISSALS:
        message_id = IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED;
        break;
      default:
        break;
    }
  }

  if (permission.type == CONTENT_SETTINGS_TYPE_ADS)
    message_id = IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE;

  if (message_id == kInvalidResourceID)
    return base::string16();
  return l10n_util::GetStringUTF16(message_id);
}

// static
SkColor PageInfoUI::GetSecondaryTextColor() {
  return SK_ColorGRAY;
}

// static
base::string16 PageInfoUI::ChosenObjectToUIString(
    const ChosenObjectInfo& object) {
  base::string16 name;
  object.object->GetString(object.ui_info.ui_name_key, &name);
  return name;
}

#if defined(OS_ANDROID)
// static
int PageInfoUI::GetIdentityIconID(PageInfo::SiteIdentityStatus status) {
  int resource_id = IDR_PAGEINFO_INFO;
  switch (status) {
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
      break;
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
      resource_id = IDR_PAGEINFO_GOOD;
      break;
    case PageInfo::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
      resource_id = IDR_PAGEINFO_WARNING_MAJOR;
      break;
    case PageInfo::SITE_IDENTITY_STATUS_ERROR:
      resource_id = IDR_PAGEINFO_BAD;
      break;
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
      resource_id = IDR_PAGEINFO_ENTERPRISE_MANAGED;
      break;
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    default:
      NOTREACHED();
      break;
  }
  return resource_id;
}

// static
int PageInfoUI::GetConnectionIconID(PageInfo::SiteConnectionStatus status) {
  int resource_id = IDR_PAGEINFO_INFO;
  switch (status) {
    case PageInfo::SITE_CONNECTION_STATUS_UNKNOWN:
    case PageInfo::SITE_CONNECTION_STATUS_INTERNAL_PAGE:
      break;
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED:
      resource_id = IDR_PAGEINFO_GOOD;
      break;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    case PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED:
      resource_id = IDR_PAGEINFO_WARNING_MAJOR;
      break;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      resource_id = IDR_PAGEINFO_BAD;
      break;
  }
  return resource_id;
}
#else  // !defined(OS_ANDROID)
// static
const gfx::ImageSkia PageInfoUI::GetPermissionIcon(const PermissionInfo& info,
                                                   SkColor related_text_color) {
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (info.type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      icon = &kCookieIcon;
      break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
      icon = &kPhotoIcon;
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      icon = &kCodeIcon;
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      icon = &kLaunchIcon;
      break;
#if BUILDFLAG(ENABLE_PLUGINS)
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      icon = &kExtensionIcon;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      icon = &vector_icons::kLocationOnIcon;
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      icon = &vector_icons::kNotificationsIcon;
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      icon = &vector_icons::kMicIcon;
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      icon = &vector_icons::kVideocamIcon;
      break;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      icon = &kFileDownloadIcon;
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      icon = &vector_icons::kMidiIcon;
      break;
    case CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
      icon = &kSyncIcon;
      break;
    case CONTENT_SETTINGS_TYPE_ADS:
      icon = &kAdsIcon;
      break;
    case CONTENT_SETTINGS_TYPE_SOUND:
      icon = &kVolumeUpIcon;
      break;
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
      icon = &kPageInfoContentPasteIcon;
      break;
    case CONTENT_SETTINGS_TYPE_SENSORS:
      icon = &kSensorsIcon;
      break;
    case CONTENT_SETTINGS_TYPE_USB_GUARD:
      icon = &vector_icons::kUsbIcon;
      break;
    default:
      // All other |ContentSettingsType|s do not have icons on desktop or are
      // not shown in the Page Info bubble.
      NOTREACHED();
      break;
  }

  ContentSetting setting = info.setting == CONTENT_SETTING_DEFAULT
                               ? info.default_setting
                               : info.setting;
  if (setting == CONTENT_SETTING_BLOCK) {
    return gfx::CreateVectorIconWithBadge(
        *icon, kVectorIconSize,
        color_utils::DeriveDefaultIconColor(related_text_color),
        kBlockedBadgeIcon);
  }
  return gfx::CreateVectorIcon(
      *icon, kVectorIconSize,
      color_utils::DeriveDefaultIconColor(related_text_color));
}

// static
const gfx::ImageSkia PageInfoUI::GetChosenObjectIcon(
    const ChosenObjectInfo& object,
    bool deleted,
    SkColor related_text_color) {
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
            object.ui_info.content_settings_type);
  const gfx::VectorIcon* icon = &vector_icons::kUsbIcon;
  if (deleted) {
    return gfx::CreateVectorIconWithBadge(
        *icon, kVectorIconSize,
        color_utils::DeriveDefaultIconColor(related_text_color),
        kBlockedBadgeIcon);
  }
  return gfx::CreateVectorIcon(
      *icon, kVectorIconSize,
      color_utils::DeriveDefaultIconColor(related_text_color));
}

// static
const gfx::ImageSkia PageInfoUI::GetCertificateIcon(
    const SkColor related_text_color) {
  return gfx::CreateVectorIcon(
      kCertificateIcon, kVectorIconSize,
      color_utils::DeriveDefaultIconColor(related_text_color));
}

// static
const gfx::ImageSkia PageInfoUI::GetSiteSettingsIcon(
    const SkColor related_text_color) {
  return gfx::CreateVectorIcon(
      kSettingsIcon, kVectorIconSize,
      color_utils::DeriveDefaultIconColor(related_text_color));
}
#endif

// static
bool PageInfoUI::ContentSettingsTypeInPageInfo(ContentSettingsType type) {
  for (const PermissionsUIInfo& info : kPermissionsUIInfo) {
    if (info.type == type)
      return true;
  }
  return false;
}

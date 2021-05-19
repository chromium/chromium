// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/page_info/features.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#if defined(OS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/safe_browsing/content/password_protection/password_protection_service.h"
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
static_assert(base::size(kPermissionButtonTextIDPolicyManaged) ==
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
static_assert(base::size(kPermissionButtonTextIDExtensionManaged) ==
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
static_assert(base::size(kPermissionButtonTextIDUserManaged) ==
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

base::span<const PermissionsUIInfo> GetContentSettingsUIInfo() {
  DCHECK(base::FeatureList::GetInstance() != nullptr);
  static const PermissionsUIInfo kPermissionsUIInfo[] = {
    {ContentSettingsType::COOKIES, 0},
    {ContentSettingsType::IMAGES, IDS_PAGE_INFO_TYPE_IMAGES},
    {ContentSettingsType::JAVASCRIPT, IDS_PAGE_INFO_TYPE_JAVASCRIPT},
    {ContentSettingsType::POPUPS, IDS_PAGE_INFO_TYPE_POPUPS_REDIRECTS},
    {ContentSettingsType::GEOLOCATION, IDS_PAGE_INFO_TYPE_LOCATION},
    {ContentSettingsType::NOTIFICATIONS, IDS_PAGE_INFO_TYPE_NOTIFICATIONS},
    {ContentSettingsType::MEDIASTREAM_MIC, IDS_PAGE_INFO_TYPE_MIC},
    {ContentSettingsType::MEDIASTREAM_CAMERA, IDS_PAGE_INFO_TYPE_CAMERA},
    {ContentSettingsType::AUTOMATIC_DOWNLOADS,
     IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {ContentSettingsType::MIDI_SYSEX, IDS_PAGE_INFO_TYPE_MIDI_SYSEX},
    {ContentSettingsType::BACKGROUND_SYNC, IDS_PAGE_INFO_TYPE_BACKGROUND_SYNC},
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
    {ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
     IDS_PAGE_INFO_TYPE_PROTECTED_MEDIA_IDENTIFIER},
#endif
    {ContentSettingsType::ADS, IDS_PAGE_INFO_TYPE_ADS},
    {ContentSettingsType::SOUND, IDS_PAGE_INFO_TYPE_SOUND},
    {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_PAGE_INFO_TYPE_CLIPBOARD},
    {ContentSettingsType::SENSORS,
     base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
         ? IDS_PAGE_INFO_TYPE_SENSORS
         : IDS_PAGE_INFO_TYPE_MOTION_SENSORS},
    {ContentSettingsType::USB_GUARD, IDS_PAGE_INFO_TYPE_USB},
#if !defined(OS_ANDROID)
    {ContentSettingsType::SERIAL_GUARD, IDS_PAGE_INFO_TYPE_SERIAL},
#endif
    {ContentSettingsType::BLUETOOTH_GUARD, IDS_PAGE_INFO_TYPE_BLUETOOTH},
    {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
     IDS_PAGE_INFO_TYPE_FILE_SYSTEM_ACCESS_WRITE},
    {ContentSettingsType::BLUETOOTH_SCANNING,
     IDS_PAGE_INFO_TYPE_BLUETOOTH_SCANNING},
    {ContentSettingsType::NFC, IDS_PAGE_INFO_TYPE_NFC},
    {ContentSettingsType::VR, IDS_PAGE_INFO_TYPE_VR},
    {ContentSettingsType::AR, IDS_PAGE_INFO_TYPE_AR},
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
     IDS_PAGE_INFO_TYPE_CAMERA_PAN_TILT_ZOOM},
    {ContentSettingsType::WINDOW_PLACEMENT,
     IDS_PAGE_INFO_TYPE_WINDOW_PLACEMENT},
    {ContentSettingsType::FONT_ACCESS, IDS_PAGE_INFO_TYPE_FONT_ACCESS},
#if !defined(OS_ANDROID)
    {ContentSettingsType::HID_GUARD, IDS_PAGE_INFO_TYPE_HID},
#endif
    {ContentSettingsType::IDLE_DETECTION, IDS_PAGE_INFO_TYPE_IDLE_DETECTION},
    {ContentSettingsType::FILE_HANDLING, IDS_PAGE_INFO_TYPE_FILE_HANDLING},
  };
  return kPermissionsUIInfo;
}

std::unique_ptr<PageInfoUI::SecurityDescription> CreateSecurityDescription(
    PageInfoUI::SecuritySummaryColor style,
    int summary_id,
    int details_id,
    PageInfoUI::SecurityDescriptionType type) {
  auto security_description =
      std::make_unique<PageInfoUI::SecurityDescription>();
  security_description->summary_style = style;
  if (summary_id)
    security_description->summary = l10n_util::GetStringUTF16(summary_id);
  if (details_id)
    security_description->details = l10n_util::GetStringUTF16(details_id);
  security_description->type = type;
  return security_description;
}

std::unique_ptr<PageInfoUI::SecurityDescription>
CreateSecurityDescriptionForSafetyTip(
    const security_state::SafetyTipStatus& safety_tip_status,
    const GURL& safe_url) {
  auto security_description =
      std::make_unique<PageInfoUI::SecurityDescription>();
  security_description->summary_style = PageInfoUI::SecuritySummaryColor::RED;

  if (safety_tip_status == security_state::SafetyTipStatus::kBadReputation ||
      safety_tip_status ==
          security_state::SafetyTipStatus::kBadReputationIgnored) {
    security_description->summary = l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE);
  } else {
    const std::u16string safe_host =
        security_interstitials::common_string_util::GetFormattedHostName(
            safe_url);
    security_description->summary = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE, safe_host);
  }
  security_description->details =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_DESCRIPTION);
  security_description->type = PageInfoUI::SecurityDescriptionType::SAFETY_TIP;
  return security_description;
}

// Gets the actual setting for a ContentSettingType, taking into account what
// the default setting value is and whether Html5ByDefault is enabled.
ContentSetting GetEffectiveSetting(ContentSettingsType type,
                                   ContentSetting setting,
                                   ContentSetting default_setting) {
  ContentSetting effective_setting = setting;
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = default_setting;

  // Display the UI string for ASK instead of DETECT for Flash.
  // TODO(tommycli): Just migrate the actual content setting to ASK.
  if (effective_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT)
    effective_setting = CONTENT_SETTING_ASK;

  return effective_setting;
}

}  // namespace

PageInfoUI::CookieInfo::CookieInfo() : allowed(-1), blocked(-1) {}

PageInfoUI::ChosenObjectInfo::ChosenObjectInfo(
    const PageInfo::ChooserUIInfo& ui_info,
    std::unique_ptr<permissions::ObjectPermissionContextBase::Object>
        chooser_object)
    : ui_info(ui_info), chooser_object(std::move(chooser_object)) {}

PageInfoUI::ChosenObjectInfo::~ChosenObjectInfo() = default;

PageInfoUI::IdentityInfo::IdentityInfo()
    : identity_status(PageInfo::SITE_IDENTITY_STATUS_UNKNOWN),
      safe_browsing_status(PageInfo::SAFE_BROWSING_STATUS_NONE),
      safety_tip_info({security_state::SafetyTipStatus::kUnknown, GURL()}),
      connection_status(PageInfo::SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button(false),
      show_change_password_buttons(false) {}

PageInfoUI::IdentityInfo::~IdentityInfo() = default;

PageInfoUI::PageFeatureInfo::PageFeatureInfo()
    : is_vr_presentation_in_headset(false) {}

std::unique_ptr<PageInfoUI::SecurityDescription>
PageInfoUI::GetSecurityDescription(const IdentityInfo& identity_info) const {
  switch (identity_info.safe_browsing_status) {
    case PageInfo::SAFE_BROWSING_STATUS_NONE:
      break;
    case PageInfo::SAFE_BROWSING_STATUS_MALWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_MALWARE_SUMMARY,
                                       IDS_PAGE_INFO_MALWARE_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_SOCIAL_ENGINEERING_SUMMARY,
                                       IDS_PAGE_INFO_SOCIAL_ENGINEERING_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_UNWANTED_SOFTWARE_SUMMARY,
                                       IDS_PAGE_INFO_UNWANTED_SOFTWARE_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE:
    case PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE: {
#if BUILDFLAG(FULL_SAFE_BROWSING)
      auto security_description = CreateSecurityDescription(
          SecuritySummaryColor::RED, IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY, 0,
          SecurityDescriptionType::SAFE_BROWSING);
      security_description->details = identity_info.safe_browsing_details;
      return security_description;
#endif
      NOTREACHED();
      break;
    }
    case PageInfo::SAFE_BROWSING_STATUS_BILLING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_BILLING_SUMMARY,
                                       IDS_PAGE_INFO_BILLING_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
  }

  std::unique_ptr<SecurityDescription> safety_tip_security_desc =
      CreateSafetyTipSecurityDescription(identity_info.safety_tip_info);
  if (safety_tip_security_desc) {
    return safety_tip_security_desc;
  }

  switch (identity_info.identity_status) {
#if defined(OS_ANDROID)
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
      return CreateSecurityDescription(SecuritySummaryColor::GREEN, 0,
                                       IDS_PAGE_INFO_INTERNAL_PAGE,
                                       SecurityDescriptionType::INTERNAL);
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
      switch (identity_info.connection_status) {
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
          return CreateSecurityDescription(
              SecuritySummaryColor::RED, IDS_PAGE_INFO_NOT_SECURE_SUMMARY_SHORT,
              IDS_PAGE_INFO_NOT_SECURE_DETAILS,
              SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
          return CreateSecurityDescription(
              SecuritySummaryColor::RED,
              IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY_SHORT,
              IDS_PAGE_INFO_NOT_SECURE_DETAILS,
              SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
          return CreateSecurityDescription(
              SecuritySummaryColor::RED,
              IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY_SHORT,
              IDS_PAGE_INFO_MIXED_CONTENT_DETAILS,
              SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_LEGACY_TLS:
          return CreateSecurityDescription(
              SecuritySummaryColor::RED,
              IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY_SHORT,
              IDS_PAGE_INFO_LEGACY_TLS_DETAILS,
              SecurityDescriptionType::CONNECTION);
        default:
          // Do not show details for secure connections.
          return CreateSecurityDescription(SecuritySummaryColor::GREEN,
                                           IDS_PAGE_INFO_SECURE_SUMMARY, 0,
                                           SecurityDescriptionType::CONNECTION);
      }
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    default:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_NOT_SECURE_SUMMARY_SHORT,
                                       IDS_PAGE_INFO_NOT_SECURE_DETAILS,
                                       SecurityDescriptionType::CONNECTION);
#else
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
      // Internal pages on desktop have their own UI implementations which
      // should never call this function.
      NOTREACHED();
      FALLTHROUGH;
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
      switch (identity_info.connection_status) {
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_NOT_SECURE_SUMMARY,
                                           IDS_PAGE_INFO_NOT_SECURE_DETAILS,
                                           SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY,
                                           IDS_PAGE_INFO_NOT_SECURE_DETAILS,
                                           SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY,
                                           IDS_PAGE_INFO_MIXED_CONTENT_DETAILS,
                                           SecurityDescriptionType::CONNECTION);
        case PageInfo::SITE_CONNECTION_STATUS_LEGACY_TLS:
          return CreateSecurityDescription(SecuritySummaryColor::RED,
                                           IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY,
                                           IDS_PAGE_INFO_LEGACY_TLS_DETAILS,
                                           SecurityDescriptionType::CONNECTION);
        default:
          return CreateSecurityDescription(SecuritySummaryColor::GREEN,
                                           IDS_PAGE_INFO_SECURE_SUMMARY,
                                           IDS_PAGE_INFO_SECURE_DETAILS,
                                           SecurityDescriptionType::CONNECTION);
      }
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    default:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_NOT_SECURE_SUMMARY,
                                       IDS_PAGE_INFO_NOT_SECURE_DETAILS,
                                       SecurityDescriptionType::CONNECTION);
#endif
  }
}

PageInfoUI::~PageInfoUI() = default;

// static
std::u16string PageInfoUI::PermissionTypeToUIString(ContentSettingsType type) {
  for (const PermissionsUIInfo& info : GetContentSettingsUIInfo()) {
    if (info.type == type)
      return l10n_util::GetStringUTF16(info.string_id);
  }
  NOTREACHED();
  return std::u16string();
}

// static
std::u16string PageInfoUI::PermissionActionToUIString(
    PageInfoUiDelegate* delegate,
    ContentSettingsType type,
    ContentSetting setting,
    ContentSetting default_setting,
    content_settings::SettingSource source,
    bool is_one_time) {
  ContentSetting effective_setting =
      GetEffectiveSetting(type, setting, default_setting);
  const int* button_text_ids = nullptr;
  switch (source) {
    case content_settings::SETTING_SOURCE_USER:
      if (setting == CONTENT_SETTING_DEFAULT) {
#if !defined(OS_ANDROID)
        if (type == ContentSettingsType::SOUND) {
          // If the block autoplay enabled preference is enabled and the
          // sound default setting is ALLOW, we will return a custom string
          // indicating that Chrome is controlling autoplay and sound
          // automatically.
          if (delegate->IsBlockAutoPlayEnabled() &&
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
      if (type == ContentSettingsType::SOUND) {
        button_text_ids = kSoundPermissionButtonTextIDUserManaged;
        break;
      }
#endif
      button_text_ids = kPermissionButtonTextIDUserManaged;
      break;
    case content_settings::SETTING_SOURCE_ALLOWLIST:
    case content_settings::SETTING_SOURCE_NONE:
    default:
      NOTREACHED();
      return std::u16string();
  }
  int button_text_id = button_text_ids[effective_setting];

  if (is_one_time) {
    DCHECK_EQ(source, content_settings::SETTING_SOURCE_USER);
    DCHECK_EQ(type, ContentSettingsType::GEOLOCATION);
    DCHECK_EQ(button_text_id, IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER);
    button_text_id = IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_ONCE_BY_USER;
  }
  DCHECK_NE(button_text_id, kInvalidResourceID);
  return l10n_util::GetStringUTF16(button_text_id);
}

// static
std::u16string PageInfoUI::PermissionDecisionReasonToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  ContentSetting effective_setting = GetEffectiveSetting(
      permission.type, permission.setting, permission.default_setting);
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

  // TODO(crbug.com/1063023): PageInfo::PermissionInfo should be modified
  // to contain all needed information regarding Automatically Blocked flag.
  if (permission.setting == CONTENT_SETTING_BLOCK &&
      permissions::PermissionUtil::IsPermission(permission.type)) {
    permissions::PermissionResult permission_result =
        delegate->GetPermissionStatus(permission.type);
    switch (permission_result.source) {
      case permissions::PermissionStatusSource::MULTIPLE_DISMISSALS:
        message_id = IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED;
        break;
      case permissions::PermissionStatusSource::MULTIPLE_IGNORES:
        message_id = IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED;
        break;
      default:
        break;
    }
  }

  if (permission.type == ContentSettingsType::ADS)
    message_id = IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE;

  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

// static
SkColor PageInfoUI::GetSecondaryTextColor() {
  return SK_ColorGRAY;
}

#if defined(OS_ANDROID)
// static
int PageInfoUI::GetIdentityIconID(PageInfo::SiteIdentityStatus status) {
  switch (status) {
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
      return IDR_PAGEINFO_GOOD;
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ERROR:
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
      return IDR_PAGEINFO_BAD;
  }

  return 0;
}

// static
int PageInfoUI::GetConnectionIconID(PageInfo::SiteConnectionStatus status) {
  switch (status) {
    case PageInfo::SITE_CONNECTION_STATUS_UNKNOWN:
    case PageInfo::SITE_CONNECTION_STATUS_INTERNAL_PAGE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED:
      return IDR_PAGEINFO_GOOD;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
    case PageInfo::SITE_CONNECTION_STATUS_LEGACY_TLS:
    case PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      return IDR_PAGEINFO_BAD;
  }

  return 0;
}

int PageInfoUI::GetIdentityIconColorID(PageInfo::SiteIdentityStatus status) {
  switch (status) {
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
      return IDR_PAGEINFO_GOOD_COLOR;
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
      return IDR_PAGEINFO_WARNING_COLOR;
    case PageInfo::SITE_IDENTITY_STATUS_ERROR:
      return IDR_PAGEINFO_BAD_COLOR;
  }
  return 0;
}

int PageInfoUI::GetConnectionIconColorID(
    PageInfo::SiteConnectionStatus status) {
  switch (status) {
    case PageInfo::SITE_CONNECTION_STATUS_UNKNOWN:
    case PageInfo::SITE_CONNECTION_STATUS_INTERNAL_PAGE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED:
      return IDR_PAGEINFO_GOOD_COLOR;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
    case PageInfo::SITE_CONNECTION_STATUS_LEGACY_TLS:
    case PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED:
      return IDR_PAGEINFO_WARNING_COLOR;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      return IDR_PAGEINFO_BAD_COLOR;
  }
  return 0;
}

#else  // !defined(OS_ANDROID)
// static
const ui::ImageModel PageInfoUI::GetPermissionIcon(
    const PageInfo::PermissionInfo& info) {
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (info.type) {
    case ContentSettingsType::COOKIES:
      icon = &vector_icons::kCookieIcon;
      break;
    case ContentSettingsType::IMAGES:
      icon = &vector_icons::kPhotoIcon;
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = &vector_icons::kCodeIcon;
      break;
    case ContentSettingsType::POPUPS:
      icon = &vector_icons::kLaunchIcon;
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = &vector_icons::kLocationOnIcon;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = &vector_icons::kNotificationsIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = &vector_icons::kMicIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = &vector_icons::kVideocamIcon;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = &vector_icons::kFileDownloadIcon;
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = &vector_icons::kProtectedContentIcon;
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = &vector_icons::kMidiIcon;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = &vector_icons::kSyncIcon;
      break;
    case ContentSettingsType::ADS:
      icon = &vector_icons::kAdsIcon;
      break;
    case ContentSettingsType::SOUND:
      icon = &vector_icons::kVolumeUpIcon;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = &vector_icons::kPageInfoContentPasteIcon;
      break;
    case ContentSettingsType::SENSORS:
      icon = &vector_icons::kSensorsIcon;
      break;
    case ContentSettingsType::USB_GUARD:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = &vector_icons::kBluetoothScanningIcon;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = &vector_icons::kSaveOriginalFileIcon;
      break;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      icon = &vector_icons::kVrHeadsetIcon;
      break;
    case ContentSettingsType::WINDOW_PLACEMENT:
      icon = &vector_icons::kSelectWindowIcon;
      break;
    case ContentSettingsType::FONT_ACCESS:
      icon = &vector_icons::kFontDownloadIcon;
      break;
    case ContentSettingsType::HID_GUARD:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = &vector_icons::kDevicesIcon;
      break;
    case ContentSettingsType::FILE_HANDLING:
      icon = &vector_icons::kDescriptionIcon;
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
  return ui::ImageModel::FromVectorIcon(
      *icon, ui::NativeTheme::kColorId_DefaultIconColor, kVectorIconSize,
      (setting == CONTENT_SETTING_BLOCK) ? &vector_icons::kBlockedBadgeIcon
                                         : nullptr);
}

// static
const ui::ImageModel PageInfoUI::GetChosenObjectIcon(
    const ChosenObjectInfo& object,
    bool deleted) {
  // The permissions data for device APIs will always appear even if the device
  // is not currently conncted to the system.
  // TODO(https://crbug.com/1048860): Check the connected status of devices and
  // change the icon to one that reflects that status.
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (object.ui_info.content_settings_type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::HID_CHOOSER_DATA:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    default:
      // All other content settings types do not represent chosen object
      // permissions.
      NOTREACHED();
      break;
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::NativeTheme::kColorId_DefaultIconColor, kVectorIconSize,
      deleted ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoUI::GetValidCertificateIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kCertificateIcon,
      ui::NativeTheme::kColorId_DefaultIconColor, kVectorIconSize);
}

// static
const ui::ImageModel PageInfoUI::GetInvalidCertificateIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kCertificateIcon,
      ui::NativeTheme::kColorId_DefaultIconColor, kVectorIconSize,
      &vector_icons::kBlockedBadgeIcon);
}

// static
const ui::ImageModel PageInfoUI::GetSiteSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kSettingsIcon, ui::NativeTheme::kColorId_DefaultIconColor,
      kVectorIconSize);
}

// static
const ui::ImageModel PageInfoUI::GetVrSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kVrHeadsetIcon, ui::NativeTheme::kColorId_DefaultIconColor,
      kVectorIconSize);
}

// static
const ui::ImageModel PageInfoUI::GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchIcon, ui::NativeTheme::kColorId_SecondaryIconColor,
      kVectorIconSize);
}

// static
const ui::ImageModel PageInfoUI::GetConnectionNotSecureIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kNotSecureWarningIcon,
      ui::NativeTheme::kColorId_AlertSeverityHigh);
}

// static
const ui::ImageModel PageInfoUI::GetConnectionSecureIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kHttpsValidIcon,
      ui::NativeTheme::kColorId_DefaultIconColor);
}

// static
const ui::ImageModel PageInfoUI::GetOpenSubpageIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon,
      ui::NativeTheme::kColorId_DefaultIconColor);
}

#endif

// static
bool PageInfoUI::ContentSettingsTypeInPageInfo(ContentSettingsType type) {
  for (const PermissionsUIInfo& info : GetContentSettingsUIInfo()) {
    if (info.type == type)
      return true;
  }
  return false;
}

// static
std::unique_ptr<PageInfoUI::SecurityDescription>
PageInfoUI::CreateSafetyTipSecurityDescription(
    const security_state::SafetyTipInfo& info) {
  switch (info.status) {
    case security_state::SafetyTipStatus::kBadReputation:
    case security_state::SafetyTipStatus::kBadReputationIgnored:
    case security_state::SafetyTipStatus::kLookalike:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
      return CreateSecurityDescriptionForSafetyTip(info.status, info.safe_url);

    case security_state::SafetyTipStatus::kBadKeyword:
      // Keyword safety tips are only used to collect metrics for now and are
      // not visible to the user, so don't affect Page Info.
      break;

    case security_state::SafetyTipStatus::kDigitalAssetLinkMatch:
    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      break;
  }
  return nullptr;
}

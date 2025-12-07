// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <variant>

#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/core/features.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#endif

namespace {

using content_settings::PermissionSettingsRegistry;
using content_settings::SettingSource;

const int kInvalidResourceID = -1;

base::span<const PageInfoUI::PermissionUIInfo> GetContentSettingsUIInfo() {
  DCHECK(base::FeatureList::GetInstance() != nullptr);
  static const PageInfoUI::PermissionUIInfo kPermissionUIInfo[] = {
      {ContentSettingsType::COOKIES, IDS_SITE_SETTINGS_TYPE_COOKIES,
       IDS_SITE_SETTINGS_TYPE_COOKIES_MID_SENTENCE},
      {ContentSettingsType::JAVASCRIPT, IDS_SITE_SETTINGS_TYPE_JAVASCRIPT,
       IDS_SITE_SETTINGS_TYPE_JAVASCRIPT_MID_SENTENCE},
      {ContentSettingsType::POPUPS, IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS,
       IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS_MID_SENTENCE},
      {ContentSettingsType::GEOLOCATION, IDS_SITE_SETTINGS_TYPE_LOCATION,
       IDS_SITE_SETTINGS_TYPE_LOCATION_MID_SENTENCE},
      {ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
       IDS_SITE_SETTINGS_TYPE_LOCATION,
       IDS_SITE_SETTINGS_TYPE_LOCATION_MID_SENTENCE},
      {ContentSettingsType::NOTIFICATIONS, IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS,
       IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS_MID_SENTENCE},
      {ContentSettingsType::MEDIASTREAM_MIC, IDS_SITE_SETTINGS_TYPE_MIC,
       IDS_SITE_SETTINGS_TYPE_MIC_MID_SENTENCE},
      {ContentSettingsType::MEDIASTREAM_CAMERA, IDS_SITE_SETTINGS_TYPE_CAMERA,
       IDS_SITE_SETTINGS_TYPE_CAMERA_MID_SENTENCE},
      {ContentSettingsType::AUTOMATIC_DOWNLOADS,
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS_MID_SENTENCE},
      {ContentSettingsType::MIDI_SYSEX, IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX,
       IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX_MID_SENTENCE},
      {ContentSettingsType::BACKGROUND_SYNC,
       IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC,
       IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC_MID_SENTENCE},
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      {ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
       IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID,
       IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID_MID_SENTENCE},
#endif
      {ContentSettingsType::ADS, IDS_SITE_SETTINGS_TYPE_ADS,
       IDS_SITE_SETTINGS_TYPE_ADS_MID_SENTENCE},
      {ContentSettingsType::SOUND, IDS_SITE_SETTINGS_TYPE_SOUND,
       IDS_SITE_SETTINGS_TYPE_SOUND_MID_SENTENCE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_SITE_SETTINGS_TYPE_CLIPBOARD,
       IDS_SITE_SETTINGS_TYPE_CLIPBOARD_MID_SENTENCE},
      {
          ContentSettingsType::SENSORS,
          base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
              ? IDS_SITE_SETTINGS_TYPE_SENSORS
              : IDS_SITE_SETTINGS_TYPE_MOTION_SENSORS,
          base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
              ? IDS_SITE_SETTINGS_TYPE_SENSORS_MID_SENTENCE
              : IDS_SITE_SETTINGS_TYPE_MOTION_SENSORS_MID_SENTENCE,
      },
      {ContentSettingsType::USB_GUARD, IDS_SITE_SETTINGS_TYPE_USB_DEVICES,
       IDS_SITE_SETTINGS_TYPE_USB_DEVICES_MID_SENTENCE},
      {ContentSettingsType::BLUETOOTH_GUARD,
       IDS_SITE_SETTINGS_TYPE_BLUETOOTH_DEVICES,
       IDS_SITE_SETTINGS_TYPE_BLUETOOTH_DEVICES_MID_SENTENCE},
      {ContentSettingsType::BLUETOOTH_SCANNING,
       IDS_SITE_SETTINGS_TYPE_BLUETOOTH_SCANNING,
       IDS_SITE_SETTINGS_TYPE_BLUETOOTH_SCANNING_MID_SENTENCE},
      {ContentSettingsType::NFC, IDS_SITE_SETTINGS_TYPE_NFC,
       IDS_SITE_SETTINGS_TYPE_NFC_MID_SENTENCE},
      {ContentSettingsType::VR, IDS_SITE_SETTINGS_TYPE_VR,
       IDS_SITE_SETTINGS_TYPE_VR_MID_SENTENCE},
      {ContentSettingsType::AR, IDS_SITE_SETTINGS_TYPE_AR,
       IDS_SITE_SETTINGS_TYPE_AR_MID_SENTENCE},
      {ContentSettingsType::HAND_TRACKING, IDS_SITE_SETTINGS_TYPE_HAND_TRACKING,
       IDS_SITE_SETTINGS_TYPE_HAND_TRACKING_MID_SENTENCE},
      {ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
       IDS_SITE_SETTINGS_TYPE_CAMERA_PAN_TILT_ZOOM,
       IDS_SITE_SETTINGS_TYPE_CAMERA_PAN_TILT_ZOOM_MID_SENTENCE},
      {ContentSettingsType::FEDERATED_IDENTITY_API,
       IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API,
       IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API_MID_SENTENCE},
      {ContentSettingsType::IDLE_DETECTION,
       IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION,
       IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION_MID_SENTENCE},
      {ContentSettingsType::STORAGE_ACCESS,
       IDS_SITE_SETTINGS_TYPE_STORAGE_ACCESS,
       IDS_SITE_SETTINGS_TYPE_STORAGE_ACCESS_MID_SENTENCE},
      {ContentSettingsType::AUTOMATIC_FULLSCREEN,
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_FULLSCREEN,
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_FULLSCREEN_MID_SENTENCE},
      {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
       IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE,
       IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE_MID_SENTENCE},
      {ContentSettingsType::LOCAL_NETWORK_ACCESS,
       IDS_SITE_SETTINGS_TYPE_LOCAL_NETWORK_ACCESS,
       IDS_SITE_SETTINGS_TYPE_LOCAL_NETWORK_ACCESS_MID_SENTENCE},
      {ContentSettingsType::WINDOW_MANAGEMENT,
       IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT,
       IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT_MID_SENTENCE},
      {ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
       IDS_SITE_SETTINGS_TYPE_AUTO_PICTURE_IN_PICTURE,
       IDS_SITE_SETTINGS_TYPE_AUTO_PICTURE_IN_PICTURE_MID_SENTENCE},
#if !BUILDFLAG(IS_ANDROID)
      // Page Info Permissions that are not defined in Android.
      {ContentSettingsType::CAPTURED_SURFACE_CONTROL,
       IDS_SITE_SETTINGS_TYPE_CAPTURED_SURFACE_CONTROL_SHARED_TABS,
       IDS_SITE_SETTINGS_TYPE_CAPTURED_SURFACE_CONTROL_SHARED_TABS_MID_SENTENCE},
      {ContentSettingsType::KEYBOARD_LOCK, IDS_SITE_SETTINGS_TYPE_KEYBOARD_LOCK,
       IDS_SITE_SETTINGS_TYPE_KEYBOARD_LOCK_MID_SENTENCE},
      {ContentSettingsType::LOCAL_FONTS, IDS_SITE_SETTINGS_TYPE_FONT_ACCESS,
       IDS_SITE_SETTINGS_TYPE_FONT_ACCESS_MID_SENTENCE},
      {ContentSettingsType::HID_GUARD, IDS_SITE_SETTINGS_TYPE_HID_DEVICES,
       IDS_SITE_SETTINGS_TYPE_HID_DEVICES_MID_SENTENCE},
      {ContentSettingsType::IMAGES, IDS_SITE_SETTINGS_TYPE_IMAGES,
       IDS_SITE_SETTINGS_TYPE_IMAGES_MID_SENTENCE},
      {ContentSettingsType::POINTER_LOCK, IDS_SITE_SETTINGS_TYPE_POINTER_LOCK,
       IDS_SITE_SETTINGS_TYPE_POINTER_LOCK_MID_SENTENCE},
      {ContentSettingsType::SERIAL_GUARD, IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS,
       IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS_MID_SENTENCE},
      {ContentSettingsType::WEB_PRINTING, IDS_SITE_SETTINGS_TYPE_WEB_PRINTING,
       IDS_SITE_SETTINGS_TYPE_WEB_PRINTING_MID_SENTENCE},
      {ContentSettingsType::WEB_APP_INSTALLATION,
       IDS_SITE_SETTINGS_TYPE_WEB_APP_INSTALLATION,
       IDS_SITE_SETTINGS_TYPE_WEB_APP_INSTALLATION_MID_SENTENCE},
#endif
  };
  return kPermissionUIInfo;
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

  const std::u16string safe_host =
      security_interstitials::common_string_util::GetFormattedHostName(
          safe_url);
  security_description->summary = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE, safe_host);
  security_description->details =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_DESCRIPTION);
  security_description->type = PageInfoUI::SecurityDescriptionType::SAFETY_TIP;
  return security_description;
}

// Gets the actual setting for a ContentSettingsType.
PermissionSetting GetEffectiveSetting(
    ContentSettingsType type,
    const std::optional<PermissionSetting>& setting,
    const PermissionSetting& default_setting) {
  // Check that we are passing nullopt instead of CONTENT_SETTING_DEFAULT.
  CHECK_NE(setting, PermissionSetting{CONTENT_SETTING_DEFAULT});
  return setting.value_or(default_setting);
}

void CreateOppositeToDefaultSiteException(
    PageInfo::PermissionInfo& permission,
    ContentSetting opposite_to_block_setting) {
  if (!std::holds_alternative<ContentSetting>(permission.default_setting)) {
    NOTIMPLEMENTED();
  }

  ContentSetting default_setting =
      std::get<ContentSetting>(permission.default_setting);
  // For Automatic Picture-in-Picture, we show the toggle in the "on" position
  // while the setting is ASK, so the opposite to the default when the default
  // is ASK should be BLOCK instead of ALLOW.
  if (permission.type == ContentSettingsType::AUTO_PICTURE_IN_PICTURE) {
    permission.setting = default_setting == CONTENT_SETTING_BLOCK
                             ? CONTENT_SETTING_ALLOW
                             : CONTENT_SETTING_BLOCK;
    return;
  }

  // For guard content settings opposite to block setting is ask, for the
  // rest opposite is allow.
  permission.setting = default_setting == opposite_to_block_setting
                           ? CONTENT_SETTING_BLOCK
                           : opposite_to_block_setting;
}

std::u16string GetPermissionAskStateString(ContentSettingsType type) {
  int message_id = kInvalidResourceID;

  switch (type) {
    case ContentSettingsType::GEOLOCATION:
      message_id = IDS_PAGE_INFO_STATE_TEXT_LOCATION_ASK;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      message_id = IDS_PAGE_INFO_STATE_TEXT_NOTIFICATIONS_ASK;
      break;
    case ContentSettingsType::MIDI_SYSEX:
      message_id = IDS_PAGE_INFO_STATE_TEXT_MIDI_SYSEX_ASK;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CAMERA_ASK;
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CAMERA_PAN_TILT_ZOOM_ASK;
      break;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CAPTURED_SURFACE_CONTROL_ASK;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      message_id = IDS_PAGE_INFO_STATE_TEXT_MIC_ASK;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CLIPBOARD_ASK;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      message_id = IDS_PAGE_INFO_STATE_TEXT_AUTOMATIC_DOWNLOADS_ASK;
      break;
    case ContentSettingsType::HAND_TRACKING:
      message_id = IDS_PAGE_INFO_STATE_TEXT_HAND_TRACKING_ASK;
      break;
    case ContentSettingsType::VR:
      message_id = IDS_PAGE_INFO_STATE_TEXT_VR_ASK;
      break;
    case ContentSettingsType::AR:
      message_id = IDS_PAGE_INFO_STATE_TEXT_AR_ASK;
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      message_id = IDS_PAGE_INFO_STATE_TEXT_WINDOW_MANAGEMENT_ASK;
      break;
    case ContentSettingsType::LOCAL_FONTS:
      message_id = IDS_PAGE_INFO_STATE_TEXT_FONT_ACCESS_ASK;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      message_id = IDS_PAGE_INFO_STATE_TEXT_IDLE_DETECTION_ASK;
      break;
    case ContentSettingsType::LOCAL_NETWORK_ACCESS:
      message_id = IDS_PAGE_INFO_STATE_TEXT_LOCAL_NETWORK_ACCESS_ASK;
      break;
    // Guard content settings:
    case ContentSettingsType::USB_GUARD:
      message_id = IDS_PAGE_INFO_STATE_TEXT_USB_ASK;
      break;
    case ContentSettingsType::HID_GUARD:
      message_id = IDS_PAGE_INFO_STATE_TEXT_HID_DEVICES_ASK;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      message_id = IDS_PAGE_INFO_STATE_TEXT_SERIAL_ASK;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      message_id = IDS_PAGE_INFO_STATE_TEXT_BLUETOOTH_DEVICES_ASK;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      message_id = IDS_PAGE_INFO_STATE_TEXT_BLUETOOTH_SCANNING_ASK;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      message_id = IDS_PAGE_INFO_STATE_TEXT_FILE_SYSTEM_WRITE_ASK;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      message_id = IDS_PAGE_INFO_STATE_TEXT_STORAGE_ACCESS_ASK;
      break;
    case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
      message_id = IDS_PAGE_INFO_STATE_TEXT_AUTO_PICTURE_IN_PICTURE_ASK;
      break;
    case ContentSettingsType::KEYBOARD_LOCK:
      message_id = IDS_PAGE_INFO_STATE_TEXT_KEYBOARD_LOCK_ASK;
      break;
    case ContentSettingsType::POINTER_LOCK:
      message_id = IDS_PAGE_INFO_STATE_TEXT_POINTER_LOCK_ASK;
      break;
    case ContentSettingsType::WEB_APP_INSTALLATION:
      message_id = IDS_PAGE_INFO_STATE_TEXT_WEB_APP_INSTALLATION_ASK;
      break;
#if BUILDFLAG(IS_CHROMEOS)
    case ContentSettingsType::WEB_PRINTING:
      message_id = IDS_PAGE_INFO_STATE_TEXT_WEB_PRINTING_ASK;
      break;
#endif
    default:
      NOTREACHED();
  }

  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace

PageInfoUI::CookiesInfo::CookiesInfo() = default;
PageInfoUI::CookiesInfo::CookiesInfo(CookiesInfo&& cookie_info) = default;
PageInfoUI::CookiesInfo::~CookiesInfo() = default;

PageInfoUI::CookiesRwsInfo::CookiesRwsInfo(const std::u16string& owner_name)
    : owner_name(owner_name) {}

PageInfoUI::CookiesRwsInfo::~CookiesRwsInfo() = default;

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

bool PageInfoUI::AdPersonalizationInfo::is_empty() const {
  return !has_joined_user_to_interest_group && accessed_topics.empty();
}

PageInfoUI::AdPersonalizationInfo::AdPersonalizationInfo() = default;
PageInfoUI::AdPersonalizationInfo::~AdPersonalizationInfo() = default;

std::unique_ptr<PageInfoUI::SecurityDescription>
PageInfoUI::GetSecurityDescription(const IdentityInfo& identity_info) const {
  switch (identity_info.safe_browsing_status) {
    case PageInfo::SAFE_BROWSING_STATUS_NONE:
      break;
    case PageInfo::SAFE_BROWSING_STATUS_MALWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_SAFE_BROWSING_SUMMARY,
                                       IDS_PAGE_INFO_MALWARE_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_SAFE_BROWSING_SUMMARY,
                                       IDS_PAGE_INFO_SOCIAL_ENGINEERING_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_SAFE_BROWSING_SUMMARY,
                                       IDS_PAGE_INFO_UNWANTED_SOFTWARE_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE: {
#if BUILDFLAG(FULL_SAFE_BROWSING)
      auto security_description = CreateSecurityDescription(
          SecuritySummaryColor::RED,
          IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY, 0,
          SecurityDescriptionType::SAFE_BROWSING);
      security_description->details = identity_info.safe_browsing_details;
      return security_description;
#else
      NOTREACHED();
#endif
    }
    case PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE: {
#if BUILDFLAG(FULL_SAFE_BROWSING)
      auto security_description = CreateSecurityDescription(
          SecuritySummaryColor::RED, IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY, 0,
          SecurityDescriptionType::SAFE_BROWSING);
      security_description->details = identity_info.safe_browsing_details;
      return security_description;
#else
      NOTREACHED();
#endif
    }
    case PageInfo::SAFE_BROWSING_STATUS_BILLING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_BILLING_SUMMARY,
                                       IDS_PAGE_INFO_BILLING_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN:
      return CreateSecurityDescription(SecuritySummaryColor::ENTERPRISE,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_SUMMARY,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK:
      return CreateSecurityDescription(SecuritySummaryColor::ENTERPRISE,
                                       IDS_PAGE_INFO_ENTERPRISE_BLOCK_SUMMARY,
                                       IDS_PAGE_INFO_ENTERPRISE_BLOCK_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
  }

  std::unique_ptr<SecurityDescription> safety_tip_security_desc =
      CreateSafetyTipSecurityDescription(identity_info.safety_tip_info);
  if (safety_tip_security_desc) {
    return safety_tip_security_desc;
  }

  switch (identity_info.identity_status) {
#if BUILDFLAG(IS_ANDROID)
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
      return CreateSecurityDescription(SecuritySummaryColor::GREEN, 0,
                                       IDS_PAGE_INFO_INTERNAL_PAGE,
                                       SecurityDescriptionType::INTERNAL);
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
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
        default:
          // Do not show details for secure connections.
          int details = 0;
          if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
            // When the QWAC feature is enabled, a UI that more closely matches
            // desktop is used.
            details = IDS_PAGE_INFO_SECURE_DETAILS;
          }
          return CreateSecurityDescription(
              SecuritySummaryColor::GREEN, IDS_PAGE_INFO_SECURE_SUMMARY,
              details, SecurityDescriptionType::CONNECTION);
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
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP:
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
  for (const PermissionUIInfo& info : GetContentSettingsUIInfo()) {
    if (info.type == type)
      return l10n_util::GetStringUTF16(info.string_id);
  }
  NOTREACHED();
}

// static
std::u16string PageInfoUI::PermissionTypeToUIStringMidSentence(
    ContentSettingsType type) {
  for (const PermissionUIInfo& info : GetContentSettingsUIInfo()) {
    if (info.type == type)
      return l10n_util::GetStringUTF16(info.string_id_mid_sentence);
  }
  NOTREACHED();
}

// static
std::u16string PageInfoUI::PermissionTooltipUiString(
    ContentSettingsType type,
    const std::optional<url::Origin>& requesting_origin) {
  switch (type) {
    case ContentSettingsType::STORAGE_ACCESS:
      return l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SELECTOR_STORAGE_ACCESS_TOOLTIP,
          url_formatter::FormatOriginForSecurityDisplay(
              *requesting_origin,
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      DCHECK(!requesting_origin.has_value());
      return l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SELECTOR_TOOLTIP,
          PageInfoUI::PermissionTypeToUIString(type));
  }
}

// static
std::u16string PageInfoUI::PermissionSubpageButtonTooltipString(
    ContentSettingsType type) {
  return l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_BUTTON_TOOLTIP,
      PageInfoUI::PermissionTypeToUIStringMidSentence(type));
}

// static
base::span<const PageInfoUI::PermissionUIInfo>
PageInfoUI::GetContentSettingsUIInfoForTesting() {
  return GetContentSettingsUIInfo();
}

// static
std::u16string PageInfoUI::PermissionStateToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  int message_id = kInvalidResourceID;

  auto* info = PermissionSettingsRegistry::GetInstance()->Get(permission.type);
  CHECK(info);

  const PermissionSetting effective_setting = GetEffectiveSetting(
      permission.type, permission.setting, permission.default_setting);
  if (info->delegate().IsAnyPermissionAllowed(effective_setting)) {
#if !BUILDFLAG(IS_ANDROID)
    if (permission.type == ContentSettingsType::SOUND &&
        delegate->IsBlockAutoPlayEnabled() && !permission.setting) {
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_BUTTON_TEXT_AUTOMATIC_BY_DEFAULT);
    }
#endif
    if (!permission.setting) {
      message_id = IDS_PAGE_INFO_STATE_TEXT_ALLOWED_BY_DEFAULT;
#if !BUILDFLAG(IS_ANDROID)
      } else if (permission.is_one_time) {
        DCHECK_EQ(permission.source, SettingSource::kUser);
        DCHECK(permissions::PermissionUtil::DoesSupportTemporaryGrants(
            permission.type));
        message_id = IDS_PAGE_INFO_STATE_TEXT_ALLOWED_ONCE;
#endif
      } else {
        message_id = IDS_PAGE_INFO_STATE_TEXT_ALLOWED;
      }
  } else if (info->delegate().IsUndecided(effective_setting)) {
    return GetPermissionAskStateString(permission.type);
  } else {
    if (!permission.setting) {
#if !BUILDFLAG(IS_ANDROID)
        if (permission.type == ContentSettingsType::SOUND) {
          return l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_DEFAULT);
        }
#endif
        message_id = IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_BY_DEFAULT;
    } else {
#if !BUILDFLAG(IS_ANDROID)
        if (permission.type == ContentSettingsType::SOUND) {
          return l10n_util::GetStringUTF16(IDS_PAGE_INFO_STATE_TEXT_MUTED);
        }
#endif
        message_id = IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED;
    }
  }

  return l10n_util::GetStringUTF16(message_id);
}

// static
std::u16string PageInfoUI::PermissionMainPageStateToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  std::u16string auto_blocked_text =
      PermissionAutoBlockedToUIString(delegate, permission);
  if (!auto_blocked_text.empty())
    return auto_blocked_text;

  auto* info = PermissionSettingsRegistry::GetInstance()->Get(permission.type);
  CHECK(info);

  if (permission.is_one_time || !permission.setting ||
      info->delegate().IsUndecided(*permission.setting)) {
    return PermissionStateToUIString(delegate, permission);
  }

  if (permission.is_in_use) {
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW);
  }

  if (!info->delegate().IsAnyPermissionAllowed(*permission.setting) ||
      permission.last_used == base::Time()) {
    return std::u16string();
  }

  base::TimeDelta time_delta = base::Time::Now() - permission.last_used;
  if (time_delta < base::Minutes(1)) {
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_RECENTLY_USED);
  }

  std::u16string used_time_string =
      ui::TimeFormat::Simple(ui::TimeFormat::Format::FORMAT_DURATION,
                             ui::TimeFormat::Length::LENGTH_LONG, time_delta);
  return l10n_util::GetStringFUTF16(IDS_PAGE_INFO_PERMISSION_USED_TIME_AGO,
                                    used_time_string);
}

// static
std::u16string PageInfoUI::PermissionManagedTooltipToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  int message_id = kInvalidResourceID;
  switch (permission.source) {
    case SettingSource::kPolicy:
      message_id = IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY;
      break;
    case SettingSource::kExtension:
      // TODO(crbug.com/40775890): Consider "enforced" instead of "managed".
      message_id = IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION;
      break;
    default:
      break;
  }

  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

// static
std::u16string PageInfoUI::PermissionAutoBlockedToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  int message_id = kInvalidResourceID;
  // TODO(crbug.com/40123120): PageInfo::PermissionInfo should be modified
  // to contain all needed information regarding Automatically Blocked flag.
  auto* info = PermissionSettingsRegistry::GetInstance()->Get(permission.type);
  CHECK(info);
  if (permission.setting && !info->delegate().IsBlocked(*permission.setting) &&
      permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
          permission.type)) {
    content::PermissionResult permission_result(
        PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED);
    if (permissions::PermissionUtil::IsPermission(permission.type)) {
      blink::PermissionType permission_type =
          permissions::PermissionUtil::ContentSettingsTypeToPermissionType(
              permission.type);
      permission_result = delegate->GetPermissionResult(permission_type);
    } else if (permission.type == ContentSettingsType::FEDERATED_IDENTITY_API) {
      std::optional<content::PermissionResult> embargo_result =
          delegate->GetEmbargoResult(permission.type);
      if (embargo_result)
        permission_result = *embargo_result;
    }

    switch (permission_result.source) {
      case content::PermissionStatusSource::MULTIPLE_DISMISSALS:
        message_id = IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED;
        break;
      case content::PermissionStatusSource::MULTIPLE_IGNORES:
        message_id = IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED;
        break;
      default:
        break;
    }
  }
  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

// static
void PageInfoUI::ToggleBetweenAllowAndBlock(
    PageInfo::PermissionInfo& permission) {
  if (!std::holds_alternative<ContentSetting>(permission.default_setting)) {
    NOTIMPLEMENTED();
  }

  auto opposite_to_block_setting =
      permissions::PermissionUtil::IsGuardContentSetting(permission.type)
          ? CONTENT_SETTING_ASK
          : CONTENT_SETTING_ALLOW;
  ContentSetting setting = std::get<ContentSetting>(
      permission.setting.value_or(CONTENT_SETTING_DEFAULT));
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      DCHECK_EQ(opposite_to_block_setting, CONTENT_SETTING_ALLOW);
      permission.setting = CONTENT_SETTING_BLOCK;
      permission.is_one_time = false;
      permission.is_in_use = false;
      break;
    case CONTENT_SETTING_BLOCK:
      permission.setting = opposite_to_block_setting;
      permission.is_one_time = false;
      permission.is_in_use = false;
      break;
    case CONTENT_SETTING_DEFAULT: {
      CreateOppositeToDefaultSiteException(permission,
                                           opposite_to_block_setting);

      // If one-time permissions are supported, permission should go from
      // default state to allow once state, not directly to allow.
      if (permissions::PermissionUtil::DoesSupportTemporaryGrants(
              permission.type)) {
        permission.is_one_time = true;
      }
      permission.is_in_use = false;
      break;
    }
    case CONTENT_SETTING_ASK:
      DCHECK_EQ(opposite_to_block_setting, CONTENT_SETTING_ASK);
      permission.setting = CONTENT_SETTING_BLOCK;
      break;
    default:
      NOTREACHED();
  }
}

// static
void PageInfoUI::ToggleBetweenRememberAndForget(
    PageInfo::PermissionInfo& permission) {
  DCHECK(permissions::PermissionUtil::IsPermission(permission.type));
  if (!std::holds_alternative<ContentSetting>(permission.default_setting)) {
    NOTIMPLEMENTED();
  }

  ContentSetting setting = std::get<ContentSetting>(
      permission.setting.value_or(CONTENT_SETTING_DEFAULT));
  switch (setting) {
    case CONTENT_SETTING_ALLOW: {
      // If one-time permissions are supported, toggle is_one_time.
      // Otherwise, go directly to default.
      if (permissions::PermissionUtil::DoesSupportTemporaryGrants(
              permission.type)) {
        permission.is_one_time = !permission.is_one_time;
      } else {
        permission.setting.reset();
      }
      break;
    }
    case CONTENT_SETTING_BLOCK:
      // TODO(olesiamarukhno): If content setting is in the blocklist, setting
      // it to default, doesn't do anything. Fix this before introducing
      // subpages for content settings (not permissions).
      permission.setting.reset();
      permission.is_one_time = false;
      break;
    case CONTENT_SETTING_DEFAULT: {
      // When user checks the checkbox to remember the permission setting,
      // it should go to the "allow" state, only if default setting is
      // explicitly allow.

      ContentSetting default_setting =
          std::get<ContentSetting>(permission.default_setting);
      if (default_setting == CONTENT_SETTING_ALLOW) {
        permission.setting = CONTENT_SETTING_ALLOW;
      } else {
        permission.setting = CONTENT_SETTING_BLOCK;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

// static
bool PageInfoUI::IsToggleOn(const PageInfo::PermissionInfo& permission) {
  PermissionSetting effective_setting = GetEffectiveSetting(
      permission.type, permission.setting, permission.default_setting);

  // Since Automatic Picture-in-Picture is essentially allowed while in the ASK
  // state, we display the toggle as on for either ASK or ALLOW.
  if (permission.type == ContentSettingsType::AUTO_PICTURE_IN_PICTURE) {
    ContentSetting effective_content_setting =
        std::get<ContentSetting>(effective_setting);
    return effective_content_setting == CONTENT_SETTING_ASK ||
           effective_content_setting == CONTENT_SETTING_ALLOW;
  }

  auto* info = PermissionSettingsRegistry::GetInstance()->Get(permission.type);
  CHECK(info);
  return permissions::PermissionUtil::IsGuardContentSetting(permission.type)
             ? std::get<ContentSetting>(effective_setting) ==
                   CONTENT_SETTING_ASK
             : info->delegate().IsAnyPermissionAllowed(effective_setting);
}

// static
SkColor PageInfoUI::GetSecondaryTextColor() {
  return SK_ColorGRAY;
}

#if BUILDFLAG(IS_ANDROID)
// static
int PageInfoUI::GetIdentityIconID(PageInfo::SiteIdentityStatus status) {
  switch (status) {
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
      if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
        return IDR_PAGEINFO_INTERNAL;
      } else {
        return IDR_PAGEINFO_GOOD;
      }
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP:
      if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
        return IDR_PAGEINFO_GOOD_NEW;
      } else {
        return IDR_PAGEINFO_GOOD;
      }
    case PageInfo::SITE_IDENTITY_STATUS_NO_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ERROR:
    case PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
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
    case PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP:
      return IDR_PAGEINFO_GOOD_COLOR;
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
    case PageInfo::SITE_CONNECTION_STATUS_ISOLATED_WEB_APP:
      return IDR_PAGEINFO_GOOD_COLOR;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
    case PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED:
      return IDR_PAGEINFO_WARNING_COLOR;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      return IDR_PAGEINFO_BAD_COLOR;
  }
  return 0;
}

#endif  // BUILDFLAG(IS_ANDROID)

// static
bool PageInfoUI::ContentSettingsTypeInPageInfo(ContentSettingsType type) {
  for (const PermissionUIInfo& info : GetContentSettingsUIInfo()) {
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
    case security_state::SafetyTipStatus::kLookalike:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
      return CreateSecurityDescriptionForSafetyTip(info.status, info.safe_url);

    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      break;
  }
  return nullptr;
}

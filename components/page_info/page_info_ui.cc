// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_info/core/features.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
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

const int kInvalidResourceID = -1;

// The resource IDs for the strings that are displayed on the permissions
// button if the permission setting is managed by policy.
const int kPermissionButtonTextIDPolicyManaged[] = {
    kInvalidResourceID,
    IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_POLICY,
    IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_POLICY,
    IDS_PAGE_INFO_PERMISSION_ASK_BY_POLICY,
    kInvalidResourceID,
    kInvalidResourceID};
static_assert(std::size(kPermissionButtonTextIDPolicyManaged) ==
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
static_assert(std::size(kPermissionButtonTextIDExtensionManaged) ==
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
static_assert(std::size(kPermissionButtonTextIDUserManaged) ==
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
static_assert(std::size(kPermissionButtonTextIDDefaultSetting) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kPermissionButtonTextIDDefaultSetting array size is incorrect");

#if !BUILDFLAG(IS_ANDROID)
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
    std::size(kSoundPermissionButtonTextIDUserManaged) ==
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
    std::size(kSoundPermissionButtonTextIDDefaultSetting) ==
        CONTENT_SETTING_NUM_SETTINGS,
    "kSoundPermissionButtonTextIDDefaultSetting array size is incorrect");
#endif

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
    {ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
     IDS_SITE_SETTINGS_TYPE_CAMERA_PAN_TILT_ZOOM,
     IDS_SITE_SETTINGS_TYPE_CAMERA_PAN_TILT_ZOOM_MID_SENTENCE},
    {ContentSettingsType::FEDERATED_IDENTITY_API,
     IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API,
     IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API_MID_SENTENCE},
    {ContentSettingsType::IDLE_DETECTION, IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION,
     IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION_MID_SENTENCE},
#if !BUILDFLAG(IS_ANDROID)
    // Page Info Permissions that are not defined in Android.
    {ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
     IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE,
     IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE_MID_SENTENCE},
    {ContentSettingsType::LOCAL_FONTS, IDS_SITE_SETTINGS_TYPE_FONT_ACCESS,
     IDS_SITE_SETTINGS_TYPE_FONT_ACCESS_MID_SENTENCE},
    {ContentSettingsType::HID_GUARD, IDS_SITE_SETTINGS_TYPE_HID_DEVICES,
     IDS_SITE_SETTINGS_TYPE_HID_DEVICES_MID_SENTENCE},
    {ContentSettingsType::IMAGES, IDS_SITE_SETTINGS_TYPE_IMAGES,
     IDS_SITE_SETTINGS_TYPE_IMAGES_MID_SENTENCE},
    {ContentSettingsType::SERIAL_GUARD, IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS,
     IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS_MID_SENTENCE},
    {ContentSettingsType::WINDOW_MANAGEMENT,
     IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT,
     IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT_MID_SENTENCE},
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

void SetTargetContentSetting(PageInfo::PermissionInfo& permission,
                             ContentSetting target_setting) {
  // If content setting's default setting matches target setting, set
  // default setting to avoid crearing a site exception.
  permission.setting = permission.default_setting == target_setting
                           ? CONTENT_SETTING_DEFAULT
                           : target_setting;
}

void CreateOppositeToDefaultSiteException(
    PageInfo::PermissionInfo& permission,
    ContentSetting opposite_to_block_setting) {
  // For guard content settings opposite to block setting is ask, for the
  // rest opposite is allow.
  permission.setting = permission.default_setting == opposite_to_block_setting
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
      message_id = IDS_PAGE_INFO_STATE_TEXT_MIDI_ASK;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CAMERA_ASK;
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      message_id = IDS_PAGE_INFO_STATE_TEXT_CAMERA_PAN_TILT_ZOOM_ASK;
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
    default:
      NOTREACHED();
  }

  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace

PageInfoUI::CookieInfo::CookieInfo() : allowed(-1), blocked(-1) {}

PageInfoUI::CookiesNewInfo::CookiesNewInfo() = default;

PageInfoUI::CookiesNewInfo::~CookiesNewInfo() = default;

PageInfoUI::CookiesFpsInfo::CookiesFpsInfo(const std::u16string& owner_name)
    : owner_name(owner_name) {}

PageInfoUI::CookiesFpsInfo::~CookiesFpsInfo() = default;

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
    case PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE: {
#if BUILDFLAG(FULL_SAFE_BROWSING)
      auto security_description = CreateSecurityDescription(
          SecuritySummaryColor::RED,
          IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY, 0,
          SecurityDescriptionType::SAFE_BROWSING);
      security_description->details = identity_info.safe_browsing_details;
      return security_description;
#endif
      NOTREACHED();
      break;
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
#endif
      NOTREACHED();
      break;
    }
    case PageInfo::SAFE_BROWSING_STATUS_BILLING:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_BILLING_SUMMARY,
                                       IDS_PAGE_INFO_BILLING_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_SUMMARY,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_DETAILS,
                                       SecurityDescriptionType::SAFE_BROWSING);
    case PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK:
      return CreateSecurityDescription(SecuritySummaryColor::RED,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_SUMMARY,
                                       IDS_PAGE_INFO_ENTERPRISE_WARN_DETAILS,
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
      [[fallthrough]];
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
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

          auto description = CreateSecurityDescription(
              SecuritySummaryColor::GREEN, IDS_PAGE_INFO_SECURE_SUMMARY,
              base::FeatureList::IsEnabled(
                  omnibox::kUpdatedConnectionSecurityIndicators)
                  ? IDS_PAGE_INFO_SECURE_DETAILS_V2
                  : IDS_PAGE_INFO_SECURE_DETAILS,
              SecurityDescriptionType::CONNECTION);
          if (identity_info.identity_status ==
              PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT) {
            description->details = l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_ADMIN_PROVIDED_CERT_DETAILS,
                base::UTF8ToUTF16(identity_info.site_identity));
          }
          return description;
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
  return std::u16string();
}

// static
std::u16string PageInfoUI::PermissionTypeToUIStringMidSentence(
    ContentSettingsType type) {
  for (const PermissionUIInfo& info : GetContentSettingsUIInfo()) {
    if (info.type == type)
      return l10n_util::GetStringUTF16(info.string_id_mid_sentence);
  }
  NOTREACHED();
  return std::u16string();
}

// static
base::span<const PageInfoUI::PermissionUIInfo>
PageInfoUI::GetContentSettingsUIInfoForTesting() {
  return GetContentSettingsUIInfo();
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
#if !BUILDFLAG(IS_ANDROID)
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
      [[fallthrough]];
    case content_settings::SETTING_SOURCE_POLICY:
    case content_settings::SETTING_SOURCE_EXTENSION:
#if !BUILDFLAG(IS_ANDROID)
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
std::u16string PageInfoUI::PermissionStateToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  int message_id = kInvalidResourceID;
  ContentSetting effective_setting = GetEffectiveSetting(
      permission.type, permission.setting, permission.default_setting);
  switch (effective_setting) {
    case CONTENT_SETTING_ALLOW:
#if !BUILDFLAG(IS_ANDROID)
      if (permission.type == ContentSettingsType::SOUND &&
          delegate->IsBlockAutoPlayEnabled() &&
          permission.setting == CONTENT_SETTING_DEFAULT) {
        message_id = IDS_PAGE_INFO_BUTTON_TEXT_AUTOMATIC_BY_DEFAULT;
        break;
      }
#endif
      if (permission.setting == CONTENT_SETTING_DEFAULT) {
        message_id = IDS_PAGE_INFO_STATE_TEXT_ALLOWED_BY_DEFAULT;
#if !BUILDFLAG(IS_ANDROID)
      } else if (permission.is_one_time) {
        DCHECK_EQ(permission.source, content_settings::SETTING_SOURCE_USER);
        DCHECK(permissions::PermissionUtil::CanPermissionBeAllowedOnce(
            permission.type));
        message_id = delegate->IsMultipleTabsOpen()
                         ? IDS_PAGE_INFO_STATE_TEXT_ALLOWED_ONCE_MULTIPLE_TAB
                         : IDS_PAGE_INFO_STATE_TEXT_ALLOWED_ONCE_ONE_TAB;
#endif
      } else {
        message_id = IDS_PAGE_INFO_STATE_TEXT_ALLOWED;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      if (permission.setting == CONTENT_SETTING_DEFAULT) {
#if !BUILDFLAG(IS_ANDROID)
        if (permission.type == ContentSettingsType::SOUND) {
          message_id = IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_DEFAULT;
          break;
        }
#endif
        message_id = IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_BY_DEFAULT;
      } else {
#if !BUILDFLAG(IS_ANDROID)
        if (permission.type == ContentSettingsType::SOUND) {
          message_id = IDS_PAGE_INFO_STATE_TEXT_MUTED;
          break;
        }
#endif
        message_id = IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED;
      }
      break;
    case CONTENT_SETTING_ASK:
      return GetPermissionAskStateString(permission.type);
    default:
      NOTREACHED();
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

  if (permission.is_one_time || permission.setting == CONTENT_SETTING_DEFAULT ||
      permission.setting == CONTENT_SETTING_ASK) {
    return PermissionStateToUIString(delegate, permission);
  }

  return std::u16string();
}

// static
std::u16string PageInfoUI::PermissionManagedTooltipToUIString(
    PageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission) {
  int message_id = kInvalidResourceID;
  switch (permission.source) {
    case content_settings::SettingSource::SETTING_SOURCE_POLICY:
      message_id = IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY;
      break;
    case content_settings::SettingSource::SETTING_SOURCE_EXTENSION:
      // TODO(crbug.com/1225563): Consider "enforced" instead of "managed".
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
  // TODO(crbug.com/1063023): PageInfo::PermissionInfo should be modified
  // to contain all needed information regarding Automatically Blocked flag.
  if (permission.setting == CONTENT_SETTING_BLOCK &&
      permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
          permission.type)) {
    permissions::PermissionResult permission_result(
        CONTENT_SETTING_DEFAULT,
        permissions::PermissionStatusSource::UNSPECIFIED);
    if (permissions::PermissionUtil::IsPermission(permission.type)) {
      blink::PermissionType permission_type =
          permissions::PermissionUtil::ContentSettingTypeToPermissionType(
              permission.type);
      permission_result = delegate->GetPermissionResult(permission_type);
    } else if (permission.type == ContentSettingsType::FEDERATED_IDENTITY_API) {
      absl::optional<permissions::PermissionResult> embargo_result =
          delegate->GetEmbargoResult(permission.type);
      if (embargo_result)
        permission_result = *embargo_result;
    }

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
  if (message_id == kInvalidResourceID)
    return std::u16string();
  return l10n_util::GetStringUTF16(message_id);
}

// static
void PageInfoUI::ToggleBetweenAllowAndBlock(
    PageInfo::PermissionInfo& permission) {
  auto opposite_to_block_setting =
      permissions::PermissionUtil::IsGuardContentSetting(permission.type)
          ? CONTENT_SETTING_ASK
          : CONTENT_SETTING_ALLOW;
  switch (permission.setting) {
    case CONTENT_SETTING_ALLOW:
      DCHECK_EQ(opposite_to_block_setting, CONTENT_SETTING_ALLOW);
      if (permission.is_one_time) {
        permission.setting = CONTENT_SETTING_DEFAULT;
      } else {
        SetTargetContentSetting(permission, CONTENT_SETTING_BLOCK);
      }
      permission.is_one_time = false;
      break;
    case CONTENT_SETTING_BLOCK:
      SetTargetContentSetting(permission, opposite_to_block_setting);
      permission.is_one_time = false;
      break;
    case CONTENT_SETTING_DEFAULT: {
      CreateOppositeToDefaultSiteException(permission,
                                           opposite_to_block_setting);
      // If one-time permissions are supported, permission should go from
      // default state to allow once state, not directly to allow.
      if (permissions::PermissionUtil::CanPermissionBeAllowedOnce(
              permission.type)) {
        permission.is_one_time = true;
      }
      break;
    }
    case CONTENT_SETTING_ASK:
      DCHECK_EQ(opposite_to_block_setting, CONTENT_SETTING_ASK);
      SetTargetContentSetting(permission, CONTENT_SETTING_BLOCK);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// static
void PageInfoUI::ToggleBetweenRememberAndForget(
    PageInfo::PermissionInfo& permission) {
  DCHECK(permissions::PermissionUtil::IsPermission(permission.type));
  switch (permission.setting) {
    case CONTENT_SETTING_ALLOW: {
      // If one-time permissions are supported, toggle is_one_time.
      // Otherwise, go directly to default.
      if (permissions::PermissionUtil::CanPermissionBeAllowedOnce(
              permission.type)) {
        permission.is_one_time = !permission.is_one_time;
      } else {
        permission.setting = CONTENT_SETTING_DEFAULT;
      }
      break;
    }
    case CONTENT_SETTING_BLOCK:
      // TODO(olesiamarukhno): If content setting is in the blocklist, setting
      // it to default, doesn't do anything. Fix this before introducing
      // subpages for content settings (not permissions).
      permission.setting = CONTENT_SETTING_DEFAULT;
      permission.is_one_time = false;
      break;
    case CONTENT_SETTING_DEFAULT:
      // When user checks the checkbox to remember the permission setting,
      // it should go to the "allow" state, only if default setting is
      // explicitly allow.
      if (permission.default_setting == CONTENT_SETTING_ALLOW) {
        permission.setting = CONTENT_SETTING_ALLOW;
      } else {
        permission.setting = CONTENT_SETTING_BLOCK;
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

// static
bool PageInfoUI::IsToggleOn(const PageInfo::PermissionInfo& permission) {
  ContentSetting effective_setting = GetEffectiveSetting(
      permission.type, permission.setting, permission.default_setting);
  return permissions::PermissionUtil::IsGuardContentSetting(permission.type)
             ? effective_setting == CONTENT_SETTING_ASK
             : effective_setting == CONTENT_SETTING_ALLOW;
}

// static
SkColor PageInfoUI::GetSecondaryTextColor() {
  return SK_ColorGRAY;
}

#if BUILDFLAG(IS_ANDROID)
// static
int PageInfoUI::GetIdentityIconID(PageInfo::SiteIdentityStatus status) {
  switch (status) {
    case PageInfo::SITE_IDENTITY_STATUS_UNKNOWN:
    case PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE:
    case PageInfo::SITE_IDENTITY_STATUS_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_EV_CERT:
    case PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP:
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
    case PageInfo::SITE_CONNECTION_STATUS_ISOLATED_WEB_APP:
      return IDR_PAGEINFO_GOOD;
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE:
    case PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION:
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
    case PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP:
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

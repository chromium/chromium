// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/security_state/core/features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using content::BrowserThread;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::RequestOutcome;

namespace {

// The list of content settings types to display on the Page Info UI. THE
// ORDER OF THESE ITEMS IS IMPORTANT and comes from https://crbug.com/610358. To
// propose changing it, email security-dev@chromium.org.
ContentSettingsType kPermissionType[] = {
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::SENSORS,
    ContentSettingsType::NOTIFICATIONS,
    ContentSettingsType::JAVASCRIPT,
#if !defined(OS_ANDROID)
    ContentSettingsType::PLUGINS,
    ContentSettingsType::IMAGES,
#endif
    ContentSettingsType::POPUPS,
    ContentSettingsType::ADS,
    ContentSettingsType::BACKGROUND_SYNC,
    ContentSettingsType::SOUND,
    ContentSettingsType::AUTOMATIC_DOWNLOADS,
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
#endif
    ContentSettingsType::AUTOPLAY,
    ContentSettingsType::MIDI_SYSEX,
    ContentSettingsType::CLIPBOARD_READ,
#if defined(OS_ANDROID)
    ContentSettingsType::NFC,
#endif
    ContentSettingsType::USB_GUARD,
#if !defined(OS_ANDROID)
    ContentSettingsType::SERIAL_GUARD,
    ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
#endif
    ContentSettingsType::BLUETOOTH_SCANNING,
};

// Checks whether this permission is currently the factory default, as set by
// Chrome. Specifically, that the following three conditions are true:
//   - The current active setting comes from the default or pref provider.
//   - The setting is the factory default setting (as opposed to a global
//     default setting set by the user).
//   - The setting is a wildcard setting applying to all origins (which can only
//     be set from the default provider).
bool IsPermissionFactoryDefault(HostContentSettingsMap* content_settings,
                                const PageInfoUI::PermissionInfo& info) {
  const ContentSetting factory_default_setting =
      content_settings::ContentSettingsRegistry::GetInstance()
          ->Get(info.type)
          ->GetInitialDefaultSetting();
  return (info.source == content_settings::SETTING_SOURCE_USER &&
          factory_default_setting == info.default_setting &&
          info.setting == CONTENT_SETTING_DEFAULT);
}

// Determines whether to show permission |type| in the Page Info UI. Only
// applies to permissions listed in |kPermissionType|.
bool ShouldShowPermission(
    const PageInfoUI::PermissionInfo& info,
    const GURL& site_url,
    HostContentSettingsMap* content_settings,
    content::WebContents* web_contents,
    TabSpecificContentSettings* tab_specific_content_settings) {
  // Note |ContentSettingsType::ADS| will show up regardless of its default
  // value when it has been activated on the current origin.
  if (info.type == ContentSettingsType::ADS) {
    if (!base::FeatureList::IsEnabled(
            subresource_filter::kSafeBrowsingSubresourceFilter)) {
      return false;
    }

    // The setting for subresource filtering should not show up if the site is
    // not activated, both on android and desktop platforms.
    return content_settings->GetWebsiteSetting(
               site_url, GURL(), ContentSettingsType::ADS_DATA, std::string(),
               nullptr) != nullptr;
  }

  if (info.type == ContentSettingsType::SOUND) {
    // The sound content setting should always show up when the tab has played
    // audio.
    if (web_contents && web_contents->WasEverAudible())
      return true;
  }

#if defined(OS_ANDROID)
  // Special geolocation DSE settings apply only on Android, so make sure it
  // gets checked there regardless of default setting on Desktop.
  if (info.type == ContentSettingsType::GEOLOCATION)
    return true;

  // The Native File System write permission is desktop only at the moment.
  if (info.type == ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD)
    return false;
#else
  // Flash is shown if the user has ever changed its setting for |site_url|.
  if (info.type == ContentSettingsType::PLUGINS &&
      content_settings->GetWebsiteSetting(site_url, site_url,
                                          ContentSettingsType::PLUGINS_DATA,
                                          std::string(), nullptr) != nullptr) {
    return true;
  }

  // Autoplay is Android-only at the moment.
  if (info.type == ContentSettingsType::AUTOPLAY)
    return false;

  // NFC is Android-only at the moment.
  if (info.type == ContentSettingsType::NFC)
    return false;

  // Display the Native File System write permission if the Native File System
  // API is currently being used.
  if (info.type == ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD &&
      web_contents->HasNativeFileSystemHandles()) {
    return true;
  }
#endif

  // Show the content setting if it has been changed by the user since the last
  // page load.
  if (tab_specific_content_settings->HasContentSettingChangedViaPageInfo(
          info.type)) {
    return true;
  }

  // Show the content setting when it has a non-default value.
  if (!IsPermissionFactoryDefault(content_settings, info))
    return true;

  return false;
}

// If the |visible_security_state| indicates that mixed content or certificate
// errors were present, update |connection_status| and |connection_details|.
void ReportAnyInsecureContent(
    const security_state::VisibleSecurityState& visible_security_state,
    PageInfo::SiteConnectionStatus* connection_status,
    base::string16* connection_details) {
  bool displayed_insecure_content =
      visible_security_state.displayed_mixed_content;
  bool ran_insecure_content = visible_security_state.ran_mixed_content;
  // Only note subresources with certificate errors if the main resource was
  // loaded without major certificate errors. If the main resource had a
  // certificate error, then it would not be that useful (and could
  // potentially be confusing) to warn about subresources that had certificate
  // errors too.
  if (!net::IsCertStatusError(visible_security_state.cert_status)) {
    displayed_insecure_content =
        displayed_insecure_content ||
        visible_security_state.displayed_content_with_cert_errors;
    ran_insecure_content = ran_insecure_content ||
                           visible_security_state.ran_content_with_cert_errors;
  }

  // Only one insecure content warning is displayed; show the most severe.
  if (ran_insecure_content) {
    *connection_status =
        PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE;
    connection_details->assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK, *connection_details,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_ERROR)));
    return;
  }
  if (visible_security_state.contained_mixed_form) {
    *connection_status = PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION;
    connection_details->assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK, *connection_details,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_FORM_WARNING)));
    return;
  }
  if (displayed_insecure_content) {
    *connection_status =
        PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE;
    connection_details->assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK, *connection_details,
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)));
  }
}

base::string16 GetSimpleSiteName(const GURL& url) {
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

#if !defined(OS_ANDROID)
ChooserContextBase* GetSerialChooserContext(Profile* profile) {
  return SerialChooserContextFactory::GetForProfile(profile);
}
#endif

// The list of chooser types that need to display entries in the Website
// Settings UI. THE ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it,
// email security-dev@chromium.org.
const PageInfo::ChooserUIInfo kChooserUIInfo[] = {
    {ContentSettingsType::USB_CHOOSER_DATA, &GetUsbChooserContext,
     IDS_PAGE_INFO_USB_DEVICE_SECONDARY_LABEL,
     IDS_PAGE_INFO_USB_DEVICE_ALLOWED_BY_POLICY_LABEL,
     IDS_PAGE_INFO_DELETE_USB_DEVICE, &UsbChooserContext::GetObjectName},
#if !defined(OS_ANDROID)
    {ContentSettingsType::SERIAL_CHOOSER_DATA, &GetSerialChooserContext,
     IDS_PAGE_INFO_SERIAL_PORT_SECONDARY_LABEL,
     /*allowed_by_policy_description_string_id=*/-1,
     IDS_PAGE_INFO_DELETE_SERIAL_PORT, &SerialChooserContext::GetObjectName},
#endif
};

// Time open histogram prefixes.
const char kPageInfoTimePrefix[] = "Security.PageInfo.TimeOpen";
const char kPageInfoTimeActionPrefix[] = "Security.PageInfo.TimeOpen.Action";
const char kPageInfoTimeNoActionPrefix[] =
    "Security.PageInfo.TimeOpen.NoAction";

}  // namespace

PageInfo::PageInfo(
    PageInfoUI* ui,
    Profile* profile,
    TabSpecificContentSettings* tab_specific_content_settings,
    content::WebContents* web_contents,
    const GURL& url,
    security_state::SecurityLevel security_level,
    const security_state::VisibleSecurityState& visible_security_state)
    : content::WebContentsObserver(web_contents),
      ui_(ui),
      show_info_bar_(false),
      site_url_(url),
      site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      safe_browsing_status_(SAFE_BROWSING_STATUS_NONE),
      safety_tip_info_({security_state::SafetyTipStatus::kUnknown, GURL()}),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button_(false),
      content_settings_(HostContentSettingsMapFactory::GetForProfile(profile)),
      chrome_ssl_host_state_delegate_(
          ChromeSSLHostStateDelegateFactory::GetForProfile(profile)),
      tab_specific_content_settings_(tab_specific_content_settings),
      did_revoke_user_ssl_decisions_(false),
      profile_(profile),
      security_level_(security_state::NONE),
      visible_security_state_for_metrics_(visible_security_state),
#if BUILDFLAG(FULL_SAFE_BROWSING)
      password_protection_service_(
          safe_browsing::ChromePasswordProtectionService::
              GetPasswordProtectionService(profile_)),
#endif
      show_change_password_buttons_(false),
      did_perform_action_(false) {
  ComputeUIInputs(url, security_level, visible_security_state);

  PresentSitePermissions();
  PresentSiteIdentity();
  PresentSiteData();
  PresentPageFeatureInfo();

  // Every time the Page Info UI is opened a |PageInfo| object is
  // created. So this counts how ofter the Page Info UI is opened.
  RecordPageInfoAction(PAGE_INFO_OPENED);

  // Record the time when the Page Info UI is opened so the total time it is
  // open can be measured.
  start_time_ = base::TimeTicks::Now();
}

PageInfo::~PageInfo() {
  // Check if Re-enable warnings button was visible, if so, log on UMA whether
  // it was clicked or not.
  SSLCertificateDecisionsDidRevoke user_decision =
      did_revoke_user_ssl_decisions_ ? USER_CERT_DECISIONS_REVOKED
                                     : USER_CERT_DECISIONS_NOT_REVOKED;
  if (show_ssl_decision_revoke_button_) {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.did_user_revoke_decisions2",
                              user_decision,
                              END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM);
  }

  // Record the total time the Page Info UI was open for all opens as well as
  // split between whether any action was taken.
  base::UmaHistogramCustomTimes(
      security_state::GetSecurityLevelHistogramName(
          kPageInfoTimePrefix, security_level_),
      base::TimeTicks::Now() - start_time_,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1), 100);
  base::UmaHistogramCustomTimes(
      security_state::GetSafetyTipHistogramName(kPageInfoTimePrefix,
                                                safety_tip_info_.status),
      base::TimeTicks::Now() - start_time_,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1), 100);
  base::UmaHistogramCustomTimes(
      security_state::GetLegacyTLSHistogramName(
          kPageInfoTimePrefix, visible_security_state_for_metrics_),
      base::TimeTicks::Now() - start_time_,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1), 100);

  if (did_perform_action_) {
    base::UmaHistogramCustomTimes(
        security_state::GetSecurityLevelHistogramName(
            kPageInfoTimeActionPrefix, security_level_),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
    base::UmaHistogramCustomTimes(
        security_state::GetSafetyTipHistogramName(kPageInfoTimeActionPrefix,
                                                  safety_tip_info_.status),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
    base::UmaHistogramCustomTimes(
        security_state::GetLegacyTLSHistogramName(
            kPageInfoTimeActionPrefix, visible_security_state_for_metrics_),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
  } else {
    base::UmaHistogramCustomTimes(
        security_state::GetSecurityLevelHistogramName(
            kPageInfoTimeNoActionPrefix, security_level_),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
    base::UmaHistogramCustomTimes(
        security_state::GetSafetyTipHistogramName(kPageInfoTimeNoActionPrefix,
                                                  safety_tip_info_.status),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
    base::UmaHistogramCustomTimes(
        security_state::GetLegacyTLSHistogramName(
            kPageInfoTimeNoActionPrefix, visible_security_state_for_metrics_),
        base::TimeTicks::Now() - start_time_,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
        100);
  }
}

void PageInfo::UpdateSecurityState(
    security_state::SecurityLevel security_level,
    const security_state::VisibleSecurityState& visible_security_state) {
  visible_security_state_for_metrics_ = visible_security_state;
  ComputeUIInputs(site_url_, security_level, visible_security_state);
  PresentSiteIdentity();
}

void PageInfo::RecordPageInfoAction(PageInfoAction action) {
  if (action != PAGE_INFO_OPENED)
    did_perform_action_ = true;

  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.Action", action, PAGE_INFO_COUNT);

  if (web_contents()) {
    ukm::builders::PageInfoBubble(
        ukm::GetSourceIdForWebContentsDocument(web_contents()))
        .SetActionTaken(action)
        .Record(ukm::UkmRecorder::Get());
  }

  base::UmaHistogramEnumeration(
      security_state::GetSafetyTipHistogramName(
          "Security.SafetyTips.PageInfo.Action", safety_tip_info_.status),
      action, PAGE_INFO_COUNT);

  base::UmaHistogramEnumeration(security_state::GetLegacyTLSHistogramName(
                                    "Security.LegacyTLS.PageInfo.Action",
                                    visible_security_state_for_metrics_),
                                action, PAGE_INFO_COUNT);

  std::string histogram_name;
  if (site_url_.SchemeIsCryptographic()) {
    if (security_level_ == security_state::SECURE) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.ValidNonEV",
                                action, PAGE_INFO_COUNT);
    } else if (security_level_ == security_state::EV_SECURE) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.ValidEV",
                                action, PAGE_INFO_COUNT);
    } else if (security_level_ == security_state::NONE) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.Downgraded",
                                action, PAGE_INFO_COUNT);
    } else if (security_level_ == security_state::DANGEROUS) {
      UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpsUrl.Dangerous",
                                action, PAGE_INFO_COUNT);
    }
    return;
  }

  if (security_level_ == security_state::WARNING) {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Warning",
                              action, PAGE_INFO_COUNT);
  } else if (security_level_ == security_state::DANGEROUS) {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Dangerous",
                              action, PAGE_INFO_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Security.PageInfo.Action.HttpUrl.Neutral",
                              action, PAGE_INFO_COUNT);
  }
}

void PageInfo::OnSitePermissionChanged(ContentSettingsType type,
                                       ContentSetting setting) {
  tab_specific_content_settings_->ContentSettingChangedViaPageInfo(type);

  // Count how often a permission for a specific content type is changed using
  // the Page Info UI.
  size_t num_values;
  int histogram_value = ContentSettingTypeToHistogramValue(type, &num_values);
  UMA_HISTOGRAM_EXACT_LINEAR("WebsiteSettings.OriginInfo.PermissionChanged",
                             histogram_value, num_values);

  if (setting == ContentSetting::CONTENT_SETTING_ALLOW) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "WebsiteSettings.OriginInfo.PermissionChanged.Allowed", histogram_value,
        num_values);
  } else if (setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "WebsiteSettings.OriginInfo.PermissionChanged.Blocked", histogram_value,
        num_values);
  }

  // This is technically redundant given the histogram above, but putting the
  // total count of permission changes in another histogram makes it easier to
  // compare it against other kinds of actions in Page Info.
  RecordPageInfoAction(PAGE_INFO_CHANGED_PERMISSION);
  if (type == ContentSettingsType::SOUND) {
    ContentSetting default_setting =
        content_settings_->GetDefaultContentSetting(ContentSettingsType::SOUND,
                                                    nullptr);
    bool mute = (setting == CONTENT_SETTING_BLOCK) ||
                (setting == CONTENT_SETTING_DEFAULT &&
                 default_setting == CONTENT_SETTING_BLOCK);
    if (mute) {
      base::RecordAction(
          base::UserMetricsAction("SoundContentSetting.MuteBy.PageInfo"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("SoundContentSetting.UnmuteBy.PageInfo"));
    }
  }

  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile_, site_url_, site_url_, type, PermissionSourceUI::OIB);

  // The permission may have been blocked due to being under embargo, so if it
  // was changed away from BLOCK, clear embargo status if it exists.
  if (setting != CONTENT_SETTING_BLOCK) {
    PermissionDecisionAutoBlocker::GetForProfile(profile_)->RemoveEmbargoByUrl(
        site_url_, type);
  }
  content_settings_->SetNarrowestContentSetting(site_url_, site_url_, type,
                                                setting);

  // When the sound setting is changed, no reload is necessary.
  if (type != ContentSettingsType::SOUND)
    show_info_bar_ = true;

  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
}

void PageInfo::OnSiteChosenObjectDeleted(const ChooserUIInfo& ui_info,
                                         const base::Value& object) {
  // TODO(reillyg): Create metrics for revocations. crbug.com/556845
  ChooserContextBase* context = ui_info.get_context(profile_);
  const auto origin = url::Origin::Create(site_url_);
  context->RevokeObjectPermission(origin, origin, object);
  show_info_bar_ = true;

  // Refresh the UI to reflect the changed settings.
  PresentSitePermissions();
}

void PageInfo::OnUIClosing(bool* reload_prompt) {
  if (reload_prompt)
    *reload_prompt = false;
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  if (show_info_bar_ && web_contents() && !web_contents()->IsBeingDestroyed()) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
    if (infobar_service) {
      PageInfoInfoBarDelegate::Create(infobar_service);
      if (reload_prompt)
        *reload_prompt = true;
    }
  }
#endif
}

void PageInfo::OnRevokeSSLErrorBypassButtonPressed() {
  DCHECK(chrome_ssl_host_state_delegate_);
  chrome_ssl_host_state_delegate_->RevokeUserAllowExceptionsHard(
      site_url().host());
  did_revoke_user_ssl_decisions_ = true;
}

void PageInfo::OpenSiteSettingsView() {
#if defined(OS_ANDROID)
  NOTREACHED();
#else
  chrome::ShowSiteSettings(chrome::FindBrowserWithWebContents(web_contents()),
                           site_url());
  RecordPageInfoAction(PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED);
#endif
}

void PageInfo::OnChangePasswordButtonPressed(
    content::WebContents* web_contents) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  DCHECK(password_protection_service_);
  DCHECK(safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE ||
         safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE ||
         safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE);

  password_protection_service_->OnUserAction(
      web_contents,
      password_protection_service_
          ->reused_password_account_type_for_last_shown_warning(),
      RequestOutcome::UNKNOWN,
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      /*verdict_token=*/"", safe_browsing::WarningUIType::PAGE_INFO,
      safe_browsing::WarningAction::CHANGE_PASSWORD);
#endif
}

void PageInfo::OnWhitelistPasswordReuseButtonPressed(
    content::WebContents* web_contents) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  DCHECK(password_protection_service_);
  DCHECK(safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE ||
         safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE ||
         safe_browsing_status_ ==
             SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE);

  password_protection_service_->OnUserAction(
      web_contents,
      password_protection_service_
          ->reused_password_account_type_for_last_shown_warning(),
      RequestOutcome::UNKNOWN,
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      /*verdict_token=*/"", safe_browsing::WarningUIType::PAGE_INFO,
      safe_browsing::WarningAction::MARK_AS_LEGITIMATE);
#endif
}

void PageInfo::ComputeUIInputs(
    const GURL& url,
    security_state::SecurityLevel security_level,
    const security_state::VisibleSecurityState& visible_security_state) {
#if !defined(OS_ANDROID)
  // On desktop, internal URLs aren't handled by this class. Instead, a
  // custom and simpler bubble is shown.
  DCHECK(!url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
         !url.SchemeIs(content::kViewSourceScheme) &&
         !url.SchemeIs(content_settings::kExtensionScheme));
#endif

  bool is_chrome_ui_native_scheme = false;
#if defined(OS_ANDROID)
  is_chrome_ui_native_scheme = url.SchemeIs(chrome::kChromeUINativeScheme);
#endif

  security_level_ = security_level;

  if (url.SchemeIs(url::kAboutScheme)) {
    // All about: URLs except about:blank are redirected.
    DCHECK_EQ(url::kAboutBlankURL, url.spec());
    site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
#if defined(OS_ANDROID)
    identity_status_description_android_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY);
#endif
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;
    site_connection_details_ = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        UTF8ToUTF16(url.spec()));
    return;
  }

  if (url.SchemeIs(content::kChromeUIScheme) || is_chrome_ui_native_scheme) {
    site_identity_status_ = SITE_IDENTITY_STATUS_INTERNAL_PAGE;
#if defined(OS_ANDROID)
    identity_status_description_android_ =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
#endif
    site_connection_status_ = SITE_CONNECTION_STATUS_INTERNAL_PAGE;
    return;
  }

  // Identity section.
  certificate_ = visible_security_state.certificate;

  if (certificate_ &&
      (!net::IsCertStatusError(visible_security_state.cert_status))) {
    // HTTPS with no or minor errors.
    if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT) {
#if defined(OS_CHROMEOS)
      site_identity_status_ = SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
#else
      DCHECK(false) << "Policy certificates exist only on ChromeOS";
#endif
    } else {
      // No major or minor errors.
      if (visible_security_state.cert_status & net::CERT_STATUS_IS_EV) {
        // EV HTTPS page.
        site_identity_status_ = SITE_IDENTITY_STATUS_EV_CERT;
      } else {
        // Non-EV OK HTTPS page.
        site_identity_status_ = SITE_IDENTITY_STATUS_CERT;
        base::string16 issuer_name(
            UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
        if (issuer_name.empty()) {
          issuer_name.assign(l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
        }

#if defined(OS_ANDROID)
        // This string is shown on all non-error HTTPS sites on Android when
        // the user taps "Details" link on page info.
        identity_status_description_android_.assign(l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURE_IDENTITY_VERIFIED, issuer_name));
#endif
      }
      if (security_state::IsSHA1InChain(visible_security_state)) {
        site_identity_status_ =
            SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM;

#if defined(OS_ANDROID)
        identity_status_description_android_ +=
            UTF8ToUTF16("\n\n") +
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_DEPRECATED_SIGNATURE_ALGORITHM);
#endif
      }
    }
  } else {
    // HTTP or HTTPS with errors (not warnings).
    if (!security_state::IsSchemeCryptographic(visible_security_state.url) ||
        !visible_security_state.certificate) {
      site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
    } else {
      site_identity_status_ = SITE_IDENTITY_STATUS_ERROR;
    }
#if defined(OS_ANDROID)
    const base::string16 bullet = UTF8ToUTF16("\n • ");
    std::vector<ssl_errors::ErrorInfo> errors;
    ssl_errors::ErrorInfo::GetErrorsForCertStatus(
        certificate_, visible_security_state.cert_status, url, &errors);

    identity_status_description_android_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    for (const ssl_errors::ErrorInfo& error : errors) {
      identity_status_description_android_ += bullet;
      identity_status_description_android_ += error.short_description();
    }

    if (visible_security_state.cert_status & net::CERT_STATUS_NON_UNIQUE_NAME) {
      identity_status_description_android_ += ASCIIToUTF16("\n\n");
      identity_status_description_android_ +=
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_NON_UNIQUE_NAME);
    }
#endif
  }

  if (visible_security_state.malicious_content_status !=
      security_state::MALICIOUS_CONTENT_STATUS_NONE) {
    // The site has been flagged by Safe Browsing. Takes precedence over TLS.
    base::string16 safe_browsing_details;
    GetSafeBrowsingStatusByMaliciousContentStatus(
        visible_security_state.malicious_content_status, &safe_browsing_status_,
        &safe_browsing_details);
#if defined(OS_ANDROID)
    identity_status_description_android_ = safe_browsing_details;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
    bool old_show_change_pw_buttons = show_change_password_buttons_;
#endif
    show_change_password_buttons_ =
        (visible_security_state.malicious_content_status ==
             security_state::
                 MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE ||
         visible_security_state.malicious_content_status ==
             security_state::
                 MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE ||
         visible_security_state.malicious_content_status ==
             security_state::
                 MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE);
#if BUILDFLAG(FULL_SAFE_BROWSING)
    // Only record password reuse when adding the button, not on updates.
    if (show_change_password_buttons_ && !old_show_change_pw_buttons) {
      RecordPasswordReuseEvent();
    }
#endif
  }

  safety_tip_info_ = visible_security_state.safety_tip_info;
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(security_state::features::kSafetyTipUI)) {
    // identity_status_description_android_ is only displayed on Android when
    // the user taps "Details" link on the page info. Reuse the description from
    // page info UI.
    std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
        PageInfoUI::CreateSafetyTipSecurityDescription(safety_tip_info_);
    if (security_description) {
      identity_status_description_android_ = security_description->details;
    }
  }
#endif

  // Site Connection
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  site_connection_status_ = SITE_CONNECTION_STATUS_UNKNOWN;

  base::string16 subject_name(GetSimpleSiteName(url));
  if (subject_name.empty()) {
    subject_name.assign(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
  }

  if (!visible_security_state.certificate ||
      !security_state::IsSchemeCryptographic(visible_security_state.url)) {
    // Page is still loading (so SSL status is not yet available) or
    // loaded over HTTP or loaded over HTTPS with no cert.
    site_connection_status_ = SITE_CONNECTION_STATUS_UNENCRYPTED;

    site_connection_details_.assign(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
        subject_name));
  } else if (!visible_security_state.connection_info_initialized) {
    DCHECK_NE(security_level, security_state::NONE);
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED_ERROR;
  } else {
    site_connection_status_ = SITE_CONNECTION_STATUS_ENCRYPTED;

    if (net::ObsoleteSSLStatus(
            visible_security_state.connection_status,
            visible_security_state.peer_signature_algorithm) ==
        net::OBSOLETE_SSL_NONE) {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT, subject_name));
    } else {
      site_connection_details_.assign(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_WEAK_ENCRYPTION_CONNECTION_TEXT,
          subject_name));
    }

    if (base::FeatureList::IsEnabled(
            security_state::features::kLegacyTLSWarnings) &&
        visible_security_state.connection_used_legacy_tls &&
        !visible_security_state.should_suppress_legacy_tls_warning) {
      site_connection_status_ = SITE_CONNECTION_STATUS_LEGACY_TLS;
    }

    ReportAnyInsecureContent(visible_security_state, &site_connection_status_,
                             &site_connection_details_);
  }

  uint16_t cipher_suite = net::SSLConnectionStatusToCipherSuite(
      visible_security_state.connection_status);
  if (visible_security_state.connection_info_initialized && cipher_suite) {
    int ssl_version = net::SSLConnectionStatusToVersion(
        visible_security_state.connection_status);
    const char* ssl_version_str;
    net::SSLVersionToString(&ssl_version_str, ssl_version);
    site_connection_details_ += ASCIIToUTF16("\n\n");
    site_connection_details_ += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION, ASCIIToUTF16(ssl_version_str));

    const char *key_exchange, *cipher, *mac;
    bool is_aead, is_tls13;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);

    site_connection_details_ += ASCIIToUTF16("\n\n");
    if (is_aead) {
      if (is_tls13) {
        // For TLS 1.3 ciphers, report the group (historically, curve) as the
        // key exchange.
        key_exchange =
            SSL_get_curve_name(visible_security_state.key_exchange_group);
        if (!key_exchange) {
          NOTREACHED();
          key_exchange = "";
        }
      }
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS_AEAD,
          ASCIIToUTF16(cipher), ASCIIToUTF16(key_exchange));
    } else {
      site_connection_details_ += l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTION_DETAILS, ASCIIToUTF16(cipher),
          ASCIIToUTF16(mac), ASCIIToUTF16(key_exchange));
    }
  }

  // Check if a user decision has been made to allow or deny certificates with
  // errors on this site.
  ChromeSSLHostStateDelegate* delegate =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile_);
  DCHECK(delegate);
  // Only show an SSL decision revoke button if the user has chosen to bypass
  // SSL host errors for this host in the past, and we're not presently on a
  // Safe Browsing error (since otherwise it's confusing which warning you're
  // re-enabling).
  show_ssl_decision_revoke_button_ =
      delegate->HasAllowException(url.host()) &&
      visible_security_state.malicious_content_status ==
          security_state::MALICIOUS_CONTENT_STATUS_NONE;
}

void PageInfo::PresentSitePermissions() {
  PermissionInfoList permission_info_list;
  ChosenObjectInfoList chosen_object_info_list;

  PageInfoUI::PermissionInfo permission_info;
  for (size_t i = 0; i < base::size(kPermissionType); ++i) {
    permission_info.type = kPermissionType[i];

    content_settings::SettingInfo info;
    std::unique_ptr<base::Value> value = content_settings_->GetWebsiteSetting(
        site_url_, site_url_, permission_info.type, std::string(), &info);
    DCHECK(value.get());
    if (value->type() == base::Value::Type::INTEGER) {
      permission_info.setting =
          content_settings::ValueToContentSetting(value.get());
    } else {
      NOTREACHED();
    }

    permission_info.source = info.source;
    permission_info.is_incognito = profile_->IsOffTheRecord();

    if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      permission_info.default_setting = permission_info.setting;
      permission_info.setting = CONTENT_SETTING_DEFAULT;
    } else {
      permission_info.default_setting =
          content_settings_->GetDefaultContentSetting(permission_info.type,
                                                      NULL);
    }

    // For permissions that are still prompting the user and haven't been
    // explicitly set by another source, check its embargo status.
    if (PermissionUtil::IsPermission(permission_info.type) &&
        permission_info.setting == CONTENT_SETTING_DEFAULT &&
        permission_info.source ==
            content_settings::SettingSource::SETTING_SOURCE_USER) {
      // TODO(raymes): Use GetPermissionStatus() to retrieve information
      // about *all* permissions once it has default behaviour implemented for
      // ContentSettingTypes that aren't permissions.
      PermissionResult permission_result =
          PermissionManager::Get(profile_)->GetPermissionStatus(
              permission_info.type, site_url_, site_url_);

      // If under embargo, update |permission_info| to reflect that.
      if (permission_result.content_setting == CONTENT_SETTING_BLOCK &&
          permission_result.source ==
              PermissionStatusSource::MULTIPLE_DISMISSALS) {
        permission_info.setting = permission_result.content_setting;
      }
    }

    if (ShouldShowPermission(permission_info, site_url_, content_settings_,
                             web_contents(), tab_specific_content_settings_)) {
      permission_info_list.push_back(permission_info);
    }
  }

  const auto origin = url::Origin::Create(site_url_);
  for (const ChooserUIInfo& ui_info : kChooserUIInfo) {
    ChooserContextBase* context = ui_info.get_context(profile_);
    auto chosen_objects = context->GetGrantedObjects(origin, origin);
    for (std::unique_ptr<ChooserContextBase::Object>& object : chosen_objects) {
      chosen_object_info_list.push_back(
          std::make_unique<PageInfoUI::ChosenObjectInfo>(ui_info,
                                                         std::move(object)));
    }
  }

  ui_->SetPermissionInfo(permission_info_list,
                         std::move(chosen_object_info_list));
}

void PageInfo::PresentSiteData() {
  CookieInfoList cookie_info_list;
  const LocalSharedObjectsContainer& allowed_objects =
      tab_specific_content_settings_->allowed_local_shared_objects();
  const LocalSharedObjectsContainer& blocked_objects =
      tab_specific_content_settings_->blocked_local_shared_objects();

  // Add first party cookie and site data counts.
  PageInfoUI::CookieInfo cookie_info;
  cookie_info.allowed = allowed_objects.GetObjectCountForDomain(site_url_);
  cookie_info.blocked = blocked_objects.GetObjectCountForDomain(site_url_);
  cookie_info.is_first_party = true;
  cookie_info_list.push_back(cookie_info);

  // Add third party cookie counts.
  cookie_info.allowed = allowed_objects.GetObjectCount() - cookie_info.allowed;
  cookie_info.blocked = blocked_objects.GetObjectCount() - cookie_info.blocked;
  cookie_info.is_first_party = false;
  cookie_info_list.push_back(cookie_info);

  ui_->SetCookieInfo(cookie_info_list);
}

void PageInfo::PresentSiteIdentity() {
  // After initialization the status about the site's connection and its
  // identity must be available.
  DCHECK_NE(site_identity_status_, SITE_IDENTITY_STATUS_UNKNOWN);
  DCHECK_NE(site_connection_status_, SITE_CONNECTION_STATUS_UNKNOWN);
  PageInfoUI::IdentityInfo info;
  info.site_identity = UTF16ToUTF8(GetSimpleSiteName(site_url_));

  info.connection_status = site_connection_status_;
  info.connection_status_description = UTF16ToUTF8(site_connection_details_);
  info.identity_status = site_identity_status_;
  info.safe_browsing_status = safe_browsing_status_;
  if (base::FeatureList::IsEnabled(security_state::features::kSafetyTipUI)) {
    info.safety_tip_info = safety_tip_info_;
  }
#if defined(OS_ANDROID)
  info.identity_status_description_android =
      UTF16ToUTF8(identity_status_description_android_);
#endif

  info.certificate = certificate_;
  info.show_ssl_decision_revoke_button = show_ssl_decision_revoke_button_;
  info.show_change_password_buttons = show_change_password_buttons_;
  ui_->SetIdentityInfo(info);
}

void PageInfo::PresentPageFeatureInfo() {
  PageInfoUI::PageFeatureInfo info;
  info.is_vr_presentation_in_headset =
      vr::VrTabHelper::IsContentDisplayedInHeadset(web_contents());

  ui_->SetPageFeatureInfo(info);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void PageInfo::RecordPasswordReuseEvent() {
  if (!password_protection_service_) {
    return;
  }
  safe_browsing::LogWarningAction(
      safe_browsing::WarningUIType::PAGE_INFO,
      safe_browsing::WarningAction::SHOWN,
      password_protection_service_
          ->reused_password_account_type_for_last_shown_warning());
}
#endif

std::vector<ContentSettingsType> PageInfo::GetAllPermissionsForTesting() {
  std::vector<ContentSettingsType> permission_list;
  for (size_t i = 0; i < base::size(kPermissionType); ++i) {
#if !defined(OS_ANDROID)
    if (kPermissionType[i] == ContentSettingsType::AUTOPLAY)
      continue;
#endif
    permission_list.push_back(kPermissionType[i]);
  }
  return permission_list;
}

void PageInfo::GetSafeBrowsingStatusByMaliciousContentStatus(
    security_state::MaliciousContentStatus malicious_content_status,
    PageInfo::SafeBrowsingStatus* status,
    base::string16* details) {
  switch (malicious_content_status) {
    case security_state::MALICIOUS_CONTENT_STATUS_NONE:
      NOTREACHED();
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_MALWARE:
      *status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
      *details = l10n_util::GetStringUTF16(IDS_PAGE_INFO_MALWARE_DETAILS);
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING:
      *status = PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING;
      *details =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SOCIAL_ENGINEERING_DETAILS);
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE:
      *status = PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE;
      *details =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_UNWANTED_SOFTWARE_DETAILS);
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status = PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE;
      // |password_protection_service_| may be null in test.
      *details =
          password_protection_service_
              ? password_protection_service_->GetWarningDetailText(
                    password_protection_service_
                        ->reused_password_account_type_for_last_shown_warning())
              : base::string16();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status = PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
      // |password_protection_service_| may be null in test.
      *details =
          password_protection_service_
              ? password_protection_service_->GetWarningDetailText(
                    password_protection_service_
                        ->reused_password_account_type_for_last_shown_warning())
              : base::string16();
#endif
      break;
    case security_state::
        MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
      // |password_protection_service_| may be null in test.
      *details =
          password_protection_service_
              ? password_protection_service_->GetWarningDetailText(
                    password_protection_service_
                        ->reused_password_account_type_for_last_shown_warning())
              : base::string16();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status = PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE;
      // |password_protection_service_| maybe null in test.
      *details =
          password_protection_service_
              ? password_protection_service_->GetWarningDetailText(
                    password_protection_service_
                        ->reused_password_account_type_for_last_shown_warning())
              : base::string16();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_BILLING:
      *status = PageInfo::SAFE_BROWSING_STATUS_BILLING;
      *details = l10n_util::GetStringUTF16(IDS_PAGE_INFO_BILLING_DETAILS);
      break;
  }
}

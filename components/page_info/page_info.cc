// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/page_info/page_info_delegate.h"
#include "components/page_info/page_info_ui.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/origin_keyed_permission_action_service.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "net/base/schemeful_site.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/util/android/url_constants.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_branded_strings.h"
#else
#include "third_party/blink/public/common/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

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
    ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::SENSORS,
    ContentSettingsType::NOTIFICATIONS,
    ContentSettingsType::JAVASCRIPT,
#if !BUILDFLAG(IS_ANDROID)
    ContentSettingsType::IMAGES,
#endif
    ContentSettingsType::POPUPS,
    ContentSettingsType::WINDOW_MANAGEMENT,
    ContentSettingsType::ADS,
    ContentSettingsType::BACKGROUND_SYNC,
    ContentSettingsType::SOUND,
    ContentSettingsType::AUTOMATIC_DOWNLOADS,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
#endif
    ContentSettingsType::MIDI_SYSEX,
    ContentSettingsType::CLIPBOARD_READ_WRITE,
#if BUILDFLAG(IS_ANDROID)
    ContentSettingsType::NFC,
#endif
    ContentSettingsType::USB_GUARD,
#if !BUILDFLAG(IS_ANDROID)
    ContentSettingsType::HID_GUARD,
    ContentSettingsType::SERIAL_GUARD,
    ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
    ContentSettingsType::LOCAL_FONTS,
#endif
    ContentSettingsType::BLUETOOTH_GUARD,
    ContentSettingsType::BLUETOOTH_SCANNING,
    ContentSettingsType::HAND_TRACKING,
    ContentSettingsType::VR,
    ContentSettingsType::AR,
    ContentSettingsType::IDLE_DETECTION,
    ContentSettingsType::FEDERATED_IDENTITY_API,
#if !BUILDFLAG(IS_ANDROID)
    ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
    ContentSettingsType::CAPTURED_SURFACE_CONTROL,
#endif  // !BUILDFLAG(IS_ANDROID)
    ContentSettingsType::AUTOMATIC_FULLSCREEN,
#if !BUILDFLAG(IS_ANDROID)
    ContentSettingsType::KEYBOARD_LOCK,
    ContentSettingsType::POINTER_LOCK,
    ContentSettingsType::WEB_APP_INSTALLATION,
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS)
    ContentSettingsType::WEB_PRINTING,
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// The list of setting types which request permission for a pair of requesting
// origin and top-level-origin that need to display entries in the Website
// Settings UI. THE ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it,
// email security-dev@chromium.org.
ContentSettingsType kTwoPatternPermissions[] = {
    ContentSettingsType::STORAGE_ACCESS,
};

// If the |visible_security_state| indicates that mixed content or certificate
// errors were present, update |connection_status| and |connection_details|.
void ReportAnyInsecureContent(
    const security_state::VisibleSecurityState& visible_security_state,
    PageInfo::SiteConnectionStatus* connection_status,
    std::u16string* connection_details) {
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

// The list of chooser types that need to display entries in the Website
// Settings UI. THE ORDER OF THESE ITEMS IS IMPORTANT. To propose changing it,
// email security-dev@chromium.org.
const PageInfo::ChooserUIInfo kChooserUIInfo[] = {
    {ContentSettingsType::USB_CHOOSER_DATA,
     IDS_PAGE_INFO_USB_DEVICE_SECONDARY_LABEL,
     IDS_PAGE_INFO_USB_DEVICE_ALLOWED_BY_POLICY_LABEL,
     IDS_PAGE_INFO_DELETE_USB_DEVICE_WITH_NAME},
#if !BUILDFLAG(IS_ANDROID)
    {ContentSettingsType::HID_CHOOSER_DATA,
     IDS_PAGE_INFO_HID_DEVICE_SECONDARY_LABEL,
     IDS_PAGE_INFO_HID_DEVICE_ALLOWED_BY_POLICY_LABEL,
     IDS_PAGE_INFO_DELETE_HID_DEVICE_WITH_NAME},
    {ContentSettingsType::SERIAL_CHOOSER_DATA,
     IDS_PAGE_INFO_SERIAL_PORT_SECONDARY_LABEL,
     IDS_PAGE_INFO_SERIAL_PORT_ALLOWED_BY_POLICY_LABEL,
     IDS_PAGE_INFO_DELETE_SERIAL_PORT_WITH_NAME},
#endif
    {ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
     IDS_PAGE_INFO_BLUETOOTH_DEVICE_SECONDARY_LABEL,
     /*allowed_by_policy_description_string_id=*/-1,
     IDS_PAGE_INFO_DELETE_BLUETOOTH_DEVICE_WITH_NAME},
};

void LogTimeOpenHistogram(const std::string& name, base::TimeTicks start_time) {
  base::UmaHistogramCustomTimes(name, base::TimeTicks::Now() - start_time,
                                base::Milliseconds(1), base::Hours(1), 100);
}

// Time open histogram prefixes.
const char kPageInfoTimePrefix[] = "Security.PageInfo.TimeOpen";
const char kPageInfoTimeActionPrefix[] = "Security.PageInfo.TimeOpen.Action";
const char kPageInfoTimeNoActionPrefix[] =
    "Security.PageInfo.TimeOpen.NoAction";
const base::TimeDelta kRecordPageInfoPermissionChangeWindow = base::Minutes(1);
}  // namespace

using PermissionInfo = PageInfo::PermissionInfo;
PermissionInfo::PermissionInfo() = default;
PermissionInfo::PermissionInfo(const PermissionInfo& other) = default;
PermissionInfo& PermissionInfo::operator=(const PermissionInfo& other) =
    default;
PermissionInfo::~PermissionInfo() = default;

PageInfo::PageInfo(std::unique_ptr<PageInfoDelegate> delegate,
                   content::WebContents* web_contents,
                   const GURL& url)
    : web_contents_(web_contents->GetWeakPtr()),
      delegate_(std::move(delegate)),
      show_info_bar_(false),
      site_url_(url),
      site_identity_status_(SITE_IDENTITY_STATUS_UNKNOWN),
      safe_browsing_status_(SAFE_BROWSING_STATUS_NONE),
      safety_tip_info_({security_state::SafetyTipStatus::kUnknown, GURL()}),
      site_connection_status_(SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button_(false),
      did_revoke_user_ssl_decisions_(false),
      show_change_password_buttons_(false),
      did_perform_action_(false) {
  DCHECK(delegate_);
  security_level_ = delegate_->GetSecurityLevel();
  visible_security_state_for_metrics_ = delegate_->GetVisibleSecurityState();

  // TabSpecificContentSetting needs to be created before page load.
  ComputeUIInputs(site_url_);

  // Every time this is created, page info dialog is opened.
  // So this counts how often the page Info dialog is opened.
  RecordPageInfoAction(page_info::PAGE_INFO_OPENED);

  // Record the time when the page info dialog is opened so the total time it is
  // open can be measured.
  start_time_ = base::TimeTicks::Now();

#if !BUILDFLAG(IS_ANDROID)
  if (web_contents) {
    controller_ = delegate_->CreateCookieControlsController();
    observation_.Observe(controller_.get());

    // TODO(crbug.com/40901748): SetCookieInfo is called twice, once from here
    // and once from InitializeUiState. This should be cleaned up.
    controller_->Update(web_contents);

    auto* pscs = GetPageSpecificContentSettings();
    if (pscs) {
      pscs->AddPermissionUsageObserver(this);
    }
  }
#endif
}

PageInfo::~PageInfo() {
  // Check if Re-enable warnings button was visible, if so, log on UMA whether
  // it was clicked or not.
  SSLCertificateDecisionsDidRevoke user_decision =
      did_revoke_user_ssl_decisions_ ? USER_CERT_DECISIONS_REVOKED
                                     : USER_CERT_DECISIONS_NOT_REVOKED;
  if (show_ssl_decision_revoke_button_) {
    base::UmaHistogramEnumeration(
        "interstitial.ssl.did_user_revoke_decisions2", user_decision,
        END_OF_SSL_CERTIFICATE_DECISIONS_DID_REVOKE_ENUM);
  }

  // Record the total time the Page Info UI was open for all opens as well as
  // split between whether any action was taken.
  LogTimeOpenHistogram(security_state::GetSecurityLevelHistogramName(
                           kPageInfoTimePrefix, security_level_),
                       start_time_);
  LogTimeOpenHistogram(security_state::GetSafetyTipHistogramName(
                           kPageInfoTimePrefix, safety_tip_info_.status),
                       start_time_);
  if (did_perform_action_) {
    LogTimeOpenHistogram(security_state::GetSecurityLevelHistogramName(
                             kPageInfoTimeActionPrefix, security_level_),
                         start_time_);
    LogTimeOpenHistogram(
        security_state::GetSafetyTipHistogramName(kPageInfoTimeActionPrefix,
                                                  safety_tip_info_.status),
        start_time_);
  } else {
    LogTimeOpenHistogram(security_state::GetSecurityLevelHistogramName(
                             kPageInfoTimeNoActionPrefix, security_level_),
                         start_time_);
    LogTimeOpenHistogram(
        security_state::GetSafetyTipHistogramName(kPageInfoTimeNoActionPrefix,
                                                  safety_tip_info_.status),
        start_time_);
  }

  base::RecordAction(base::UserMetricsAction("PageInfo.Closed"));

#if !BUILDFLAG(IS_ANDROID)
  if (web_contents_) {
    auto* pscs = GetPageSpecificContentSettings();
    if (pscs) {
      pscs->RemovePermissionUsageObserver(this);
    }
  }
#endif
}

void PageInfo::OnStatusChanged(
    bool controls_visible,
    bool protections_on,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration,
    std::vector<content_settings::TrackingProtectionFeature> features) {
  if (controls_visible_ != controls_visible ||
      protections_on_ != protections_on || enforcement != enforcement_ ||
      blocking_status != blocking_status_ ||
      expiration != cookie_exception_expiration_ || features_ != features) {
    controls_visible_ = controls_visible;
    protections_on_ = protections_on;
    enforcement_ = enforcement;
    blocking_status_ = blocking_status;
    features_ = features;
    cookie_exception_expiration_ = expiration;
    PresentSiteData(base::DoNothing());
  }
}

void PageInfo::OnThirdPartyToggleClicked(bool block_third_party_cookies) {
  DCHECK(controls_visible_);
  RecordPageInfoAction(block_third_party_cookies
                           ? page_info::PAGE_INFO_COOKIES_BLOCKED_FOR_SITE
                           : page_info::PAGE_INFO_COOKIES_ALLOWED_FOR_SITE);
  controller_->OnCookieBlockingEnabledForSite(block_third_party_cookies);
  show_info_bar_ = true;
}

// static
bool PageInfo::IsPermissionFactoryDefault(const PermissionInfo& info,
                                          bool is_incognito) {
  const ContentSetting factory_default_setting =
      content_settings::ContentSettingsRegistry::GetInstance()
          ->Get(info.type)
          ->GetInitialDefaultSetting();

  // Settings that are granted in regular mode get reduced to ASK in incognito
  // mode. These settings should not be displayed either.
  const bool is_incognito_default =
      is_incognito && info.setting == CONTENT_SETTING_ASK &&
      factory_default_setting == CONTENT_SETTING_ASK;

  return info.source == content_settings::SettingSource::kUser &&
         factory_default_setting == info.default_setting &&
         (info.setting == CONTENT_SETTING_DEFAULT || is_incognito_default);
}

// static
bool PageInfo::IsFileOrInternalPage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeDevToolsScheme) ||
         url.SchemeIs(content::kViewSourceScheme) ||
         url.SchemeIs(url::kFileScheme);
}

void PageInfo::InitializeUiState(PageInfoUI* ui, base::OnceClosure done) {
  ui_ = ui;
  DCHECK(ui_);

  PresentSitePermissions();
  PresentSiteIdentity();
  PresentPageFeatureInfo();
  PresentSiteData(std::move(done));
  PresentAdPersonalizationData();
}

void PageInfo::UpdateSecurityState() {
  ComputeUIInputs(site_url_);
  PresentSiteIdentity();
}

void PageInfo::RecordPageInfoAction(page_info::PageInfoAction action) {
  if (action != page_info::PAGE_INFO_OPENED) {
    did_perform_action_ = true;
  }

#if !BUILDFLAG(IS_ANDROID)
  delegate_->OnPageInfoActionOccurred(action);
#endif

  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                action);

  if (web_contents_) {
    ukm::builders::PageInfoBubble(
        web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId())
        .SetActionTaken(action)
        .Record(ukm::UkmRecorder::Get());
  }

  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return;
  }

  bool has_topic = settings->HasAccessedTopics();
  bool has_fledge = settings->HasJoinedUserToInterestGroup();
  switch (action) {
    case page_info::PAGE_INFO_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.Opened"));
      base::UmaHistogramBoolean("Security.PageInfo.AdPersonalizationRowShown",
                                has_fledge || has_topic);
      break;
    case page_info::PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED:
      if (has_fledge && has_topic) {
        base::RecordAction(base::UserMetricsAction(
            "PageInfo.AdPersonalization.OpenedWithFledgeAndTopics"));
      } else if (has_fledge) {
        base::RecordAction(base::UserMetricsAction(
            "PageInfo.AdPersonalization.OpenedWithFledge"));
      } else if (has_topic) {
        base::RecordAction(base::UserMetricsAction(
            "PageInfo.AdPersonalization.OpenedWithTopics"));
      }
      break;
    case page_info::PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED:
      base::RecordAction(base::UserMetricsAction(
          "PageInfo.AdPersonalization.ManageInterestClicked"));
      break;
    case page_info::PAGE_INFO_CERTIFICATE_DIALOG_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Security.Certificate.Opened"));
      break;
    case page_info::PAGE_INFO_CONNECTION_HELP_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Security.ConnectionHelp.Opened"));
      break;
    case page_info::PAGE_INFO_SECURITY_DETAILS_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.Security.Opened"));
      break;
    case page_info::PAGE_INFO_SITE_SETTINGS_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.SiteSettings.Opened"));
      break;
    case page_info::PAGE_INFO_COOKIES_DIALOG_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.Cookies.Opened"));
      break;
    case page_info::PAGE_INFO_COOKIES_ALLOWED_FOR_SITE:
      base::RecordAction(base::UserMetricsAction("PageInfo.Cookies.Allowed"));
      break;
    case page_info::PAGE_INFO_COOKIES_BLOCKED_FOR_SITE:
      base::RecordAction(base::UserMetricsAction("PageInfo.Cookies.Blocked"));
      break;
    case page_info::PAGE_INFO_COOKIES_CLEARED:
      base::RecordAction(base::UserMetricsAction("PageInfo.Cookies.Cleared"));
      break;
    case page_info::PAGE_INFO_PERMISSION_DIALOG_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.Permission.Opened"));
      break;
    case page_info::PAGE_INFO_CHANGED_PERMISSION:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Permission.Changed"));
      break;
    case page_info::PAGE_INFO_PERMISSIONS_CLEARED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Permission.Cleared"));
      break;
    case page_info::PAGE_INFO_CHOOSER_OBJECT_DELETED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Permission.ChooserObjectDeleted"));
      break;
    case page_info::PAGE_INFO_RESET_DECISIONS_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.Permission.ResetDecisions"));
      break;
    case page_info::PAGE_INFO_FORGET_SITE_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.ForgetSite.Opened"));
      break;
    case page_info::PAGE_INFO_FORGET_SITE_CLEARED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.ForgetSite.Cleared"));
      break;
    case page_info::PAGE_INFO_HISTORY_OPENED:
      base::RecordAction(base::UserMetricsAction("PageInfo.History.Opened"));
      break;
    case page_info::PAGE_INFO_HISTORY_ENTRY_REMOVED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.History.EntryRemoved"));
      break;
    case page_info::PAGE_INFO_HISTORY_ENTRY_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.History.EntryClicked"));
      break;
    case page_info::PAGE_INFO_PASSWORD_REUSE_ALLOWED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.PasswordReuseAllowed"));
      break;
    case page_info::PAGE_INFO_CHANGE_PASSWORD_PRESSED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.ChangePasswordPressed"));
      break;
    case page_info::PAGE_INFO_SAFETY_TIP_HELP_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.SafetyTip.HelpOpened"));
      break;
    case page_info::PAGE_INFO_STORE_INFO_CLICKED:
      base::RecordAction(base::UserMetricsAction("PageInfo.StoreInfo.Opened"));
      break;
    case page_info::PAGE_INFO_ABOUT_THIS_SITE_PAGE_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.AboutThisSite.Opened"));
      break;
    case page_info::PAGE_INFO_ABOUT_THIS_SITE_SOURCE_LINK_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.AboutThisSite.SourceLinkClicked"));
      break;
    case page_info::PAGE_INFO_ABOUT_THIS_SITE_MORE_ABOUT_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.AboutThisSite.MoreAboutClicked"));
      break;
    case page_info::PAGE_INFO_COOKIES_PAGE_OPENED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.CookiesSubpage.Opened"));
      break;
    case page_info::PAGE_INFO_COOKIES_SETTINGS_OPENED:
      base::RecordAction(base::UserMetricsAction(
          "PageInfo.CookiesSubpage.SettingsLinkClicked"));
      break;
    case page_info::PAGE_INFO_ALL_SITES_WITH_FPS_FILTER_OPENED:
      base::RecordAction(base::UserMetricsAction(
          "PageInfo.CookiesSubpage.AllSitesFilteredOpened"));
      break;
    case page_info::PAGE_INFO_SHOW_FULL_HISTORY_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("PageInfo.History.ShowFullHistoryClicked"));
      break;
  }
}

void PageInfo::UpdatePermissions() {
  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
}

void PageInfo::OnSitePermissionChanged(
    ContentSettingsType type,
    ContentSetting setting,
    std::optional<url::Origin> requesting_origin,
    bool is_one_time) {
  ContentSettingChangedViaPageInfo(type);

  // Count how often a permission for a specific content type is changed using
  // the Page Info UI.
  content_settings_uma_util::RecordContentSettingsHistogram(
      "WebsiteSettings.OriginInfo.PermissionChanged", type);

  if (setting == ContentSetting::CONTENT_SETTING_ALLOW) {
    content_settings_uma_util::RecordContentSettingsHistogram(
        "WebsiteSettings.OriginInfo.PermissionChanged.Allowed", type);
  } else if (setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    content_settings_uma_util::RecordContentSettingsHistogram(
        "WebsiteSettings.OriginInfo.PermissionChanged.Blocked", type);
  }

  // This is technically redundant given the histogram above, but putting the
  // total count of permission changes in another histogram makes it easier to
  // compare it against other kinds of actions in Page Info.
  HostContentSettingsMap* map = GetContentSettings();
  RecordPageInfoAction(page_info::PAGE_INFO_CHANGED_PERMISSION);
  if (type == ContentSettingsType::SOUND) {
    ContentSetting default_setting =
        map->GetDefaultContentSetting(ContentSettingsType::SOUND, nullptr);
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

  DCHECK(web_contents_);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          web_contents_.get());

  if (manager && permissions::IsRequestablePermissionType(type)) {
    // Retrieve latest permission action for the current origin and the current
    // content settings type. Note that these values are only kept in memory and
    // not persisted across browser sessions.
    std::optional<permissions::PermissionActionTime> entry =
        permissions::PermissionsClient::Get()
            ->GetOriginKeyedPermissionActionService(
                web_contents_->GetBrowserContext())
            ->GetLastActionEntry(
                permissions::PermissionUtil::GetLastCommittedOriginAsURL(
                    web_contents_->GetPrimaryMainFrame()),
                type);
    // If a value was found, and the record is from less than a minute ago,
    // record the change of mind of the user to UMA.
    if (entry.has_value() && (base::TimeTicks::Now() - entry->second <=
                              kRecordPageInfoPermissionChangeWindow)) {
      permissions::PermissionUmaUtil::RecordPageInfoPermissionChangeWithin1m(
          type, entry->first, setting);
    }
  }

  auto primary_url =
      requesting_origin.has_value() ? requesting_origin->GetURL() : site_url_;
  ContentSetting setting_old =
      map->GetContentSetting(primary_url, site_url_, type);

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(web_contents_->GetBrowserContext(),
                                 primary_url, site_url_, type,
                                 permissions::PermissionSourceUI::OIB);

  // The permission may have been blocked due to being under embargo, so if it
  // was changed away from BLOCK, clear embargo status if it exists.
  if (setting != CONTENT_SETTING_BLOCK) {
    delegate_->GetPermissionDecisionAutoblocker()->RemoveEmbargoAndResetCounts(
        site_url_, type);
  }
  using Constraints = content_settings::ContentSettingConstraints;
  Constraints constraints;
  if (is_one_time) {
    constraints.set_session_model(
        content_settings::mojom::SessionModel::ONE_TIME);
    if (base::FeatureList::IsEnabled(
            content_settings::features::kActiveContentSettingExpiry)) {
      constraints.set_lifetime(permissions::kOneTimePermissionMaximumLifetime);
    }
  }
  if (type == ContentSettingsType::STORAGE_ACCESS) {
    constraints.set_lifetime(
        permissions::kStorageAccessAPIExplicitPermissionLifetime);
  }

  map->SetNarrowestContentSetting(primary_url, site_url_, type, setting,
                                  constraints);

  bool is_subscribed_to_permission_change_event = false;

  // Suppress the infobar only if permission is allowed. Camera and
  // Microphone support all permission status changes.
  if (type == ContentSettingsType::MEDIASTREAM_MIC ||
      type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    content::PermissionController* permission_controller =
        web_contents_->GetBrowserContext()->GetPermissionController();

    blink::PermissionType permission_type =
        permissions::PermissionUtil::ContentSettingTypeToPermissionType(type);

    // An origin should subscribe to a permission status change from the top
    // frame. Hence we verify only the main frame.
    is_subscribed_to_permission_change_event =
        permission_controller->IsSubscribedToPermissionChangeEvent(
            permission_type, web_contents_->GetPrimaryMainFrame()) ||
        is_subscribed_to_permission_change_for_testing;

    permissions::PermissionUmaUtil::RecordPageInfoPermissionChange(
        type, setting_old, setting, is_subscribed_to_permission_change_event);
  }

  // Show the infobar only if permission's status is not handled by an origin.
  // When the sound or auto picture-in-picture settings are changed, no reload
  // is necessary.
  if (!is_subscribed_to_permission_change_event &&
      type != ContentSettingsType::SOUND &&
      type != ContentSettingsType::AUTO_PICTURE_IN_PICTURE) {
    show_info_bar_ = true;
  }

  if (permissions::IsRequestablePermissionType(type)) {
    auto* permission_tracker =
        permissions::PermissionRecoverySuccessRateTracker::FromWebContents(
            web_contents_.get());

    permission_tracker->PermissionStatusChanged(type, setting, show_info_bar_);
  }

  // Refresh the UI to reflect the new setting.
  PresentSitePermissions();
}

void PageInfo::OnSiteChosenObjectDeleted(const ChooserUIInfo& ui_info,
                                         const base::Value& object) {
  permissions::ObjectPermissionContextBase* context =
      delegate_->GetChooserContext(ui_info.content_settings_type);
  const auto origin = url::Origin::Create(site_url_);
  context->RevokeObjectPermission(origin, object.GetDict());
  show_info_bar_ = true;

  // Refresh the UI to reflect the changed settings.
  PresentSitePermissions();
  RecordPageInfoAction(page_info::PAGE_INFO_CHOOSER_OBJECT_DELETED);
}

void PageInfo::OnUIClosing(bool* reload_prompt) {
  if (reload_prompt) {
    *reload_prompt = false;
  }
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  if (show_info_bar_ && web_contents_ && !web_contents_->IsBeingDestroyed()) {
    if (delegate_->CreateInfoBarDelegate() && reload_prompt) {
      *reload_prompt = true;
    }
  }
  delegate_->OnUIClosing();
#endif
}

void PageInfo::OnRevokeSSLErrorBypassButtonPressed() {
  auto* stateful_ssl_host_state_delegate =
      delegate_->GetStatefulSSLHostStateDelegate();
  DCHECK(stateful_ssl_host_state_delegate);
  stateful_ssl_host_state_delegate->RevokeUserAllowExceptionsHard(
      site_url().host());
  did_revoke_user_ssl_decisions_ = true;
  RecordPageInfoAction(page_info::PAGE_INFO_RESET_DECISIONS_CLICKED);
}

void PageInfo::OnPermissionUsageChange() {
  UpdatePermissions();
}

void PageInfo::OpenSiteSettingsView() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  RecordPageInfoAction(page_info::PAGE_INFO_SITE_SETTINGS_OPENED);
  delegate_->ShowSiteSettings(site_url());
#endif
}

void PageInfo::OpenCookiesSettingsView() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  RecordPageInfoAction(page_info::PAGE_INFO_COOKIES_SETTINGS_OPENED);
  delegate_->ShowCookiesSettings();
#endif
}

void PageInfo::OpenAllSitesViewFilteredToRws() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  auto rws_owner = delegate_->GetRwsOwner(site_url_);
  RecordPageInfoAction(page_info::PAGE_INFO_ALL_SITES_WITH_FPS_FILTER_OPENED);
  if (rws_owner) {
    delegate_->ShowAllSitesSettingsFilteredByRwsOwner(*rws_owner);
  } else {
    delegate_->ShowAllSitesSettingsFilteredByRwsOwner(std::u16string());
  }

#endif
}

void PageInfo::OpenCookiesDialog() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }

  RecordPageInfoAction(page_info::PAGE_INFO_COOKIES_DIALOG_OPENED);
  delegate_->OpenCookiesDialog();
#endif
}

void PageInfo::OpenCertificateDialog(net::X509Certificate* certificate) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }

  gfx::NativeWindow top_window = web_contents_->GetTopLevelNativeWindow();
  if (certificate && top_window) {
    RecordPageInfoAction(page_info::PAGE_INFO_CERTIFICATE_DIALOG_OPENED);
    delegate_->OpenCertificateDialog(certificate);
  }
#endif
}

void PageInfo::OpenSafetyTipHelpCenterPage() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  RecordPageInfoAction(page_info::PAGE_INFO_SAFETY_TIP_HELP_OPENED);
  delegate_->OpenSafetyTipHelpCenterPage();
#endif
}

void PageInfo::OpenConnectionHelpCenterPage(const ui::Event& event) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  RecordPageInfoAction(page_info::PAGE_INFO_CONNECTION_HELP_OPENED);
  delegate_->OpenConnectionHelpCenterPage(event);
#endif
}

void PageInfo::OpenContentSettingsExceptions(
    ContentSettingsType content_settings_type) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#else
  RecordPageInfoAction(page_info::PAGE_INFO_CONNECTION_HELP_OPENED);
  delegate_->OpenContentSettingsExceptions(content_settings_type);
#endif
}

void PageInfo::OnChangePasswordButtonPressed() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  RecordPageInfoAction(page_info::PAGE_INFO_CHANGE_PASSWORD_PRESSED);
  delegate_->OnUserActionOnPasswordUi(
      safe_browsing::WarningAction::CHANGE_PASSWORD);
#endif
}

void PageInfo::OnAllowlistPasswordReuseButtonPressed() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  RecordPageInfoAction(page_info::PAGE_INFO_PASSWORD_REUSE_ALLOWED);
  delegate_->OnUserActionOnPasswordUi(
      safe_browsing::WarningAction::MARK_AS_LEGITIMATE);
#endif
}

void PageInfo::OnCookiesPageOpened() {
  RecordPageInfoAction(page_info::PAGE_INFO_COOKIES_PAGE_OPENED);
  delegate_->OnCookiesPageOpened();
}

permissions::ObjectPermissionContextBase* PageInfo::GetChooserContextFromUIInfo(
    const ChooserUIInfo& ui_info) const {
  return delegate_->GetChooserContext(ui_info.content_settings_type);
}

std::u16string PageInfo::GetSubjectNameForDisplay() const {
  if (!site_name_for_testing_.empty()) {
    return site_name_for_testing_;
  }

  return delegate_->GetSubjectName(site_url_);
}

void PageInfo::ComputeUIInputs(const GURL& url) {
  if (IsIsolatedWebApp()) {
    site_identity_status_ = SITE_IDENTITY_STATUS_ISOLATED_WEB_APP;
    site_connection_status_ = SITE_CONNECTION_STATUS_ISOLATED_WEB_APP;
    return;
  }

  auto security_level = delegate_->GetSecurityLevel();
  auto visible_security_state = delegate_->GetVisibleSecurityState();
#if !BUILDFLAG(IS_ANDROID)
  // On desktop, internal URLs aren't handled by this class. Instead, a
  // custom and simpler bubble is shown.
  DCHECK(!url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
         !url.SchemeIs(content::kViewSourceScheme) &&
         !url.SchemeIs(content_settings::kExtensionScheme));
#endif

  bool is_chrome_ui_native_scheme = false;
#if BUILDFLAG(IS_ANDROID)
  is_chrome_ui_native_scheme = url.SchemeIs(browser_ui::kChromeUINativeScheme);
#endif

  if (url.SchemeIs(url::kAboutScheme)) {
    // All about: URLs except about:blank are redirected.
    DCHECK_EQ(url::kAboutBlankURL, url.spec());
    site_identity_status_ = SITE_IDENTITY_STATUS_NO_CERT;
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
        std::u16string issuer_name(
            UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
        if (issuer_name.empty()) {
          issuer_name.assign(l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
        }

#if BUILDFLAG(IS_ANDROID)
        // This string is shown on all non-error HTTPS sites on Android when
        // the user taps "Details" link on page info.
        identity_status_description_android_.assign(l10n_util::GetStringFUTF16(
            IDS_PAGE_INFO_SECURE_IDENTITY_VERIFIED,
            delegate_->GetClientApplicationName(), issuer_name));
#endif
      }
      if (security_state::IsSHA1InChain(visible_security_state)) {
        site_identity_status_ =
            SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM;

#if BUILDFLAG(IS_ANDROID)
        identity_status_description_android_ +=
            u"\n\n" +
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
#if BUILDFLAG(IS_ANDROID)
    const std::u16string bullet = u"\n • ";
    std::vector<ssl_errors::ErrorInfo> errors;
    ssl_errors::ErrorInfo::GetErrorsForCertStatus(
        certificate_, visible_security_state.cert_status, url, &errors);

    identity_status_description_android_.assign(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
    for (const ssl_errors::ErrorInfo& error : errors) {
      identity_status_description_android_ += bullet;
      identity_status_description_android_ += error.short_description();
    }
#endif
  }

  if (visible_security_state.malicious_content_status !=
      security_state::MALICIOUS_CONTENT_STATUS_NONE) {
    // The site has been flagged by Safe Browsing. Takes precedence over TLS.
    GetSafeBrowsingStatusByMaliciousContentStatus(
        visible_security_state.malicious_content_status, &safe_browsing_status_,
        &safe_browsing_details_);
#if BUILDFLAG(IS_ANDROID)
    identity_status_description_android_ = safe_browsing_details_;
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
                 MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE ||
         visible_security_state.malicious_content_status ==
             security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE);
#if BUILDFLAG(FULL_SAFE_BROWSING)
    // Only record password reuse when adding the button, not on updates.
    if (show_change_password_buttons_ && !old_show_change_pw_buttons) {
      RecordPasswordReuseEvent();
    }
#endif
  }

  safety_tip_info_ = visible_security_state.safety_tip_info;
#if BUILDFLAG(IS_ANDROID)
  // identity_status_description_android_ is only displayed on Android when
  // the user taps "Details" link on the page info. Reuse the description from
  // page info UI.
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      PageInfoUI::CreateSafetyTipSecurityDescription(safety_tip_info_);
  if (security_description) {
    identity_status_description_android_ = security_description->details;
  }
#endif

  // Site Connection
  // We consider anything less than 80 bits encryption to be weak encryption.
  // TODO(wtc): Bug 1198735: report mixed/unsafe content for unencrypted and
  // weakly encrypted connections.
  site_connection_status_ = SITE_CONNECTION_STATUS_UNKNOWN;

  std::u16string subject_name(GetSubjectNameForDisplay());
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
    site_connection_details_ += u"\n\n";
    site_connection_details_ += l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_SECURITY_TAB_SSL_VERSION, ASCIIToUTF16(ssl_version_str));

    const char *key_exchange, *cipher, *mac;
    bool is_aead, is_tls13;
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);

    site_connection_details_ += u"\n\n";
    if (is_aead) {
      if (is_tls13) {
        // For TLS 1.3 ciphers, report the group (historically, curve) as the
        // key exchange.
        key_exchange =
            SSL_get_curve_name(visible_security_state.key_exchange_group);
        if (!key_exchange) {
          NOTREACHED_IN_MIGRATION();
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
  // errors on this site, or made a decision to allow HTTP for this site.
  StatefulSSLHostStateDelegate* delegate =
      delegate_->GetStatefulSSLHostStateDelegate();
  DCHECK(delegate);
  DCHECK(web_contents_);
  bool has_cert_allow_exception = delegate->HasCertAllowException(
      url.host(), web_contents_->GetPrimaryMainFrame()->GetStoragePartition());
  bool has_http_allow_exception = delegate->IsHttpAllowedForHost(
      url.host(), web_contents_->GetPrimaryMainFrame()->GetStoragePartition());

  // HTTP allowlist entries can be added because of silent HTTPS-Upgrades
  // without the user proceeding through a warning. Only show a warning decision
  // revocation button for HTTP allowlist entries added because HTTPS was
  // enforced by HTTPS-First Mode.
  bool is_https_enforced =
      delegate->IsHttpsEnforcedForUrl(
          url, web_contents_->GetPrimaryMainFrame()->GetStoragePartition()) ||
      delegate_->IsHttpsFirstModeEnabled();

  bool has_warning_bypass_exception =
      has_cert_allow_exception ||
      (has_http_allow_exception && is_https_enforced);

  // Only show a warning decision revocation button if the user has chosen to
  // bypass SSL host errors / HTTP warnings for this host in the past, and we're
  // not presently on a Safe Browsing error (since otherwise it's confusing
  // which warning you're re-enabling).
  show_ssl_decision_revoke_button_ =
      has_warning_bypass_exception &&
      visible_security_state.malicious_content_status ==
          security_state::MALICIOUS_CONTENT_STATUS_NONE;
}

void PageInfo::PopulatePermissionInfo(PermissionInfo& permission_info,
                                      HostContentSettingsMap* content_settings,
                                      const content_settings::SettingInfo& info,
                                      ContentSetting setting) const {
  DCHECK(permission_info.type != ContentSettingsType::DEFAULT);
  permission_info.setting = setting;

  permission_info.source = info.source;
  permission_info.is_one_time =
      (info.metadata.session_model() ==
       content_settings::mojom::SessionModel::ONE_TIME);

  auto* page_specific_content_settings = GetPageSpecificContentSettings();
  if (page_specific_content_settings && setting == CONTENT_SETTING_ALLOW) {
    permission_info.is_in_use =
        page_specific_content_settings->IsInUse(permission_info.type);

    permission_info.last_used =
        page_specific_content_settings->GetLastUsedTime(permission_info.type);
  }

  if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
    permission_info.default_setting = permission_info.setting;
    permission_info.setting = CONTENT_SETTING_DEFAULT;
  } else {
    permission_info.default_setting =
        content_settings->GetDefaultContentSetting(permission_info.type,
                                                   nullptr);
  }

  // Check embargo status if the content setting supports embargo.
  if (permissions::PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
          permission_info.type) &&
      permission_info.setting == CONTENT_SETTING_DEFAULT &&
      permission_info.source == content_settings::SettingSource::kUser) {
    content::PermissionResult permission_result(
        PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED);
    if (permissions::PermissionUtil::IsPermission(permission_info.type)) {
      permission_result = delegate_->GetPermissionResult(
          permissions::PermissionUtil::ContentSettingTypeToPermissionType(
              permission_info.type),
          url::Origin::Create(site_url_), permission_info.requesting_origin);
    } else if (permission_info.type ==
               ContentSettingsType::FEDERATED_IDENTITY_API) {
      std::optional<content::PermissionResult> embargo_result =
          delegate_->GetPermissionDecisionAutoblocker()->GetEmbargoResult(
              site_url_, permission_info.type);
      if (embargo_result) {
        permission_result = embargo_result.value();
      }
    }

    // If under embargo, update |permission_info| to reflect that.
    if (permission_result.status == PermissionStatus::DENIED &&
        (permission_result.source ==
             content::PermissionStatusSource::MULTIPLE_DISMISSALS ||
         permission_result.source ==
             content::PermissionStatusSource::MULTIPLE_IGNORES)) {
      permission_info.setting =
          permissions::PermissionUtil::PermissionStatusToContentSetting(
              permission_result.status);
    }
  }
}

// Determines whether to show permission |type| in the Page Info UI. Only
// applies to permissions listed in |kPermissionType|.
bool PageInfo::ShouldShowPermission(
    const PageInfo::PermissionInfo& info) const {
  // Note |ContentSettingsType::ADS| will show up regardless of its default
  // value when it has been activated on the current origin.
  if (info.type == ContentSettingsType::ADS) {
    if (!base::FeatureList::IsEnabled(
            subresource_filter::kSafeBrowsingSubresourceFilter)) {
      return false;
    }

    return delegate_->IsSubresourceFilterActivated(site_url_);
  }

  if (info.type == ContentSettingsType::SOUND) {
    // The sound content setting should always show up when the tab has played
    // audio.
    if (web_contents_ && web_contents_->WasEverAudible()) {
      return true;
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  if (info.type == ContentSettingsType::AUTO_PICTURE_IN_PICTURE) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kMediaSessionEnterPictureInPicture)) {
      return false;
    }
    if (delegate_->HasAutoPictureInPictureBeenRegistered()) {
      return true;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (info.type == ContentSettingsType::AUTOMATIC_FULLSCREEN &&
      !base::FeatureList::IsEnabled(
          features::kAutomaticFullscreenContentSetting)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (info.type == ContentSettingsType::WEB_PRINTING &&
      !base::FeatureList::IsEnabled(blink::features::kWebPrinting)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  const bool is_incognito =
      web_contents_->GetBrowserContext()->IsOffTheRecord();
#if BUILDFLAG(IS_ANDROID)
  // Special geolocation DSE settings apply only on Android, so make sure it
  // gets checked there regardless of default setting on Desktop.
  // DSE settings don't apply to incognito mode.
  if (info.type == ContentSettingsType::GEOLOCATION && !is_incognito) {
    return true;
  }

  // The File System write permission is desktop only at the moment.
  if (info.type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD) {
    return false;
  }
#else
  // NFC is Android-only at the moment.
  if (info.type == ContentSettingsType::NFC) {
    return false;
  }

  // Display the File System Access write permission if the File System Access
  // API is currently being used.
  if (info.type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD &&
      web_contents_->HasFileSystemAccessHandles()) {
    return true;
  }

  // Hide camera if camera PTZ is granted or blocked.
  if (info.type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    ContentSetting camera_ptz_setting = GetContentSettings()->GetContentSetting(
        site_url_, site_url_, ContentSettingsType::CAMERA_PAN_TILT_ZOOM);
    if (camera_ptz_setting == CONTENT_SETTING_ALLOW ||
        camera_ptz_setting == CONTENT_SETTING_BLOCK) {
      return false;
    }
  }
#endif

  // TODO(crbug.com/40064079): Filter out FPS related STORAGE_ACCESS
  // permissions.

  // Show the content setting if it has been changed by the user since the last
  // page load.
  if (HasContentSettingChangedViaPageInfo(info.type)) {
    return true;
  }

  // Show the Bluetooth guard permission if the new permissions backend is
  // enabled.
  if (info.type == ContentSettingsType::BLUETOOTH_GUARD &&
      base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend) &&
      !PageInfo::IsPermissionFactoryDefault(info, is_incognito)) {
    return true;
  }

  // Show the content setting when it has a non-default value.
  if (!PageInfo::IsPermissionFactoryDefault(info, is_incognito)) {
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (info.type == ContentSettingsType::WEB_APP_INSTALLATION &&
      base::FeatureList::IsEnabled(blink::features::kWebAppInstallation)) {
    return true;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return false;
}

void PageInfo::PresentSitePermissions() {
  PermissionInfoList permission_info_list;
  ChosenObjectInfoList chosen_object_info_list;

  HostContentSettingsMap* content_settings = GetContentSettings();
  DCHECK(web_contents_);
  for (const ContentSettingsType type : kPermissionType) {
    PermissionInfo permission_info;
    permission_info.type = type;

    content_settings::SettingInfo info;
    ContentSetting setting = content_settings->GetContentSetting(
        site_url_, site_url_, permission_info.type, &info);
    PopulatePermissionInfo(permission_info, content_settings, info, setting);
    if (ShouldShowPermission(permission_info)) {
      permission_info_list.push_back(permission_info);
    }
  }

  for (ContentSettingsType type : kTwoPatternPermissions) {
    for (auto& requester : GetTwoSitePermissionRequesters(type)) {
      PermissionInfo permission_info;
      permission_info.type = type;
      permission_info.requesting_origin =
          url::Origin::Create(requester.GetURL());
      content_settings::SettingInfo info;
      ContentSetting setting = content_settings->GetContentSetting(
          requester.GetURL(), site_url_, permission_info.type, &info);

      if (IsGrantedByRelatedWebsiteSets(type, info.metadata) &&
          !base::FeatureList::IsEnabled(
              permissions::features::kShowRelatedWebsiteSetsPermissionGrants)) {
        continue;
      }

      PopulatePermissionInfo(permission_info, content_settings, info, setting);
      if (ShouldShowPermission(permission_info)) {
        permission_info_list.push_back(permission_info);
      }
    }
  }

  const auto origin = url::Origin::Create(site_url_);
  for (const ChooserUIInfo& ui_info : kChooserUIInfo) {
    permissions::ObjectPermissionContextBase* context =
        delegate_->GetChooserContext(ui_info.content_settings_type);
    if (!context) {
      continue;
    }
    auto chosen_objects = context->GetGrantedObjects(origin);
    for (std::unique_ptr<permissions::ObjectPermissionContextBase::Object>&
             object : chosen_objects) {
      chosen_object_info_list.push_back(
          std::make_unique<PageInfoUI::ChosenObjectInfo>(ui_info,
                                                         std::move(object)));
    }
  }

  ui_->SetPermissionInfo(permission_info_list,
                         std::move(chosen_object_info_list));
}

std::set<net::SchemefulSite> PageInfo::GetTwoSitePermissionRequesters(
    ContentSettingsType type) {
  std::set<net::SchemefulSite> requesters;
  // Collect sites that have tried to request a permission.
  auto* pscs = GetPageSpecificContentSettings();
  if (pscs) {
    for (auto& [requester, allowed] : pscs->GetTwoSiteRequests(type)) {
      requesters.insert(requester);
    }
  }
  // Collect sites that were previously granted a permission
  auto* map = GetContentSettings();
  for (auto& setting : map->GetSettingsForOneType(type)) {
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      continue;  // Skip default setting.
    }
    // Settings that specify two origins shouldn't have wildcards for either
    // pattern.
    DCHECK_NE(setting.primary_pattern, ContentSettingsPattern::Wildcard())
        << "type: " << static_cast<int>(type);
    DCHECK_NE(setting.secondary_pattern, ContentSettingsPattern::Wildcard())
        << "type: " << static_cast<int>(type);

    if (!setting.secondary_pattern.Matches(site_url_)) {
      continue;  // Skip unrelated settings.
    }
    if (type == ContentSettingsType::STORAGE_ACCESS) {
      if (setting.primary_pattern.Matches(site_url_)) {
        continue;  // Skip first-party settings.
      }
      if (IsGrantedByRelatedWebsiteSets(type, setting.metadata) &&
          !base::FeatureList::IsEnabled(
              permissions::features::kShowRelatedWebsiteSetsPermissionGrants)) {
        continue;
      }
    }
    GURL requesting_url = setting.primary_pattern.ToRepresentativeUrl();
    requesters.insert(net::SchemefulSite(requesting_url));
  }
  return requesters;
}

void PageInfo::PresentSiteDataInternal(base::OnceClosure done) {
  // Since this is called asynchronously, the associated `WebContents` object
  // might no longer be available.
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }

  // Presenting site data is only needed if `PageInfoUI` is available.
  if (!ui_) {
    return;
  }

  PageInfoUI::CookiesNewInfo cookies_info;
  cookies_info.allowed_sites_count = GetSitesWithAllowedCookiesAccessCount();

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI)) {
    auto rws_owner = delegate_->GetRwsOwner(site_url_);
    if (rws_owner) {
      cookies_info.rws_info = PageInfoUI::CookiesRwsInfo(*rws_owner);
      cookies_info.rws_info->is_managed = delegate_->IsRwsManaged();
    }
  }
#endif

  cookies_info.controls_visible = controls_visible_;
  cookies_info.protections_on = protections_on_;
  cookies_info.enforcement = enforcement_;
  cookies_info.blocking_status = blocking_status_;
  cookies_info.features = features_;
  cookies_info.expiration = cookie_exception_expiration_;
  cookies_info.is_otr = web_contents_->GetBrowserContext()->IsOffTheRecord();
  ui_->SetCookieInfo(cookies_info);

  std::move(done).Run();
}

void PageInfo::PresentSiteData(base::OnceClosure done) {
  auto* settings = GetPageSpecificContentSettings();
  if (settings && weak_factory_.GetWeakPtr()) {
    PresentSiteDataInternal(std::move(done));
  } else {
    std::move(done).Run();
  }
}

void PageInfo::PresentSiteIdentity() {
  // After initialization the status about the site's connection and its
  // identity must be available.
  DCHECK_NE(site_identity_status_, SITE_IDENTITY_STATUS_UNKNOWN);
  DCHECK_NE(site_connection_status_, SITE_CONNECTION_STATUS_UNKNOWN);
  PageInfoUI::IdentityInfo info;
  info.site_identity = UTF16ToUTF8(GetSubjectNameForDisplay());

  info.connection_status = site_connection_status_;
  info.connection_status_description = UTF16ToUTF8(site_connection_details_);
  info.identity_status = site_identity_status_;
  info.safe_browsing_status = safe_browsing_status_;
  info.safe_browsing_details = safe_browsing_details_;
  info.safety_tip_info = safety_tip_info_;
#if BUILDFLAG(IS_ANDROID)
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
      delegate_->IsContentDisplayedInVrHeadset();

  ui_->SetPageFeatureInfo(info);
}

void PageInfo::PresentAdPersonalizationData() {
  PageInfoUI::AdPersonalizationInfo info;
  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return;
  }

  info.has_joined_user_to_interest_group =
      settings->HasJoinedUserToInterestGroup();
  info.accessed_topics = settings->GetAccessedTopics();
  std::sort(info.accessed_topics.begin(), info.accessed_topics.end(),
            [](const privacy_sandbox::CanonicalTopic& a,
               const privacy_sandbox::CanonicalTopic& b) {
              return a.GetLocalizedRepresentation() <
                     b.GetLocalizedRepresentation();
            });
  ui_->SetAdPersonalizationInfo(info);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void PageInfo::RecordPasswordReuseEvent() {
  auto* password_protection_service = delegate_->GetPasswordProtectionService();
  if (!password_protection_service) {
    return;
  }
  safe_browsing::LogWarningAction(
      safe_browsing::WarningUIType::PAGE_INFO,
      safe_browsing::WarningAction::SHOWN,
      password_protection_service
          ->reused_password_account_type_for_last_shown_warning());
}
#endif

HostContentSettingsMap* PageInfo::GetContentSettings() const {
  return delegate_->GetContentSettings();
}

std::vector<ContentSettingsType> PageInfo::GetAllPermissionsForTesting() {
  std::vector<ContentSettingsType> permission_list;
  for (const ContentSettingsType type : kPermissionType) {
    permission_list.push_back(type);
  }

  return permission_list;
}

void PageInfo::SetSiteNameForTesting(const std::u16string& site_name) {
  site_name_for_testing_ = site_name;
  PresentSiteIdentity();
}

void PageInfo::GetSafeBrowsingStatusByMaliciousContentStatus(
    security_state::MaliciousContentStatus malicious_content_status,
    PageInfo::SafeBrowsingStatus* status,
    std::u16string* details) {
  switch (malicious_content_status) {
    case security_state::MALICIOUS_CONTENT_STATUS_NONE:
      NOTREACHED_IN_MIGRATION();
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
      *details = delegate_->GetWarningDetailText();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status = PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
      *details = delegate_->GetWarningDetailText();
#endif
      break;
    case security_state::
        MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
      *details = delegate_->GetWarningDetailText();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE:
#if BUILDFLAG(FULL_SAFE_BROWSING)
      *status = PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE;
      *details = delegate_->GetWarningDetailText();
#endif
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_BILLING:
      *status = PageInfo::SAFE_BROWSING_STATUS_BILLING;
      *details = l10n_util::GetStringUTF16(IDS_PAGE_INFO_BILLING_DETAILS);
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK:
      *status = PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK;
      *details =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_ENTERPRISE_BLOCK_DETAILS);
      break;
    case security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN:
      *status = PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN;
      *details =
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_ENTERPRISE_WARN_DETAILS);
      break;
  }
}

content_settings::PageSpecificContentSettings*
PageInfo::GetPageSpecificContentSettings() const {
  // TODO(https://crbug.com/1103176, https://crbug.com/1233122): PageInfo should
  // be per page. Why is it a WebContentsObserver if it is not observing
  // anything?
  DCHECK(web_contents_);
  return content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents_->GetPrimaryMainFrame());
}

bool PageInfo::HasContentSettingChangedViaPageInfo(
    ContentSettingsType type) const {
  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return false;
  }

  return settings->HasContentSettingChangedViaPageInfo(type);
}

void PageInfo::ContentSettingChangedViaPageInfo(ContentSettingsType type) {
  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return;
  }

  return settings->ContentSettingChangedViaPageInfo(type);
}

int PageInfo::GetSitesWithAllowedCookiesAccessCount() {
  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return 0;
  }
  return browsing_data::GetUniqueHostCount(
      *(settings->allowed_browsing_data_model()));
}

int PageInfo::GetThirdPartySitesWithBlockedCookiesAccessCount(
    const GURL& site_url) {
  auto* settings = GetPageSpecificContentSettings();
  if (!settings) {
    return 0;
  }
  return browsing_data::GetUniqueThirdPartyCookiesHostCount(
      site_url, *(settings->blocked_browsing_data_model()));
}

bool PageInfo::IsIsolatedWebApp() const {
#if !BUILDFLAG(IS_ANDROID)
  return delegate_->IsIsolatedWebApp();
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

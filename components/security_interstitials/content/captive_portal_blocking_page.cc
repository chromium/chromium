// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/captive_portal_blocking_page.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_metrics.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/wifi/wifi_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif

// static
const void* const CaptivePortalBlockingPage::kTypeForTesting =
    &CaptivePortalBlockingPage::kTypeForTesting;

CaptivePortalBlockingPage::CaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& login_url,
    bool can_show_enhanced_protection_message,
    const net::SSLInfo& ssl_info,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client,
    const OpenLoginCallback& open_login_callback)
    : SSLBlockingPageBase(web_contents,
                          ssl_info,
                          request_url,
                          false /* overridable */,
                          base::Time::Now(),
                          can_show_enhanced_protection_message,
                          std::move(controller_client)),
      open_login_callback_(open_login_callback),
      login_url_(login_url),
      ssl_info_(ssl_info) {
  captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
      captive_portal::CaptivePortalMetrics::SHOW_ALL);
}

CaptivePortalBlockingPage::~CaptivePortalBlockingPage() = default;

const void* CaptivePortalBlockingPage::GetTypeForTesting() {
  return CaptivePortalBlockingPage::kTypeForTesting;
}

void CaptivePortalBlockingPage::OverrideWifiInfoForTesting(
    bool is_wifi_connection,
    const std::string& wifi_ssid) {
  is_wifi_info_overridden_for_testing_ = true;
  is_wifi_connection_for_testing_ = is_wifi_connection;
  wifi_ssid_for_testing_ = wifi_ssid;
}

bool CaptivePortalBlockingPage::IsWifiConnection() const {
  if (is_wifi_info_overridden_for_testing_)
    return is_wifi_connection_for_testing_;

  // |net::NetworkChangeNotifier::GetConnectionType| isn't accurate on Linux
  // and Windows. See https://crbug.com/160537 for details.
  // TODO(meacer): Add heuristics to get a more accurate connection type on
  //               these platforms.
  return net::NetworkChangeNotifier::GetConnectionType() ==
         net::NetworkChangeNotifier::CONNECTION_WIFI;
}

std::string CaptivePortalBlockingPage::GetWiFiSSID() const {
  if (is_wifi_info_overridden_for_testing_)
    return wifi_ssid_for_testing_;

  // On Windows and Mac, |WiFiService| provides an easy to use API to get the
  // currently associated WiFi access point. |WiFiService| isn't available on
  // Linux so |net::GetWifiSSID| is used instead.
  std::string ssid;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  std::unique_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  wifi_service->Initialize(nullptr);
  std::string error;
  wifi_service->GetConnectedNetworkSSID(&ssid, &error);
  if (!error.empty())
    return std::string();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ssid = net::GetWifiSSID();
#elif BUILDFLAG(IS_ANDROID)
  ssid = net::android::GetWifiSSID();
#endif
  // TODO(meacer): Handle non UTF8 SSIDs.
  if (!base::IsStringUTF8(ssid))
    return std::string();
  return ssid;
}

void CaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("iconClass", "icon-offline");
  load_time_data.Set("type", "CAPTIVE_PORTAL");
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);

  // |IsWifiConnection| isn't accurate on some platforms, so always try to get
  // the Wi-Fi SSID even if |IsWifiConnection| is false.
  std::string wifi_ssid = GetWiFiSSID();
  bool is_wifi = !wifi_ssid.empty() || IsWifiConnection();

  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));

  std::u16string tab_title =
      l10n_util::GetStringUTF16(is_wifi ? IDS_CAPTIVE_PORTAL_HEADING_WIFI
                                        : IDS_CAPTIVE_PORTAL_HEADING_WIRED);
  load_time_data.Set("tabTitle", tab_title);
  load_time_data.Set("heading", tab_title);

  std::u16string paragraph;
  if (login_url_.is_empty() ||
      login_url_.spec() == captive_portal::CaptivePortalDetector::kDefaultURL) {
    // Don't show the login url when it's empty or is the portal detection URL.
    // login_url_ can be empty when:
    // - The captive portal intercepted requests without HTTP redirects, in
    // which case the login url would be the same as the captive portal
    // detection url.
    // - The captive portal was detected via Captive portal certificate list.
    // - The captive portal was reported by the OS.
    if (wifi_ssid.empty()) {
      paragraph = l10n_util::GetStringUTF16(
          is_wifi ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI
                  : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIRED);
    } else {
      paragraph = l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI_SSID,
          base::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid)));
    }
  } else {
    // Portal redirection was done with HTTP redirects, so show the login URL.
    // If |languages| is empty, punycode in |login_host| will always be decoded.
    std::u16string login_host = url_formatter::IDNToUnicode(login_url_.host());
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    if (wifi_ssid.empty()) {
      paragraph = l10n_util::GetStringFUTF16(
          is_wifi ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI
                  : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIRED,
          login_host);
    } else {
      paragraph = l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI_SSID,
          base::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid)), login_host);
    }
  }
  load_time_data.Set("primaryParagraph", std::move(paragraph));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  // Explicitly specify other expected fields to empty.
  load_time_data.Set("openDetails", "");
  load_time_data.Set("closeDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set(security_interstitials::kDisplayCheckBox, false);

  PopulateEnhancedProtectionMessage(load_time_data);
}

void CaptivePortalBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }
  int command_num = 0;
  bool command_is_num = base::StringToInt(command, &command_num);
  DCHECK(command_is_num) << command;
  security_interstitials::SecurityInterstitialCommand cmd =
      static_cast<security_interstitials::SecurityInterstitialCommand>(
          command_num);
  switch (cmd) {
    case security_interstitials::CMD_OPEN_LOGIN:
      captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
          captive_portal::CaptivePortalMetrics::OPEN_LOGIN_PAGE);
      open_login_callback_.Run(web_contents());
      break;
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
      controller()->OpenExtendedReportingPrivacyPolicy(true);
      break;
    case security_interstitials::CMD_OPEN_WHITEPAPER:
      controller()->OpenExtendedReportingWhitepaper(true);
      break;
    case security_interstitials::CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
      controller()->OpenEnhancedProtectionSettings();
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Command " << cmd
          << " isn't handled by the captive portal interstitial.";
  }
}

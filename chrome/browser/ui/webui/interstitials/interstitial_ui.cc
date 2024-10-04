// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_blocking_page_quiet.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_only_mode_controller_client.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/insecure_form/insecure_form_controller_client.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/https_only_mode_blocking_page.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"  // nogncheck
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page_blocked_sites.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page_youtube.h"
#endif

using security_interstitials::TestSafeBrowsingBlockingPageQuiet;

InterstitialUIConfig::InterstitialUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIInterstitialHost) {}

namespace {

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::AtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::Minutes(5),
          base::Time::Now() + base::Minutes(5), &unused_key, &cert_der)) {
    return nullptr;
  }

  return net::X509Certificate::CreateFromBytes(
      base::as_bytes(base::make_span(cert_der)));
}

// Implementation of chrome://interstitials demonstration pages. This code is
// not used in displaying any real interstitials.
class InterstitialHTMLSource : public content::URLDataSource {
 public:
  InterstitialHTMLSource() = default;

  InterstitialHTMLSource(const InterstitialHTMLSource&) = delete;
  InterstitialHTMLSource& operator=(const InterstitialHTMLSource&) = delete;

  ~InterstitialHTMLSource() override = default;

  // content::URLDataSource:
  std::string GetMimeType(const GURL& url) override;
  std::string GetSource() override;
  std::string GetContentSecurityPolicy(
      const network::mojom::CSPDirectiveName directive) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

 private:
  std::string GetSupervisedUserInterstitialHTML(const std::string& path);
};

std::unique_ptr<SSLBlockingPage> CreateSslBlockingPage(
    content::WebContents* web_contents) {
  // Random parameters for SSL blocking page.
  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  base::Time time_triggered_ = base::Time::NowFromSystemTime();
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid()) {
      request_url = GURL(url_param);
    }
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(),
                                 "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "type",
                                 &type_param)) {
    if (type_param == "hpkp_failure") {
      cert_error = net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
    } else if (type_param == "ct_failure") {
      cert_error = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateSSLPage(web_contents, cert_error, ssl_info,
                                             request_url, options_mask,
                                             time_triggered_, GURL());
}

std::unique_ptr<MITMSoftwareBlockingPage> CreateMITMSoftwareBlockingPage(
    content::WebContents* web_contents) {
  const int cert_error = net::ERR_CERT_AUTHORITY_INVALID;
  const GURL request_url("https://example.com");
  const std::string mitm_software_name = "Misconfigured Antivirus";
  bool is_enterprise_managed = false;

  std::string is_enterprise_managed_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "enterprise",
                                 &is_enterprise_managed_param)) {
    is_enterprise_managed = is_enterprise_managed_param == "1";
  }

  ChromeSecurityBlockingPageFactory blocking_page_factory;
  blocking_page_factory.SetEnterpriseManagedForTesting(is_enterprise_managed);

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  return blocking_page_factory.CreateMITMSoftwareBlockingPage(
      web_contents, cert_error, request_url, ssl_info, mitm_software_name);
}

std::unique_ptr<BlockedInterceptionBlockingPage>
CreateBlockedInterceptionBlockingPage(content::WebContents* web_contents) {
  const int cert_error = net::ERR_CERT_AUTHORITY_INVALID;
  const GURL request_url("https://example.com");

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateBlockedInterceptionBlockingPage(
      web_contents, cert_error, request_url, ssl_info);
}

std::unique_ptr<BadClockBlockingPage> CreateBadClockBlockingPage(
    content::WebContents* web_contents) {
  // Set up a fake clock error.
  int cert_error = net::ERR_CERT_DATE_INVALID;
  GURL request_url("https://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "url",
                                 &url_param) &&
      GURL(url_param).is_valid()) {
    request_url = GURL(url_param);
  }

  // Determine whether to change the clock to be ahead or behind.
  std::string clock_manipulation_param;
  ssl_errors::ClockState clock_state = ssl_errors::CLOCK_STATE_PAST;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(),
                                 "clock_manipulation",
                                 &clock_manipulation_param)) {
    int time_offset;
    if (base::StringToInt(clock_manipulation_param, &time_offset)) {
      clock_state = time_offset > 0 ? ssl_errors::CLOCK_STATE_FUTURE
                                    : ssl_errors::CLOCK_STATE_PAST;
    }
  }

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateBadClockBlockingPage(
      web_contents, cert_error, ssl_info, request_url, base::Time::Now(),
      clock_state);
}

std::unique_ptr<LookalikeUrlBlockingPage> CreateLookalikeInterstitialPage(
    content::WebContents* web_contents) {
  GURL request_url("https://example.net");
  GURL safe_url("https://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "no-safe-url",
                                 &url_param)) {
    safe_url = GURL();
  }
  return std::make_unique<LookalikeUrlBlockingPage>(
      web_contents, safe_url, request_url, ukm::kInvalidSourceId,
      lookalikes::LookalikeUrlMatchType::kNone, false,
      /*triggered_by_initial_url=*/false,
      std::make_unique<LookalikeUrlControllerClient>(web_contents, request_url,
                                                     safe_url));
}

std::unique_ptr<security_interstitials::InsecureFormBlockingPage>
CreateInsecureFormPage(content::WebContents* web_contents) {
  GURL request_url("http://example.com");
  return std::make_unique<security_interstitials::InsecureFormBlockingPage>(
      web_contents, request_url,
      std::make_unique<InsecureFormControllerClient>(web_contents,
                                                     request_url));
}

std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
CreateHttpsOnlyModePage(content::WebContents* web_contents) {
  GURL request_url("http://example.com");
  return std::make_unique<security_interstitials::HttpsOnlyModeBlockingPage>(
      web_contents, request_url,
      std::make_unique<HttpsOnlyModeControllerClient>(web_contents,
                                                      request_url),
      security_interstitials::https_only_mode::HttpInterstitialState{},
      /*use_new_interstitial=*/IsNewHttpsFirstModeInterstitialEnabled());
}

std::unique_ptr<security_interstitials::SecurityInterstitialPage>
CreateSafeBrowsingBlockingPage(content::WebContents* web_contents) {
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid()) {
      request_url = GURL(url_param);
    }
  }
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "type",
                                 &type_param)) {
    if (type_param == "malware") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "unwanted") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
    } else if (type_param == "clientside_phishing") {
      threat_type =
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    } else if (type_param == "billing") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
    }
  }
  auto* primary_main_frame = web_contents->GetPrimaryMainFrame();
  const content::GlobalRenderFrameHostId primary_main_frame_id =
      primary_main_frame->GetGlobalId();
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.threat_type = threat_type;
  resource.render_process_id = primary_main_frame_id.child_id;
  resource.render_frame_token = primary_main_frame->GetFrameToken().value();
  resource.threat_source =
      g_browser_process->safe_browsing_service()
          ->database_manager()
          ->GetBrowseUrlThreatSource(
              safe_browsing::CheckBrowseUrlType::kHashDatabase);

  // Normally safebrowsing interstitial types which block the main page load
  // (SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_PHISHING, and
  // SB_THREAT_TYPE_URL_UNWANTED on main-frame loads) would expect there to be a
  // pending navigation when the SafeBrowsingBlockingPage is created. This demo
  // creates a SafeBrowsingBlockingPage but does not actually show a real
  // interstitial. Instead it extracts the html and displays it manually, so the
  // parts which depend on the NavigationEntry are not hit.
  auto* ui_manager =
      g_browser_process->safe_browsing_service()->ui_manager().get();
  return base::WrapUnique<security_interstitials::SecurityInterstitialPage>(
      ui_manager->CreateBlockingPage(
          web_contents, request_url, {resource},
          /*forward_extension_event=*/false,
          /*blocked_page_shown_timestamp=*/std::nullopt));
}

std::unique_ptr<EnterpriseBlockPage> CreateEnterpriseBlockPage(
    content::WebContents* web_contents) {
  const GURL kRequestUrl("https://enterprise-block.example.net");
  return std::make_unique<EnterpriseBlockPage>(
      web_contents, kRequestUrl,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<EnterpriseBlockControllerClient>(web_contents,
                                                        kRequestUrl));
}

std::unique_ptr<ManagedProfileRequiredPage> CreateManagedProfileRequiredPage(
    content::WebContents* web_contents) {
  const GURL kRequestUrl("https://example.com");
  return std::make_unique<ManagedProfileRequiredPage>(
      web_contents, kRequestUrl, /*manager=*/u"example.com",
      /*email=*/u"alice@example.com",
      std::make_unique<ManagedProfileRequiredControllerClient>(web_contents,
                                                               kRequestUrl));
}

std::unique_ptr<EnterpriseWarnPage> CreateEnterpriseWarnPage(
    content::WebContents* web_contents) {
  const GURL kRequestUrl("https://enterprise-warn.example.net");

  auto* ui_manager =
      g_browser_process->safe_browsing_service()->ui_manager().get();

  auto* primary_main_frame = web_contents->GetPrimaryMainFrame();
  const content::GlobalRenderFrameHostId primary_main_frame_id =
      primary_main_frame->GetGlobalId();
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = kRequestUrl;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN;
  resource.render_process_id = primary_main_frame_id.child_id;
  resource.render_frame_token = primary_main_frame->GetFrameToken().value();
  resource.threat_source =
      g_browser_process->safe_browsing_service()
          ->database_manager()
          ->GetBrowseUrlThreatSource(
              safe_browsing::CheckBrowseUrlType::kHashDatabase);

  return std::make_unique<EnterpriseWarnPage>(
      ui_manager, web_contents, kRequestUrl,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList({resource}),
      std::make_unique<EnterpriseWarnControllerClient>(web_contents,
                                                       kRequestUrl));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
std::unique_ptr<SupervisedUserVerificationPageForYouTube>
CreateSupervisedUserVerificationPageForYouTube(
    content::WebContents* web_contents,
    bool is_main_frame) {
  const GURL kRequestUrl("https://supervised-user-verification.example.net");
  return std::make_unique<SupervisedUserVerificationPageForYouTube>(
      web_contents, "first.last@gmail.com", kRequestUrl,
      /*child_account_service*/ nullptr, ukm::kInvalidSourceId,
      std::make_unique<SupervisedUserVerificationControllerClient>(
          web_contents,
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL), kRequestUrl),
      is_main_frame);
}

std::unique_ptr<SupervisedUserVerificationPageForBlockedSites>
CreateSupervisedUserVerificationPageForBlockedSites(
    content::WebContents* web_contents,
    bool is_main_frame) {
  const GURL kRequestUrl("https://supervised-user-verification.example.net");
  return std::make_unique<SupervisedUserVerificationPageForBlockedSites>(
      web_contents, "first.last@gmail.com", kRequestUrl,
      /*child_account_service*/ nullptr,
      std::make_unique<SupervisedUserVerificationControllerClient>(
          web_contents,
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL), kRequestUrl),
      supervised_user::FilteringBehaviorReason::DEFAULT, is_main_frame);
}
#endif

std::unique_ptr<TestSafeBrowsingBlockingPageQuiet>
CreateSafeBrowsingQuietBlockingPage(content::WebContents* web_contents) {
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  std::string type_param;
  bool is_giant_webview = false;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "type",
                                 &type_param)) {
    if (type_param == "malware") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "unwanted") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
    } else if (type_param == "billing") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING;
    } else if (type_param == "giant") {
      threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
      is_giant_webview = true;
    }
  }
  auto* primary_main_frame = web_contents->GetPrimaryMainFrame();
  const content::GlobalRenderFrameHostId primary_main_frame_id =
      primary_main_frame->GetGlobalId();
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.threat_type = threat_type;
  resource.render_process_id = primary_main_frame_id.child_id;
  resource.render_frame_token = primary_main_frame->GetFrameToken().value();
  resource.threat_source =
      g_browser_process->safe_browsing_service()
          ->database_manager()
          ->GetBrowseUrlThreatSource(
              safe_browsing::CheckBrowseUrlType::kHashDatabase);

  // Normally safebrowsing interstitial types which block the main page load
  // (SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_PHISHING, and
  // SB_THREAT_TYPE_URL_UNWANTED on main-frame loads) would expect there to be a
  // pending navigation when the SafeBrowsingBlockingPage is created. This demo
  // creates a SafeBrowsingBlockingPage but does not actually show a real
  // interstitial. Instead it extracts the html and displays it manually, so the
  // parts which depend on the NavigationEntry are not hit.
  return base::WrapUnique<TestSafeBrowsingBlockingPageQuiet>(
      TestSafeBrowsingBlockingPageQuiet::CreateBlockingPage(
          g_browser_process->safe_browsing_service()->ui_manager().get(),
          web_contents, request_url, resource, is_giant_webview));
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
std::unique_ptr<CaptivePortalBlockingPage> CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents) {
  bool is_wifi_connection = false;
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");
  // Not initialized to a default value, since non-empty wifi_ssid is
  // considered a wifi connection, even if is_wifi_connection is false.
  std::string wifi_ssid;

  std::string request_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "url",
                                 &request_url_param)) {
    if (GURL(request_url_param).is_valid())
      request_url = GURL(request_url_param);
  }
  std::string landing_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "landing_page",
                                 &landing_url_param)) {
    if (GURL(landing_url_param).is_valid())
      landing_url = GURL(landing_url_param);
  }
  std::string wifi_connection_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "is_wifi",
                                 &wifi_connection_param)) {
    is_wifi_connection = wifi_connection_param == "1";
  }
  std::string wifi_ssid_param;
  if (net::GetValueForKeyInQuery(web_contents->GetVisibleURL(), "wifi_name",
                                 &wifi_ssid_param)) {
    wifi_ssid = wifi_ssid_param;
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  std::unique_ptr<CaptivePortalBlockingPage> blocking_page =
      blocking_page_factory.CreateCaptivePortalBlockingPage(
          web_contents, request_url, landing_url, ssl_info,
          net::ERR_CERT_COMMON_NAME_INVALID);
  blocking_page->OverrideWifiInfoForTesting(is_wifi_connection, wifi_ssid);
  return blocking_page;
}
#endif

}  //  namespace

InterstitialUI::InterstitialUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                              std::make_unique<InterstitialHTMLSource>());
}

InterstitialUI::~InterstitialUI() = default;

// InterstitialHTMLSource

std::string InterstitialHTMLSource::GetMimeType(const GURL&) {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() {
  return chrome::kChromeUIInterstitialHost;
}

std::string InterstitialHTMLSource::GetContentSecurityPolicy(
    const network::mojom::CSPDirectiveName directive) {
  if (directive == network::mojom::CSPDirectiveName::ScriptSrc) {
    // 'unsafe-inline' is added to script-src.
    return "script-src chrome://resources 'self' 'unsafe-inline';";
  } else if (directive == network::mojom::CSPDirectiveName::StyleSrc) {
    return "style-src 'self' 'unsafe-inline';";
  } else if (directive == network::mojom::CSPDirectiveName::ImgSrc) {
    return "img-src data:;";
  }

  return content::URLDataSource::GetContentSecurityPolicy(directive);
}

void InterstitialHTMLSource::StartDataRequest(
    const GURL& request_url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug.com/40050262): Simplify usages of |path| since |request_url| is
  // available.
  const std::string path =
      content::URLDataSource::URLToRequestPath(request_url);
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents) {
    // When browser-side navigation is enabled, web_contents can be null if
    // the tab is closing. Nothing to do in this case.
    return;
  }
  std::unique_ptr<security_interstitials::SecurityInterstitialPage>
      interstitial_delegate;
  std::string html;
  // Using this form of the path so we can do exact matching, while ignoring the
  // query (everything after the ? character).
  GURL url =
      GURL(chrome::kChromeUIInterstitialURL).GetWithEmptyPath().Resolve(path);
  std::string path_without_query = url.path();
  if (path_without_query == "/ssl") {
    interstitial_delegate = CreateSslBlockingPage(web_contents);
  } else if (path_without_query == "/mitm-software-ssl") {
    interstitial_delegate = CreateMITMSoftwareBlockingPage(web_contents);
  } else if (path_without_query == "/blocked-interception") {
    interstitial_delegate = CreateBlockedInterceptionBlockingPage(web_contents);
  } else if (path_without_query == "/safebrowsing") {
    interstitial_delegate = CreateSafeBrowsingBlockingPage(web_contents);
  } else if (path_without_query == "/enterprise-block") {
    interstitial_delegate = CreateEnterpriseBlockPage(web_contents);
  } else if (path_without_query == "/enterprise-warn") {
    interstitial_delegate = CreateEnterpriseWarnPage(web_contents);
  } else if (path_without_query == "/clock") {
    interstitial_delegate = CreateBadClockBlockingPage(web_contents);
  } else if (path_without_query == "/lookalike") {
    interstitial_delegate = CreateLookalikeInterstitialPage(web_contents);
  } else if (path_without_query == "/managed-profile-required") {
    interstitial_delegate = CreateManagedProfileRequiredPage(web_contents);
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  } else if (path_without_query == "/captiveportal") {
    interstitial_delegate = CreateCaptivePortalBlockingPage(web_contents);
#endif
  } else if (path_without_query == "/insecure_form") {
    interstitial_delegate = CreateInsecureFormPage(web_contents);
  } else if (path_without_query == "/https_only") {
    interstitial_delegate = CreateHttpsOnlyModePage(web_contents);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  } else if (path_without_query == "/supervised-user-verify") {
    interstitial_delegate = CreateSupervisedUserVerificationPageForYouTube(
        web_contents, /*is_main_frame=*/true);
  } else if (path_without_query == "/supervised-user-verify-blocked-site") {
    interstitial_delegate = CreateSupervisedUserVerificationPageForBlockedSites(
        web_contents, /*is_main_frame=*/true);
  } else if (path_without_query == "/supervised-user-verify-subframe") {
    interstitial_delegate = CreateSupervisedUserVerificationPageForYouTube(
        web_contents, /*is_main_frame=*/false);
  } else if (path_without_query ==
             "/supervised-user-verify-blocked-site-subframe") {
    interstitial_delegate = CreateSupervisedUserVerificationPageForBlockedSites(
        web_contents, /*is_main_frame=*/false);
#endif
  }

  if (path_without_query == "/quietsafebrowsing") {
    std::unique_ptr<TestSafeBrowsingBlockingPageQuiet> blocking_page =
        CreateSafeBrowsingQuietBlockingPage(web_contents);
    html = blocking_page->GetHTML();
    interstitial_delegate = std::move(blocking_page);
  } else if (path_without_query == "/supervised-user-ask-parent") {
    html = GetSupervisedUserInterstitialHTML(path);
  } else if (interstitial_delegate.get()) {
    html = interstitial_delegate.get()->GetHTMLContents();
  } else {
    html = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_SECURITY_INTERSTITIAL_UI_HTML);
  }
  scoped_refptr<base::RefCountedString> html_bytes = new base::RefCountedString;
  html_bytes->as_string() = html;
  std::move(callback).Run(html_bytes.get());
}

std::string InterstitialHTMLSource::GetSupervisedUserInterstitialHTML(
    const std::string& path) {
  GURL url("https://localhost/" + path);

  bool allow_access_requests = true;
  std::string allow_access_requests_string;
  if (net::GetValueForKeyInQuery(url, "allow_access_requests",
                                 &allow_access_requests_string)) {
    allow_access_requests = allow_access_requests_string == "1";
  }

  std::string custodian = "Alice";
  net::GetValueForKeyInQuery(url, "custodian", &custodian);
  std::string second_custodian = "Bob";
  net::GetValueForKeyInQuery(url, "second_custodian", &second_custodian);
  std::string custodian_email = "alice.bloggs@gmail.com";
  net::GetValueForKeyInQuery(url, "custodian_email", &custodian_email);
  std::string second_custodian_email = "bob.bloggs@gmail.com";
  net::GetValueForKeyInQuery(url, "second_custodian_email",
                             &second_custodian_email);
  // The interstitial implementation provides a fallback image so no need to set
  // one here.
  std::string profile_image_url;
  net::GetValueForKeyInQuery(url, "profile_image_url", &profile_image_url);
  std::string profile_image_url2;
  net::GetValueForKeyInQuery(url, "profile_image_url2", &profile_image_url2);

  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::DEFAULT;
  std::string reason_string;
  if (net::GetValueForKeyInQuery(url, "reason", &reason_string)) {
    if (reason_string == "safe_sites") {
      reason = supervised_user::FilteringBehaviorReason::ASYNC_CHECKER;
    } else if (reason_string == "manual") {
      reason = supervised_user::FilteringBehaviorReason::MANUAL;
    }
  }

  bool show_banner = false;
  std::string show_banner_string;
  if (net::GetValueForKeyInQuery(url, "show_banner", &show_banner_string)) {
    show_banner = show_banner_string == "1";
  }

  return supervised_user::BuildErrorPageHtml(
      allow_access_requests, profile_image_url, profile_image_url2, custodian,
      custodian_email, second_custodian, second_custodian_email, reason,
      g_browser_process->GetApplicationLocale(), /*already_sent_request=*/false,
      /*is_main_frame=*/true, show_banner);
}

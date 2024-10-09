// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_handler.h"

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_helper.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/security_interstitials/content/captive_portal_helper_android.h"
#endif

BASE_FEATURE(kMITMSoftwareInterstitial,
             "MITMSoftwareInterstitial",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCaptivePortalInterstitial,
             "CaptivePortalInterstitial",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

BASE_FEATURE(kSSLCommonNameMismatchHandling,
             "SSLCommonNameMismatchHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default delay in milliseconds before displaying the SSL interstitial.
// This can be changed in tests.
// - If there is a name mismatch and a suggested URL available result arrives
//   during this time, the user is redirected to the suggester URL.
// - If a "captive portal detected" result arrives during this time,
//   a captive portal interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
const int64_t kInterstitialDelayInMilliseconds = 3000;

const char kHistogram[] = "interstitial.ssl_error_handler";

// Adds a message to console after navigation commits and then, deletes itself.
// Also deletes itself if the navigation is stopped.
class CommonNameMismatchRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommonNameMismatchRedirectObserver> {
 public:
  CommonNameMismatchRedirectObserver(
      const CommonNameMismatchRedirectObserver&) = delete;
  CommonNameMismatchRedirectObserver& operator=(
      const CommonNameMismatchRedirectObserver&) = delete;

  ~CommonNameMismatchRedirectObserver() override = default;

  static void AddToConsoleAfterNavigation(
      content::WebContents* web_contents,
      const std::string& request_url_hostname,
      const std::string& suggested_url_hostname) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new CommonNameMismatchRedirectObserver(
            web_contents, request_url_hostname, suggested_url_hostname)));
  }

 private:
  friend class content::WebContentsUserData<CommonNameMismatchRedirectObserver>;
  CommonNameMismatchRedirectObserver(content::WebContents* web_contents,
                                     const std::string& request_url_hostname,
                                     const std::string& suggested_url_hostname)
      : WebContentsObserver(web_contents),
        content::WebContentsUserData<CommonNameMismatchRedirectObserver>(
            *web_contents),
        request_url_hostname_(request_url_hostname),
        suggested_url_hostname_(suggested_url_hostname) {}

  // WebContentsObserver:
  void NavigationStopped() override {
    // Deletes |this|.
    GetWebContents().RemoveUserData(UserDataKey());
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& /* load_details */) override {
    GetWebContents().GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Redirecting navigation %s -> %s because the server presented a "
            "certificate valid for %s but not for %s. To disable such "
            "redirects launch Chrome with the following flag: "
            "--disable-features=SSLCommonNameMismatchHandling",
            request_url_hostname_.c_str(), suggested_url_hostname_.c_str(),
            suggested_url_hostname_.c_str(), request_url_hostname_.c_str()));
    GetWebContents().RemoveUserData(UserDataKey());
  }

  void WebContentsDestroyed() override {
    GetWebContents().RemoveUserData(UserDataKey());
  }

  const std::string request_url_hostname_;
  const std::string suggested_url_hostname_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommonNameMismatchRedirectObserver);

void RecordUMA(SSLErrorHandler::UMAEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kHistogram, event,
                            SSLErrorHandler::SSL_ERROR_HANDLER_EVENT_COUNT);
}

bool IsCaptivePortalInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalInterstitial);
}

bool IsMITMSoftwareInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kMITMSoftwareInterstitial);
}

bool IsSSLCommonNameMismatchHandlingEnabled() {
  return base::FeatureList::IsEnabled(kSSLCommonNameMismatchHandling);
}

// Configuration for SSLErrorHandler.
class ConfigSingleton {
 public:
  ConfigSingleton();

  base::TimeDelta interstitial_delay() const;
  SSLErrorHandler::TimerStartedCallback* timer_started_callback() const;
  base::Clock* clock() const;
  SSLErrorHandler::OnBlockingPageShownCallback on_blocking_page_shown_callback()
      const;

  bool IsKnownCaptivePortalCertificate(const net::SSLInfo& ssl_info);

  // Returns the name of a known MITM software provider that matches the
  // certificate passed in as the |cert| parameter. Returns empty string if
  // there is no match.
  const std::string MatchKnownMITMSoftware(
      const scoped_refptr<net::X509Certificate> cert);

  // Returns a DynamicInterstitialInfo that matches with |ssl_info|. If is no
  // match, return null.
  std::optional<DynamicInterstitialInfo> MatchDynamicInterstitial(
      const net::SSLInfo& ssl_info,
      bool is_overridable);

  // Testing methods:
  void ResetForTesting();
  void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  void SetTimerStartedCallbackForTesting(
      SSLErrorHandler::TimerStartedCallback* callback);
  void SetClockForTesting(base::Clock* clock);
  void SetReportNetworkConnectivityCallbackForTesting(
      base::OnceClosure callback);

  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          error_assistant_proto);

  void SetClientCallbackOnInterstitialsShown(
      SSLErrorHandler::OnBlockingPageShownCallback callback);

  int GetErrorAssistantProtoVersionIdForTesting() const;

  void SetOSReportsCaptivePortalForTesting(bool os_reports_captive_portal);
  bool DoesOSReportCaptivePortalForTesting() const;

  base::OnceClosure report_network_connectivity_callback() {
    return std::move(report_network_connectivity_callback_);
  }

 private:
  base::TimeDelta interstitial_delay_;

  // Callback to call when the interstitial timer is started. Used for
  // testing.
  raw_ptr<SSLErrorHandler::TimerStartedCallback, DanglingUntriaged>
      timer_started_callback_ = nullptr;

  // The clock to use when deciding which error type to display. Used for
  // testing.
  raw_ptr<base::Clock, DanglingUntriaged> testing_clock_ = nullptr;

  base::OnceClosure report_network_connectivity_callback_;

  SSLErrorHandler::OnBlockingPageShownCallback on_blocking_page_shown_callback_;

  enum OSCaptivePortalStatus {
    OS_CAPTIVE_PORTAL_STATUS_NOT_SET,
    OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL,
    OS_CAPTIVE_PORTAL_STATUS_NOT_BEHIND_PORTAL,
  };
  OSCaptivePortalStatus os_captive_portal_status_for_testing_;

  std::unique_ptr<SSLErrorAssistant> ssl_error_assistant_;
};

ConfigSingleton::ConfigSingleton()
    : interstitial_delay_(base::Milliseconds(kInterstitialDelayInMilliseconds)),
      os_captive_portal_status_for_testing_(OS_CAPTIVE_PORTAL_STATUS_NOT_SET),
      ssl_error_assistant_(std::make_unique<SSLErrorAssistant>()) {}

base::TimeDelta ConfigSingleton::interstitial_delay() const {
  return interstitial_delay_;
}

SSLErrorHandler::TimerStartedCallback* ConfigSingleton::timer_started_callback()
    const {
  return timer_started_callback_;
}

base::Clock* ConfigSingleton::clock() const {
  return testing_clock_;
}

SSLErrorHandler::OnBlockingPageShownCallback
ConfigSingleton::on_blocking_page_shown_callback() const {
  return on_blocking_page_shown_callback_;
}

void ConfigSingleton::ResetForTesting() {
  interstitial_delay_ = base::Milliseconds(kInterstitialDelayInMilliseconds);
  timer_started_callback_ = nullptr;
  on_blocking_page_shown_callback_ =
      SSLErrorHandler::OnBlockingPageShownCallback();
  testing_clock_ = nullptr;
  ssl_error_assistant_->ResetForTesting();
  os_captive_portal_status_for_testing_ = OS_CAPTIVE_PORTAL_STATUS_NOT_SET;
}

void ConfigSingleton::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  interstitial_delay_ = delay;
}

void ConfigSingleton::SetTimerStartedCallbackForTesting(
    SSLErrorHandler::TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  timer_started_callback_ = callback;
}

void ConfigSingleton::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ConfigSingleton::SetReportNetworkConnectivityCallbackForTesting(
    base::OnceClosure closure) {
  report_network_connectivity_callback_ = std::move(closure);
}

int ConfigSingleton::GetErrorAssistantProtoVersionIdForTesting() const {
  return ssl_error_assistant_->GetErrorAssistantProtoVersionIdForTesting();
}

void ConfigSingleton::SetOSReportsCaptivePortalForTesting(
    bool os_reports_captive_portal) {
  os_captive_portal_status_for_testing_ =
      os_reports_captive_portal ? OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL
                                : OS_CAPTIVE_PORTAL_STATUS_NOT_BEHIND_PORTAL;
}

bool ConfigSingleton::DoesOSReportCaptivePortalForTesting() const {
  return os_captive_portal_status_for_testing_ ==
         OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL;
}

void ConfigSingleton::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  ssl_error_assistant_->SetErrorAssistantProto(std::move(proto));
}

void ConfigSingleton::SetClientCallbackOnInterstitialsShown(
    SSLErrorHandler::OnBlockingPageShownCallback callback) {
  on_blocking_page_shown_callback_ = callback;
}

bool ConfigSingleton::IsKnownCaptivePortalCertificate(
    const net::SSLInfo& ssl_info) {
  return ssl_error_assistant_->IsKnownCaptivePortalCertificate(ssl_info);
}

const std::string ConfigSingleton::MatchKnownMITMSoftware(
    const scoped_refptr<net::X509Certificate> cert) {
  return ssl_error_assistant_->MatchKnownMITMSoftware(cert);
}

std::optional<DynamicInterstitialInfo>
ConfigSingleton::MatchDynamicInterstitial(const net::SSLInfo& ssl_info,
                                          bool is_overridable) {
  return ssl_error_assistant_->MatchDynamicInterstitial(ssl_info,
                                                        is_overridable);
}

class SSLErrorHandlerDelegateImpl : public SSLErrorHandler::Delegate {
 public:
  SSLErrorHandlerDelegateImpl(
      content::WebContents* web_contents,
      const net::SSLInfo& ssl_info,
      content::BrowserContext* const browser_context,
      int cert_error,
      int options_mask,
      const GURL& request_url,
      captive_portal::CaptivePortalService* captive_portal_service,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      SSLErrorHandler::OnBlockingPageShownCallback
          on_blocking_page_shown_callback,
      SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback)
      : web_contents_(web_contents),
        ssl_info_(ssl_info),
        browser_context_(browser_context),
        cert_error_(cert_error),
        options_mask_(options_mask),
        request_url_(request_url),
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
        captive_portal_service_(captive_portal_service),
#endif
        blocking_page_factory_(std::move(blocking_page_factory)),
        on_blocking_page_shown_callback_(on_blocking_page_shown_callback),
        blocking_page_ready_callback_(std::move(blocking_page_ready_callback)) {
    DCHECK(!blocking_page_ready_callback_.is_null());
  }
  ~SSLErrorHandlerDelegateImpl() override;

  // SSLErrorHandler::Delegate methods:
  void CheckForCaptivePortal() override;
  bool DoesOSReportCaptivePortal() override;
  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override;
  void CheckSuggestedUrl(
      const GURL& suggested_url,
      CommonNameMismatchHandler::CheckUrlCallback callback) override;
  void NavigateToSuggestedURL(const GURL& suggested_url) override;
  bool IsErrorOverridable() const override;
  void ShowCaptivePortalInterstitial(const GURL& landing_url) override;
  void ShowMITMSoftwareInterstitial(
      const std::string& mitm_software_name) override;
  void ShowSSLInterstitial(const GURL& support_url) override;
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override;
  void ShowBlockedInterceptionInterstitial() override;
  void ReportNetworkConnectivity(base::OnceClosure callback) override;
  bool HasBlockedInterception() const override;

 private:
  // Calls the |blocking_page_ready_callback_| if it's not null, else calls
  // Show() on the given interstitial.
  void OnBlockingPageReady(
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          interstitial_page);

  raw_ptr<content::WebContents> web_contents_;
  const net::SSLInfo ssl_info_;
  const raw_ptr<content::BrowserContext> browser_context_;
  const int cert_error_;
  const int options_mask_;
  const GURL request_url_;
  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  raw_ptr<captive_portal::CaptivePortalService> captive_portal_service_;
#endif
  std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory_;
  SSLErrorHandler::OnBlockingPageShownCallback on_blocking_page_shown_callback_;
  SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback_;
};

SSLErrorHandlerDelegateImpl::~SSLErrorHandlerDelegateImpl() {
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
}

void SSLErrorHandlerDelegateImpl::CheckForCaptivePortal() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service_->DetectCaptivePortal();
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

bool SSLErrorHandlerDelegateImpl::DoesOSReportCaptivePortal() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  return security_interstitials::IsBehindCaptivePortal();
#else
  return false;
#endif
}

bool SSLErrorHandlerDelegateImpl::GetSuggestedUrl(
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandlerDelegateImpl::CheckSuggestedUrl(
    const GURL& suggested_url,
    CommonNameMismatchHandler::CheckUrlCallback callback) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory(
      browser_context_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
  common_name_mismatch_handler_ = std::make_unique<CommonNameMismatchHandler>(
      request_url_, url_loader_factory);

  common_name_mismatch_handler_->CheckSuggestedUrl(suggested_url,
                                                   std::move(callback));
}

void SSLErrorHandlerDelegateImpl::NavigateToSuggestedURL(
    const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

bool SSLErrorHandlerDelegateImpl::IsErrorOverridable() const {
  return SSLBlockingPage::IsOverridable(options_mask_);
}

void SSLErrorHandlerDelegateImpl::ShowCaptivePortalInterstitial(
    const GURL& landing_url) {
  // Show captive portal blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateCaptivePortalBlockingPage(
      web_contents_, request_url_, landing_url, ssl_info_, cert_error_));
}

void SSLErrorHandlerDelegateImpl::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name) {
  // Show MITM software blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateMITMSoftwareBlockingPage(
      web_contents_, cert_error_, request_url_, ssl_info_, mitm_software_name));
}

void SSLErrorHandlerDelegateImpl::ShowSSLInterstitial(const GURL& support_url) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateSSLPage(
      web_contents_, cert_error_, ssl_info_, request_url_, options_mask_,
      base::Time::NowFromSystemTime(), support_url));
}

void SSLErrorHandlerDelegateImpl::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  // Show bad clock page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateBadClockBlockingPage(
      web_contents_, cert_error_, ssl_info_, request_url_, now, clock_state));
}

void SSLErrorHandlerDelegateImpl::ShowBlockedInterceptionInterstitial() {
  // Show interception blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(
      blocking_page_factory_->CreateBlockedInterceptionBlockingPage(
          web_contents_, cert_error_, request_url_, ssl_info_));
}

void SSLErrorHandlerDelegateImpl::ReportNetworkConnectivity(
    base::OnceClosure callback) {
#if BUILDFLAG(IS_ANDROID)
  security_interstitials::ReportNetworkConnectivity(
      base::android::AttachCurrentThread());
#else
// Nothing to do on other platforms.
#endif
  if (callback)
    std::move(callback).Run();
}

bool SSLErrorHandlerDelegateImpl::HasBlockedInterception() const {
  return ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
         ssl_errors::ErrorInfo::CERT_KNOWN_INTERCEPTION_BLOCKED;
}

void SSLErrorHandlerDelegateImpl::OnBlockingPageReady(
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        interstitial_page) {
  if (on_blocking_page_shown_callback_) {
    on_blocking_page_shown_callback_.Run(web_contents_.get(), request_url_,
                                         "SSL_ERROR", cert_error_);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(blocking_page_ready_callback_),
                                std::move(interstitial_page)));
}

}  // namespace

static base::LazyInstance<ConfigSingleton>::Leaky g_config =
    LAZY_INSTANCE_INITIALIZER;

void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback,
    network_time::NetworkTimeTracker* network_time_tracker,
    captive_portal::CaptivePortalService* captive_portal_service,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    bool user_can_proceed_past_interstitial /*=true*/) {
  DCHECK(!FromWebContents(web_contents));

  bool hard_override_disabled = !user_can_proceed_past_interstitial;

  int options_mask = security_interstitials::CalculateSSLErrorOptionsMask(
      cert_error, hard_override_disabled, ssl_info.is_fatal_cert_error);

  SSLErrorHandler* error_handler = new SSLErrorHandler(
      std::unique_ptr<SSLErrorHandler::Delegate>(
          new SSLErrorHandlerDelegateImpl(
              web_contents, ssl_info, web_contents->GetBrowserContext(),
              cert_error, options_mask, request_url, captive_portal_service,
              std::move(blocking_page_factory),
              g_config.Pointer()->on_blocking_page_shown_callback(),
              std::move(blocking_page_ready_callback))),
      web_contents, cert_error, ssl_info, network_time_tracker,
      captive_portal_service, request_url);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(error_handler));
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::ResetConfigForTesting() {
  g_config.Pointer()->ResetForTesting();
}

// static
void SSLErrorHandler::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  g_config.Pointer()->SetInterstitialDelayForTesting(delay);
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(
    TimerStartedCallback* callback) {
  g_config.Pointer()->SetTimerStartedCallbackForTesting(callback);
}

// static
void SSLErrorHandler::SetClockForTesting(base::Clock* testing_clock) {
  g_config.Pointer()->SetClockForTesting(testing_clock);
}

// static
void SSLErrorHandler::SetReportNetworkConnectivityCallbackForTesting(
    base::OnceClosure closure) {
  g_config.Pointer()->SetReportNetworkConnectivityCallbackForTesting(
      std::move(closure));
}

// static
std::string SSLErrorHandler::GetHistogramNameForTesting() {
  return kHistogram;
}

// static
int SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() {
  return g_config.Pointer()->GetErrorAssistantProtoVersionIdForTesting();
}

// static
void SSLErrorHandler::SetOSReportsCaptivePortalForTesting(
    bool os_reports_captive_portal) {
  g_config.Pointer()->SetOSReportsCaptivePortalForTesting(
      os_reports_captive_portal);
}

bool SSLErrorHandler::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

// static
void SSLErrorHandler::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto) {
  g_config.Pointer()->SetErrorAssistantProto(std::move(config_proto));
}

// static
void SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
    OnBlockingPageShownCallback callback) {
  g_config.Pointer()->SetClientCallbackOnInterstitialsShown(callback);
}

SSLErrorHandler::SSLErrorHandler(
    std::unique_ptr<Delegate> delegate,
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    network_time::NetworkTimeTracker* network_time_tracker,
    captive_portal::CaptivePortalService* captive_portal_service,
    const GURL& request_url)
    : content::WebContentsUserData<SSLErrorHandler>(*web_contents),
      content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      network_time_tracker_(network_time_tracker)
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
      ,
      captive_portal_service_(captive_portal_service)
#endif
{
}

SSLErrorHandler::~SSLErrorHandler() = default;

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  if (delegate_->HasBlockedInterception()) {
    return ShowBlockedInterceptionInterstitial();
  }

  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    HandleCertDateInvalidError();
    return;
  }

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      g_config.Pointer()->MatchDynamicInterstitial(
          ssl_info_, delegate_->IsErrorOverridable());
  if (dynamic_interstitial) {
    ShowDynamicInterstitial(dynamic_interstitial.value());
    return;
  }

  bool is_captive_portal_login_tab = false;
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Check whether this SSL Error is for a Captive Portal Login tab. If so, we
  // must not show another Captive Portal interstitial because doing so will
  // remove a user's ability to "ignore" the cert error to log onto the portal.
  captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents());
  if (captive_portal_tab_helper && captive_portal_tab_helper->IsLoginTab())
    is_captive_portal_login_tab = true;
#endif

  // Ideally, a captive portal interstitial should only be displayed if the only
  // SSL error is a name mismatch error. However, captive portal detector always
  // opens a new tab if it detects a portal ignoring the types of SSL errors. To
  // be consistent with captive portal detector, use the result of OS detection
  // without checking only_error_is_name_mismatch.
  if (IsCaptivePortalInterstitialEnabled() &&
      (g_config.Pointer()->DoesOSReportCaptivePortalForTesting() ||
       delegate_->DoesOSReportCaptivePortal())) {
    delegate_->ReportNetworkConnectivity(
        g_config.Pointer()->report_network_connectivity_callback());
    RecordUMA(OS_REPORTS_CAPTIVE_PORTAL);

    if (!is_captive_portal_login_tab) {
      ShowCaptivePortalInterstitial(GURL());
      return;
    }
  }

  const bool only_error_is_name_mismatch =
      IsOnlyCertError(net::CERT_STATUS_COMMON_NAME_INVALID);

  // Check known captive portal certificate list if the only error is
  // name-mismatch. If there are multiple errors, it indicates that the captive
  // portal landing page itself will have SSL errors, and so it's not a very
  // helpful place to direct the user to go.
  if (only_error_is_name_mismatch) {
    delegate_->ReportNetworkConnectivity(
        g_config.Pointer()->report_network_connectivity_callback());
  }

  // The MITM software interstitial is displayed if and only if:
  // - the error thrown is not overridable
  // - the only certificate error is CERT_STATUS_AUTHORITY_INVALID
  // - the certificate contains a string that indicates it was issued by a
  //   MITM software
  if (IsMITMSoftwareInterstitialEnabled() && !delegate_->IsErrorOverridable() &&
      IsOnlyCertError(net::CERT_STATUS_AUTHORITY_INVALID)) {
    const std::string found_mitm_software =
        g_config.Pointer()->MatchKnownMITMSoftware(ssl_info_.cert);
    if (!found_mitm_software.empty()) {
      ShowMITMSoftwareInterstitial(found_mitm_software);
      return;
    }
  }

  if (IsSSLCommonNameMismatchHandlingEnabled() &&
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      delegate_->IsErrorOverridable()) {
    std::vector<std::string> dns_names;
    ssl_info_.cert->GetSubjectAltName(&dns_names, nullptr);
    GURL suggested_url;
    if (!dns_names.empty() &&
        delegate_->GetSuggestedUrl(dns_names, &suggested_url)) {
      RecordUMA(WWW_MISMATCH_FOUND_IN_SAN);

      // Show the SSL interstitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
      // the only error. Need not check for captive portal in this case (See
      // the comment below). Also show the SSL interstitial if we're already
      // on a login tab launched by the Captive Portal interstitial.
      if (!only_error_is_name_mismatch || is_captive_portal_login_tab) {
        ShowSSLInterstitial();
        return;
      }
      delegate_->CheckSuggestedUrl(
          suggested_url,
          base::BindOnce(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                         weak_ptr_factory_.GetWeakPtr()));
      timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                   &SSLErrorHandler::ShowSSLInterstitial);

      if (g_config.Pointer()->timer_started_callback())
        g_config.Pointer()->timer_started_callback()->Run(web_contents());

      // Do not check for a captive portal in this case, because a captive
      // portal most likely cannot serve a valid certificate which passes the
      // similarity check.
      return;
    }
  }

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  subscription_ = captive_portal_service_->RegisterCallback(
      base::BindRepeating(&SSLErrorHandler::Observe, base::Unretained(this)));

  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info_);
  }

  if (IsCaptivePortalInterstitialEnabled() && !is_captive_portal_login_tab) {
    delegate_->CheckForCaptivePortal();
    timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                 &SSLErrorHandler::ShowSSLInterstitial);
    if (g_config.Pointer()->timer_started_callback())
      g_config.Pointer()->timer_started_callback()->Run(web_contents());
    return;
  }
#endif
  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE
                : SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowCaptivePortalInterstitial(landing_url);

  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this". It also destroys the timer.
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(SHOW_MITM_SOFTWARE_INTERSTITIAL);
  delegate_->ShowMITMSoftwareInterstitial(mitm_software_name);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowSSLInterstitial() {
  GURL support_url = (cert_error_ == net::ERR_CERT_SYMANTEC_LEGACY)
                         ? GURL(kSymantecSupportUrl)
                         : GURL();

  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowSSLInterstitial(support_url);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  RecordUMA(SHOW_BAD_CLOCK);
  delegate_->ShowBadClockInterstitial(now, clock_state);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowDynamicInterstitial(
    const DynamicInterstitialInfo dynamic_interstitial) {
  switch (dynamic_interstitial.interstitial_type) {
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_NONE:
      NOTREACHED_IN_MIGRATION();
      return;
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL:
      delegate_->ShowSSLInterstitial(dynamic_interstitial.support_url);
      return;
    case chrome_browser_ssl::DynamicInterstitial::
        INTERSTITIAL_PAGE_CAPTIVE_PORTAL:
      delegate_->ShowCaptivePortalInterstitial(GURL());
      return;
    case chrome_browser_ssl::DynamicInterstitial::
        INTERSTITIAL_PAGE_MITM_SOFTWARE:
      DCHECK(!dynamic_interstitial.mitm_software_name.empty());
      delegate_->ShowMITMSoftwareInterstitial(
          dynamic_interstitial.mitm_software_name);
      return;
  }
}

void SSLErrorHandler::ShowBlockedInterceptionInterstitial() {
  // Show a blocking page. The interstitial owns the blocking page.
  RecordUMA(SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL);
  delegate_->ShowBlockedInterceptionInterstitial();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    CommonNameMismatchHandler::SuggestedUrlCheckResult result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    CommonNameMismatchRedirectObserver::AddToConsoleAfterNavigation(
        web_contents(), request_url_.host(), suggested_url.host());
    delegate_->NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}

void SSLErrorHandler::Observe(
    const captive_portal::CaptivePortalService::Results& results) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  timer_.Stop();
  if (results.result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL)
    ShowCaptivePortalInterstitial(results.landing_url);
  else
    ShowSSLInterstitial();
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void SSLErrorHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Destroy the error handler on all new navigations. This ensures that the
  // handler is properly recreated when a hanging page is navigated to an SSL
  // error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
  // Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  delegate_.reset();
  // Deletes |this| and also destroys the timer.
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::HandleCertDateInvalidError() {
  const base::TimeTicks now = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(),
               base::BindOnce(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                              base::Unretained(this), now));
  // Try kicking off a time fetch to get an up-to-date estimate of the
  // true time. This will only have an effect if network time is
  // unavailable or if there is not already a query in progress.
  //
  // Pass a weak pointer as the callback; if the timer fires before the
  // fetch completes and shows an interstitial, this SSLErrorHandler
  // will be deleted.
  if (!network_time_tracker_->StartTimeFetch(
          base::BindOnce(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                         weak_ptr_factory_.GetWeakPtr(), now))) {
    HandleCertDateInvalidErrorImpl(now);
    return;
  }

  if (g_config.Pointer()->timer_started_callback())
    g_config.Pointer()->timer_started_callback()->Run(web_contents());
}

void SSLErrorHandler::HandleCertDateInvalidErrorImpl(
    base::TimeTicks started_handling_error) {
  timer_.Stop();
  base::Clock* testing_clock = g_config.Pointer()->clock();
  const base::Time now =
      testing_clock ? testing_clock->Now() : base::Time::NowFromSystemTime();

  ssl_errors::ClockState clock_state =
      ssl_errors::GetClockState(now, network_time_tracker_);
  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
      clock_state == ssl_errors::CLOCK_STATE_PAST) {
    ShowBadClockInterstitial(now, clock_state);
    return;  // |this| is deleted after showing the interstitial.
  }
  ShowSSLInterstitial();
}

// Returns true if |only_cert_error_expected| is the only error code present in
// the certificate. The parameter |only_cert_error_expected| is a
// net::CertStatus code representing the most serious error identified on the
// certificate. For example, this could be net::CERT_STATUS_COMMON_NAME_INVALID.
// This function is useful for rendering interstitials that are triggered by one
// specific error code only.
bool SSLErrorHandler::IsOnlyCertError(
    net::CertStatus only_cert_error_expected) const {
  const net::CertStatus other_errors =
      ssl_info_.cert_status ^ only_cert_error_expected;

  return cert_error_ ==
             net::MapCertStatusToNetError(only_cert_error_expected) &&
         !net::IsCertStatusError(other_errors);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SSLErrorHandler);

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_HANDLER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/security_interstitials/content/common_name_mismatch_handler.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

class SecurityBlockingPageFactory;
class CommonNameMismatchHandler;
struct DynamicInterstitialInfo;

namespace base {
class Clock;
class TimeDelta;
}  // namespace base

namespace content {
class WebContents;
}

namespace network_time {
class NetworkTimeTracker;
}

BASE_DECLARE_FEATURE(kMITMSoftwareInterstitial);
BASE_DECLARE_FEATURE(kCaptivePortalInterstitial);

// This class is responsible for deciding what type of interstitial to display
// for an SSL validation error and actually displaying it. The display of the
// interstitial might be delayed by a few seconds while trying to determine the
// cause of the error. During this window, the class will:
// - Check for a clock error
// - Check for a known captive portal certificate SPKI
// - Wait for a name-mismatch suggested URL
// - or Wait for a captive portal result to arrive.
//
// Based on the result of these checks, SSLErrorHandler will show a customized
// interstitial, redirect to a different suggested URL, or, if all else fails,
// show the normal SSL interstitial. When --committed--interstitials is enabled,
// it passes a constructed blocking page to the |blocking_page_ready_callback|
// that was given to HandleSSLError(), instead of showing the interstitial
// directly.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService which can only be accessed on the
// UI thread.
class SSLErrorHandler : public content::WebContentsUserData<SSLErrorHandler>,
                        public content::WebContentsObserver {
 public:
  typedef base::RepeatingCallback<void(content::WebContents*)>
      TimerStartedCallback;
  typedef base::OnceCallback<void(
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
      BlockingPageReadyCallback;

  // Callback that is optionally used to inform the client that a blocking page
  // has been shown in the specified WebContents for the specified URL with the
  // given error string and network error code.
  typedef base::RepeatingCallback<
      void(content::WebContents*, const GURL&, const std::string&, int)>
      OnBlockingPageShownCallback;

  SSLErrorHandler(const SSLErrorHandler&) = delete;
  SSLErrorHandler& operator=(const SSLErrorHandler&) = delete;

  ~SSLErrorHandler() override;

  // Events for UMA. Do not rename or remove values, add new values to the end.
  // Public for testing.
  // If you change this, change the values in CaptivePortalTest.java as well.
  enum UMAEvent {
    HANDLE_ALL = 0,
    SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE = 1,
    SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE = 2,
    SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE = 3,
    SHOW_SSL_INTERSTITIAL_OVERRIDABLE = 4,
    WWW_MISMATCH_FOUND = 5,  // Deprecated in M59 by WWW_MISMATCH_FOUND_IN_SAN.
    WWW_MISMATCH_URL_AVAILABLE = 6,
    WWW_MISMATCH_URL_NOT_AVAILABLE = 7,
    SHOW_BAD_CLOCK = 8,
    CAPTIVE_PORTAL_CERT_FOUND = 9,
    WWW_MISMATCH_FOUND_IN_SAN = 10,
    SHOW_MITM_SOFTWARE_INTERSTITIAL = 11,
    OS_REPORTS_CAPTIVE_PORTAL = 12,
    SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL = 13,
    SHOW_LEGACY_TLS_INTERSTITIAL = 14,  // Deprecated in M98.
    SSL_ERROR_HANDLER_EVENT_COUNT
  };

  // This delegate allows unit tests to provide their own Chrome specific
  // actions.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void CheckForCaptivePortal() = 0;
    virtual bool DoesOSReportCaptivePortal() = 0;
    virtual bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                                 GURL* suggested_url) const = 0;
    virtual void CheckSuggestedUrl(
        const GURL& suggested_url,
        CommonNameMismatchHandler::CheckUrlCallback callback) = 0;
    virtual void NavigateToSuggestedURL(const GURL& suggested_url) = 0;
    virtual bool IsErrorOverridable() const = 0;
    virtual void ShowCaptivePortalInterstitial(const GURL& landing_url) = 0;
    virtual void ShowMITMSoftwareInterstitial(
        const std::string& mitm_software_name) = 0;
    virtual void ShowSSLInterstitial(const GURL& support_url) = 0;
    virtual void ShowBadClockInterstitial(
        const base::Time& now,
        ssl_errors::ClockState clock_state) = 0;
    virtual void ShowBlockedInterceptionInterstitial() = 0;
    virtual void ReportNetworkConnectivity(base::OnceClosure callback) = 0;
    virtual bool HasBlockedInterception() const = 0;
  };

  // Entry point for the class. Most parameters are the same as
  // SSLBlockingPage constructor.
  // Extra parameters:
  // |blocking_page_ready_callback| is intended for committed interstitials. If
  // |blocking_page_ready_callback| is null, this function will create a
  // blocking page and call Show() on it. Otherwise, this function creates an
  // interstitial and passes it to |blocking_page_ready_callback|.
  // |blocking_page_ready_callback| is guaranteed not to be called
  // synchronously.
  // |user_can_proceed_past_interstitial| can be given a value of false to
  // change the default behavior of giving users the option to proceed past
  // SSL error interstitials.
  // |blocking_page_factory| will be used to create the interstitial pages
  // shown.
  static void HandleSSLError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      BlockingPageReadyCallback blocking_page_ready_callback,
      network_time::NetworkTimeTracker* network_time_tracker,
      captive_portal::CaptivePortalService* captive_portal_service,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      bool user_can_proceed_past_interstitial = true);

  // Sets the binary proto for SSL error assistant. The binary proto
  // can be downloaded by the component updater, or set by tests.
  static void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          config_proto);

  // Invoke this method to have |callback| called whenever an interstitial is
  // shown in an SSLErrorHandler instance.
  static void SetClientCallbackOnInterstitialsShown(
      OnBlockingPageShownCallback callback);

  // Testing methods.
  static void ResetConfigForTesting();
  static void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  // The callback pointer must remain valid for the duration of error handling.
  static void SetInterstitialTimerStartedCallbackForTesting(
      TimerStartedCallback* callback);
  static void SetClockForTesting(base::Clock* testing_clock);
  static void SetReportNetworkConnectivityCallbackForTesting(
      base::OnceClosure callback);
  static void SetEnterpriseManagedForTesting(bool enterprise_managed);
  static bool IsEnterpriseManagedFlagSetForTesting();
  static std::string GetHistogramNameForTesting();
  static int GetErrorAssistantProtoVersionIdForTesting();
  static void SetOSReportsCaptivePortalForTesting(
      bool os_reports_captive_portal);
  bool IsTimerRunningForTesting() const;

 protected:
  SSLErrorHandler(std::unique_ptr<Delegate> delegate,
                  content::WebContents* web_contents,
                  int cert_error,
                  const net::SSLInfo& ssl_info,
                  network_time::NetworkTimeTracker* network_time_tracker,
                  captive_portal::CaptivePortalService* captive_portal_service,
                  const GURL& request_url);

  // Called when an SSL cert error is encountered. Triggers a captive portal
  // check and fires a one shot timer to wait for a "captive portal detected"
  // result to arrive. Protected for testing.
  void StartHandlingError();

 private:
  friend class content::WebContentsUserData<SSLErrorHandler>;
  FRIEND_TEST_ALL_PREFIXES(SSLErrorHandlerTest, CalculateOptionsMask);
  FRIEND_TEST_ALL_PREFIXES(SSLErrorHandlerTest,
                           NonPrimaryMainframeShouldNotAffectSSLErrorHandler);
  FRIEND_TEST_ALL_PREFIXES(SSLErrorHandlerNameMismatchTest,
                           ShouldShowCustomInterstitialOnCaptivePortalResult);
  FRIEND_TEST_ALL_PREFIXES(SSLErrorHandlerNameMismatchTest,
                           ShouldShowSSLInterstitialOnNoCaptivePortalResult);

  void ShowCaptivePortalInterstitial(const GURL& landing_url);
  void ShowMITMSoftwareInterstitial(const std::string& mitm_software_name);
  void ShowSSLInterstitial();
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state);
  void ShowDynamicInterstitial(const DynamicInterstitialInfo interstitial);
  void ShowBlockedInterceptionInterstitial();

  // Gets the result of whether the suggested URL is valid. Displays
  // common name mismatch interstitial or ssl interstitial accordingly.
  void CommonNameMismatchHandlerCallback(
      CommonNameMismatchHandler::SuggestedUrlCheckResult result,
      const GURL& suggested_url);

  // Callback invoked with the result of a query for captive portal status.
  void Observe(const captive_portal::CaptivePortalService::Results& results);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // content::WebContentsObserver:
  void NavigationStopped() override;

  // Deletes the SSLErrorHandler. This method is called when the page
  // load stops or when there is a new navigation.
  void DeleteSSLErrorHandler();

  void HandleCertDateInvalidError();
  void HandleCertDateInvalidErrorImpl(base::TimeTicks started_handling_error);

  bool IsOnlyCertError(net::CertStatus only_cert_error_expected) const;

  std::unique_ptr<Delegate> delegate_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const GURL request_url_;
  raw_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  // The below field is unused if captive portal detection is not enabled,
  // which causes a compiler error.
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  raw_ptr<captive_portal::CaptivePortalService, DanglingUntriaged>
      captive_portal_service_;
#endif

  base::CallbackListSubscription subscription_;

  base::OneShotTimer timer_;

  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;

  base::WeakPtrFactory<SSLErrorHandler> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_HANDLER_H_

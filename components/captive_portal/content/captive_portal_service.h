// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_SERVICE_H_
#define COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

class Browser;
class CaptivePortalBrowserTest;
class HttpsUpgradesBrowserTest;

namespace captive_portal {

class CaptivePortalServiceTest;

// Service that checks for captive portals when queried and sends updates to
// its registered consumers when results are obtained.
//
// Captive portal checks are rate-limited.  The CaptivePortalService may only
// be accessed on the UI thread.
// Design doc:
// https://docs.google.com/document/d/1k-gP2sswzYNvryu9NcgN7q5XrsMlUdlUdoW9WRaEmfM/edit
class CaptivePortalService : public KeyedService {
 public:
  enum TestingState {
    NOT_TESTING,
    DISABLED_FOR_TESTING,        // The service is always disabled.
    IGNORE_REQUESTS_FOR_TESTING  // Disables actual portal checks.
  };

  // The details sent to consumers on a query completing.
  struct Results {
    // The result of the second most recent captive portal check.
    CaptivePortalResult previous_result;
    // The result of the most recent captive portal check.
    CaptivePortalResult result;
    // Landing url of the captive portal check ping. If behind a captive portal,
    // this points to the login page.
    GURL landing_url;
  };

  CaptivePortalService(
      content::BrowserContext* browser_context,
      PrefService* pref_service,
      const base::TickClock* clock_for_testing = nullptr,
      network::mojom::URLLoaderFactory* loader_factory_for_testing = nullptr);

  CaptivePortalService(const CaptivePortalService&) = delete;
  CaptivePortalService& operator=(const CaptivePortalService&) = delete;

  ~CaptivePortalService() override;

  // Triggers a check for a captive portal.  If there's already a check in
  // progress, does nothing.  Throttles the rate at which requests are sent.
  // Always sends the result notification asynchronously.
  void DetectCaptivePortal();

  base::CallbackListSubscription RegisterCallback(
      const base::RepeatingCallback<void(const Results&)>& cb) {
    return callback_list_.Add(cb);
  }

  // Returns the URL used for captive portal testing.  When a captive portal is
  // detected, this URL will take us to the captive portal landing page.
  const GURL& test_url() const { return test_url_; }

  // Result of the most recent captive portal check.
  CaptivePortalResult last_detection_result() const {
    return last_detection_result_;
  }

  // Whether or not the CaptivePortalService is enabled.  When disabled, all
  // checks return INTERNET_CONNECTED.
  bool enabled() const { return enabled_; }

  // Used to disable captive portal detection so it doesn't interfere with
  // tests.  Should be called before the service is created.
  static void set_state_for_testing(TestingState testing_state) {
    testing_state_ = testing_state;
  }
  static TestingState get_state_for_testing() { return testing_state_; }

 private:
  friend class CaptivePortalServiceTest;
  friend class ::CaptivePortalBrowserTest;
  friend class ::HttpsUpgradesBrowserTest;

  enum State {
    // No check is running or pending.
    STATE_IDLE,
    // The timer to check for a captive portal is running.
    STATE_TIMER_RUNNING,
    // There's an outstanding HTTP request to check for a captive portal.
    STATE_CHECKING_FOR_PORTAL,
  };

  // Contains all the information about the minimum time allowed between two
  // consecutive captive portal checks.
  struct RecheckPolicy {
    // Constructor initializes all values to defaults.
    RecheckPolicy();

    // The minimum amount of time between two captive portal checks, when the
    // last check found no captive portal.
    int initial_backoff_no_portal_ms;

    // The minimum amount of time between two captive portal checks, when the
    // last check found a captive portal.  This is expected to be less than
    // |initial_backoff_no_portal_ms|.  Also used when the service is disabled.
    int initial_backoff_portal_ms;

    net::BackoffEntry::Policy backoff_policy;
  };

  // Initiates a captive portal check, without any throttling.  If the service
  // is disabled, just acts like there's an Internet connection.
  void DetectCaptivePortalInternal();

  // Called by CaptivePortalDetector when detection completes.
  void OnPortalDetectionCompleted(
      const CaptivePortalDetector::Results& results);

  // KeyedService:
  void Shutdown() override;

  // Called when a captive portal check completes.  Passes the result to all
  // observers.
  void OnResult(CaptivePortalResult result, const GURL& landing_url);

  // Updates BackoffEntry::Policy and creates a new BackoffEntry, which
  // resets the count used for throttling.
  void ResetBackoffEntry(CaptivePortalResult result);

  // Updates |enabled_| based on command line flags and Profile preferences,
  // and sets |state_| to STATE_NONE if it's false.
  // TODO(mmenke): Figure out on which platforms, if any, should not use
  //               automatic captive portal detection.  Currently it's enabled
  //               on all platforms, though this code is not compiled on
  //               Android, since it lacks the Browser class.
  void UpdateEnabledState();

  base::TimeTicks GetCurrentTimeTicks() const;

  bool DetectionInProgress() const;

  // Returns true if the timer to try and detect a captive portal is running.
  bool TimerRunning() const;

  State state() const { return state_; }

  RecheckPolicy& recheck_policy() { return recheck_policy_; }

  void set_test_url(const GURL& test_url) { test_url_ = test_url; }

  // The BrowserContext that owns this CaptivePortalService.
  const raw_ptr<content::BrowserContext> browser_context_;

  State state_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Detector for checking active network for a portal state.
  std::unique_ptr<CaptivePortalDetector> captive_portal_detector_;

  // True if the service is enabled.  When not enabled, all checks will return
  // RESULT_INTERNET_CONNECTED.
  bool enabled_;

  // The result of the most recent captive portal check.
  CaptivePortalResult last_detection_result_;

  base::RepeatingCallbackList<void(const Results&)> callback_list_;

  // Time the last captive portal check completed.
  base::TimeTicks last_check_time_;

  // Policy for throttling portal checks.
  RecheckPolicy recheck_policy_;

  // Implements behavior needed by |recheck_policy_|.  Whenever there's a new
  // CaptivePortalResult, BackoffEntry::Policy is updated and
  // |backoff_entry_| is recreated.  Each check that returns the same result
  // is considered a "failure", to trigger throttling.
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL test_url_;

  // The pref member for whether navigation errors should be resolved with a web
  // service.  Actually called "alternate_error_pages", since it's also used for
  // the Link Doctor.
  BooleanPrefMember resolve_errors_with_web_service_;

  base::OneShotTimer check_captive_portal_timer_;

  static TestingState testing_state_;

  // Test tick clock used by unit tests.
  const raw_ptr<const base::TickClock> tick_clock_for_testing_;  // Not owned.
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_SERVICE_H_

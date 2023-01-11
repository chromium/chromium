// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_service.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace captive_portal {

CaptivePortalService::TestingState CaptivePortalService::testing_state_ =
    NOT_TESTING;

CaptivePortalService::RecheckPolicy::RecheckPolicy()
    : initial_backoff_no_portal_ms(600 * 1000),
      initial_backoff_portal_ms(20 * 1000) {
  // Receiving a new Result is considered a success.  All subsequent requests
  // that get the same Result are considered "failures", so a value of N
  // means exponential backoff starts after getting a result N + 2 times:
  // +1 for the initial success, and +1 because N failures are ignored.
  //
  // A value of 6 means to start backoff on the 7th failure, which is the 8th
  // time the same result is received.
  backoff_policy.num_errors_to_ignore = 6;

  // It doesn't matter what this is initialized to.  It will be overwritten
  // after the first captive portal detection request.
  backoff_policy.initial_delay_ms = initial_backoff_no_portal_ms;

  backoff_policy.multiply_factor = 2.0;
  backoff_policy.jitter_factor = 0.3;
  backoff_policy.maximum_backoff_ms = 2 * 60 * 1000;

  // -1 means the entry never expires.  This doesn't really matter, as the
  // service never checks for its expiration.
  backoff_policy.entry_lifetime_ms = -1;

  backoff_policy.always_use_initial_delay = true;
}

CaptivePortalService::CaptivePortalService(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    const base::TickClock* clock_for_testing,
    network::mojom::URLLoaderFactory* loader_factory_for_testing)
    : browser_context_(browser_context),
      state_(STATE_IDLE),
      enabled_(false),
      last_detection_result_(RESULT_INTERNET_CONNECTED),
      test_url_(CaptivePortalDetector::kDefaultURL),
      tick_clock_for_testing_(clock_for_testing) {
  network::mojom::URLLoaderFactory* loader_factory;
  if (loader_factory_for_testing) {
    loader_factory = loader_factory_for_testing;
  } else {
    shared_url_loader_factory_ = browser_context->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess();
    loader_factory = shared_url_loader_factory_.get();
  }
  captive_portal_detector_ =
      std::make_unique<CaptivePortalDetector>(loader_factory);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The order matters here:
  // |resolve_errors_with_web_service_| must be initialized and |backoff_entry_|
  // created before the call to UpdateEnabledState.
  resolve_errors_with_web_service_.Init(
      embedder_support::kAlternateErrorPagesEnabled, pref_service,
      base::BindRepeating(&CaptivePortalService::UpdateEnabledState,
                          base::Unretained(this)));
  ResetBackoffEntry(last_detection_result_);

  UpdateEnabledState();
}

CaptivePortalService::~CaptivePortalService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CaptivePortalService::DetectCaptivePortal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Detection should be disabled only in tests.
  if (testing_state_ == IGNORE_REQUESTS_FOR_TESTING)
    return;

  // If a request is pending or running, do nothing.
  if (state_ == STATE_CHECKING_FOR_PORTAL || state_ == STATE_TIMER_RUNNING)
    return;

  base::TimeDelta time_until_next_check = backoff_entry_->GetTimeUntilRelease();

  // Start asynchronously.
  state_ = STATE_TIMER_RUNNING;
  check_captive_portal_timer_.Start(
      FROM_HERE, time_until_next_check,
      base::BindOnce(&CaptivePortalService::DetectCaptivePortalInternal,
                     base::Unretained(this)));
}

void CaptivePortalService::DetectCaptivePortalInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(state_ == STATE_TIMER_RUNNING || state_ == STATE_IDLE);
  DCHECK(!TimerRunning());

  state_ = STATE_CHECKING_FOR_PORTAL;

  // When not enabled, just claim there's an Internet connection.
  if (!enabled_) {
    // Count this as a success, so the backoff entry won't apply exponential
    // backoff, but will apply the standard delay.
    backoff_entry_->InformOfRequest(true);
    OnResult(RESULT_INTERNET_CONNECTED, GURL());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("captive_portal_service", R"(
        semantics {
          sender: "Captive Portal Service"
          description:
            "Checks if the system is behind a captive portal. To do so, makes"
            "an unlogged, dataless connection to a Google server and checks"
            "the response."
          trigger:
            "It is triggered on multiple cases: It is run on certain SSL "
            "errors (ERR_CONNECTION_TIMED_OUT, ERR_SSL_PROTOCOL_ERROR, and all "
            "SSL interstitials)."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable/disable this feature by toggling 'Use a web "
            "service to resolve network errors' in Chromium settings under "
            "Privacy. This feature is enabled by default."
          chrome_policy {
            AlternateErrorPagesEnabled {
              policy_options {mode: MANDATORY}
              AlternateErrorPagesEnabled: false
            }
          }
        })");

  captive_portal_detector_->DetectCaptivePortal(
      test_url_,
      base::BindOnce(&CaptivePortalService::OnPortalDetectionCompleted,
                     base::Unretained(this)),
      traffic_annotation);
}

void CaptivePortalService::OnPortalDetectionCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(STATE_CHECKING_FOR_PORTAL, state_);
  DCHECK(!TimerRunning());
  DCHECK(enabled_);

  CaptivePortalResult result = results.result;
  const base::TimeDelta& retry_after_delta = results.retry_after_delta;
  base::TimeTicks now = GetCurrentTimeTicks();

  if (last_check_time_.is_null() || result != last_detection_result_) {
    // Reset the backoff entry both to update the default time and clear
    // previous failures.
    ResetBackoffEntry(result);

    backoff_entry_->SetCustomReleaseTime(now + retry_after_delta);
    // The BackoffEntry is not informed of this request, so there's no delay
    // before the next request.  This allows for faster login when a captive
    // portal is first detected.  It can also help when moving between captive
    // portals.
  } else {
    // Requests that have the same Result as the last one are considered
    // "failures", to trigger backoff.
    backoff_entry_->SetCustomReleaseTime(now + retry_after_delta);
    backoff_entry_->InformOfRequest(false);
  }

  last_check_time_ = now;

  OnResult(result, results.landing_url);
}

void CaptivePortalService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CaptivePortalService::OnResult(CaptivePortalResult result,
                                    const GURL& landing_url) {
  DCHECK_EQ(STATE_CHECKING_FOR_PORTAL, state_);
  state_ = STATE_IDLE;

  Results results;
  results.previous_result = last_detection_result_;
  results.result = result;
  results.landing_url = landing_url;
  last_detection_result_ = result;

  callback_list_.Notify(results);
}

void CaptivePortalService::ResetBackoffEntry(CaptivePortalResult result) {
  if (!enabled_ || result == RESULT_BEHIND_CAPTIVE_PORTAL) {
    // Use the shorter time when the captive portal service is not enabled, or
    // behind a captive portal.
    recheck_policy_.backoff_policy.initial_delay_ms =
        recheck_policy_.initial_backoff_portal_ms;
  } else {
    recheck_policy_.backoff_policy.initial_delay_ms =
        recheck_policy_.initial_backoff_no_portal_ms;
  }

  backoff_entry_ = std::make_unique<net::BackoffEntry>(
      &recheck_policy().backoff_policy, tick_clock_for_testing_);
}

void CaptivePortalService::UpdateEnabledState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool enabled_before = enabled_;
  enabled_ = testing_state_ != DISABLED_FOR_TESTING &&
             resolve_errors_with_web_service_.GetValue();

  if (enabled_before == enabled_)
    return;

  last_check_time_ = base::TimeTicks();
  ResetBackoffEntry(last_detection_result_);

  if (state_ == STATE_CHECKING_FOR_PORTAL || state_ == STATE_TIMER_RUNNING) {
    // If a captive portal check was running or pending, cancel check
    // and the timer.
    check_captive_portal_timer_.Stop();
    captive_portal_detector_->Cancel();
    state_ = STATE_IDLE;

    // Since a captive portal request was queued or running, something may be
    // expecting to receive a captive portal result.
    DetectCaptivePortal();
  }
}

base::TimeTicks CaptivePortalService::GetCurrentTimeTicks() const {
  if (tick_clock_for_testing_)
    return tick_clock_for_testing_->NowTicks();
  return base::TimeTicks::Now();
}

bool CaptivePortalService::DetectionInProgress() const {
  return state_ == STATE_CHECKING_FOR_PORTAL;
}

bool CaptivePortalService::TimerRunning() const {
  return check_captive_portal_timer_.IsRunning();
}

}  // namespace captive_portal

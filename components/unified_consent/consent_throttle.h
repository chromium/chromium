// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_CONSENT_THROTTLE_H_
#define COMPONENTS_UNIFIED_CONSENT_CONSENT_THROTTLE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace unified_consent {

// This class throttles consent requests when the underlying consent helper is
// still initializing. It operates in two modes:
//
//  1) If the underlying consent helper is still initializing:
//      - It holds requests in a queue for up to as long as the timeout.
//      - If the consent helper finishes initializing in that time, it will call
//        the request callback with the definitive answer.
//      - If the consent helper doesn't finish initializing in that time, it
//        will call the callback with a False answer.
//
//  2) If the underlying consent helper is already initialized, this class is
//     just a passthrough, and the underlying consent helper and request
//     callback are both called synchronously.
class ConsentThrottle : public UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  using RequestCallback = base::OnceCallback<void(bool)>;

  explicit ConsentThrottle(
      std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper,
      base::TimeDelta timeout = base::Seconds(5));
  ConsentThrottle(const ConsentThrottle&) = delete;
  ConsentThrottle& operator=(const ConsentThrottle&) = delete;
  virtual ~ConsentThrottle();

  // UrlKeyedDataCollectionConsentHelper::Observer:
  void OnUrlKeyedDataCollectionConsentStateChanged(
      UrlKeyedDataCollectionConsentHelper* consent_helper) override;

  // If the underlying consent helper is initialized already, this method calls
  // `callback` synchronously with the result. If not, it will hold the request
  // up until the timeout for the consent helper to initialize.
  void EnqueueRequest(RequestCallback callback);

 private:
  // This is run periodically to sweep away old queued requests.
  void OnTimeoutExpired();

  // The underlying consent helper.
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper_;
  // The minimum timeout duration to hold the request for.
  base::TimeDelta timeout_;

  // Observe the consent helper to handle to state changes immediately.
  base::ScopedObservation<UrlKeyedDataCollectionConsentHelper, ConsentThrottle>
      handle_observation_{this};

  // Requests waiting for the consent throttle to initialize. Requests are
  // stored in the queue in order of their arrival.
  std::vector<RequestCallback> enqueued_request_callbacks_;

  // Timer used to periodically process unanswered enqueued requests, and
  // respond to them in the negative.
  base::OneShotTimer request_processing_timer_;
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_CONSENT_THROTTLE_H_

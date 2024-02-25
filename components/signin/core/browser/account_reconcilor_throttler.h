// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_THROTTLER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_THROTTLER_H_

#include <optional>

#include "base/time/time.h"
#include "components/signin/public/base/multilogin_parameters.h"

// Used for UMA logging, do not remove or reorder values.
enum class MultiloginRequestType {
  kPreserveCookieAccountsOrder = 0,
  kUpdateCookieAccountsOrder = 1,
  kLogoutAllAccounts = 2,
  kMaxValue = kLogoutAllAccounts,
};

// Helper class to avoid the account reconcilor from getting into a loop,
// repeating the same request and generating a spike in the number of requests.
class AccountReconcilorThrottler {
 public:
  AccountReconcilorThrottler();
  ~AccountReconcilorThrottler();

  AccountReconcilorThrottler(const AccountReconcilorThrottler&) = delete;
  AccountReconcilorThrottler& operator=(const AccountReconcilorThrottler&) =
      delete;

  // The multilogin operation should be sent or blocked based on the result of
  // this function. It returns true if not all the |available_requests_ >= 1.0|
  // has been consumed, and consumes one of the available requests.
  bool TryMultiloginOperation(const signin::MultiloginParameters& params);

  // Any request passed to |IsMultiloginOperationAllowed()| after |Reset()| is
  // called is considered as a new different request.
  // |available_requests_bucket_| is reset to its max allowance.
  void Reset();

  // Max bucket size. The throttler tolerates up to 30 successive identical
  // requests before throttling.
  static constexpr float kMaxAllowedRequestsPerBucket = 30.0f;

  // Requests bucket is refilled with the rate of 0.5 per minute. If all the
  // available requests have been consumed, the reconcilor will need to wait for
  // 2 minutes from the last request to perform another identical request.
  static constexpr float kRefillRequestsBucketRatePerMinute = 0.5f;

 private:
  bool IsDifferentRequest(const signin::MultiloginParameters& params) const;
  void RefillAllowedRequests();
  void RecordMultiloginOperation(const signin::MultiloginParameters& params);
  void RecordAndResetNumberOfRejectedRequests();

  // Reset for every new request with different parameters.
  float available_requests_bucket_;
  base::TimeTicks last_refill_time_stamp_;
  std::optional<signin::MultiloginParameters> last_request_params_;
  size_t consecutive_rejected_requests_ = 0;
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_THROTTLER_H_

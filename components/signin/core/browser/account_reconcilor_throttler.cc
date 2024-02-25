// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor_throttler.h"

#include "base/metrics/histogram_functions.h"

namespace {
MultiloginRequestType GetMultiloginRequestType(
    const signin::MultiloginParameters& params) {
  if (params.mode ==
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER) {
    return MultiloginRequestType::kPreserveCookieAccountsOrder;
  }

  // Update mode.
  DCHECK_EQ(params.mode,
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  if (params.accounts_to_send.size())
    return MultiloginRequestType::kUpdateCookieAccountsOrder;

  // Accounts to send is empty.
  return MultiloginRequestType::kLogoutAllAccounts;
}
}  // namespace

// static
constexpr float AccountReconcilorThrottler::kMaxAllowedRequestsPerBucket;
// static
constexpr float AccountReconcilorThrottler::kRefillRequestsBucketRatePerMinute;

AccountReconcilorThrottler::AccountReconcilorThrottler() {
  Reset();
}

AccountReconcilorThrottler::~AccountReconcilorThrottler() {
  RecordAndResetNumberOfRejectedRequests();
}

void AccountReconcilorThrottler::Reset() {
  // Needed for the case when the reconcilor is a no op and calls reset.
  RecordAndResetNumberOfRejectedRequests();
  last_request_params_ = std::nullopt;
  available_requests_bucket_ = kMaxAllowedRequestsPerBucket;
  last_refill_time_stamp_ = base::TimeTicks::Now();
}

bool AccountReconcilorThrottler::IsDifferentRequest(
    const signin::MultiloginParameters& params) const {
  return !last_request_params_.has_value() ||
         params != last_request_params_.value();
}

bool AccountReconcilorThrottler::TryMultiloginOperation(
    const signin::MultiloginParameters& params) {
  if (IsDifferentRequest(params))
    Reset();

  RefillAllowedRequests();
  if (available_requests_bucket_ < 1.0) {
    ++consecutive_rejected_requests_;
    return false;
  }

  RecordMultiloginOperation(params);
  RecordAndResetNumberOfRejectedRequests();
  return true;
}

void AccountReconcilorThrottler::RecordMultiloginOperation(
    const signin::MultiloginParameters& params) {
  DCHECK_GE(available_requests_bucket_, 1.0f);
  last_request_params_ = params;
  available_requests_bucket_ -= 1.0f;
}

void AccountReconcilorThrottler::RefillAllowedRequests() {
  float refill_requests =
      (base::TimeTicks::Now() - last_refill_time_stamp_).InSecondsF() / 60.0 *
      kRefillRequestsBucketRatePerMinute;

  available_requests_bucket_ =
      std::min(available_requests_bucket_ + refill_requests,
               kMaxAllowedRequestsPerBucket);
  last_refill_time_stamp_ = base::TimeTicks::Now();
}

void AccountReconcilorThrottler::RecordAndResetNumberOfRejectedRequests() {
  if (!consecutive_rejected_requests_)
    return;

  DCHECK(last_request_params_.has_value());
  switch (GetMultiloginRequestType(last_request_params_.value())) {
    case MultiloginRequestType::kPreserveCookieAccountsOrder:
      base::UmaHistogramCounts1000(
          "Signin.Reconciler.RejectedRequestsDueToThrottler.Preserve",
          consecutive_rejected_requests_);
      break;
    case MultiloginRequestType::kUpdateCookieAccountsOrder:
      base::UmaHistogramCounts1000(
          "Signin.Reconciler.RejectedRequestsDueToThrottler.Update",
          consecutive_rejected_requests_);
      break;
    case MultiloginRequestType::kLogoutAllAccounts:
      base::UmaHistogramCounts1000(
          "Signin.Reconciler.RejectedRequestsDueToThrottler.LogoutAll",
          consecutive_rejected_requests_);
      break;
  }

  consecutive_rejected_requests_ = 0;
}

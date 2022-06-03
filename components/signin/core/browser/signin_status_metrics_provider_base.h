// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_

#include "components/metrics/metrics_provider.h"

// The base class for collecting the sign-in status of all opened profiles
// during one UMA session and recording the status in a histogram before the UMA
// log is uploaded.
class SigninStatusMetricsProviderBase : public metrics::MetricsProvider {
 public:
  SigninStatusMetricsProviderBase();

  SigninStatusMetricsProviderBase(const SigninStatusMetricsProviderBase&) =
      delete;
  SigninStatusMetricsProviderBase& operator=(
      const SigninStatusMetricsProviderBase&) = delete;

  ~SigninStatusMetricsProviderBase() override;

  // Possible sign-in status of all opened profiles during one UMA session. For
  // MIXED_SIGNIN_STATUS, at least one signed-in profile and at least one
  // unsigned-in profile were opened between two UMA log uploads. Some statuses
  // are not applicable to all platforms.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum SigninStatus {
    ALL_PROFILES_SIGNED_IN,
    ALL_PROFILES_NOT_SIGNED_IN,
    MIXED_SIGNIN_STATUS,
    UNKNOWN_SIGNIN_STATUS,
    ERROR_GETTING_SIGNIN_STATUS,
    SIGNIN_STATUS_MAX,
  };

  // Sets the value of |signin_status_|. It ensures that |signin_status_| will
  // not be changed if its value is already ERROR_GETTING_SIGNIN_STATUS.
  void UpdateSigninStatus(SigninStatus new_status);

  SigninStatus signin_status() const { return signin_status_; }

 protected:
  // Record the sign in status into the proper histogram bucket. This should be
  // called exactly once for each UMA session.
  void RecordSigninStatusHistogram(SigninStatus signin_status);

  // Resets the value of |signin_status_| to be UNKNOWN_SIGNIN_STATUS regardless
  // of its current value;
  void ResetSigninStatus();

 private:
  // Sign-in status of all profiles seen so far.
  SigninStatus signin_status_;
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_

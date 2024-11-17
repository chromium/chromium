// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_ENABLED_STATE_PROVIDER_H_
#define COMPONENTS_METRICS_ENABLED_STATE_PROVIDER_H_

namespace metrics {

// An interface that provides whether metrics should be reported.
class EnabledStateProvider {
 public:
  virtual ~EnabledStateProvider() = default;

  // Indicates that the user has provided consent to collect and report metrics.
  virtual bool IsConsentGiven() const = 0;

  // Should collection and reporting be enabled. This should depend on consent
  // being given.
  virtual bool IsReportingEnabled() const;

  // Enable or disable checking whether field trials are forced or not at
  // EnabledStateProvider::IsReportingEnabled().
  static void SetIgnoreForceFieldTrialsForTesting(bool ignore_trials);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_ENABLED_STATE_PROVIDER_H_

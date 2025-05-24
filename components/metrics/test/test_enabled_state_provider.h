// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_TEST_ENABLED_STATE_PROVIDER_H_
#define COMPONENTS_METRICS_TEST_TEST_ENABLED_STATE_PROVIDER_H_

#include "components/metrics/enabled_state_provider.h"

namespace metrics {

// A simple concrete implementation of the EnabledStateProvider interface, for
// use in tests.
class TestEnabledStateProvider : public EnabledStateProvider {
 public:
  TestEnabledStateProvider(bool consent, bool enabled)
      : consent_(consent), enabled_(enabled) {}

  TestEnabledStateProvider(const TestEnabledStateProvider&) = delete;
  TestEnabledStateProvider& operator=(const TestEnabledStateProvider&) = delete;

  ~TestEnabledStateProvider() override = default;

  // EnabledStateProvider
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  void set_consent(bool consent) { consent_ = consent; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  bool consent_;
  bool enabled_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_TEST_ENABLED_STATE_PROVIDER_H_

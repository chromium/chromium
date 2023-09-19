// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_ENABLED_STATE_PROVIDER_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_ENABLED_STATE_PROVIDER_H_

#include <components/metrics/enabled_state_provider.h>

namespace variations::cros_early_boot::evaluate_seed {

// Override of EnabledStateProvider to determine whether to report metrics in
// early-boot contexts on ChromeOS.
// This is a trivial class: we never want to report metrics in early-boot, as
// they'd be too easily confused with metrics from chromium.
class EarlyBootEnabledStateProvider : public metrics::EnabledStateProvider {
 public:
  EarlyBootEnabledStateProvider();

  EarlyBootEnabledStateProvider(const EarlyBootEnabledStateProvider&) = delete;
  EarlyBootEnabledStateProvider& operator=(
      const EarlyBootEnabledStateProvider&) = delete;

  ~EarlyBootEnabledStateProvider() override;

  // EnabledStateProvider methods
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;
};

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_ENABLED_STATE_PROVIDER_H_

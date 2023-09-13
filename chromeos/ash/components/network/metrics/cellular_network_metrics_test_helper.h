// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_TEST_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_TEST_HELPER_H_

#include <cstddef>

namespace base {
class HistogramTester;
}  // namespace base

namespace ash {
namespace cellular_metrics {

// This helper struct is used in tests to simplify the logic around checking the
// many eSIM installation histograms. This struct allows clients to assert the
// counts for all histograms without any added complexity when writing tests.
struct ESimInstallHistogramState {
  struct HistogramState {
    size_t success_count = 0u;
    size_t inhibit_failed_count = 0u;
    size_t hermes_failed_count = 0u;
  };

  ESimInstallHistogramState();

  void Check(const base::HistogramTester* tester) const;
  void CheckHistogram(const base::HistogramTester* tester,
                      const char* histogram,
                      const HistogramState& state) const;

  HistogramState policy_install_user_errors_filtered_all;
  HistogramState policy_install_user_errors_filtered_smdp_initial;
  HistogramState policy_install_user_errors_filtered_smdp_retry;
  HistogramState policy_install_user_errors_filtered_smds_initial;
  HistogramState policy_install_user_errors_filtered_smds_retry;
  HistogramState policy_install_user_errors_included_all;
  HistogramState policy_install_user_errors_included_smdp_initial;
  HistogramState policy_install_user_errors_included_smdp_retry;
  HistogramState policy_install_user_errors_included_smds_initial;
  HistogramState policy_install_user_errors_included_smds_retry;
};

}  // namespace cellular_metrics
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_TEST_HELPER_H_

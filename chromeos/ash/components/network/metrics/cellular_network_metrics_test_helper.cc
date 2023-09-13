// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_test_helper.h"

#include "base/check.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace cellular_metrics {

ESimInstallHistogramState::ESimInstallHistogramState() = default;

void ESimInstallHistogramState::Check(
    const base::HistogramTester* tester) const {
  DCHECK(tester);
  CheckHistogram(
      tester,
      CellularNetworkMetricsLogger::kESimPolicyInstallUserErrorsFilteredAll,
      policy_install_user_errors_filtered_all);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsFilteredViaSmdpInitial,
                 policy_install_user_errors_filtered_smdp_initial);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsFilteredViaSmdpRetry,
                 policy_install_user_errors_filtered_smdp_retry);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsFilteredViaSmdsInitial,
                 policy_install_user_errors_filtered_smds_initial);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsFilteredViaSmdsRetry,
                 policy_install_user_errors_filtered_smds_retry);
  CheckHistogram(
      tester,
      CellularNetworkMetricsLogger::kESimPolicyInstallUserErrorsIncludedAll,
      policy_install_user_errors_included_all);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsIncludedViaSmdpInitial,
                 policy_install_user_errors_included_smdp_initial);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsIncludedViaSmdpRetry,
                 policy_install_user_errors_included_smdp_retry);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsIncludedViaSmdsInitial,
                 policy_install_user_errors_included_smds_initial);
  CheckHistogram(tester,
                 CellularNetworkMetricsLogger::
                     kESimPolicyInstallUserErrorsIncludedViaSmdsRetry,
                 policy_install_user_errors_included_smds_retry);
}

void ESimInstallHistogramState::CheckHistogram(
    const base::HistogramTester* tester,
    const char* histogram,
    const HistogramState& state) const {
  DCHECK(histogram);
  using InstallESimProfileResult =
      CellularNetworkMetricsLogger::ESimInstallResult;
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kSuccess,
                            /*expected_count=*/state.success_count);
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kInhibitFailed,
                            /*expected_count=*/state.inhibit_failed_count);
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kHermesFailed,
                            /*expected_count=*/state.hermes_failed_count);
}

}  // namespace cellular_metrics
}  // namespace ash

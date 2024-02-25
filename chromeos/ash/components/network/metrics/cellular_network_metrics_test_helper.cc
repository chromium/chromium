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

void ESimOperationResultBucket::Check(const base::HistogramTester* tester,
                                      const char* histogram) const {
  DCHECK(histogram);
  using InstallESimProfileResult =
      CellularNetworkMetricsLogger::ESimOperationResult;
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kSuccess,
                            /*expected_count=*/success_count);
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kInhibitFailed,
                            /*expected_count=*/inhibit_failed_count);
  tester->ExpectBucketCount(histogram, InstallESimProfileResult::kHermesFailed,
                            /*expected_count=*/hermes_failed_count);
}

ESimInstallHistogramState::ESimInstallHistogramState() = default;

void ESimInstallHistogramState::Check(
    const base::HistogramTester* tester) const {
  DCHECK(tester);
  policy_install_user_errors_filtered_all.Check(
      tester,
      CellularNetworkMetricsLogger::kESimPolicyInstallUserErrorsFilteredAll);
  policy_install_user_errors_filtered_smdp_initial.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsFilteredViaSmdpInitial);
  policy_install_user_errors_filtered_smdp_retry.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsFilteredViaSmdpRetry);
  policy_install_user_errors_filtered_smds_initial.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsFilteredViaSmdsInitial);
  policy_install_user_errors_filtered_smds_retry.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsFilteredViaSmdsRetry);
  policy_install_user_errors_included_all.Check(
      tester,
      CellularNetworkMetricsLogger::kESimPolicyInstallUserErrorsIncludedAll);
  policy_install_user_errors_included_smdp_initial.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsIncludedViaSmdpInitial);
  policy_install_user_errors_included_smdp_retry.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsIncludedViaSmdpRetry);
  policy_install_user_errors_included_smds_initial.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsIncludedViaSmdsInitial);
  policy_install_user_errors_included_smds_retry.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimPolicyInstallUserErrorsIncludedViaSmdsRetry);
  user_install_user_errors_filtered_all.Check(
      tester,
      CellularNetworkMetricsLogger::kESimUserInstallUserErrorsFilteredAll);
  user_install_user_errors_filtered_via_activation_code_after_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsFilteredViaActivationCodeAfterSmds);
  user_install_user_errors_filtered_via_activation_code_skipped_smds.Check(
      tester,
      CellularNetworkMetricsLogger::
          kESimUserInstallUserErrorsFilteredViaActivationCodeSkippedSmds);
  user_install_user_errors_filtered_via_qr_code_after_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsFilteredViaQrCodeAfterSmds);
  user_install_user_errors_filtered_via_qr_code_skipped_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsFilteredViaQrCodeSkippedSmds);
  user_install_user_errors_filtered_via_smds.Check(
      tester,
      CellularNetworkMetricsLogger::kESimUserInstallUserErrorsFilteredViaSmds);
  user_install_user_errors_included_all.Check(
      tester,
      CellularNetworkMetricsLogger::kESimUserInstallUserErrorsIncludedAll);
  user_install_user_errors_included_via_activation_code_after_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsIncludedViaActivationCodeAfterSmds);
  user_install_user_errors_included_via_activation_code_skipped_smds.Check(
      tester,
      CellularNetworkMetricsLogger::
          kESimUserInstallUserErrorsIncludedViaActivationCodeSkippedSmds);
  user_install_user_errors_included_via_qr_code_after_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsIncludedViaQrCodeAfterSmds);
  user_install_user_errors_included_via_qr_code_skipped_smds.Check(
      tester, CellularNetworkMetricsLogger::
                  kESimUserInstallUserErrorsIncludedViaQrCodeSkippedSmds);
  user_install_user_errors_included_via_smds.Check(
      tester,
      CellularNetworkMetricsLogger::kESimUserInstallUserErrorsIncludedViaSmds);
}

ESimSmdsScanHistogramState::ESimSmdsScanHistogramState() = default;

void ESimSmdsScanHistogramState::Check(
    const base::HistogramTester* tester) const {
  smds_scan_other_user_errors_filtered.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanOtherUserErrorsFiltered);
  smds_scan_other_user_errors_included.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanOtherUserErrorsIncluded);
  smds_scan_android_user_errors_filtered.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanAndroidUserErrorsFiltered);
  smds_scan_android_user_errors_included.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanAndroidUserErrorsIncluded);
  smds_scan_gsma_user_errors_filtered.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanGsmaUserErrorsFiltered);
  smds_scan_gsma_user_errors_included.Check(
      tester,
      CellularNetworkMetricsLogger::kESimSmdsScanGsmaUserErrorsIncluded);
}

}  // namespace cellular_metrics
}  // namespace ash

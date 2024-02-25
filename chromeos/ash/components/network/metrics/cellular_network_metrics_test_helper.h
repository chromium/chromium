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

struct ESimOperationResultBucket {
  void Check(const base::HistogramTester* tester, const char* histogram) const;

  size_t success_count = 0u;
  size_t inhibit_failed_count = 0u;
  size_t hermes_failed_count = 0u;
};

// This helper struct is used in tests to simplify the logic around checking the
// many eSIM installation histograms. This struct allows clients to assert the
// counts for all histograms without any added complexity when writing tests.
struct ESimInstallHistogramState {
  ESimInstallHistogramState();

  void Check(const base::HistogramTester* tester) const;

  ESimOperationResultBucket policy_install_user_errors_filtered_all;
  ESimOperationResultBucket policy_install_user_errors_filtered_smdp_initial;
  ESimOperationResultBucket policy_install_user_errors_filtered_smdp_retry;
  ESimOperationResultBucket policy_install_user_errors_filtered_smds_initial;
  ESimOperationResultBucket policy_install_user_errors_filtered_smds_retry;
  ESimOperationResultBucket policy_install_user_errors_included_all;
  ESimOperationResultBucket policy_install_user_errors_included_smdp_initial;
  ESimOperationResultBucket policy_install_user_errors_included_smdp_retry;
  ESimOperationResultBucket policy_install_user_errors_included_smds_initial;
  ESimOperationResultBucket policy_install_user_errors_included_smds_retry;
  ESimOperationResultBucket user_install_user_errors_filtered_all;
  ESimOperationResultBucket
      user_install_user_errors_filtered_via_activation_code_after_smds;
  ESimOperationResultBucket
      user_install_user_errors_filtered_via_activation_code_skipped_smds;
  ESimOperationResultBucket
      user_install_user_errors_filtered_via_qr_code_after_smds;
  ESimOperationResultBucket
      user_install_user_errors_filtered_via_qr_code_skipped_smds;
  ESimOperationResultBucket user_install_user_errors_filtered_via_smds;
  ESimOperationResultBucket user_install_user_errors_included_all;
  ESimOperationResultBucket
      user_install_user_errors_included_via_activation_code_after_smds;
  ESimOperationResultBucket
      user_install_user_errors_included_via_activation_code_skipped_smds;
  ESimOperationResultBucket
      user_install_user_errors_included_via_qr_code_after_smds;
  ESimOperationResultBucket
      user_install_user_errors_included_via_qr_code_skipped_smds;
  ESimOperationResultBucket user_install_user_errors_included_via_smds;
};

struct ESimSmdsScanHistogramState {
  ESimSmdsScanHistogramState();

  void Check(const base::HistogramTester* tester) const;

  ESimOperationResultBucket smds_scan_other_user_errors_filtered;
  ESimOperationResultBucket smds_scan_other_user_errors_included;
  ESimOperationResultBucket smds_scan_android_user_errors_filtered;
  ESimOperationResultBucket smds_scan_android_user_errors_included;
  ESimOperationResultBucket smds_scan_gsma_user_errors_filtered;
  ESimOperationResultBucket smds_scan_gsma_user_errors_included;
};

}  // namespace cellular_metrics
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_TEST_HELPER_H_

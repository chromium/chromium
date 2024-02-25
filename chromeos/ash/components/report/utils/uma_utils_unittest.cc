// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/uma_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::report::utils {

class UmaUtilsTest : public testing::Test {
 public:
  UmaUtilsTest() = default;
  UmaUtilsTest(const UmaUtilsTest&) = delete;
  UmaUtilsTest& operator=(const UmaUtilsTest&) = delete;
  ~UmaUtilsTest() override = default;

 protected:
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Test case for RecordNetErrorCode function.
TEST_F(UmaUtilsTest, RecordNetErrorCode) {
  int net_code = 0;  // Successful net code

  RecordNetErrorCode(PsmUseCase::k1DA, PsmRequest::kImport, net_code);
  RecordNetErrorCode(PsmUseCase::k28DA, PsmRequest::kImport, net_code);
  RecordNetErrorCode(PsmUseCase::kCohort, PsmRequest::kImport, net_code);
  RecordNetErrorCode(PsmUseCase::kObservation, PsmRequest::kImport, net_code);

  histogram_tester()->ExpectBucketCount(
      "Ash.Report.Psm1DAImportResponseNetErrorCode", PsmRequest::kImport, 1);
  histogram_tester()->ExpectBucketCount(
      "Ash.Report.Psm28DAImportResponseNetErrorCode", PsmRequest::kImport, 1);
  histogram_tester()->ExpectBucketCount(
      "Ash.Report.PsmCohortImportResponseNetErrorCode", PsmRequest::kImport, 1);
  histogram_tester()->ExpectBucketCount(
      "Ash.Report.PsmObservationImportResponseNetErrorCode",
      PsmRequest::kImport, 1);
}

// Test case for RecordCheckMembershipCases function.
TEST_F(UmaUtilsTest, RecordCheckMembershipCases) {
  CheckMembershipResponseCases response_case =
      CheckMembershipResponseCases::kNotHasRlweOprfResponse;

  RecordCheckMembershipCases(PsmUseCase::k1DA, response_case);
  RecordCheckMembershipCases(PsmUseCase::k28DA, response_case);
  RecordCheckMembershipCases(PsmUseCase::kCohort, response_case);
  RecordCheckMembershipCases(PsmUseCase::kObservation, response_case);

  histogram_tester()->ExpectBucketCount("Ash.Report.1DACheckMembershipCases",
                                        response_case, 1);
  histogram_tester()->ExpectBucketCount("Ash.Report.28DACheckMembershipCases",
                                        response_case, 1);
  histogram_tester()->ExpectBucketCount("Ash.Report.CohortCheckMembershipCases",
                                        response_case, 1);
  histogram_tester()->ExpectBucketCount(
      "Ash.Report.ObservationCheckMembershipCases", response_case, 1);
}

}  // namespace ash::report::utils

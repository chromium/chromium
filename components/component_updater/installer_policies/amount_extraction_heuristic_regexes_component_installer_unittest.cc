// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/amount_extraction_heuristic_regexes_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.pb.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::core::browser::payments::HeuristicRegexes;

namespace component_updater {

namespace {
constexpr char kAmountExtractionHeuristicRegexesVersion[] = "1";
const base::FilePath::CharType
    kAmountExtractionHeuristicRegexesBinaryPbFileName[] =
        FILE_PATH_LITERAL("heuristic_regexes.binarypb");
const char* kAmountExtractionComponentInstallationResultMetric =
    "Autofill.AmountExtraction.HeuristicRegexesComponentInstallationResult";
}  // namespace

class AmountExtractionHeuristicRegexesInstallerPolicyTest
    : public testing::Test {
 public:
  AmountExtractionHeuristicRegexesInstallerPolicyTest() = default;
  AmountExtractionHeuristicRegexesInstallerPolicyTest(
      const AmountExtractionHeuristicRegexesInstallerPolicyTest&) = delete;
  AmountExtractionHeuristicRegexesInstallerPolicyTest& operator=(
      const AmountExtractionHeuristicRegexesInstallerPolicyTest&) = delete;
  ~AmountExtractionHeuristicRegexesInstallerPolicyTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

  base::FilePath component_install_dir() const {
    return component_install_dir_.GetPath();
  }

  bool CreateTestAmountExtractionHeuristicRegexesData(
      const std::string& file_content) {
    base::FilePath file_path = component_install_dir().Append(
        kAmountExtractionHeuristicRegexesBinaryPbFileName);
    return base::WriteFile(file_path, file_content);
  }

  void LoadTestAmountExtractionHeuristicRegexesData() {
    base::Value::Dict manifest;
    ASSERT_TRUE(policy_.VerifyInstallation(manifest, component_install_dir()));
    const base::Version expected_version(
        kAmountExtractionHeuristicRegexesVersion);
    policy_.ComponentReady(expected_version, component_install_dir(),
                           std::move(manifest));
    RunUntilIdle();
  }

  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) {
    return policy_.VerifyInstallation(manifest, install_dir);
  }

  void AssertWriteMetrics(
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult
          result,
      int count) {
    histogram_tester_.ExpectBucketCount(
        kAmountExtractionComponentInstallationResultMetric, result, count);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir component_install_dir_;
  MockComponentUpdateService component_update_service_;
  AmountExtractionHeuristicRegexesInstallerPolicy policy_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AmountExtractionHeuristicRegexesInstallerPolicyTest,
       ComponentRegistered) {
  EXPECT_CALL(component_update_service_, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterAmountExtractionHeuristicRegexesComponent(&component_update_service_);
  RunUntilIdle();
}

TEST_F(AmountExtractionHeuristicRegexesInstallerPolicyTest,
       LoadFileWithDefualtPattern) {
  autofill::payments::AmountExtractionHeuristicRegexes::GetInstance()
      .ResetRegexStringPatternsForTesting();
  HeuristicRegexes heuristic_regexes;
  ASSERT_TRUE(CreateTestAmountExtractionHeuristicRegexesData(
      heuristic_regexes.SerializeAsString()));
  ASSERT_NO_FATAL_FAILURE(LoadTestAmountExtractionHeuristicRegexesData());

  const autofill::payments::AmountExtractionHeuristicRegexes& regex_data =
      autofill::payments::AmountExtractionHeuristicRegexes::GetInstance();
  EXPECT_EQ(regex_data.keyword_pattern(), "^(Order Total|Total):?$");
  EXPECT_EQ(regex_data.amount_pattern(),
            R"regexp((?:\$)\s*\d{1,3}(?:[.,]\d{3})*(?:[.,]\d{2})?)regexp");
}

TEST_F(AmountExtractionHeuristicRegexesInstallerPolicyTest, LoadFileWithData) {
  HeuristicRegexes heuristic_regexes;
  heuristic_regexes.mutable_generic_details()->set_keyword_pattern(
      "^(Total Amount):?$");
  heuristic_regexes.mutable_generic_details()->set_amount_pattern(
      R"regexp(\$\d+\.\d{2})regexp");
  heuristic_regexes.mutable_generic_details()
      ->set_number_of_ancestor_levels_to_search(4);
  ASSERT_TRUE(CreateTestAmountExtractionHeuristicRegexesData(
      heuristic_regexes.SerializeAsString()));
  ASSERT_NO_FATAL_FAILURE(LoadTestAmountExtractionHeuristicRegexesData());

  const autofill::payments::AmountExtractionHeuristicRegexes& regex_data =
      autofill::payments::AmountExtractionHeuristicRegexes::GetInstance();
  EXPECT_EQ(regex_data.keyword_pattern(), "^(Total Amount):?$");
  EXPECT_EQ(regex_data.amount_pattern(), R"regexp(\$\d+\.\d{2})regexp");
  EXPECT_EQ(regex_data.number_of_ancestor_levels_to_search(), 4u);

  AssertWriteMetrics(
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kSuccessful,
      1);
}

TEST_F(AmountExtractionHeuristicRegexesInstallerPolicyTest,
       WriteMetrics_InvalidInstallationPath) {
  ASSERT_FALSE(VerifyInstallation(
      base::Value::Dict(), base::FilePath(FILE_PATH_LITERAL("invalid_dir"))));

  AssertWriteMetrics(
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kInvalidInstallationPath,
      1);
}

TEST_F(AmountExtractionHeuristicRegexesInstallerPolicyTest,
       WriteMetrics_EmptyBinaryFile) {
  autofill::payments::AmountExtractionHeuristicRegexes::GetInstance()
      .ResetRegexStringPatternsForTesting();
  HeuristicRegexes heuristic_regexes;
  ASSERT_TRUE(CreateTestAmountExtractionHeuristicRegexesData(
      heuristic_regexes.SerializeAsString()));
  ASSERT_NO_FATAL_FAILURE(LoadTestAmountExtractionHeuristicRegexesData());

  AssertWriteMetrics(
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kEmptyBinaryFile,
      1);
}

}  // namespace component_updater

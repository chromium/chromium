// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr ukm::SourceId kTestSourceId = 0x1234;

using password_manager::CreateLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogMetricsRecorder;
using password_manager::metrics_util::LeakDialogType;
using testing::StrictMock;
using UkmEntry = ukm::builders::PasswordManager_LeakWarningDialog;

constexpr char kUrl[] = "https://www.example.co.uk";
constexpr char16_t kUsername[] = u"Jane";

class MockCredentialLeakPrompt : public CredentialLeakPrompt {
 public:
  MockCredentialLeakPrompt() = default;

  MockCredentialLeakPrompt(const MockCredentialLeakPrompt&) = delete;
  MockCredentialLeakPrompt& operator=(const MockCredentialLeakPrompt&) = delete;

  MOCK_METHOD(void, ShowCredentialLeakPrompt, (), (override));
  MOCK_METHOD(void, ControllerGone, (), (override));
};

class CredentialLeakDialogControllerTest : public testing::Test {
 public:
  void SetUpController(password_manager::CredentialLeakType leak_type) {
    auto recorder = std::make_unique<LeakDialogMetricsRecorder>(
        kTestSourceId, password_manager::GetLeakDialogType(leak_type));
    // Set sampling rate to 100% for UKM metrics.
    recorder->SetSamplingRateForTesting(1.0);
    controller_ = std::make_unique<CredentialLeakDialogControllerImpl>(
        &ui_controller_mock_, leak_type, GURL(kUrl), kUsername,
        std::move(recorder));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  PasswordsLeakDialogDelegateMock& ui_controller_mock() {
    return ui_controller_mock_;
  }

  ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return test_ukm_recorder_;
  }

  MockCredentialLeakPrompt& leak_prompt() { return leak_prompt_; }

  CredentialLeakDialogControllerImpl& controller() { return *controller_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  StrictMock<PasswordsLeakDialogDelegateMock> ui_controller_mock_;
  StrictMock<MockCredentialLeakPrompt> leak_prompt_;
  std::unique_ptr<CredentialLeakDialogControllerImpl> controller_;
};

void CheckUkmMetricsExpectations(
    ukm::TestAutoSetUkmRecorder& recorder,
    LeakDialogType expected_dialog_type,
    LeakDialogDismissalReason expected_dismissal_reason) {
  const auto& entries = recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    recorder.ExpectEntryMetric(entry,
                               UkmEntry::kPasswordLeakDetectionDialogTypeName,
                               static_cast<int64_t>(expected_dialog_type));
    recorder.ExpectEntryMetric(
        entry, UkmEntry::kPasswordLeakDetectionDialogDismissalReasonName,
        static_cast<int64_t>(expected_dismissal_reason));
  }
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogClose) {
  SetUpController(
      CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCloseDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder(), LeakDialogType::kChange,
                              LeakDialogDismissalReason::kNoDirectInteraction);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogOk) {
  SetUpController(
      CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnAcceptDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedOk, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kClickedOk, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder(), LeakDialogType::kChange,
                              LeakDialogDismissalReason::kClickedOk);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogCancel) {
  SetUpController(
      CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCancelDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedClose, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.CheckupAndChange",
      LeakDialogDismissalReason::kClickedClose, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder(),
                              LeakDialogType::kCheckupAndChange,
                              LeakDialogDismissalReason::kClickedClose);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogCheckPasswords) {
  SetUpController(
      CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(
      ui_controller_mock(),
      NavigateToPasswordCheckup(
          password_manager::PasswordCheckReferrer::kPasswordBreachDialog));
  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnAcceptDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Checkup",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  CheckUkmMetricsExpectations(
      test_ukm_recorder(), LeakDialogType::kCheckup,
      LeakDialogDismissalReason::kClickedCheckPasswords);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

}  // namespace

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include <stdint.h>

#include <string>

#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using autofill::FieldPropertiesFlags;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

constexpr ukm::SourceId kTestSourceId = 0x1234;

using features_util::PasswordAccountStorageUsageLevel;
using UkmEntry = ukm::builders::PasswordForm;
using StoreSet = std::set<std::pair<std::u16string, PasswordForm::Store>>;

// Create a UkmEntryBuilder with kTestSourceId.
scoped_refptr<PasswordFormMetricsRecorder> CreatePasswordFormMetricsRecorder(
    bool is_main_frame_secure,
    PrefService* pref_service) {
  return base::MakeRefCounted<PasswordFormMetricsRecorder>(
      is_main_frame_secure, kTestSourceId, pref_service);
}

// TODO(crbug.com/40528506) Replace this with generalized infrastructure.
// Verifies that the metric |metric_name| was recorded with value |value| in the
// single entry of |test_ukm_recorder_| exactly |expected_count| times.
void ExpectUkmValueCount(ukm::TestUkmRecorder* test_ukm_recorder,
                         const char* metric_name,
                         int64_t value,
                         int64_t expected_count) {
  auto entries = test_ukm_recorder->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    if (expected_count) {
      test_ukm_recorder->ExpectEntryMetric(entry, metric_name, value);
    } else {
      const int64_t* count =
          test_ukm_recorder->GetEntryMetric(entry, metric_name);
      EXPECT_TRUE(count == nullptr || *count != expected_count);
    }
  }
}

}  // namespace

class PasswordFormMetricsRecorderTest : public PlatformTest {
 public:
  void SetUp() override {
    PasswordManager::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Test the metrics recorded around password generation and the user's
// interaction with the offer to generate passwords.
TEST_F(PasswordFormMetricsRecorderTest, Generation) {
  static constexpr struct {
    bool generation_available;
    bool has_generated_password;
    PasswordFormMetricsRecorder::SubmitResult submission;
  } kTests[] = {
      {false, false, PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted},
      {true, false, PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted},
      {true, true, PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted},
      {false, false, PasswordFormMetricsRecorder::SubmitResult::kFailed},
      {true, false, PasswordFormMetricsRecorder::SubmitResult::kFailed},
      {true, true, PasswordFormMetricsRecorder::SubmitResult::kFailed},
      {false, false, PasswordFormMetricsRecorder::SubmitResult::kPassed},
      {true, false, PasswordFormMetricsRecorder::SubmitResult::kPassed},
      {true, true, PasswordFormMetricsRecorder::SubmitResult::kPassed},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "generation_available=" << test.generation_available
                 << ", has_generated_password=" << test.has_generated_password
                 << ", submission=" << static_cast<int64_t>(test.submission));

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure*/ true, &pref_service_);
      if (test.generation_available) {
        recorder->MarkGenerationAvailable();
      }
      if (test.has_generated_password) {
        recorder->SetGeneratedPasswordStatus(
            PasswordFormMetricsRecorder::GeneratedPasswordStatus::
                kPasswordAccepted);
      }
      switch (test.submission) {
        case PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted:
          // Do nothing.
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kFailed:
          recorder->LogSubmitFailed();
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kPassed:
          recorder->LogSubmitPassed();
          break;
      }
    }

    ExpectUkmValueCount(
        &test_ukm_recorder, UkmEntry::kSubmission_ObservedName,
        test.submission !=
                PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted
            ? 1
            : 0,
        1);

    int expected_login_failed =
        test.submission == PasswordFormMetricsRecorder::SubmitResult::kFailed
            ? 1
            : 0;
    EXPECT_EQ(expected_login_failed,
              user_action_tester.GetActionCount("PasswordManager_LoginFailed"));
    ExpectUkmValueCount(&test_ukm_recorder,
                        UkmEntry::kSubmission_SubmissionResultName,
                        static_cast<int64_t>(
                            PasswordFormMetricsRecorder::SubmitResult::kFailed),
                        expected_login_failed);

    int expected_login_failed_after_generation =
        test.has_generated_password && expected_login_failed ? 1 : 0;
    ExpectUkmValueCount(
        &test_ukm_recorder,
        UkmEntry::kSubmission_SubmissionResult_GeneratedPasswordName,
        static_cast<int64_t>(
            PasswordFormMetricsRecorder::SubmitResult::kFailed),
        expected_login_failed_after_generation);

    int expected_login_passed =
        test.submission == PasswordFormMetricsRecorder::SubmitResult::kPassed
            ? 1
            : 0;
    EXPECT_EQ(expected_login_passed,
              user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
    ExpectUkmValueCount(&test_ukm_recorder,
                        UkmEntry::kSubmission_SubmissionResultName,
                        static_cast<int64_t>(
                            PasswordFormMetricsRecorder::SubmitResult::kPassed),
                        expected_login_passed);

    int expected_login_passed_after_generation =
        test.has_generated_password && expected_login_passed ? 1 : 0;
    ExpectUkmValueCount(
        &test_ukm_recorder,
        UkmEntry::kSubmission_SubmissionResult_GeneratedPasswordName,
        static_cast<int64_t>(
            PasswordFormMetricsRecorder::SubmitResult::kPassed),
        expected_login_passed_after_generation);

    if (test.has_generated_password) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::GENERATED_PASSWORD_FORCE_SAVED, 1);
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
      }
    }

    if (!test.has_generated_password && test.generation_available) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::SubmitResult::kNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMISSION_FAILED, 1);
          break;
        case PasswordFormMetricsRecorder::SubmitResult::kPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
      }
    }
  }
}

TEST_F(PasswordFormMetricsRecorderTest, SubmittedFormType) {
  static constexpr struct {
    // Stimuli:
    std::optional<metrics_util::SubmittedFormType> form_type;
    bool was_form_submitted;
    // Expectations:
    bool should_record_metrics;
  } kTests[] = {
      {metrics_util::SubmittedFormType::kLogin, true, true},
      {metrics_util::SubmittedFormType::kSignup, true, true},
      {metrics_util::SubmittedFormType::kLogin, false, false},
      {std::nullopt, true, false},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "Was form_type set = " << test.form_type.has_value()
                 << ", was_form_submitted =" << test.was_form_submitted);

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    base::HistogramTester histogram_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      if (test.form_type) {
        recorder->SetSubmittedFormType(test.form_type.value());
      }
      if (test.was_form_submitted) {
        recorder->LogSubmitPassed();
      }
    }

    if (test.should_record_metrics) {
      histogram_tester.ExpectUniqueSample("PasswordManager.SubmittedFormType2",
                                          test.form_type.value(), 1);
      ExpectUkmValueCount(&test_ukm_recorder,
                          UkmEntry::kSubmission_SubmittedFormType2Name,
                          static_cast<int64_t>(test.form_type.value()), 1);
    } else {
      histogram_tester.ExpectTotalCount("PasswordManager.SubmittedFormType2",
                                        0);
    }
  }
}

TEST_F(PasswordFormMetricsRecorderTest, RecordPasswordBubbleShown) {
  using Trigger = PasswordFormMetricsRecorder::BubbleTrigger;
  static constexpr struct {
    // Stimuli:
    metrics_util::CredentialSourceType credential_source_type;
    metrics_util::UIDisplayDisposition display_disposition;
    // Expectations:
    const char* expected_trigger_metric;
    Trigger expected_trigger_value;
    bool expected_save_prompt_shown;
    bool expected_update_prompt_shown;
  } kTests[] = {
      // Source = PasswordManager, Saving.
      {metrics_util::CredentialSourceType::kPasswordManager,
       metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING,
       UkmEntry::kSaving_Prompt_TriggerName,
       Trigger::kPasswordManagerSuggestionAutomatic, true, false},
      {metrics_util::CredentialSourceType::kPasswordManager,
       metrics_util::MANUAL_WITH_PASSWORD_PENDING,
       UkmEntry::kSaving_Prompt_TriggerName,
       Trigger::kPasswordManagerSuggestionManual, true, false},
      // Source = PasswordManager, Updating.
      {metrics_util::CredentialSourceType::kPasswordManager,
       metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
       UkmEntry::kUpdating_Prompt_TriggerName,
       Trigger::kPasswordManagerSuggestionAutomatic, false, true},
      {metrics_util::CredentialSourceType::kPasswordManager,
       metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE,
       UkmEntry::kUpdating_Prompt_TriggerName,
       Trigger::kPasswordManagerSuggestionManual, false, true},
      // Source = Credential Management API, Saving.
      {metrics_util::CredentialSourceType::kCredentialManagementAPI,
       metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING,
       UkmEntry::kSaving_Prompt_TriggerName,
       Trigger::kCredentialManagementAPIAutomatic, true, false},
      {metrics_util::CredentialSourceType::kCredentialManagementAPI,
       metrics_util::MANUAL_WITH_PASSWORD_PENDING,
       UkmEntry::kSaving_Prompt_TriggerName,
       Trigger::kCredentialManagementAPIManual, true, false},
      // Source = Credential Management API, Updating.
      {metrics_util::CredentialSourceType::kCredentialManagementAPI,
       metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
       UkmEntry::kUpdating_Prompt_TriggerName,
       Trigger::kCredentialManagementAPIAutomatic, false, true},
      {metrics_util::CredentialSourceType::kCredentialManagementAPI,
       metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE,
       UkmEntry::kUpdating_Prompt_TriggerName,
       Trigger::kCredentialManagementAPIManual, false, true},
      // Source = Unknown, Saving.
      {metrics_util::CredentialSourceType::kUnknown,
       metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING,
       UkmEntry::kSaving_Prompt_TriggerName,
       Trigger::kPasswordManagerSuggestionAutomatic, false, false},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "credential_source_type = "
                 << static_cast<int64_t>(test.credential_source_type)
                 << ", display_disposition = " << test.display_disposition);
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          true /*is_main_frame_secure*/, &pref_service_);
      recorder->RecordPasswordBubbleShown(test.credential_source_type,
                                          test.display_disposition);
    }
    // Verify data
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      EXPECT_EQ(kTestSourceId, entry->source_id);

      if (test.credential_source_type !=
          metrics_util::CredentialSourceType::kUnknown) {
        test_ukm_recorder.ExpectEntryMetric(
            entry, test.expected_trigger_metric,
            static_cast<int64_t>(test.expected_trigger_value));
      } else {
        EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
            entry, UkmEntry::kSaving_Prompt_TriggerName));
        EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
            entry, UkmEntry::kUpdating_Prompt_TriggerName));
      }
      test_ukm_recorder.ExpectEntryMetric(entry,
                                          UkmEntry::kSaving_Prompt_ShownName,
                                          test.expected_save_prompt_shown);
      test_ukm_recorder.ExpectEntryMetric(entry,
                                          UkmEntry::kUpdating_Prompt_ShownName,
                                          test.expected_update_prompt_shown);
    }
  }
}

TEST_F(PasswordFormMetricsRecorderTest, RecordUIDismissalReason) {
  static constexpr struct {
    // Stimuli:
    metrics_util::UIDisplayDisposition display_disposition;
    metrics_util::UIDismissalReason dismissal_reason;
    // Expectations:
    const char* expected_trigger_metric;
    PasswordFormMetricsRecorder::BubbleDismissalReason expected_metric_value;
  } kTests[] = {
      {metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING,
       metrics_util::CLICKED_ACCEPT, UkmEntry::kSaving_Prompt_InteractionName,
       PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted},
      {metrics_util::MANUAL_WITH_PASSWORD_PENDING, metrics_util::CLICKED_CANCEL,
       UkmEntry::kSaving_Prompt_InteractionName,
       PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined},
      {metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
       metrics_util::CLICKED_NEVER, UkmEntry::kUpdating_Prompt_InteractionName,
       PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined},
      {metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE,
       metrics_util::NO_DIRECT_INTERACTION,
       UkmEntry::kUpdating_Prompt_InteractionName,
       PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "display_disposition = " << test.display_disposition
                 << ", dismissal_reason = " << test.dismissal_reason);
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          true /*is_main_frame_secure*/, &pref_service_);
      recorder->RecordPasswordBubbleShown(
          metrics_util::CredentialSourceType::kPasswordManager,
          test.display_disposition);
      recorder->RecordUIDismissalReason(test.dismissal_reason);
    }
    // Verify data
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      EXPECT_EQ(kTestSourceId, entry->source_id);
      test_ukm_recorder.ExpectEntryMetric(
          entry, test.expected_trigger_metric,
          static_cast<int64_t>(test.expected_metric_value));
    }
  }
}

// Verify that it is ok to open and close the password bubble more than once
// and still get accurate metrics.
TEST_F(PasswordFormMetricsRecorderTest, SequencesOfBubbles) {
  using BubbleDismissalReason =
      PasswordFormMetricsRecorder::BubbleDismissalReason;
  using BubbleTrigger = PasswordFormMetricsRecorder::BubbleTrigger;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &pref_service_);
    // Open and confirm an automatically triggered saving prompt.
    recorder->RecordPasswordBubbleShown(
        metrics_util::CredentialSourceType::kPasswordManager,
        metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
    recorder->RecordUIDismissalReason(metrics_util::CLICKED_ACCEPT);
    // Open and confirm a manually triggered update prompt.
    recorder->RecordPasswordBubbleShown(
        metrics_util::CredentialSourceType::kPasswordManager,
        metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE);
    recorder->RecordUIDismissalReason(metrics_util::CLICKED_ACCEPT);
  }
  // Verify recorded UKM data.
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kSaving_Prompt_InteractionName,
        static_cast<int64_t>(BubbleDismissalReason::kAccepted));
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUpdating_Prompt_InteractionName,
        static_cast<int64_t>(BubbleDismissalReason::kAccepted));
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUpdating_Prompt_ShownName, 1);
    test_ukm_recorder.ExpectEntryMetric(entry,
                                        UkmEntry::kSaving_Prompt_ShownName, 1);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kSaving_Prompt_TriggerName,
        static_cast<int64_t>(
            BubbleTrigger::kPasswordManagerSuggestionAutomatic));
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUpdating_Prompt_TriggerName,
        static_cast<int64_t>(BubbleTrigger::kPasswordManagerSuggestionManual));
  }
}

// Verify that one-time actions are only recorded once per life-cycle of a
// PasswordFormMetricsRecorder.
TEST_F(PasswordFormMetricsRecorderTest, RecordDetailedUserAction) {
  using Action = PasswordFormMetricsRecorder::DetailedUserAction;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &pref_service_);
    recorder->RecordDetailedUserAction(Action::kCorrectedUsernameInForm);
    recorder->RecordDetailedUserAction(Action::kCorrectedUsernameInForm);
    recorder->RecordDetailedUserAction(Action::kEditedUsernameInBubble);
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUser_Action_CorrectedUsernameInFormName, 2u);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUser_Action_EditedUsernameInBubbleName, 1u);
    EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
        entry, UkmEntry::kUser_Action_SelectedDifferentPasswordInBubbleName));
  }
}

// Verify that the the mapping is correct and that metrics are actually
// recorded.
TEST_F(PasswordFormMetricsRecorderTest, RecordShowManualFallbackForSaving) {
  struct {
    bool has_generated_password;
    bool is_update;
    int expected_value;
  } kTests[] = {
      {false, false, 1},
      {true, false, 1 + 2},
      {false, true, 1 + 4},
      {true, true, 1 + 2 + 4},
  };
  for (const auto& test : kTests) {
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          true /*is_main_frame_secure*/, &pref_service_);
      recorder->RecordShowManualFallbackForSaving(test.has_generated_password,
                                                  test.is_update);
    }
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(kTestSourceId, entries[0]->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entries[0], UkmEntry::kSaving_ShowedManualFallbackForSavingName,
        test.expected_value);
  }
}

// Verify that no 0 is recorded if now fallback icon is shown.
TEST_F(PasswordFormMetricsRecorderTest, NoRecordShowManualFallbackForSaving) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &pref_service_);
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
      entries[0], UkmEntry::kSaving_ShowedManualFallbackForSavingName));
}

// Verify that only the latest value is recorded
TEST_F(PasswordFormMetricsRecorderTest,
       RecordShowManualFallbackForSavingLatestOnly) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &pref_service_);
    recorder->RecordShowManualFallbackForSaving(true, false);
    recorder->RecordShowManualFallbackForSaving(true, true);
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], UkmEntry::kSaving_ShowedManualFallbackForSavingName,
      1 + 2 + 4);
}

TEST_F(PasswordFormMetricsRecorderTest, FormChangeBitmapNoMetricRecorded) {
  auto recorder = CreatePasswordFormMetricsRecorder(
      true /*is_main_frame_secure*/, &pref_service_);
  recorder.reset();
  histogram_tester_.ExpectTotalCount("PasswordManager.DynamicFormChanges", 0);
}

TEST_F(PasswordFormMetricsRecorderTest, FormChangeBitmapRecordedOnce) {
  auto recorder = CreatePasswordFormMetricsRecorder(
      true /*is_main_frame_secure*/, &pref_service_);
  recorder->RecordFormChangeBitmask(PasswordFormMetricsRecorder::kFieldsNumber);
  recorder.reset();
  histogram_tester_.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                       1 /* kFieldsNumber */, 1);
}

TEST_F(PasswordFormMetricsRecorderTest, FormChangeBitmapRecordedMultipleTimes) {
  auto recorder = CreatePasswordFormMetricsRecorder(
      true /*is_main_frame_secure*/, &pref_service_);
  recorder->RecordFormChangeBitmask(PasswordFormMetricsRecorder::kFieldsNumber);
  recorder->RecordFormChangeBitmask(
      PasswordFormMetricsRecorder::kFormControlTypes);
  recorder.reset();
  uint32_t expected = 1 /* fields number */ | (1 << 3) /* control types */;
  histogram_tester_.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                       expected, 1);
}

// todo add namespace

struct TestCaseFieldInfo {
  std::string value;
  std::string user_input;
  bool user_typed = false;
  bool automatically_filled = false;
  bool manually_filled = false;
  bool is_password = false;
};

struct FillingAssistanceTestCase {
  const char* description_for_logging;

  bool is_blocklisted = false;
  bool submission_detected = true;
  bool submission_is_successful = true;

  std::vector<TestCaseFieldInfo> fields;
  std::vector<std::string> saved_usernames;
  std::vector<std::string> saved_passwords;
  std::vector<InteractionsStats> interactions_stats;

  std::optional<PasswordFormMetricsRecorder::FillingAssistance> expectation;
};

PasswordForm ConvertToPasswordForm(
    const std::vector<TestCaseFieldInfo>& fields) {
  PasswordForm password_form;
  for (const auto& field : fields) {
    FormFieldData form_field;
    form_field.set_value(ASCIIToUTF16(field.value));
    form_field.set_user_input(ASCIIToUTF16(field.user_input));

    if (field.user_typed) {
      form_field.set_properties_mask(form_field.properties_mask() |
                                     FieldPropertiesFlags::kUserTyped);
    }

    if (field.manually_filled) {
      form_field.set_properties_mask(
          form_field.properties_mask() |
          FieldPropertiesFlags::kAutofilledOnUserTrigger);
    }

    if (field.automatically_filled) {
      form_field.set_properties_mask(
          form_field.properties_mask() |
          FieldPropertiesFlags::kAutofilledOnPageLoad);
    }

    form_field.set_form_control_type(
        field.is_password ? autofill::FormControlType::kInputPassword
                          : autofill::FormControlType::kInputText);

    std::u16string value =
        ASCIIToUTF16(field.user_input.empty() ? field.value : field.user_input);
    if (field.is_password) {
      password_form.password_value = value;
    } else {
      password_form.username_value = value;
    }

    test_api(password_form.form_data).Append(form_field);
  }
  return password_form;
}

StoreSet ConvertToString16AndStoreSet(
    const std::vector<std::string>& profile_store_values,
    const std::vector<std::string>& account_store_values) {
  StoreSet result;
  for (const std::string& str : profile_store_values) {
    result.emplace(ASCIIToUTF16(str), PasswordForm::Store::kProfileStore);
  }
  for (const std::string& str : account_store_values) {
    result.emplace(ASCIIToUTF16(str), PasswordForm::Store::kAccountStore);
  }
  return result;
}

// Returns basic username and password store sets that can be filled.
std::tuple<StoreSet, StoreSet> BasicUsernameAndPasswordStoreSets() {
  StoreSet saved_usernames =
      ConvertToString16AndStoreSet({"user1", "user2"},
                                   /*account_store_values=*/{});
  StoreSet saved_passwords =
      ConvertToString16AndStoreSet({"password1", "secret"},
                                   /*account_store_values=*/{});
  return std::tuple<StoreSet, StoreSet>(std::move(saved_usernames),
                                        std::move(saved_passwords));
}

// Picks the first value in `store_set`.
std::string PickFirstValueInStoreSet(StoreSet store_set) {
  return base::UTF16ToASCII(store_set.begin()->first);
}

void CheckFillingAssistanceTestCase(
    const FillingAssistanceTestCase& test_case) {
  struct SubCase {
    bool is_main_frame_secure;
    PasswordAccountStorageUsageLevel account_storage_usage_level;
    // Is mixed form only makes sense when is_main_frame_secure is true.
    bool is_mixed_form = false;
  } sub_cases[] = {
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kNotUsingAccountStorage,
       .is_mixed_form = false},
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kNotUsingAccountStorage,
       .is_mixed_form = true},
      {.is_main_frame_secure = false,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kNotUsingAccountStorage},
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kUsingAccountStorage,
       .is_mixed_form = false},
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kUsingAccountStorage,
       .is_mixed_form = true},
      {.is_main_frame_secure = false,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kUsingAccountStorage},
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kSyncing,
       .is_mixed_form = false},
      {.is_main_frame_secure = true,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kUsingAccountStorage,
       .is_mixed_form = true},
      {.is_main_frame_secure = false,
       .account_storage_usage_level =
           PasswordAccountStorageUsageLevel::kSyncing}};
  for (SubCase sub_case : sub_cases) {
    SCOPED_TRACE(
        testing::Message("Test description: ")
        << test_case.description_for_logging
        << ", is_main_frame_secure: " << std::boolalpha
        << sub_case.is_main_frame_secure << ", account_storage_usage_level: "
        << metrics_util::GetPasswordAccountStorageUsageLevelHistogramSuffix(
               sub_case.account_storage_usage_level)
        << ", is_mixed_form: " << std::boolalpha << sub_case.is_mixed_form);

    sync_preferences::TestingPrefServiceSyncable pref_service;
    PasswordManager::RegisterProfilePrefs(pref_service.registry());
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(test_case.fields);
    if (sub_case.is_main_frame_secure) {
      if (sub_case.is_mixed_form) {
        password_form_data.form_data.set_action(GURL("http://notsecure.test"));
      } else {
        password_form_data.form_data.set_action(GURL("https://secure.test"));
      }
    }

    // Note: Don't bother with the profile store vs. account store distinction
    // here; there are separate tests that cover the filling source.
    StoreSet saved_usernames =
        ConvertToString16AndStoreSet(test_case.saved_usernames,
                                     /*account_store_values=*/{});
    StoreSet saved_passwords =
        ConvertToString16AndStoreSet(test_case.saved_passwords,
                                     /*account_store_values=*/{});

    auto recorder = CreatePasswordFormMetricsRecorder(
        sub_case.is_main_frame_secure, &pref_service);
    if (test_case.submission_detected) {
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          test_case.is_blocklisted, test_case.interactions_stats,
          sub_case.account_storage_usage_level);
    }

    if (test_case.submission_is_successful) {
      recorder->LogSubmitPassed();
    }
    recorder.reset();

    int expected_count = test_case.expectation ? 1 : 0;

    // Split by secure/insecure origin.
    int expected_insecure_count =
        !sub_case.is_main_frame_secure ? expected_count : 0;
    int expected_secure_count =
        sub_case.is_main_frame_secure ? expected_count : 0;
    int expected_mixed_count =
        sub_case.is_main_frame_secure && sub_case.is_mixed_form ? expected_count
                                                                : 0;

    // Split by account storage usage level.
    int expected_not_using_account_storage_count = 0;
    int expected_using_account_storage_count = 0;
    int expected_syncing_count = 0;
    switch (sub_case.account_storage_usage_level) {
      case PasswordAccountStorageUsageLevel::kNotUsingAccountStorage:
        expected_not_using_account_storage_count = expected_count;
        break;
      case PasswordAccountStorageUsageLevel::kUsingAccountStorage:
        expected_using_account_storage_count = expected_count;
        break;
      case PasswordAccountStorageUsageLevel::kSyncing:
        expected_syncing_count = expected_count;
        break;
    }

    // Verifies that filling assistance isn't calculated for single username
    // form.
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistanceForSingleUsername", 0);

    histogram_tester.ExpectTotalCount("PasswordManager.FillingAssistance",
                                      expected_count);

    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.InsecureOrigin",
        expected_insecure_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.SecureOrigin",
        expected_secure_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.MixedForm", expected_mixed_count);

    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.NotUsingAccountStorage",
        expected_not_using_account_storage_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.UsingAccountStorage",
        expected_using_account_storage_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.Syncing", expected_syncing_count);

    if (test_case.expectation) {
      histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                          *test_case.expectation, 1);

      histogram_tester.ExpectUniqueSample(
          sub_case.is_main_frame_secure
              ? "PasswordManager.FillingAssistance.SecureOrigin"
              : "PasswordManager.FillingAssistance.InsecureOrigin",
          *test_case.expectation, 1);

      if (sub_case.is_main_frame_secure && sub_case.is_mixed_form) {
        histogram_tester.ExpectUniqueSample(
            "PasswordManager.FillingAssistance.MixedForm",
            *test_case.expectation, 1);
      }

      std::string account_storage_histogram;
      switch (sub_case.account_storage_usage_level) {
        case PasswordAccountStorageUsageLevel::kNotUsingAccountStorage:
          account_storage_histogram =
              "PasswordManager.FillingAssistance.NotUsingAccountStorage";
          break;
        case PasswordAccountStorageUsageLevel::kUsingAccountStorage:
          account_storage_histogram =
              "PasswordManager.FillingAssistance.UsingAccountStorage";
          break;
        case PasswordAccountStorageUsageLevel::kSyncing:
          account_storage_histogram =
              "PasswordManager.FillingAssistance.Syncing";
          break;
      }
      histogram_tester.ExpectUniqueSample(account_storage_histogram,
                                          *test_case.expectation, 1);
    }
  }
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_NoSubmission) {
  CheckFillingAssistanceTestCase({
      .description_for_logging = "No submission, no histogram recorded",
      .submission_detected = false,
      .fields = {{.value = "user1", .automatically_filled = true},
                 {.value = "password1",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_usernames = {"user1", "user2"},
      .saved_passwords = {"password1", "secret"},
  });
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_NoSuccessfulSubmission) {
  CheckFillingAssistanceTestCase({
      .description_for_logging =
          "No sucessful submission, no histogram recorded",
      .submission_detected = true,
      .submission_is_successful = false,
      .fields = {{.value = "user1", .automatically_filled = true},
                 {.value = "password1",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_usernames = {"user1", "user2"},
      .saved_passwords = {"password1", "secret"},
  });
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_NoSavedCredentials) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "No credentials, even when automatically filled",
       // This case might happen when credentials were filled that the user
       // removed them from the store.
       .fields = {{.value = "user1", .automatically_filled = true},
                  {.value = "password1",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {},
       .saved_passwords = {},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoSavedCredentials});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_PasswordNotTypedNorFilled) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "Form submitted without detected typing or filling",
       .fields = {{.value = ""},
                  {.value = "dummy_password", .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "secret", "password1"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoUserInputNoFillingInPasswordFields});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_AutomaticFilling) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Automatically filled sign-in form",
       .fields = {{.value = "user1", .automatically_filled = true},
                  {.value = "password1",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "secret"},

       .expectation =
           PasswordFormMetricsRecorder::FillingAssistance::kAutomatic});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_ManualFilling) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Manually filled sign-in form",
       .fields = {{.value = "user2", .manually_filled = true},
                  {.value = "password2",
                   .manually_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::kManual});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_AutomaticAndManualFilling) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "Manually filled sign-in form after automatic fill",
       .fields = {{.value = "user2",
                   .automatically_filled = true,
                   .manually_filled = true},
                  {.value = "password2",
                   .automatically_filled = true,
                   .manually_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::kManual});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_UserTypedPassword) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "The user typed into password field",
       .fields = {{.value = "user2", .automatically_filled = true},
                  {.user_input = "password2",
                   .user_typed = true,
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kKnownPasswordTyped});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_UserTypedUsername) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "The user typed into password field",
       .fields = {{.value = "user2", .user_typed = true},
                  {.user_input = "password2",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kUsernameTypedPasswordFilled});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_UserTypedNewCredentials) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "New credentials were typed",
       .fields = {{
                      .value = "user2",
                      .automatically_filled = true,
                  },
                  {.user_input = "password3",
                   .user_typed = true,
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNewPasswordTypedWhileCredentialsExisted});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_ChangePasswordForm) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Change password form",
       .fields =
           {{.value = "old_password",
             .manually_filled = true,
             .is_password = true},
            {.value = "new_password", .user_typed = true, .is_password = true},
            {.value = "new_password", .user_typed = true, .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"old_password", "secret"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::kManual});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_AutomaticallyFilledUserTypedInOtherFields) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "Credentials filled, the user typed in other fields",
       .fields =
           {
               {.value = "12345", .user_typed = true},
               {.value = "user1", .automatically_filled = true},
               {.value = "mail@example.com", .user_typed = true},
               {.value = "password2",
                .automatically_filled = true,
                .is_password = true},
               {.value = "pin", .user_typed = true, .is_password = true},
           },
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation =
           PasswordFormMetricsRecorder::FillingAssistance::kAutomatic});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_ManuallyFilledUserTypedInOtherFields) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "A password filled manually, a username "
                                  "manually, the user typed in other fields",
       .fields =
           {
               {.value = "12345", .user_typed = true},
               {.value = "user1", .automatically_filled = true},
               {.value = "mail@example.com", .user_typed = true},
               {.value = "password2",
                .manually_filled = true,
                .is_password = true},
               {.value = "pin", .user_typed = true, .is_password = true},
           },
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::kManual});
}

TEST_F(PasswordFormMetricsRecorderTest, FillingAssistance_BlocklistedDomain) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Submission while domain is blocklisted",
       .is_blocklisted = true,
       .fields = {{.value = "user1"},
                  {.value = "password1", .is_password = true}},
       .saved_usernames = {},
       .saved_passwords = {},
       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoSavedCredentialsAndBlocklisted});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_BlocklistedDomainWithCredential) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "Submission while domain is blocklisted but a credential is stored",
       .is_blocklisted = true,
       .fields = {{.value = "user1", .automatically_filled = true},
                  {
                      .value = "password1",
                      .automatically_filled = true,
                      .is_password = true,
                  }},
       .saved_usernames = {"user1"},
       .saved_passwords = {"password1"},
       .expectation =
           PasswordFormMetricsRecorder::FillingAssistance::kAutomatic});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_BlocklistedBySmartBubble) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Submission without saved credentials while "
                                  "smart bubble suppresses saving",
       .fields = {{.value = "user1"},
                  {.value = "password1", .is_password = true}},
       .saved_usernames = {},
       .saved_passwords = {},
       .interactions_stats = {{.username_value = u"user1",
                               .dismissal_count = 10}},
       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoSavedCredentialsAndBlocklistedBySmartBubble});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_FilledPasswordMatchesSavedUsername) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "A filled password matches a saved username",
       .fields = {{.value = "secret",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"secret"},
       .saved_passwords = {"secret"},

       .expectation =
           PasswordFormMetricsRecorder::FillingAssistance::kAutomatic});
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_FilledValueMatchesSavedUsernameAndPassword) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "A filled value matches a saved username and password. Field is "
           "likely not a password field",
       .fields = {{.value = "secret", .automatically_filled = true},
                  {.value = "password",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"secret"},
       .saved_passwords = {"secret", "password"},

       .expectation =
           PasswordFormMetricsRecorder::FillingAssistance::kAutomatic});
}

// Tests the calculation of the filling assistance metric for a single username
// form when the username is automatically filled (without any user
// interaction).
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_AutomaticFilling) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames),
       .automatically_filled = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance =
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::kAutomatic;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when the username is manually filled (where the user manually selects
// the suggestions).
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_ManualFilling) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames),
       .manually_filled = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance =
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::kManual;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when the username is automatically and manually filled.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_AutomaticAndManualFilling) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames),
       .automatically_filled = true,
       .manually_filled = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance =
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::kManual;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when a known username is typed without using filling.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_UserTypedKnownUsername) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames), .user_typed = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance = PasswordFormMetricsRecorder::
      SingleUsernameFillingAssistance::kKnownUsernameTyped;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when a known username is filled and edited.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_UserEditedFilledKnownUsername) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames),
       .user_typed = true,
       .automatically_filled = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  // Verify that editing has precedence over filling.
  auto expected_assistance = PasswordFormMetricsRecorder::
      SingleUsernameFillingAssistance::kKnownUsernameTyped;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when a new username is typed while credentials exist.
TEST_F(
    PasswordFormMetricsRecorderTest,
    FillingAssistance_OnSingleUsername_UserTypedNewUsername_WhileCredentials) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = "new-username", .user_typed = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, saved_usernames, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance = PasswordFormMetricsRecorder::
      SingleUsernameFillingAssistance::kNewUsernameTypedWhileCredentialsExisted;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when a new username is typed while there are no credentials.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_NewUsername_WhileNoCredentials) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const std::vector<TestCaseFieldInfo> fields = {
      {.value = "new-username", .user_typed = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, /*saved_usernames=*/{}, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance = PasswordFormMetricsRecorder::
      SingleUsernameFillingAssistance::kNoSavedCredentials;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when a new username is typed when the domain is blocklisted.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_BlocklistedDomain) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const std::vector<TestCaseFieldInfo> fields = {{.value = "new-username"}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, /*saved_usernames=*/{}, /*saved_passwords=*/{},
      /*is_blocklisted=*/true, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance = PasswordFormMetricsRecorder::
      SingleUsernameFillingAssistance::kNoSavedCredentialsAndBlocklisted;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests the calculation of the filling assistance metric for a single username
// form when an existing username is filled while the domain is blocklisted.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_BlocklistedDomain_WithCredentials) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto [saved_usernames, _] = BasicUsernameAndPasswordStoreSets();

  const std::vector<TestCaseFieldInfo> fields = {
      {.value = PickFirstValueInStoreSet(saved_usernames),
       .automatically_filled = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, /*saved_usernames=*/saved_usernames,
      /*saved_passwords=*/{},
      /*is_blocklisted=*/true, /*interactions_stats=*/{},
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance =
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::kAutomatic;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_BlocklistedBySmartBubble) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const std::string username = "user1";

  const std::vector<TestCaseFieldInfo> fields = {{.value = username}};
  PasswordForm password_form_data = ConvertToPasswordForm(fields);

  // Set interactions stats so it blocks autofill.
  std::vector<InteractionsStats> interactions_stats = {
      {.username_value = ASCIIToUTF16(username), .dismissal_count = 10}};

  // Calculate filling assistance metrics for the given form.
  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);
  recorder->CalculateFillingAssistanceMetric(
      password_form_data, /*saved_usernames=*/{}, /*saved_passwords=*/{},
      /*is_blocklisted=*/false, std::move(interactions_stats),
      PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  recorder.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester_.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  auto expected_assistance =
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::
          kNoSavedCredentialsAndBlocklistedBySmartBubble;
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername", expected_assistance,
      1);
  ExpectUkmValueCount(&test_ukm_recorder,
                      UkmEntry::kManagerFill_AssistanceForSingleUsernameName,
                      static_cast<int64_t>(expected_assistance),
                      /*expected_count=*/1);
}

// Tests that if filling assistance is recorded for both a password form and
// single username form (which means that the form mutated or something), the
// filling assistance metric for the password form takes precedence over the one
// for the single username form.
TEST_F(PasswordFormMetricsRecorderTest,
       FillingAssistance_OnSingleUsername_Precedence) {
  auto [saved_usernames, saved_passwords] = BasicUsernameAndPasswordStoreSets();

  auto filled_username_value = PickFirstValueInStoreSet(saved_usernames);

  auto recorder = CreatePasswordFormMetricsRecorder(
      /*is_main_frame_secure=*/true, &pref_service_);

  {
    // 1. Calculate filling assistance metrics when the form is a single
    // username form.
    const std::vector<TestCaseFieldInfo> fields = {
        {.value = filled_username_value, .automatically_filled = true}};
    PasswordForm password_form_data = ConvertToPasswordForm(fields);
    recorder->CalculateFillingAssistanceMetric(
        password_form_data, saved_usernames, saved_passwords,
        /*is_blocklisted=*/false, /*interactions_stats=*/{},
        PasswordAccountStorageUsageLevel::kUsingAccountStorage);
  }

  {
    // 2. Calculate filling assistance metrics when the form is a password form.
    const std::vector<TestCaseFieldInfo> fields = {
        {.value = filled_username_value, .automatically_filled = true},
        {.value = PickFirstValueInStoreSet(saved_passwords),
         .automatically_filled = true,
         .is_password = true}};
    PasswordForm password_form_data = ConvertToPasswordForm(fields);
    recorder->CalculateFillingAssistanceMetric(
        password_form_data, saved_usernames, saved_passwords,
        /*is_blocklisted=*/false, /*interactions_stats=*/{},
        PasswordAccountStorageUsageLevel::kUsingAccountStorage);
    recorder->LogSubmitPassed();
  }

  // Reset the recorder to record the metrics.
  recorder.reset();

  // Verify that the last assistance metric to be calculated is the one
  // recorded.
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.FillingAssistanceForSingleUsername", 0);

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.FillingAssistance",
      PasswordFormMetricsRecorder::FillingAssistance::kAutomatic, 1);
}

#if !BUILDFLAG(IS_IOS)
struct FillingSourceTestCase {
  std::vector<TestCaseFieldInfo> fields;

  std::vector<std::string> saved_profile_usernames;
  std::vector<std::string> saved_profile_passwords;
  std::vector<std::string> saved_account_usernames;
  std::vector<std::string> saved_account_passwords;

  std::optional<PasswordFormMetricsRecorder::FillingSource> expectation;
};

void CheckFillingSourceTestCase(const FillingSourceTestCase& test_case) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  PasswordManager::RegisterProfilePrefs(pref_service.registry());
  base::HistogramTester histogram_tester;

  PasswordForm password_form_data = ConvertToPasswordForm(test_case.fields);

  StoreSet saved_usernames = ConvertToString16AndStoreSet(
      test_case.saved_profile_usernames, test_case.saved_account_usernames);
  StoreSet saved_passwords = ConvertToString16AndStoreSet(
      test_case.saved_profile_passwords, test_case.saved_account_passwords);

  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        /*is_main_frame_secure=*/true, &pref_service);
    recorder->CalculateFillingAssistanceMetric(
        password_form_data, saved_usernames, saved_passwords,
        /*is_blocklisted=*/false,
        /*interactions_stats=*/{},
        PasswordAccountStorageUsageLevel::kUsingAccountStorage);
    recorder->LogSubmitPassed();
  }

  if (test_case.expectation) {
    histogram_tester.ExpectUniqueSample("PasswordManager.FillingSource",
                                        *test_case.expectation, 1);
  } else {
    histogram_tester.ExpectTotalCount("PasswordManager.FillingSource", 0);
  }
}

TEST_F(PasswordFormMetricsRecorderTest, FillingSourceNone) {
  CheckFillingSourceTestCase({
      .fields = {{.value = "manualuser", .automatically_filled = true},
                 {.value = "manualpass",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_profile_usernames = {"profileuser"},
      .saved_profile_passwords = {"profilepass"},
      .saved_account_usernames = {"accountuser"},
      .saved_account_passwords = {"accountpass"},
      .expectation = PasswordFormMetricsRecorder::FillingSource::kNotFilled,
  });
}

TEST_F(PasswordFormMetricsRecorderTest, FillingSourceProfile) {
  CheckFillingSourceTestCase({
      .fields = {{.value = "profileuser", .automatically_filled = true},
                 {.value = "profilepass",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_profile_usernames = {"profileuser"},
      .saved_profile_passwords = {"profilepass"},
      .saved_account_usernames = {"accountuser"},
      .saved_account_passwords = {"accountpass"},
      .expectation =
          PasswordFormMetricsRecorder::FillingSource::kFilledFromProfileStore,
  });
}

TEST_F(PasswordFormMetricsRecorderTest, FillingSourceAccount) {
  CheckFillingSourceTestCase({
      .fields = {{.value = "accountuser", .automatically_filled = true},
                 {.value = "accountpass",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_profile_usernames = {"profileuser"},
      .saved_profile_passwords = {"profilepass"},
      .saved_account_usernames = {"accountuser"},
      .saved_account_passwords = {"accountpass"},
      .expectation =
          PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore,
  });
}

TEST_F(PasswordFormMetricsRecorderTest, FillingSourceBoth) {
  CheckFillingSourceTestCase({
      .fields = {{.value = "user", .automatically_filled = true},
                 {.value = "pass",
                  .automatically_filled = true,
                  .is_password = true}},
      .saved_profile_usernames = {"user"},
      .saved_profile_passwords = {"pass"},
      .saved_account_usernames = {"user"},
      .saved_account_passwords = {"pass"},
      .expectation =
          PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores,
  });
}

TEST_F(PasswordFormMetricsRecorderTest, FillingSourceBothDifferent) {
  // This test covers a rare edge case: If a password from the profile store and
  // a *different* password from the account store were both filled, then this
  // should also be recorded as kFilledFromBothStores.
  CheckFillingSourceTestCase({
      .fields = {{.value = "profileuser", .manually_filled = true},
                 {.value = "profilepass",
                  .manually_filled = true,
                  .is_password = true},
                 {.value = "accountuser", .manually_filled = true},
                 {.value = "accountpass",
                  .manually_filled = true,
                  .is_password = true}},
      .saved_profile_usernames = {"profileuser"},
      .saved_profile_passwords = {"profilepass"},
      .saved_account_usernames = {"accountuser"},
      .saved_account_passwords = {"accountpass"},
      .expectation =
          PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores,
  });
}

TEST_F(PasswordFormMetricsRecorderTest, StoresUsedForFillingInLast7And28Days) {
  StoreSet saved_usernames =
      ConvertToString16AndStoreSet({"profileuser"}, {"accountuser"});
  StoreSet saved_passwords =
      ConvertToString16AndStoreSet({"profilepass"}, {"accountpass"});

  // Phase 1: The user manually enters a credential that's not stored.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "user", .manually_filled = true},
         {.value = "pass", .manually_filled = true, .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
  }

  // Phase 2: A credential from the account store is filled.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "accountuser", .automatically_filled = true},
         {.value = "accountpass",
          .automatically_filled = true,
          .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
  }

  // Phase 3: A credential from the profile store is filled.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "profileuser", .automatically_filled = true},
         {.value = "profilepass",
          .automatically_filled = true,
          .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromProfileStore, 1);
    // Even though this credential came from the profile store, a credential
    // from the account store was also filled recently (in phase 2).
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
  }

  // Phase 4: The user again manually enters a credential that's not stored.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "user", .manually_filled = true},
         {.value = "pass", .manually_filled = true, .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    // Even though this credential did not come from either store, both stores
    // were used recently (in phases 2 and 3).
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
  }
}

TEST_F(PasswordFormMetricsRecorderTest,
       StoresUsedForFillingInLast7And28DaysExpiry) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());

  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_usernames =
      ConvertToString16AndStoreSet({"profileuser"}, {"accountuser"});
  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_passwords =
      ConvertToString16AndStoreSet({"profilepass"}, {"accountpass"});

  // Day 0: A credential from the profile store is filled.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "profileuser", .automatically_filled = true},
         {.value = "profilepass",
          .automatically_filled = true,
          .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->set_clock_for_testing(&clock);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromProfileStore, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromProfileStore, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromProfileStore, 1);
  }

  clock.Advance(base::Days(2));

  // Day 2: A credential from the account store is filled.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "accountuser", .automatically_filled = true},
         {.value = "accountpass",
          .automatically_filled = true,
          .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->set_clock_for_testing(&clock);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
    // Even though this credential came from the account store, a credential
    // from the profile store was also filled recently.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
  }

  clock.Advance(base::Days(6));

  // Day 8: A credential from the account store is filled (again).
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "accountuser", .automatically_filled = true},
         {.value = "accountpass",
          .automatically_filled = true,
          .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->set_clock_for_testing(&clock);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
    // A credential from the profile store was last filled 8 days ago, so this
    // still shows up in the 28-day histogram but not the 7-day one.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromBothStores, 1);
  }

  clock.Advance(base::Days(27));

  // Day 35: The user manually enters a credential that's not stored.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "user", .manually_filled = true},
         {.value = "pass", .manually_filled = true, .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->set_clock_for_testing(&clock);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    // The account store was used 27 days ago, so is still relevant for the
    // 28-day histogram. The profile store was last used 35 days ago, so it's
    // gone now.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kFilledFromAccountStore, 1);
  }

  clock.Advance(base::Days(2));

  // Day 37: The user again manually enters a credential that's not stored.
  {
    base::HistogramTester histogram_tester;

    PasswordForm password_form_data = ConvertToPasswordForm(
        {{.value = "user", .manually_filled = true},
         {.value = "pass", .manually_filled = true, .is_password = true}});
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure=*/true, &pref_service_);
      recorder->set_clock_for_testing(&clock);
      recorder->CalculateFillingAssistanceMetric(
          password_form_data, saved_usernames, saved_passwords,
          /*is_blocklisted=*/false,
          /*interactions_stats=*/{},
          PasswordAccountStorageUsageLevel::kUsingAccountStorage);
      recorder->LogSubmitPassed();
    }

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.FillingSource",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    // Now both usages are > 28 days ago.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast7Days",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.StoresUsedForFillingInLast28Days",
        PasswordFormMetricsRecorder::FillingSource::kNotFilled, 1);
  }
}
#endif

// Verify that the difference between parsing during filling and saving is
// calculated and recorded correctly when form parsing doesn't change.
TEST_F(PasswordFormMetricsRecorderTest, FormParsingDifferenceNone) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        /*is_main_frame_secure=*/true, &pref_service_);
    PasswordForm form;
    form.username_element_renderer_id = FieldRendererId(1);
    form.password_element_renderer_id = FieldRendererId(2);
    recorder->CacheParsingResultInFillingMode(form);
    // Imitate the form did not change before submission.
    recorder->CalculateParsingDifferenceOnSavingAndFilling(form);
    recorder->LogSubmitPassed();
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], UkmEntry::kParsingDiffFillingAndSavingName,
      static_cast<int>(PasswordFormMetricsRecorder::ParsingDifference::kNone));
}

// Verify that the difference between parsing during filling and saving is
// calculated and recorded correctly when the username field is detected
// differently.
TEST_F(PasswordFormMetricsRecorderTest, FormParsingDifferenceUsername) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        /*is_main_frame_secure=*/true, &pref_service_);
    PasswordForm form;
    form.username_element_renderer_id = FieldRendererId(2);
    form.password_element_renderer_id = FieldRendererId(3);
    recorder->CacheParsingResultInFillingMode(form);
    // Update the username field before submission.
    form.username_element_renderer_id = FieldRendererId(1);
    recorder->CalculateParsingDifferenceOnSavingAndFilling(form);
    recorder->LogSubmitPassed();
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], UkmEntry::kParsingDiffFillingAndSavingName,
      static_cast<int>(
          PasswordFormMetricsRecorder::ParsingDifference::kUsernameDiff));
}

// Verify that the difference between parsing during filling and saving is
// calculated and recorded correctly when password fields are detected
// differently.
TEST_F(PasswordFormMetricsRecorderTest, FormParsingDifferencePassword) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        /*is_main_frame_secure=*/true, &pref_service_);
    PasswordForm form;
    form.username_element_renderer_id = FieldRendererId(1);
    form.password_element_renderer_id = FieldRendererId(2);
    recorder->CacheParsingResultInFillingMode(form);
    // Update password parsing result before submission.
    form.password_element_renderer_id = FieldRendererId(0);
    form.new_password_element_renderer_id = FieldRendererId(2);
    recorder->CalculateParsingDifferenceOnSavingAndFilling(form);
    recorder->LogSubmitPassed();
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], UkmEntry::kParsingDiffFillingAndSavingName,
      static_cast<int>(
          PasswordFormMetricsRecorder::ParsingDifference::kPasswordDiff));
}

TEST_F(PasswordFormMetricsRecorderTest, AutomationRate) {
  const std::vector<TestCaseFieldInfo> form_fields = {
      // Placeholder field should be ignored, as it had not user interaction.
      {.value = "placeholder"},
      {.value = "username", .user_typed = true},
      {.value = "OTP", .user_typed = true},
      // The only autofilled field is password field.
      {.value = "strong_password",
       .manually_filled = true,
       .is_password = true}};
  PasswordForm password_form_data = ConvertToPasswordForm(form_fields);

  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        /*is_main_frame_secure=*/true, &pref_service_);
    recorder->CalculateFillingAssistanceMetric(
        password_form_data, /*saved_usernames=*/{}, /*saved_passwords=*/{},
        /*is_blocklisted=*/false,
        /*interactions_stats=*/{},
        PasswordAccountStorageUsageLevel::kUsingAccountStorage);
    recorder->LogSubmitPassed();
  }

  int expected_rate =
      100 * form_fields[3].value.size() /
      (form_fields[1].value.size() + form_fields[2].value.size() +
       form_fields[3].value.size());
  histogram_tester_.ExpectUniqueSample("PasswordManager.FillingAutomationRate",
                                       expected_rate, 1);
}

}  // namespace password_manager

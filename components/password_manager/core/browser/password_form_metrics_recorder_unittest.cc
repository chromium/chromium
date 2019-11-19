// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

constexpr ukm::SourceId kTestSourceId = 0x1234;

using UkmEntry = ukm::builders::PasswordForm;

// Create a UkmEntryBuilder with kTestSourceId.
scoped_refptr<PasswordFormMetricsRecorder> CreatePasswordFormMetricsRecorder(
    bool is_main_frame_secure,
    ukm::TestUkmRecorder* test_ukm_recorder) {
  return base::MakeRefCounted<PasswordFormMetricsRecorder>(is_main_frame_secure,
                                                           kTestSourceId);
}

// TODO(crbug.com/738921) Replace this with generalized infrastructure.
// Verifies that the metric |metric_name| was recorded with value |value| in the
// single entry of |test_ukm_recorder_| exactly |expected_count| times.
void ExpectUkmValueCount(ukm::TestUkmRecorder* test_ukm_recorder,
                         const char* metric_name,
                         int64_t value,
                         int64_t expected_count) {
  auto entries = test_ukm_recorder->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    if (expected_count) {
      test_ukm_recorder->ExpectEntryMetric(entry, metric_name, value);
    } else {
      const int64_t* value =
          test_ukm_recorder->GetEntryMetric(entry, metric_name);
      EXPECT_TRUE(value == nullptr || *value != expected_count);
    }
  }
}

}  // namespace

// Test the metrics recorded around password generation and the user's
// interaction with the offer to generate passwords.
TEST(PasswordFormMetricsRecorder, Generation) {
  base::test::TaskEnvironment task_environment_;
  static constexpr struct {
    bool generation_available;
    bool has_generated_password;
    PasswordFormMetricsRecorder::SubmitResult submission;
  } kTests[] = {
      {false, false, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultNotSubmitted},
      {false, false, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultFailed},
      {false, false, PasswordFormMetricsRecorder::kSubmitResultPassed},
      {true, false, PasswordFormMetricsRecorder::kSubmitResultPassed},
      {true, true, PasswordFormMetricsRecorder::kSubmitResultPassed},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "generation_available=" << test.generation_available
                 << ", has_generated_password=" << test.has_generated_password
                 << ", submission=" << test.submission);

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          /*is_main_frame_secure*/ true, &test_ukm_recorder);
      if (test.generation_available)
        recorder->MarkGenerationAvailable();
      if (test.has_generated_password) {
        recorder->SetGeneratedPasswordStatus(
            PasswordFormMetricsRecorder::GeneratedPasswordStatus::
                kPasswordAccepted);
      }
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          // Do nothing.
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          recorder->LogSubmitFailed();
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          recorder->LogSubmitPassed();
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }

    ExpectUkmValueCount(
        &test_ukm_recorder, UkmEntry::kSubmission_ObservedName,
        test.submission !=
                PasswordFormMetricsRecorder::kSubmitResultNotSubmitted
            ? 1
            : 0,
        1);

    int expected_login_failed =
        test.submission == PasswordFormMetricsRecorder::kSubmitResultFailed ? 1
                                                                            : 0;
    EXPECT_EQ(expected_login_failed,
              user_action_tester.GetActionCount("PasswordManager_LoginFailed"));
    ExpectUkmValueCount(&test_ukm_recorder,
                        UkmEntry::kSubmission_SubmissionResultName,
                        PasswordFormMetricsRecorder::kSubmitResultFailed,
                        expected_login_failed);

    int expected_login_passed =
        test.submission == PasswordFormMetricsRecorder::kSubmitResultPassed ? 1
                                                                            : 0;
    EXPECT_EQ(expected_login_passed,
              user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
    ExpectUkmValueCount(&test_ukm_recorder,
                        UkmEntry::kSubmission_SubmissionResultName,
                        PasswordFormMetricsRecorder::kSubmitResultPassed,
                        expected_login_passed);

    if (test.has_generated_password) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::GENERATED_PASSWORD_FORCE_SAVED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }

    if (!test.has_generated_password && test.generation_available) {
      switch (test.submission) {
        case PasswordFormMetricsRecorder::kSubmitResultNotSubmitted:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_NOT_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultFailed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMISSION_FAILED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultPassed:
          histogram_tester.ExpectBucketCount(
              "PasswordGeneration.SubmissionAvailableEvent",
              metrics_util::PASSWORD_SUBMITTED, 1);
          break;
        case PasswordFormMetricsRecorder::kSubmitResultMax:
          NOTREACHED();
      }
    }
  }
}

// Test the recording of metrics around manager_action, user_action, and
// submit_result.
TEST(PasswordFormMetricsRecorder, Actions) {
  base::test::TaskEnvironment task_environment_;
  static constexpr struct {
    // Stimuli:
    bool is_main_frame_secure;
    PasswordFormMetricsRecorder::ManagerAction manager_action;
    UserAction user_action;
    PasswordFormMetricsRecorder::SubmitResult submit_result;
    // Expectations:
    // Histogram bucket for PasswordManager.ActionsTakenV3 and
    // PasswordManager.ActionsTakenOnNonSecureForm.
    int32_t actions_taken;
    // Result of GetActionsTakenNew.
    int32_t actions_taken_new;
  } kTests[] = {
      // Test values of manager_action.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 0, 0},
      {true, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 5, 5},
      // Test values of user_action.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kChoose /*1*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 1, 1},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kChoosePslMatch /*2*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 2, 2},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kOverridePassword /*3*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 3, 3},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultNotSubmitted /*0*/, 4, 4},
      // Test values of submit_result.
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*1*/, 15, 10},
      {true, PasswordFormMetricsRecorder::kManagerActionNone /*0*/,
       UserAction::kNone /*0*/,
       PasswordFormMetricsRecorder::kSubmitResultPassed /*2*/, 30, 20},
      // Test combination.
      {true, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*2*/, 24, 19},
      // Test non-secure form.
      {false, PasswordFormMetricsRecorder::kManagerActionAutofilled /*1*/,
       UserAction::kOverrideUsernameAndPassword /*4*/,
       PasswordFormMetricsRecorder::kSubmitResultFailed /*2*/, 24, 19},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "is_main_frame_secure=" << test.is_main_frame_secure
                 << ", manager_action=" << test.manager_action
                 << ", user_action=" << static_cast<int>(test.user_action)
                 << ", submit_result=" << test.submit_result);

    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          test.is_main_frame_secure, &test_ukm_recorder);

      recorder->SetManagerAction(test.manager_action);
      recorder->SetUserActionForTesting(test.user_action);
      if (test.submit_result ==
          PasswordFormMetricsRecorder::kSubmitResultFailed) {
        recorder->LogSubmitFailed();
      } else if (test.submit_result ==
                 PasswordFormMetricsRecorder::kSubmitResultPassed) {
        recorder->LogSubmitPassed();
      }

      EXPECT_EQ(test.actions_taken_new, recorder->GetActionsTakenNew());
    }

    EXPECT_THAT(
        histogram_tester.GetAllSamples("PasswordManager.ActionsTakenV3"),
        ::testing::ElementsAre(base::Bucket(test.actions_taken, 1)));
    if (!test.is_main_frame_secure) {
      EXPECT_THAT(histogram_tester.GetAllSamples(
                      "PasswordManager.ActionsTakenOnNonSecureForm"),
                  ::testing::ElementsAre(base::Bucket(test.actions_taken, 1)));
    }

    ExpectUkmValueCount(&test_ukm_recorder,
                        UkmEntry::kUser_ActionSimplifiedName,
                        static_cast<int64_t>(test.user_action), 1);
  }
}

TEST(PasswordFormMetricsRecorder, SubmittedFormType) {
  base::test::TaskEnvironment task_environment_;
  static constexpr struct {
    // Stimuli:
    bool is_main_frame_secure;
    PasswordFormMetricsRecorder::SubmittedFormType form_type;
    // Expectations:
    // Expectation for PasswordManager.SubmittedFormType:
    int expected_submitted_form_type;
    // Expectation for PasswordManager.SubmittedNonSecureFormType:
    int expected_submitted_non_secure_form_type;
  } kTests[] = {
      {false, PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified, 0, 0},
      {true, PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified, 0, 0},
      {false, PasswordFormMetricsRecorder::kSubmittedFormTypeLogin, 1, 1},
      {true, PasswordFormMetricsRecorder::kSubmittedFormTypeLogin, 1, 0},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "is_main_frame_secure=" << test.is_main_frame_secure
                 << ", form_type=" << test.form_type);

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    base::HistogramTester histogram_tester;

    // Use a scoped PasswordFromMetricsRecorder because some metrics are recored
    // on destruction.
    {
      auto recorder = CreatePasswordFormMetricsRecorder(
          test.is_main_frame_secure, &test_ukm_recorder);
      recorder->SetSubmittedFormType(test.form_type);
    }

    if (test.form_type !=
        PasswordFormMetricsRecorder::kSubmittedFormTypeUnspecified) {
      ExpectUkmValueCount(&test_ukm_recorder,
                          UkmEntry::kSubmission_SubmittedFormTypeName,
                          test.form_type, 1);
    }

    if (test.expected_submitted_form_type) {
      histogram_tester.ExpectBucketCount("PasswordManager.SubmittedFormType",
                                         test.form_type,
                                         test.expected_submitted_form_type);
    } else {
      histogram_tester.ExpectTotalCount("PasswordManager.SubmittedFormType", 0);
    }

    if (test.expected_submitted_non_secure_form_type) {
      histogram_tester.ExpectBucketCount(
          "PasswordManager.SubmittedNonSecureFormType", test.form_type,
          test.expected_submitted_non_secure_form_type);
    } else {
      histogram_tester.ExpectTotalCount(
          "PasswordManager.SubmittedNonSecureFormType", 0);
    }
  }
}

TEST(PasswordFormMetricsRecorder, RecordPasswordBubbleShown) {
  base::test::TaskEnvironment task_environment_;
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
          true /*is_main_frame_secure*/, &test_ukm_recorder);
      recorder->RecordPasswordBubbleShown(test.credential_source_type,
                                          test.display_disposition);
    }
    // Verify data
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
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

TEST(PasswordFormMetricsRecorder, RecordUIDismissalReason) {
  base::test::TaskEnvironment task_environment_;
  static constexpr struct {
    // Stimuli:
    metrics_util::UIDisplayDisposition display_disposition;
    metrics_util::UIDismissalReason dismissal_reason;
    // Expectations:
    const char* expected_trigger_metric;
    PasswordFormMetricsRecorder::BubbleDismissalReason expected_metric_value;
  } kTests[] = {
      {metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING,
       metrics_util::CLICKED_SAVE, UkmEntry::kSaving_Prompt_InteractionName,
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
          true /*is_main_frame_secure*/, &test_ukm_recorder);
      recorder->RecordPasswordBubbleShown(
          metrics_util::CredentialSourceType::kPasswordManager,
          test.display_disposition);
      recorder->RecordUIDismissalReason(test.dismissal_reason);
    }
    // Verify data
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      EXPECT_EQ(kTestSourceId, entry->source_id);
      test_ukm_recorder.ExpectEntryMetric(
          entry, test.expected_trigger_metric,
          static_cast<int64_t>(test.expected_metric_value));
    }
  }
}

// Verify that it is ok to open and close the password bubble more than once
// and still get accurate metrics.
TEST(PasswordFormMetricsRecorder, SequencesOfBubbles) {
  base::test::TaskEnvironment task_environment_;
  using BubbleDismissalReason =
      PasswordFormMetricsRecorder::BubbleDismissalReason;
  using BubbleTrigger = PasswordFormMetricsRecorder::BubbleTrigger;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &test_ukm_recorder);
    // Open and confirm an automatically triggered saving prompt.
    recorder->RecordPasswordBubbleShown(
        metrics_util::CredentialSourceType::kPasswordManager,
        metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
    recorder->RecordUIDismissalReason(metrics_util::CLICKED_SAVE);
    // Open and confirm a manually triggered update prompt.
    recorder->RecordPasswordBubbleShown(
        metrics_util::CredentialSourceType::kPasswordManager,
        metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE);
    recorder->RecordUIDismissalReason(metrics_util::CLICKED_SAVE);
  }
  // Verify recorded UKM data.
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
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
TEST(PasswordFormMetricsRecorder, RecordDetailedUserAction) {
  base::test::TaskEnvironment task_environment_;
  using Action = PasswordFormMetricsRecorder::DetailedUserAction;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &test_ukm_recorder);
    recorder->RecordDetailedUserAction(Action::kCorrectedUsernameInForm);
    recorder->RecordDetailedUserAction(Action::kCorrectedUsernameInForm);
    recorder->RecordDetailedUserAction(Action::kEditedUsernameInBubble);
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* const entry : entries) {
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
TEST(PasswordFormMetricsRecorder, RecordShowManualFallbackForSaving) {
  base::test::TaskEnvironment task_environment_;
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
          true /*is_main_frame_secure*/, &test_ukm_recorder);
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
TEST(PasswordFormMetricsRecorder, NoRecordShowManualFallbackForSaving) {
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &test_ukm_recorder);
  }
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTestSourceId, entries[0]->source_id);
  EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
      entries[0], UkmEntry::kSaving_ShowedManualFallbackForSavingName));
}

// Verify that only the latest value is recorded
TEST(PasswordFormMetricsRecorder, RecordShowManualFallbackForSavingLatestOnly) {
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    auto recorder = CreatePasswordFormMetricsRecorder(
        true /*is_main_frame_secure*/, &test_ukm_recorder);
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

TEST(PasswordFormMetricsRecorder, FormChangeBitmapNoMetricRecorded) {
  base::HistogramTester histogram_tester;
  auto recorder =
      CreatePasswordFormMetricsRecorder(true /*is_main_frame_secure*/, nullptr);
  recorder.reset();
  histogram_tester.ExpectTotalCount("PasswordManager.DynamicFormChanges", 0);
}

TEST(PasswordFormMetricsRecorder, FormChangeBitmapRecordedOnce) {
  base::HistogramTester histogram_tester;
  auto recorder =
      CreatePasswordFormMetricsRecorder(true /*is_main_frame_secure*/, nullptr);
  recorder->RecordFormChangeBitmask(PasswordFormMetricsRecorder::kFieldsNumber);
  recorder.reset();
  histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                      1 /* kFieldsNumber */, 1);
}

TEST(PasswordFormMetricsRecorder, FormChangeBitmapRecordedMultipleTimes) {
  base::HistogramTester histogram_tester;
  auto recorder =
      CreatePasswordFormMetricsRecorder(true /*is_main_frame_secure*/, nullptr);
  recorder->RecordFormChangeBitmask(PasswordFormMetricsRecorder::kFieldsNumber);
  recorder->RecordFormChangeBitmask(
      PasswordFormMetricsRecorder::kFormControlTypes);
  recorder.reset();
  uint32_t expected = 1 /* fields number */ | (1 << 3) /* control types */;
  histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                      expected, 1);
}

// todo add namespace

struct FillingAssistanceTestCase {
  struct FieldInfo {
    std::string value;
    std::string typed_value;
    bool user_typed = false;
    bool automatically_filled = false;
    bool manually_filled = false;
    bool is_password = false;
  };

  const char* description_for_logging;

  bool is_blacklisted = false;
  bool submission_detected = true;
  bool submission_is_successful = true;

  std::vector<FieldInfo> fields;
  std::vector<std::string> saved_usernames;
  std::vector<std::string> saved_passwords;
  std::vector<InteractionsStats> interactions_stats;

  base::Optional<PasswordFormMetricsRecorder::FillingAssistance> expectation;
};

FormData ConvertToFormData(
    const std::vector<FillingAssistanceTestCase::FieldInfo>& fields) {
  FormData form;
  for (const auto& field : fields) {
    FormFieldData form_field;
    form_field.value = ASCIIToUTF16(field.value);
    form_field.typed_value = ASCIIToUTF16(field.typed_value);

    if (field.user_typed)
      form_field.properties_mask |= FieldPropertiesFlags::USER_TYPED;

    if (field.manually_filled)
      form_field.properties_mask |=
          FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER;

    if (field.automatically_filled)
      form_field.properties_mask |=
          FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD;

    form_field.form_control_type = field.is_password ? "password" : "text";

    form.fields.push_back(form_field);
  }
  return form;
}

std::set<base::string16> ConvertToString16Set(
    const std::vector<std::string>& v) {
  std::set<base::string16> result;
  for (const std::string& str : v)
    result.insert(ASCIIToUTF16(str));
  return result;
}

void CheckFillingAssistanceTestCase(
    const FillingAssistanceTestCase& test_case) {
  for (bool is_main_frame_secure : {false, true}) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description_for_logging
                 << ", is_main_frame_secure: " << std::boolalpha
                 << is_main_frame_secure);

    base::test::TaskEnvironment task_environment_;
    base::HistogramTester histogram_tester;

    FormData form_data = ConvertToFormData(test_case.fields);
    std::set<base::string16> saved_usernames =
        ConvertToString16Set(test_case.saved_usernames);
    std::set<base::string16> saved_passwords =
        ConvertToString16Set(test_case.saved_passwords);

    auto recorder =
        CreatePasswordFormMetricsRecorder(is_main_frame_secure, nullptr);
    if (test_case.submission_detected) {
      recorder->CalculateFillingAssistanceMetric(
          form_data, saved_usernames, saved_passwords, test_case.is_blacklisted,
          test_case.interactions_stats);
    }

    if (test_case.submission_is_successful)
      recorder->LogSubmitPassed();
    recorder.reset();

    int expected_count = test_case.expectation ? 1 : 0;
    int expected_insecure_count = !is_main_frame_secure ? expected_count : 0;
    int expected_secure_count = is_main_frame_secure ? expected_count : 0;
    histogram_tester.ExpectTotalCount("PasswordManager.FillingAssistance",
                                      expected_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.InsecureOrigin",
        expected_insecure_count);
    histogram_tester.ExpectTotalCount(
        "PasswordManager.FillingAssistance.SecureOrigin",
        expected_secure_count);
    if (test_case.expectation) {
      histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                          *test_case.expectation, 1);
      histogram_tester.ExpectUniqueSample(
          is_main_frame_secure
              ? "PasswordManager.FillingAssistance.SecureOrigin"
              : "PasswordManager.FillingAssistance.InsecureOrigin",
          *test_case.expectation, 1);
    }
  }
}

TEST(PasswordFormMetricsRecorder, FillingAssistanceNoSubmission) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceNoSuccessfulSubmission) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceNoSavedCredentials) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceEmptyForm) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Weird form submitted without values",
       .fields = {{.value = ""}, {.value = "", .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "secret", "password1"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoUserInputNoFillingInPasswordFields});
}

TEST(PasswordFormMetricsRecorder, FillingAssistanceAutomaticFilling) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceManualFilling) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceAutomaticAndManualFilling) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceUserTypedPassword) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "The user typed into password field",
       .fields = {{.value = "user2", .automatically_filled = true},
                  {.typed_value = "password2",
                   .user_typed = true,
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kKnownPasswordTyped});
}

TEST(PasswordFormMetricsRecorder, FillingAssistanceUserTypedUsername) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "The user typed into password field",
       .fields = {{.value = "user2", .user_typed = true},
                  {.typed_value = "password2",
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kUsernameTypedPasswordFilled});
}

TEST(PasswordFormMetricsRecorder, FillingAssistanceUserTypedNewCredentials) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "New credentials were typed",
       .fields = {{
                      .value = "user2",
                      .automatically_filled = true,
                  },
                  {.typed_value = "password3",
                   .user_typed = true,
                   .automatically_filled = true,
                   .is_password = true}},
       .saved_usernames = {"user1", "user2"},
       .saved_passwords = {"password1", "password2"},

       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNewPasswordTypedWhileCredentialsExisted});
}

TEST(PasswordFormMetricsRecorder, FillingAssistanceChangePasswordForm) {
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

TEST(PasswordFormMetricsRecorder,
     FillingAssistanceAutomaticallyFilledUserTypedInOtherFields) {
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

TEST(PasswordFormMetricsRecorder,
     FillingAssistanceManuallyFilledUserTypedInOtherFields) {
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceBlacklistedDomain) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Submission while domain is blacklisted",
       .is_blacklisted = true,
       .fields = {{.value = "user1"},
                  {.value = "password1", .is_password = true}},
       .saved_usernames = {},
       .saved_passwords = {},
       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoSavedCredentialsAndBlacklisted});
}

TEST(PasswordFormMetricsRecorder,
     FillingAssistanceBlacklistedDomainWithCredential) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging =
           "Submission while domain is blacklisted but a credential is stored",
       .is_blacklisted = true,
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

TEST(PasswordFormMetricsRecorder, FillingAssistanceBlacklistedBySmartBubble) {
  CheckFillingAssistanceTestCase(
      {.description_for_logging = "Submission without saved credentials while "
                                  "smart bubble suppresses saving",
       .fields = {{.value = "user1"},
                  {.value = "password1", .is_password = true}},
       .saved_usernames = {},
       .saved_passwords = {},
       .interactions_stats = {{.username_value = ASCIIToUTF16("user1"),
                               .dismissal_count = 10}},
       .expectation = PasswordFormMetricsRecorder::FillingAssistance::
           kNoSavedCredentialsAndBlacklistedBySmartBubble});
}

}  // namespace password_manager

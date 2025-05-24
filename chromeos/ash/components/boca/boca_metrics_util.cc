// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca {

void RecordOnTaskLockedStateDurationPercentage(
    base::TimeDelta unlocked_state_duration,
    base::TimeDelta locked_state_duration) {
  const base::TimeDelta total_time =
      unlocked_state_duration + locked_state_duration;
  if (total_time.is_zero()) {
    return;
  }
  const double percentage_locked = 100.0 * (locked_state_duration / total_time);
  const double percentage_unlocked = 100.0 - percentage_locked;
  UMA_HISTOGRAM_PERCENTAGE(kBocaOnTaskLockedSessionDurationPercentage,
                           percentage_locked);
  UMA_HISTOGRAM_PERCENTAGE(kBocaOnTaskUnlockedSessionDurationPercentage,
                           percentage_unlocked);
}

void RecordNumOfStudentsJoinedViaCodeDuringSession(
    int num_of_students_joined_via_code) {
  UMA_HISTOGRAM_COUNTS_1000(kBocaNumOfStudentsJoinedViaCodeDuringSession,
                            num_of_students_joined_via_code);
}

void RecordNumOfActiveStudentsWhenSessionEnded(int num_of_active_students) {
  UMA_HISTOGRAM_COUNTS_1000(kBocaNumOfActiveStudentsWhenSessionEnded,
                            num_of_active_students);
}

void RecordOnTaskNumOfTabsWhenSessionEnded(int num_of_tabs) {
  UMA_HISTOGRAM_COUNTS_100(kBocaOnTaskNumOfTabsWhenSessionEnded, num_of_tabs);
}

void RecordOnTaskMaxNumOfTabsDuringSession(int max_num_of_tabs) {
  UMA_HISTOGRAM_COUNTS_100(kBocaOnTaskMaxNumOfTabsDuringSession,
                           max_num_of_tabs);
}

void RecordStudentJoinedSession() {
  base::RecordAction(
      base::UserMetricsAction(kBocaActionOfStudentJoinedSession));
}

void RecordBabelOrcaTranslationLanguage(const std::string& language) {
  base::UmaHistogramSparse(kBocaBabelorcaTargetLanguage,
                           base::HashMetricName(language));
}

void RecordBabelOrcaTranslationLanguageSwitched() {
  base::RecordAction(
      base::UserMetricsAction(kBocaBabelorcaActionOfStudentSwitchLanguage));
}

void RecordOnTaskPodNavigateBackClicked() {
  base::RecordAction(
      base::UserMetricsAction(kBocaOnTaskActionOfStudentNavigateBack));
}

void RecordOnTaskPodNavigateForwardClicked() {
  base::RecordAction(
      base::UserMetricsAction(kBocaOnTaskActionOfStudentNavigateForward));
}

void RecordOnTaskPodReloadPageClicked() {
  base::RecordAction(
      base::UserMetricsAction(kBocaOnTaskActionOfStudentReloadPage));
}

void RecordOnTaskPodToggleTabStripVisibilityClicked() {
  base::RecordAction(base::UserMetricsAction(
      kBocaOnTaskActionOfStudentToggleTabStripVisibility));
}

void RecordOnTaskPodSetSnapLocationClicked(bool is_left) {
  if (is_left) {
    base::RecordAction(base::UserMetricsAction(
        kBocaOnTaskActionOfStudentSetSnapLocationToLeft));
  } else {
    base::RecordAction(base::UserMetricsAction(
        kBocaOnTaskActionOfStudentSetSnapLocationToRight));
  }
}

void RecordOnRegisterScreenRequestSentErrorCode(
    google_apis::ApiErrorCode error_code) {
  RecordSpotlightGoogleApiErrorCode(kBocaSpotlightOnRegisterScreenRequestSent,
                                    error_code);
}

void RecordViewStudentScreenErrorCode(google_apis::ApiErrorCode error_code) {
  RecordSpotlightGoogleApiErrorCode(kBocaSpotlightViewStudentScreen,
                                    error_code);
}

void RecordEndViewStudentScreenErrorCode(google_apis::ApiErrorCode error_code) {
  RecordSpotlightGoogleApiErrorCode(kBocaSpotlightEndViewStudentScreen,
                                    error_code);
}

void RecordSetViewScreenSessionActiveErrorCode(
    google_apis::ApiErrorCode error_code) {
  RecordSpotlightGoogleApiErrorCode(kBocaSpotlightSetViewScreenSessionActive,
                                    error_code);
}

void RecordSpotlightGoogleApiErrorCode(const std::string& name,
                                       google_apis::ApiErrorCode error_code) {
  base::UmaHistogramSparse(
      base::ReplaceStringPlaceholders(
          kBocaSpotlightGoogleApiCallErrorCodeTemplate, {name},
          /*=offsets*/ nullptr),
      error_code);
}

void RecordGetSessionErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaGetSession, error_code);
}

void RecordCreateSessionErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaCreateSession, error_code);
}

void RecordEndSessionErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaEndSession, error_code);
}

void RecordUpdateSessionErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaUpdateSession, error_code);
}

void RecordJoinSessionViaAccessCodeErrorCode(
    google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaJoinSessionViaAccessCode, error_code);
}

void RecordUpdateCaptionErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaUpdateCaption, error_code);
}

void RecordAddStudentsErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaAddStudents, error_code);
}

void RecordRemoveStudentErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaRemoveStudent, error_code);
}

void RecordUpdateStudentActivitiesErrorCode(
    google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaUpdateStudentActivities, error_code);
}

void RecordStudentHeartBeatErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaStudentHeartbeat, error_code);
}

void RecordUploadTokenErrorCode(google_apis::ApiErrorCode error_code) {
  RecordGoogleApiErrorCode(kBocaUploadToken, error_code);
}

void RecordGoogleApiErrorCode(const std::string& name,
                              google_apis::ApiErrorCode error_code) {
  base::UmaHistogramSparse(base::ReplaceStringPlaceholders(
                               kBocaGoogleApiCallErrorCodeTemplate, {name},
                               /*=offsets*/ nullptr),
                           error_code);
}

void RecordPollingResult(const ::boca::Session* previous_session,
                         const ::boca::Session* current_session) {
  BocaSessionManager::BocaPollingResult polling_result;
  if (!previous_session && !current_session) {
    polling_result = BocaSessionManager::BocaPollingResult::kNoUpdate;
  } else if (!previous_session) {
    polling_result = BocaSessionManager::BocaPollingResult::kSessionStart;
  } else if (!current_session) {
    polling_result = BocaSessionManager::BocaPollingResult::kSessionEnd;
  } else if (previous_session->SerializeAsString() !=
             current_session->SerializeAsString()) {
    polling_result = BocaSessionManager::BocaPollingResult::kInSessionUpdate;
  } else {
    polling_result = BocaSessionManager::BocaPollingResult::kNoUpdate;
  }
  base::UmaHistogramEnumeration(kPollingResult, polling_result);
}

void RecordTokenRetrievalIsValidation(const bool is_validation) {
  base::UmaHistogramBoolean(kBocaTokenRetrievalIsValidation, is_validation);
}

}  // namespace ash::boca

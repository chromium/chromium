// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_

#include "base/time/time.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca {
inline constexpr char kBocaActionOfStudentJoinedSession[] =
    "Ash.Boca.StudentJoinedSession";
inline constexpr char kBocaNumOfActiveStudentsWhenSessionEnded[] =
    "Ash.Boca.NumberOfActiveStudentsWhenSessionEnded";
inline constexpr char kBocaNumOfStudentsJoinedViaCodeDuringSession[] =
    "Ash.Boca.NumberOfStudentsJoinedViaCodeDuringSession";
inline constexpr char kBocaBabelorcaActionOfStudentSwitchLanguage[] =
    "Ash.Boca.Babelorca.StudentSwitchLanguage";
inline constexpr char kBocaOnTaskActionOfStudentNavigateBack[] =
    "Ash.Boca.OnTask.StudentNavigateBack";
inline constexpr char kBocaOnTaskActionOfStudentNavigateForward[] =
    "Ash.Boca.OnTask.StudentNavigateForward";
inline constexpr char kBocaOnTaskActionOfStudentReloadPage[] =
    "Ash.Boca.OnTask.StudentReloadPage";
inline constexpr char kBocaOnTaskActionOfStudentToggleTabStripVisibility[] =
    "Ash.Boca.OnTask.StudentToggleTabStripVisibility";
inline constexpr char kBocaOnTaskActionOfStudentSetSnapLocationToLeft[] =
    "Ash.Boca.OnTask.StudentSetSnapLocationToLeft";
inline constexpr char kBocaOnTaskActionOfStudentSetSnapLocationToRight[] =
    "Ash.Boca.OnTask.StudentSetSnapLocationToRight";
inline constexpr char kBocaBabelorcaTargetLanguage[] =
    "Accessibility.LiveTranslate.Ash.Boca.Babelorca.TargetLanguage";
inline constexpr char kBocaOnTaskLockedSessionDurationPercentage[] =
    "Ash.Boca.OnTask.LockedSessionDurationPercentage";
inline constexpr char kBocaOnTaskUnlockedSessionDurationPercentage[] =
    "Ash.Boca.OnTask.UnlockedSessionDurationPercentage";
inline constexpr char kBocaOnTaskNumOfTabsWhenSessionEnded[] =
    "Ash.Boca.OnTask.NumberOfTabsWhenSessionEnded";
inline constexpr char kBocaOnTaskMaxNumOfTabsDuringSession[] =
    "Ash.Boca.OnTask.MaxNumberOfTabsDuringSession";
inline constexpr char kBocaSpotlightGoogleApiCallErrorCodeTemplate[] =
    "Ash.Boca.Spotlight.$1.ErrorCode";
inline constexpr char kBocaSpotlightOnRegisterScreenRequestSent[] =
    "RegisterScreen";
inline constexpr char kBocaSpotlightViewStudentScreen[] = "ViewStudentScreen";
inline constexpr char kBocaSpotlightEndViewStudentScreen[] =
    "EndViewStudentScreen";
inline constexpr char kBocaSpotlightSetViewScreenSessionActive[] =
    "SetViewScreenSessionActive";
inline constexpr char kBocaGoogleApiCallErrorCodeTemplate[] =
    "Ash.Boca.$1.ErrorCode";
inline constexpr char kBocaGetSession[] = "GetSession";
inline constexpr char kListCourses[] = "ListCourses";
inline constexpr char kListStudents[] = "ListStudents";
inline constexpr char kListCourseWorks[] = "ListCourseWorks";
inline constexpr char kListCourseWorkMaterials[] = "ListCourseWorkMaterials";
inline constexpr char kBocaCreateSession[] = "CreateSession";
inline constexpr char kBocaEndSession[] = "EndSession";
inline constexpr char kBocaUpdateSession[] = "UpdateSession";
inline constexpr char kBocaJoinSessionViaAccessCode[] =
    "JoinSessionViaAccessCode";
inline constexpr char kBocaUpdateCaption[] = "UpdateCaption";
inline constexpr char kBocaAddStudents[] = "AddStudents";
inline constexpr char kBocaRemoveStudent[] = "RemoveStudent";
inline constexpr char kBocaUpdateStudentActivities[] =
    "UpdateStudentActivities";
inline constexpr char kBocaStudentHeartbeat[] = "StudentHeartbeat";
inline static constexpr char kPollingResult[] = "Ash.Boca.PollingResult";
inline static constexpr char kBocaTokenRetrievalIsValidation[] =
    "Ash.Boca.TokenRetrievalIsValidation";
inline constexpr char kBocaUploadToken[] = "UploadToken";
inline constexpr char kBocaScreenSharePresentOwnScreenInSessionResult[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.Result";
inline constexpr char kBocaScreenSharePresentOwnScreenOutOfSessionResult[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.Result";
inline constexpr char kBocaScreenSharePresentStudentScreenResult[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.Result";
inline constexpr char kBocaScreenSharePresentOwnScreenInSessionFailureReason[] =
    "Ash.Boca.ScreenShare.PresentOwnScreenInSession.FailureReason";
inline constexpr char
    kBocaScreenSharePresentOwnScreenOutOfSessionFailureReason[] =
        "Ash.Boca.ScreenShare.PresentOwnScreenOutOfSession.FailureReason";
inline constexpr char kBocaScreenSharePresentStudentScreenFailureReason[] =
    "Ash.Boca.ScreenShare.PresentStudentScreen.FailureReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BocaPresentOwnScreenFailureReason {
  kFeatureDisabled = 0,
  kStudentScreenShareActive = 1,
  kTeacherScreenShareActive = 2,
  kStartKioskConnectionRequestFailed = 3,
  kGetReceiverRequestFailed = 4,
  kGetCrdConnectionCodeFailed = 5,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kGetCrdConnectionCodeFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BocaPresentStudentScreenFailureReason {
  kFeatureDisabled = 0,
  kStudentScreenShareActive = 1,
  kTeacherScreenShareActive = 2,
  kStartKioskConnectionRequestFailed = 3,
  kEndSpotlightFailed = 4,
  kNoActiveStudentDevice = 5,
  kNoSession = 6,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kNoSession,
};

// Records the percentage of the duration that a session was in a particular
// locked or unlocked state.
void RecordOnTaskLockedStateDurationPercentage(
    base::TimeDelta unlocked_state_duration,
    base::TimeDelta locked_state_duration);

// Records the number of students joined via code in a session.
void RecordNumOfStudentsJoinedViaCodeDuringSession(
    int num_of_students_joined_via_code);

// Records the number of active students left when a session ends.
void RecordNumOfActiveStudentsWhenSessionEnded(int num_of_active_students);

// Records the number of tabs sent by the provider when a session ends.
void RecordOnTaskNumOfTabsWhenSessionEnded(int num_of_tabs);

// Records the max number of tabs sent by the provider in a session.
void RecordOnTaskMaxNumOfTabsDuringSession(int max_num_of_tabs);

// Records the action of a student joined a session.
void RecordStudentJoinedSession();

// Records the target language of a translation of live captions from the
// student.
void RecordBabelOrcaTranslationLanguage(const std::string& language);

// Records the action of a student translates the live caption.
void RecordBabelOrcaTranslationLanguageSwitched();

// Records the action of a student clicks navigate back button in OnTask pod.
void RecordOnTaskPodNavigateBackClicked();

// Records the action of a student clicks navigate forward button in OnTask pod.
void RecordOnTaskPodNavigateForwardClicked();

// Records the action of a student clicks reload page button in OnTask pod.
void RecordOnTaskPodReloadPageClicked();

// Records the action of a student clicks toggle tab strip visibility button in
// OnTask pod.
void RecordOnTaskPodToggleTabStripVisibilityClicked();

// Records the action of a student clicks move left or move right buttons in
// OnTask pod.
void RecordOnTaskPodSetSnapLocationClicked(bool is_left);

// Records the error code of the spotlight OnRegisterScreenRequestSent calls.
void RecordOnRegisterScreenRequestSentErrorCode(
    google_apis::ApiErrorCode error_code);

// Records the error code of the spotlight view student screen calls.
void RecordViewStudentScreenErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the spotlight end view student screen calls.
void RecordEndViewStudentScreenErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the spotlight set view screen session active calls.
void RecordSetViewScreenSessionActiveErrorCode(
    google_apis::ApiErrorCode error_code);

// Records the error code of the spotlight Google Api calls.
void RecordSpotlightGoogleApiErrorCode(const std::string& name,
                                       google_apis::ApiErrorCode error_code);

// Records the error code of Classroom ListCourses API
void RecordListCoursesErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of Classroom ListStudents API
void RecordListStudentsErrorCode(google_apis::ApiErrorCode error_code);

// Records the error  code of Classroom ListCourseWorks API
void RecordListCourseWorksErrorCode(google_apis::ApiErrorCode error_code);

// Records the error  code of Classroom ListMaterials API
void RecordListCourseWorkMaterialsErrorCode(
    google_apis::ApiErrorCode error_code);

// Records the error code of the GetSession calls.
void RecordGetSessionErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the CreateSession calls.
void RecordCreateSessionErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the EndSession calls.
void RecordEndSessionErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the UpdateSession calls.
void RecordUpdateSessionErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the JoinSessionViaAccessCode calls.
void RecordJoinSessionViaAccessCodeErrorCode(
    google_apis::ApiErrorCode error_code);

// Records the error code of the UpdateCaption calls.
void RecordUpdateCaptionErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the AddStudent calls.
void RecordAddStudentsErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the RemoveStudent calls.
void RecordRemoveStudentErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the UpdateStudentActivities calls.
void RecordUpdateStudentActivitiesErrorCode(
    google_apis::ApiErrorCode error_code);

// Records the error code of the StudentHeartBeat calls.
void RecordStudentHeartBeatErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the UploadToken calls.
void RecordUploadTokenErrorCode(google_apis::ApiErrorCode error_code);

// Records the error code of the Google Api calls.
void RecordGoogleApiErrorCode(const std::string& name,
                              google_apis::ApiErrorCode error_code);

// Records the session polling result.
void RecordPollingResult(const ::boca::Session* previous_session,
                         const ::boca::Session* current_session);

// Records the token retrieval is validation status.
void RecordTokenRetrievalIsValidation(const bool is_validation);

// Records the result (success / failure) of presenting own screen, and whether
// the attempt was made during a session or not.
void RecordPresentOwnScreenResult(const bool result,
                                  const bool is_session_active);

// Records the result (success / failure) of presenting student screen.
void RecordPresentStudentScreenResult(const bool result);

// Records the reason that presenting own screen failed, and whether the attempt
// was made during a session or not.
void RecordPresentOwnScreenFailureReason(
    BocaPresentOwnScreenFailureReason reason,
    const bool is_session_active);

// Records the reason that presenting a student screen failed.
void RecordPresentStudentScreenFailureReason(
    BocaPresentStudentScreenFailureReason reason);

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_

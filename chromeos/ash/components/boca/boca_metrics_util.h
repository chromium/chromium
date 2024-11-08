// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_

#include "base/time/time.h"

namespace ash::boca {
inline constexpr char kBocaActionOfStudentJoinedSession[] =
    "Ash.Boca.StudentJoinedSession";
inline constexpr char kBocaNumOfActiveStudentsWhenSessionEnded[] =
    "Ash.Boca.NumberOfActiveStudentsWhenSessionEnded";
inline constexpr char kBocaNumOfStudentsJoinedViaCodeDuringSession[] =
    "Ash.Boca.NumberOfStudentsJoinedViaCodeDuringSession";
inline constexpr char kBocaBabelorcaActionOfStudentSwitchLanguage[] =
    "Ash.Boca.Babelorca.StudentSwitchLanguage";
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

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_METRICS_UTIL_H_

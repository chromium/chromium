// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_metrics_manager.h"

#include <algorithm>

#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"

namespace ash::boca {
namespace {

int CalculateNumOfActiveStudents(const ::boca::Session* session) {
  int num_of_active_students = 0;
  if (session) {
    for (const auto& [student, student_status] : session->student_statuses()) {
      if (student_status.state() == ::boca::StudentStatus::ACTIVE) {
        ++num_of_active_students;
      }
    }
  }
  return num_of_active_students;
}

}  // namespace

BocaMetricsManager::BocaMetricsManager(bool is_producer)
    : is_producer_(is_producer) {}
BocaMetricsManager::~BocaMetricsManager() = default;

void BocaMetricsManager::OnSessionStarted(
    const std::string& session_id,
    const ::boca::UserIdentity& producer) {
  if (!is_producer_) {
    RecordStudentJoinedSession();
  }
  // Set the times for when session started along with the initial set time
  // for the content locked state.
  last_switch_locked_mode_timestamp_ = base::TimeTicks::Now();
  unlocked_mode_cumulative_duration_ = base::TimeDelta();
  locked_mode_cumulative_duration_ = base::TimeDelta();
}

void BocaMetricsManager::OnSessionEnded(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CalculateDurationForContentState(is_lock_window_);
  if (is_producer_) {
    RecordOnTaskLockedStateDurationPercentage(
        unlocked_mode_cumulative_duration_, locked_mode_cumulative_duration_);
    RecordNumOfStudentsJoinedViaCodeDuringSession(
        students_join_via_code_.size());
    const ::boca::Session* const session =
        BocaAppClient::Get()->GetSessionManager()->GetPreviousSession();
    RecordNumOfActiveStudentsWhenSessionEnded(
        CalculateNumOfActiveStudents(session));
    RecordOnTaskNumOfTabsWhenSessionEnded(num_of_tabs_);
    RecordOnTaskMaxNumOfTabsDuringSession(max_num_of_tabs_);
  }
  last_switch_locked_mode_timestamp_ = base::TimeTicks();
  unlocked_mode_cumulative_duration_ = base::TimeDelta();
  locked_mode_cumulative_duration_ = base::TimeDelta();
  students_join_via_code_.clear();
  num_of_tabs_ = 0;
  max_num_of_tabs_ = 0;
}

void BocaMetricsManager::OnBundleUpdated(const ::boca::Bundle& bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  num_of_tabs_ = bundle.content_configs().size();
  max_num_of_tabs_ = std::max(max_num_of_tabs_, num_of_tabs_);
  if (is_lock_window_ == bundle.locked()) {
    return;
  }
  CalculateDurationForContentState(is_lock_window_);
  is_lock_window_ = bundle.locked();
}

void BocaMetricsManager::OnSessionRosterUpdated(const ::boca::Roster& roster) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& group : roster.student_groups()) {
    if (group.group_source() == ::boca::StudentGroup::JOIN_CODE) {
      for (const auto& student : group.students()) {
        students_join_via_code_.insert(student.email());
      }
    }
  }
}

void BocaMetricsManager::CalculateDurationForContentState(bool locked_state) {
  const base::TimeDelta duration =
      base::TimeTicks::Now() - last_switch_locked_mode_timestamp_;
  locked_state ? (locked_mode_cumulative_duration_ += duration)
               : (unlocked_mode_cumulative_duration_ += duration);
  last_switch_locked_mode_timestamp_ = base::TimeTicks::Now();
}

}  // namespace ash::boca

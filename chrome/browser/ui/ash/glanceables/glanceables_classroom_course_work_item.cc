// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_course_work_item.h"

#include <optional>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "google_apis/classroom/classroom_api_course_work_response_types.h"
#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"

namespace ash {
namespace {

bool TimesWithinDelta(const base::Time& lhs,
                      const base::Time& rhs,
                      const base::TimeDelta& max_distance) {
  return (lhs - rhs).magnitude() <= max_distance;
}

std::optional<base::Time> ConvertCourseWorkItemDue(
    const std::optional<google_apis::classroom::CourseWorkItem::DueDateTime>&
        raw_due) {
  if (!raw_due.has_value()) {
    return std::nullopt;
  }

  const base::Time::Exploded exploded_due = {.year = raw_due->year,
                                             .month = raw_due->month,
                                             .day_of_month = raw_due->day};
  base::Time due;
  if (!base::Time::FromUTCExploded(exploded_due, &due)) {
    return std::nullopt;
  }
  return due + raw_due->time_of_day;
}

GlanceablesClassroomStudentSubmissionState CalculateStudentSubmissionState(
    const google_apis::classroom::StudentSubmission* raw_student_submission) {
  const auto raw_state = raw_student_submission->state();
  if (raw_state == google_apis::classroom::StudentSubmission::State::kNew ||
      raw_state == google_apis::classroom::StudentSubmission::State::kCreated ||
      raw_state == google_apis::classroom::StudentSubmission::State::
                       kReclaimedByStudent) {
    return GlanceablesClassroomStudentSubmissionState::kAssigned;
  }

  if (raw_state ==
      google_apis::classroom::StudentSubmission::State::kTurnedIn) {
    return GlanceablesClassroomStudentSubmissionState::kTurnedIn;
  }

  if (raw_state ==
      google_apis::classroom::StudentSubmission::State::kReturned) {
    return raw_student_submission->assigned_grade().has_value()
               ? GlanceablesClassroomStudentSubmissionState::kGraded
               : GlanceablesClassroomStudentSubmissionState::kAssigned;
  }

  return GlanceablesClassroomStudentSubmissionState::kOther;
}

}  // namespace

GlanceablesClassroomCourseWorkItem::GlanceablesClassroomCourseWorkItem() =
    default;

GlanceablesClassroomCourseWorkItem::GlanceablesClassroomCourseWorkItem(
    const GlanceablesClassroomCourseWorkItem&) = default;

GlanceablesClassroomCourseWorkItem&
GlanceablesClassroomCourseWorkItem::operator=(
    const GlanceablesClassroomCourseWorkItem&) = default;

GlanceablesClassroomCourseWorkItem::~GlanceablesClassroomCourseWorkItem() =
    default;

void GlanceablesClassroomCourseWorkItem::SetCourseWorkItem(
    const google_apis::classroom::CourseWorkItem* course_work) {
  course_work_item_set_ = true;
  can_course_work_item_be_revalidated_ = false;

  title_ = course_work->title();
  link_ = course_work->alternate_link();
  due_ = ConvertCourseWorkItemDue(course_work->due_date_time());
  creation_time_ = course_work->creation_time();
  last_update_ = course_work->last_update();
}

void GlanceablesClassroomCourseWorkItem::AddStudentSubmission(
    const google_apis::classroom::StudentSubmission* submission) {
  ++current_submissions_state_.total_count;
  if (submission->last_update().has_value() &&
      submission->last_update() > most_recent_submission_update_time_) {
    most_recent_submission_update_time_ = submission->last_update().value();
  }

  switch (CalculateStudentSubmissionState(submission)) {
    case GlanceablesClassroomStudentSubmissionState::kGraded:
      ++current_submissions_state_.number_turned_in;
      ++current_submissions_state_.number_graded;
      break;
    case GlanceablesClassroomStudentSubmissionState::kTurnedIn:
      ++current_submissions_state_.number_turned_in;
      break;
    case GlanceablesClassroomStudentSubmissionState::kAssigned:
    case GlanceablesClassroomStudentSubmissionState::kOther:
      break;
  }
}

void GlanceablesClassroomCourseWorkItem::InvalidateStudentSubmissions() {
  previous_submissions_state_ = current_submissions_state_;

  current_submissions_state_.Reset();
}

void GlanceablesClassroomCourseWorkItem::RestorePreviousStudentSubmissions() {
  if (!previous_submissions_state_) {
    return;
  }

  current_submissions_state_ = *previous_submissions_state_;
  previous_submissions_state_.reset();
}

void GlanceablesClassroomCourseWorkItem::InvalidateCourseWorkItem() {
  can_course_work_item_be_revalidated_ = course_work_item_set_;
  course_work_item_set_ = false;
}

void GlanceablesClassroomCourseWorkItem::RevalidateCourseWorkItem() {
  if (can_course_work_item_be_revalidated_) {
    course_work_item_set_ = true;
  }
}

bool GlanceablesClassroomCourseWorkItem::SatisfiesPredicates(
    base::RepeatingCallback<bool(const std::optional<base::Time>&)>
        due_predicate,
    base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
        submission_state_predicate) const {
  if (!IsValid()) {
    return false;
  }

  if (!due_predicate.Run(due_)) {
    return false;
  }

  GlanceablesClassroomStudentSubmissionState effective_state =
      GlanceablesClassroomStudentSubmissionState::kAssigned;
  if (total_submissions() == graded_submissions()) {
    effective_state = GlanceablesClassroomStudentSubmissionState::kGraded;
  } else if (total_submissions() == turned_in_submissions()) {
    effective_state = GlanceablesClassroomStudentSubmissionState::kTurnedIn;
  }

  return submission_state_predicate.Run(effective_state);
}

std::unique_ptr<GlanceablesClassroomAssignment>
GlanceablesClassroomCourseWorkItem::CreateClassroomAssignment(
    const std::string& course_name,
    bool include_aggregated_submissions_state) const {
  CHECK(IsValid());

  std::optional<GlanceablesClassroomAggregatedSubmissionsState>
      aggregated_submissions_state;
  if (include_aggregated_submissions_state) {
    aggregated_submissions_state = current_submissions_state_;
  }
  return std::make_unique<GlanceablesClassroomAssignment>(
      course_name, title_, link_, due_, last_update_,
      aggregated_submissions_state);
}

bool GlanceablesClassroomCourseWorkItem::IsValid() const {
  return course_work_item_set_ && total_submissions() > 0;
}

bool GlanceablesClassroomCourseWorkItem::StudentSubmissionsNeedRefetch(
    const base::Time& now) const {
  if (last_submissions_fetch_.is_null()) {
    return true;
  }

  if (last_update_ > last_submissions_fetch_) {
    return true;
  }

  const base::TimeDelta time_from_last_refresh = now - last_submissions_fetch_;

  if (TimesWithinDelta(last_update_, now, base::Days(2))) {
    return true;
  }

  if (TimesWithinDelta(last_update_, now, base::Days(7)) &&
      time_from_last_refresh > base::Days(1)) {
    return true;
  }

  if (graded_submissions() < total_submissions() &&
      time_from_last_refresh > base::Days(7)) {
    return true;
  }

  if (due_) {
    // Course work with due date within a day to now is likely to be shown in
    // the UI - refresh it.
    if (TimesWithinDelta(*due_, now, base::Days(2))) {
      return true;
    }

    // If due date is within few days, refresh it if the student submissions
    // have not been updated recently.
    if (TimesWithinDelta(*due_, now, base::Days(5))) {
      return time_from_last_refresh > base::Hours(12);
    }

    if (TimesWithinDelta(*due_, now, base::Days(14))) {
      return time_from_last_refresh > base::Days(1);
    }
  }

  return false;
}

void GlanceablesClassroomCourseWorkItem::SetHasFreshSubmissionsState(
    bool value,
    const base::Time& now) {
  has_fresh_submissions_state_ = value;
  if (has_fresh_submissions_state_) {
    previous_submissions_state_.reset();
    last_submissions_fetch_ = now;
  }
}

}  // namespace ash

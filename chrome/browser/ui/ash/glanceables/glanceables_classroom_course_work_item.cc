// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_course_work_item.h"

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "google_apis/classroom/classroom_api_course_work_response_types.h"
#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

bool TimesWithinDelta(const base::Time& lhs,
                      const base::Time& rhs,
                      const base::TimeDelta& max_distance) {
  return (lhs - rhs).magnitude() <= max_distance;
}

absl::optional<base::Time> ConvertCourseWorkItemDue(
    const absl::optional<google_apis::classroom::CourseWorkItem::DueDateTime>&
        raw_due) {
  if (!raw_due.has_value()) {
    return absl::nullopt;
  }

  const auto exploded_due = base::Time::Exploded{.year = raw_due->year,
                                                 .month = raw_due->month,
                                                 .day_of_month = raw_due->day};
  base::Time due;
  if (!base::Time::FromUTCExploded(exploded_due, &due)) {
    return absl::nullopt;
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
  CHECK(!course_work_item_set_);
  course_work_item_set_ = true;

  title_ = course_work->title();
  link_ = course_work->alternate_link();
  due_ = ConvertCourseWorkItemDue(course_work->due_date_time());
  last_update_ = course_work->last_update();
}

void GlanceablesClassroomCourseWorkItem::AddStudentSubmission(
    const google_apis::classroom::StudentSubmission* submission) {
  ++total_submissions_;

  switch (CalculateStudentSubmissionState(submission)) {
    case GlanceablesClassroomStudentSubmissionState::kGraded:
      ++turned_in_submissions_;
      ++graded_submissions_;
      break;
    case GlanceablesClassroomStudentSubmissionState::kTurnedIn:
      ++turned_in_submissions_;
      break;
    case GlanceablesClassroomStudentSubmissionState::kAssigned:
    case GlanceablesClassroomStudentSubmissionState::kOther:
      break;
  }
}

void GlanceablesClassroomCourseWorkItem::InvalidateStudentSubmissions() {
  last_submissions_fetch_ = base::Time();

  total_submissions_ = 0;
  turned_in_submissions_ = 0;
  graded_submissions_ = 0;
}

void GlanceablesClassroomCourseWorkItem::InvalidateCourseWorkItem() {
  course_work_item_set_ = false;
}

bool GlanceablesClassroomCourseWorkItem::SatisfiesPredicates(
    base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
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
  if (total_submissions_ == graded_submissions_) {
    effective_state = GlanceablesClassroomStudentSubmissionState::kGraded;
  } else if (total_submissions_ == turned_in_submissions_) {
    effective_state = GlanceablesClassroomStudentSubmissionState::kTurnedIn;
  }

  return submission_state_predicate.Run(effective_state);
}

std::unique_ptr<GlanceablesClassroomAssignment>
GlanceablesClassroomCourseWorkItem::CreateClassroomAssignment(
    const std::string& course_name,
    bool include_aggregated_submissions_state) const {
  CHECK(IsValid());

  absl::optional<GlanceablesClassroomAggregatedSubmissionsState>
      aggregated_submissions_state;
  if (include_aggregated_submissions_state) {
    aggregated_submissions_state.emplace(
        total_submissions_, turned_in_submissions_, graded_submissions_);
  }
  return std::make_unique<GlanceablesClassroomAssignment>(
      course_name, title_, link_, due_, last_update_,
      aggregated_submissions_state);
}

bool GlanceablesClassroomCourseWorkItem::IsValid() const {
  return course_work_item_set_ && total_submissions_ > 0;
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

  if (graded_submissions_ < total_submissions_ &&
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
    last_submissions_fetch_ = now;
  }
}

}  // namespace ash

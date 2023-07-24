// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_COURSE_WORK_ITEM_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_COURSE_WORK_ITEM_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace google_apis::classroom {
class CourseWorkItem;
class StudentSubmission;
}  // namespace google_apis::classroom

namespace ash {

struct GlanceablesClassroomAssignment;

// State of the student submission. Simplified version of
// `google_apis::classroom::StudentSubmission::State` by the following rules:
// - `kNew`, `kCreated`, `kReclaimedByStudent`, `kReturned` **without**
//   an `assigned_grade()` -> `kAssigned`;
// - `kTurnedIn` -> `kTurnedIn`;
// - `kReturned` **with** an `assigned_grade()` -> `kGraded`,
// - all other unknown values -> `kOther`.
enum class GlanceablesClassroomStudentSubmissionState {
  kAssigned,
  kTurnedIn,
  kGraded,
  kOther,
};

// Information about a Google Classroom Course Work item fetched using Google
// Classroom API. The data structure contains course work item data (from
// `google_apis::classroom::CourseWorkItem`) that is needed to represent the
// course work in the UI, and aggregated state of all student submissions for
// the course work (from `google_apis::classroom::StudentSubmission`).
class GlanceablesClassroomCourseWorkItem {
 public:
  GlanceablesClassroomCourseWorkItem();
  GlanceablesClassroomCourseWorkItem(const GlanceablesClassroomCourseWorkItem&);
  GlanceablesClassroomCourseWorkItem& operator=(
      const GlanceablesClassroomCourseWorkItem&);
  ~GlanceablesClassroomCourseWorkItem();

  // Sets the course work item data fetched using Google Classroom course work
  // API (as `google_apis::classroom::CourseWorkItem`).
  void SetCourseWorkItem(
      const google_apis::classroom::CourseWorkItem* course_work);

  // Adds submission state from a single Google Classroom student submission to
  // the aggregated student submissions state. It should be called for every
  // student submission fetched for this course work item.
  void AddStudentSubmission(
      const google_apis::classroom::StudentSubmission* submission);

  // Resets the aggregated student submissions data.
  void InvalidateStudentSubmissions();

  // Marks existing course work data as invalid - `IsValid()` will return false
  // until `SetCourseWorkItem()` gets set again. This does not clear the cached
  // data, but it allows the course work item information to be overwritten, and
  // it can be used to detect whether the course work item data was set since
  // invalidation.
  void InvalidateCourseWorkItem();

  // Whether the course work item satisfies conditions defined both by
  // `due_predicate` and `submission_state_predicate`.
  // `due_predicate` - Predicate to filter course work items by their due date.
  //      If the course work item due date does not satisfy the predicate, this
  //      will return nullptr.
  // `submission_state_predicate` - Predicate to filter course work items by the
  //      associated student submissions state. If the course work item
  //      submissions do not satisfy the predicate, this will return nullptr.
  //      Note that course work item submissions state will be kGraded, or
  //      kTurned in if all submissions are in kGraded, or kTurned in state.
  bool SatisfiesPredicates(
      base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
          due_predicate,
      base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
          submission_state_predicate) const;

  // Converts the course work item data to `GlanceablesClassroomAssignment`
  // type, which is used as a course work item representation in UI layer.
  // `course_name` - the associated course name.
  // `include_aggregated_submissions_state` - whether the created
  //      `GlanceablesClassroomAssignment` should include aggregated submissions
  //      data. This can be false for student glanceables, whose UI
  //      representation does not include student submissions state.
  std::unique_ptr<GlanceablesClassroomAssignment> CreateClassroomAssignment(
      const std::string& course_name,
      bool include_aggregated_submissions_state) const;

  // Whether the course work item has been set, and at least one student
  // submission has been added.
  bool IsValid() const;

  // Depending on the current time `now`, and the course work state, returns
  // whether the student submissions for the course work item should be
  // refetched when updating the user's course work data.
  bool StudentSubmissionsNeedRefetch(const base::Time& now) const;

  // Set whether the student submissions have been fetched during the latest
  // user course work data update.
  // `now` is the timestamp of the fetch, if the submission state has been
  // refreshed.
  void SetHasFreshSubmissionsState(bool value, const base::Time& now);

  const std::string& title() const { return title_; }
  const GURL& link() const { return link_; }
  const absl::optional<base::Time>& due() const { return due_; }
  const base::Time& last_update() const { return last_update_; }

  int total_submissions() const { return total_submissions_; }
  int turned_in_submissions() const { return turned_in_submissions_; }
  int graded_submissions() const { return graded_submissions_; }

  bool has_fresh_submissions_state() const {
    return has_fresh_submissions_state_;
  }

 private:
  // Whether `SetCourseWorkItem()` was called.
  bool course_work_item_set_ = false;

  // Title of this course work item.
  std::string title_;

  // Absolute link to this course work in the Classroom web UI.
  GURL link_;

  // Due date and time in UTC of this course work item.
  absl::optional<base::Time> due_;

  // The timestamp of the last course work item update.
  base::Time last_update_;

  // The total number of students that have course work assigned to them.
  int total_submissions_ = 0;

  // The number of student submissions that have been turned in by students.
  int turned_in_submissions_ = 0;

  // The number of student submissions that have already been graded.
  int graded_submissions_ = 0;

  // Whether the student submissions have been fetched during the latest course
  // work data update.
  bool has_fresh_submissions_state_ = false;

  // If the student submissions state is valid, the time when the submissions
  // state has been last refreshed.
  base::Time last_submissions_fetch_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_COURSE_WORK_ITEM_H_

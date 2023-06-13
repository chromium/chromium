// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace google_apis::classroom {
class Courses;
class CourseWork;
class StudentSubmissions;
}  // namespace google_apis::classroom

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

// Provides implementation for `GlanceablesClassroomClient`. Responsible for
// communication with Google Classroom API.
class GlanceablesClassroomClientImpl : public GlanceablesClassroomClient {
 public:
  // Provides an instance of `google_apis::RequestSender` for the client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  explicit GlanceablesClassroomClientImpl(
      const CreateRequestSenderCallback& create_request_sender_callback);
  GlanceablesClassroomClientImpl(const GlanceablesClassroomClientImpl&) =
      delete;
  GlanceablesClassroomClientImpl& operator=(
      const GlanceablesClassroomClientImpl&) = delete;
  ~GlanceablesClassroomClientImpl() override;

  // GlanceablesClassroomClient:
  void IsStudentRoleActive(IsRoleEnabledCallback callback) override;
  void GetCompletedStudentAssignments(
      GetStudentAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithApproachingDueDate(
      GetStudentAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithMissedDueDate(
      GetStudentAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithoutDueDate(
      GetStudentAssignmentsCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourses);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourseWork);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissions);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsMultiplePages);

  // Done callback for fetching all courses for student or teacher roles.
  using FetchCoursesCallback = base::OnceCallback<void(
      const std::vector<std::unique_ptr<GlanceablesClassroomCourse>>& courses)>;

  // Done callback for fetching all course work items in a course.
  using FetchCourseWorkCallback = base::OnceCallback<void(
      const std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>&
          course_work)>;

  // Done callback for fetching all student submissions in a course.
  using FetchStudentSubmissionsCallback = base::OnceCallback<void(
      const std::vector<std::unique_ptr<GlanceablesClassroomStudentSubmission>>&
          student_submissions)>;

  enum class FetchStatus { kNotFetched, kFetching, kFetched };

  // Fetches all courses for student and teacher roles and invokes `callback`
  // when done.
  void FetchStudentCourses(FetchCoursesCallback callback);
  void FetchTeacherCourses(FetchCoursesCallback callback);

  // Fetches all course work items for the specified `course_id` and invokes
  // `callback` when done.
  void FetchCourseWork(const std::string& course_id,
                       FetchCourseWorkCallback callback);

  // Fetches all student submissions for the specified `course_id` and invokes
  // `callback` when done.
  void FetchStudentSubmissions(const std::string& course_id,
                               FetchStudentSubmissionsCallback callback);

  // Delays executing `callback` until all student data are fetched.
  void InvokeOnceStudentDataFetched(base::OnceClosure callback);

  // Fetches one page of courses.
  // `student_id`        - restricts returned courses to those having a student
  //                       with the specified identifier. Use an empty string
  //                       to avoid filtering by student id.
  // `teacher_id`        - restricts returned courses to those having a teacher
  //                       with the specified identifier. Use an empty string
  //                       to avoid filtering by teacher id.
  // `page_token`        - token specifying the result page to return, comes
  //                       from the previous fetch request. Use an empty string
  //                       to fetch the first page.
  // `courses_container` - points to the container in which the response items
  //                       are accumulated.
  // `callback`          - a callback that runs when all courses for the user
  //                       have been fetched. This may require multiple fetch
  //                       requests, in this case `callback` gets called when
  //                       the final request completes.
  void FetchCoursesPage(
      const std::string& student_id,
      const std::string& teacher_id,
      const std::string& page_token,
      std::vector<std::unique_ptr<GlanceablesClassroomCourse>>&
          courses_container,
      FetchCoursesCallback callback);

  // Callback for `FetchCoursesPage()`. If `next_page_token()` in the `result`
  // is not empty - calls another `FetchCoursesPage()`, otherwise runs done
  // `callback`.
  void OnCoursesPageFetched(
      const std::string& student_id,
      const std::string& teacher_id,
      std::vector<std::unique_ptr<GlanceablesClassroomCourse>>&
          courses_container,
      FetchCoursesCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                     google_apis::ApiErrorCode> result);

  // Callback for `FetchStudentCourses()` or `FetchTeacherCourses()`. Triggers
  // fetching course work and student submissions for fetched `courses` and
  // invokes `on_course_work_and_student_submissions_fetched` when done.
  void OnCoursesFetched(
      base::OnceClosure on_course_work_and_student_submissions_fetched,
      const std::vector<std::unique_ptr<GlanceablesClassroomCourse>>& courses);

  // Fetches one page of course work items.
  // `course_id`  - identifier of the course.
  // `page_token` - token specifying the result page to return, comes from the
  //                previous fetch request. Use an empty string to fetch the
  //                first page.
  // `callback`   - a callback that runs when all course work items in a course
  //                have been fetched. This may require multiple fetch requests,
  //                in this case `callback` gets called when the final request
  //                completes.
  void FetchCourseWorkPage(const std::string& course_id,
                           const std::string& page_token,
                           FetchCourseWorkCallback callback);

  // Callback for `FetchCourseWorkPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchCourseWorkPage()`, otherwise
  // runs done `callback`.
  void OnCourseWorkPageFetched(
      const std::string& course_id,
      FetchCourseWorkCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::CourseWork>,
                     google_apis::ApiErrorCode> result);

  // Fetches one page of student submissions.
  // `course_id`  - identifier of the course.
  // `page_token` - token specifying the result page to return, comes from the
  //                previous fetch request. Use an empty string to fetch the
  //                first page.
  // `callback`   - a callback that runs when all student submissions in a
  //                course have been fetched. This may require multiple fetch
  //                requests, in this case `callback` gets called when the final
  //                request completes.
  void FetchStudentSubmissionsPage(const std::string& course_id,
                                   const std::string& page_token,
                                   FetchStudentSubmissionsCallback callback);

  // Callback for `FetchStudentSubmissionsPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchStudentSubmissionsPage()`,
  // otherwise runs done `callback`.
  void OnStudentSubmissionsPageFetched(
      const std::string& course_id,
      FetchStudentSubmissionsCallback callback,
      base::expected<
          std::unique_ptr<google_apis::classroom::StudentSubmissions>,
          google_apis::ApiErrorCode> result);

  // Invokes all pending callbacks from `callbacks_waiting_for_student_data_`
  // once all student data are fetched (courses + course work + student
  // submissions).
  void OnStudentDataFetched();

  // Selects student assignments that satisfy both filtering predicates below.
  // `due_predicate`              - returns `true` if passed due date/time
  //                                satisfies filtering requirements.
  // `submission_state_predicate` - returns `true` if passed submission state
  //                                satisfies filtering requirements.
  // `callback`                   - invoked with filtered results.
  void GetFilteredStudentAssignments(
      base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
          due_predicate,
      base::RepeatingCallback<
          bool(GlanceablesClassroomStudentSubmission::State)>
          submission_state_predicate,
      GetStudentAssignmentsCallback callback);

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // Available courses for student and teacher roles.
  std::vector<std::unique_ptr<GlanceablesClassroomCourse>> student_courses_;
  std::vector<std::unique_ptr<GlanceablesClassroomCourse>> teacher_courses_;

  // All course work items grouped by course id.
  base::flat_map<
      std::string,
      std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>>
      course_work_;

  // All student submissions grouped by course id.
  base::flat_map<
      std::string,
      std::vector<std::unique_ptr<GlanceablesClassroomStudentSubmission>>>
      student_submissions_;

  // Fetch status of all student data.
  FetchStatus student_data_fetch_status_ = FetchStatus::kNotFetched;

  // Pending callbacks awaiting all student data.
  std::vector<base::OnceClosure> callbacks_waiting_for_student_data_;

  base::WeakPtrFactory<GlanceablesClassroomClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

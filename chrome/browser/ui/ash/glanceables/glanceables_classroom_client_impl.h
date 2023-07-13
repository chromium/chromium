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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class Profile;

namespace base {
class Time;
}  // namespace base

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

  GlanceablesClassroomClientImpl(
      Profile* profile,
      const CreateRequestSenderCallback& create_request_sender_callback);
  GlanceablesClassroomClientImpl(const GlanceablesClassroomClientImpl&) =
      delete;
  GlanceablesClassroomClientImpl& operator=(
      const GlanceablesClassroomClientImpl&) = delete;
  ~GlanceablesClassroomClientImpl() override;

  // GlanceablesClassroomClient:
  void IsStudentRoleActive(IsRoleEnabledCallback callback) override;
  void GetCompletedStudentAssignments(GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithMissedDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void IsTeacherRoleActive(IsRoleEnabledCallback callback) override;
  void GetTeacherAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsRecentlyDue(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void GetGradedTeacherAssignments(GetAssignmentsCallback callback) override;
  void OpenUrl(const GURL& url) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourses);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourseWork);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkAndSubmissions);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkAndSubmissionsMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissions);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsMultiplePages);

  // Done callback for fetching all courses for student or teacher roles.
  using CourseList = std::vector<std::unique_ptr<GlanceablesClassroomCourse>>;
  using FetchCoursesCallback =
      base::OnceCallback<void(const CourseList& courses)>;

  using CourseWorkList =
      std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>;
  // Done callback for fetching all course work items in a course.
  using FetchCourseWorkCallback =
      base::OnceCallback<void(const CourseWorkList& course_work)>;

  // Done callback for fetching all student submissions in a course.
  using SubmissionList =
      std::vector<std::unique_ptr<GlanceablesClassroomStudentSubmission>>;
  using SubmissionsPerCourseWork = base::flat_map<std::string, SubmissionList>;
  using FetchStudentSubmissionsCallback = base::OnceCallback<void(
      const SubmissionsPerCourseWork& student_submissions)>;

  enum class FetchStatus { kNotFetched, kFetching, kFetched };

  // Wrapper around course work fetch callback that tracks the number of pending
  // course work page requests.
  // While individual pages need to be fetched serially, course work fetch may
  // require fetching student submissions for course work in each of the course
  // work pages. In that case, a page request is deemed complete when all
  // required student submissions are fetched. Fetching student submissions for
  // a course work page does not block fetch of the next course work page, which
  // means that handling of different course work pages may overlap.
  class CourseWorkRequest {
   public:
    explicit CourseWorkRequest(FetchCourseWorkCallback callback);
    CourseWorkRequest(const CourseWorkRequest&) = delete;
    CourseWorkRequest& operator=(const CourseWorkRequest&) = delete;
    ~CourseWorkRequest();

    // Increases the count of pending course work page requests - should be
    // called when a fetch for a course work page is initiated.
    void IncrementPendingPageCount();

    // Decrease the count of pending course work page requests - should be
    // called when a fetch for a course work page, including student submissions
    // data (when required) completes.
    void DecrementPendingPageCount();

    // If no more page tokens are pending, runs the `callback_`.
    // Returns whether the callback was run. If the callback is run, the object
    // can be discarded. and `RespondIfComplete()` should not be called any
    // longer.
    bool RespondIfComplete(const CourseWorkList& course_work);

   private:
    FetchCourseWorkCallback callback_;
    int pending_page_requests_ = 0;
  };

  // Fetches all courses for student and teacher roles and invokes `callback`
  // when done.
  void FetchStudentCourses(FetchCoursesCallback callback);
  void FetchTeacherCourses(FetchCoursesCallback callback);

  // Fetches all course work items for the specified `course_id` and invokes
  // `callback` when done.
  void FetchCourseWork(const std::string& course_id,
                       bool fetch_submissions,
                       FetchCourseWorkCallback callback);

  // Fetches all student submissions for the specified `course_id` and
  // `course_work_id` and invokes `callback` when done.
  // To requests student submissions for all course work item in the course,
  // pass in `course_work_id` value "-".
  void FetchStudentSubmissions(const std::string& course_id,
                               const std::string& course_work_id,
                               FetchStudentSubmissionsCallback callback);

  // Delays executing `callback` until all student data are fetched.
  void InvokeOnceStudentDataFetched(base::OnceClosure callback);

  // Delays executing `callback` until all teacher data are fetched.
  void InvokeOnceTeacherDataFetched(base::OnceClosure callback);

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
  void FetchCoursesPage(const std::string& student_id,
                        const std::string& teacher_id,
                        const std::string& page_token,
                        CourseList& courses_container,
                        FetchCoursesCallback callback);

  // Callback for `FetchCoursesPage()`. If `next_page_token()` in the `result`
  // is not empty - calls another `FetchCoursesPage()`, otherwise runs done
  // `callback`.
  void OnCoursesPageFetched(
      const std::string& student_id,
      const std::string& teacher_id,
      CourseList& courses_container,
      const base::Time& request_start_time,
      FetchCoursesCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                     google_apis::ApiErrorCode> result);

  // Callback for `FetchStudentCourses()` or `FetchTeacherCourses()`. Triggers
  // fetching course work and student submissions for fetched `courses` and
  // invokes `on_course_work_and_student_submissions_fetched` when done.
  void OnCoursesFetched(
      bool fetch_submissions_per_course_work,
      base::OnceClosure on_course_work_and_student_submissions_fetched,
      const CourseList& courses);

  // Fetches one page of course work items.
  // `request_id` - the ID for the course work request that's being handled.
  //                It can be used to get the associated `CourseWorkRequest`
  //                from `course_work_requests_`.
  // `course_id`  - identifier of the course.
  // `page_token` - token specifying the result page to return, comes from the
  //                previous fetch request. Use an empty string to fetch the
  //                first page.
  // `fetch_submissions` - whether student submissions need to be fetched for
  //                       each course work item. For student glanceables,
  //                       student submissions will be fetched independently
  //                       all at once.
  void FetchCourseWorkPage(int request_id,
                           const std::string& course_id,
                           const std::string& page_token,
                           bool fetch_submissions);

  // Callback for `FetchCourseWorkPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchCourseWorkPage()`, otherwise
  // runs done `callback`.
  void OnCourseWorkPageFetched(
      int request_id,
      const std::string& course_id,
      bool fetch_submissions,
      const base::Time& request_start_time,
      base::expected<std::unique_ptr<google_apis::classroom::CourseWork>,
                     google_apis::ApiErrorCode> result);

  // Fetches one page of student submissions.
  // `course_id`      - identifier of the course.
  // `course_work_id` - identifier of the course work item. May be "-" to
  //                    request student submissions for all course work in the
  //                    course.
  // `page_token`     - token specifying the result page to return, comes from
  // the
  //                    previous fetch request. Use an empty string to fetch the
  //                    first page.
  // `callback`       - a callback that runs when all student submissions in a
  //                    course have been fetched. This may require multiple
  //                    fetch requests, in this case `callback` gets called when
  //                    the final request completes.
  void FetchStudentSubmissionsPage(const std::string& course_id,
                                   const std::string& course_work_id,
                                   const std::string& page_token,
                                   FetchStudentSubmissionsCallback callback);

  // Callback for `FetchStudentSubmissionsPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchStudentSubmissionsPage()`,
  // otherwise runs done `callback`.
  void OnStudentSubmissionsPageFetched(
      const std::string& course_id,
      const std::string& course_work_id,
      const base::Time& request_start_time,
      FetchStudentSubmissionsCallback callback,
      base::expected<
          std::unique_ptr<google_apis::classroom::StudentSubmissions>,
          google_apis::ApiErrorCode> result);

  // Callback for requests to fetch student submissions for all course work
  // items within a course work list page. The student submissions fetch is a
  // subtask of a course work request, which is identified by `request_id`.
  // When processing a page in course work list response, student submissions
  // may get requested for each course work item - this callback is called
  // when all requested student submission lists have been fetched.
  void OnCourseWorkSubmissionsFetched(int request_id,
                                      const std::string& course_id);

  // Invokes all pending callbacks from `callbacks_waiting_for_student_data_`
  // once all student data are fetched (courses + course work + student
  // submissions).
  void OnStudentDataFetched();

  // Invokes all pending callbacks from `callbacks_waiting_for_teacher_data_`
  // once all teacher data are fetched (courses + course work + student
  // submissions).
  void OnTeacherDataFetched();

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
      GetAssignmentsCallback callback);

  // Selects teacher assignments that satisfy the filtering below.
  // `due_predicate`              - returns `true` if passed due date/time
  //                                satisfies filtering requirements.
  // `graded`                     - whether or not we only want to include
  //                                course work which has a grade for every
  //                                submission.
  // `callback`                   - invoked with filtered results.
  void GetFilteredTeacherAssignments(
      base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
          due_predicate,
      bool graded,
      GetAssignmentsCallback callback);

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // The profile for which this client was created.
  const raw_ptr<Profile, ExperimentalAsh> profile_;

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // Available courses for student and teacher roles.
  CourseList student_courses_;
  CourseList teacher_courses_;

  // All course work items grouped by course id.
  base::flat_map<
      std::string,
      std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>>
      course_work_;

  // All student submissions grouped by course and course_work id.
  base::flat_map<std::string, SubmissionsPerCourseWork> student_submissions_;

  // Fetch status of all student data.
  FetchStatus student_data_fetch_status_ = FetchStatus::kNotFetched;

  // Pending callbacks awaiting all student data.
  std::vector<base::OnceClosure> callbacks_waiting_for_student_data_;

  // Fetch status of all teacher data.
  FetchStatus teacher_data_fetch_status_ = FetchStatus::kNotFetched;

  // Pending callbacks awaiting all teacher data.
  std::vector<base::OnceClosure> callbacks_waiting_for_teacher_data_;

  // The next available course work fetch request ID. The IDs will increase
  // monotonically with each new request.
  int next_course_work_request_id_ = 0;

  // In progress course work requests, mapped by the course work request ID.
  base::flat_map<int, std::unique_ptr<CourseWorkRequest>> course_work_requests_;

  base::WeakPtrFactory<GlanceablesClassroomClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "ui/base/models/list_model.h"

namespace google_apis::classroom {
class Courses;
class CourseWork;
class StudentSubmissions;
}  // namespace google_apis::classroom

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

struct GlanceablesClassroomCourse;
struct GlanceablesClassroomCourseWorkItem;
struct GlanceablesClassroomStudentSubmission;

// Provides implementation for `GlanceablesClassroomClient`. Responsible for
// communication with Google Classroom API.
class GlanceablesClassroomClientImpl : public GlanceablesClassroomClient {
 public:
  // Provides an instance of `google_apis::RequestSender` for the client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  // Done callback for fetching all courses for student or teacher roles.
  using FetchCoursesCallback = base::OnceCallback<void(
      ui::ListModel<GlanceablesClassroomCourse>* courses)>;

  // Done callback for fetching all course work items in a course.
  using FetchCourseWorkCallback = base::OnceCallback<void(
      ui::ListModel<GlanceablesClassroomCourseWorkItem>* course_work)>;

  // Done callback for fetching all student submissions in a course.
  using FetchStudentSubmissionsCallback = base::OnceCallback<void(
      ui::ListModel<GlanceablesClassroomStudentSubmission>*
          student_submissions)>;

  explicit GlanceablesClassroomClientImpl(
      const CreateRequestSenderCallback& create_request_sender_callback);
  GlanceablesClassroomClientImpl(const GlanceablesClassroomClientImpl&) =
      delete;
  GlanceablesClassroomClientImpl& operator=(
      const GlanceablesClassroomClientImpl&) = delete;
  ~GlanceablesClassroomClientImpl();

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

 private:
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
      ui::ListModel<GlanceablesClassroomCourse>* courses_container,
      FetchCoursesCallback callback);

  // Callback for `FetchCoursesPage()`. If `next_page_token()` in the `result`
  // is not empty - calls another `FetchCoursesPage()`, otherwise runs done
  // `callback`.
  void OnCoursesPageFetched(
      const std::string& student_id,
      const std::string& teacher_id,
      ui::ListModel<GlanceablesClassroomCourse>* courses_container,
      FetchCoursesCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                     google_apis::ApiErrorCode> result);

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

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // Available courses for student and teacher roles. Initialized after the
  // first fetch request to distinguish between "not fetched yet" vs. "fetched,
  // but has no items".
  std::unique_ptr<ui::ListModel<GlanceablesClassroomCourse>> student_courses_;
  std::unique_ptr<ui::ListModel<GlanceablesClassroomCourse>> teacher_courses_;

  // All course work items grouped by course id.
  base::flat_map<
      std::string,
      std::unique_ptr<ui::ListModel<GlanceablesClassroomCourseWorkItem>>>
      course_work_;

  // All student submissions grouped by course id.
  base::flat_map<
      std::string,
      std::unique_ptr<ui::ListModel<GlanceablesClassroomStudentSubmission>>>
      student_submissions_;

  base::WeakPtrFactory<GlanceablesClassroomClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "ui/base/models/list_model.h"

namespace google_apis::classroom {
class Courses;
}  // namespace google_apis::classroom

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

struct GlanceablesClassroomCourse;

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
      const std::string& page_token,
      ui::ListModel<GlanceablesClassroomCourse>* courses_container,
      FetchCoursesCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
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

  base::WeakPtrFactory<GlanceablesClassroomClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

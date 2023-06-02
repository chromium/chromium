// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "google_apis/classroom/classroom_api_course_work_response_types.h"
#include "google_apis/classroom/classroom_api_courses_response_types.h"
#include "google_apis/classroom/classroom_api_list_course_work_request.h"
#include "google_apis/classroom/classroom_api_list_courses_request.h"
#include "google_apis/classroom/classroom_api_list_student_submissions_request.h"
#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::RequestSender;
using ::google_apis::classroom::Course;
using ::google_apis::classroom::Courses;
using ::google_apis::classroom::CourseWork;
using ::google_apis::classroom::CourseWorkItem;
using ::google_apis::classroom::ListCoursesRequest;
using ::google_apis::classroom::ListCourseWorkRequest;
using ::google_apis::classroom::ListStudentSubmissionsRequest;
using ::google_apis::classroom::StudentSubmission;
using ::google_apis::classroom::StudentSubmissions;

// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";

// Special parameter value to request student submissions for all course work in
// the specified course.
constexpr char kAllStudentSubmissionsParameterValue[] = "-";

// TODO(b/282013130): Update the traffic annotation tag once all "[TBD]" items
// are ready.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("glanceables_classroom_integration", R"(
        semantics {
          sender: "Glanceables keyed service"
          description: "Provide ChromeOS users quick access to their "
                       "classroom items without opening the app or website"
          trigger: "[TBD] Depends on UI surface and pre-fetching strategy"
          internal {
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          data: "The request is authenticated with an OAuth2 access token "
                "identifying the Google account"
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-05-12"
        }
        policy {
          cookies_allowed: NO
          setting: "[TBD] This feature cannot be disabled in settings"
          policy_exception_justification: "WIP, guarded by `GlanceablesV2` flag"
        }
    )");

absl::optional<base::Time> ConvertCourseWorkItemDue(
    const absl::optional<CourseWorkItem::DueDateTime>& raw_due) {
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

GlanceablesClassroomStudentSubmission::State CalculateStudentSubmissionState(
    const std::unique_ptr<StudentSubmission>& raw_student_submission) {
  const auto raw_state = raw_student_submission->state();
  if (raw_state == StudentSubmission::State::kNew ||
      raw_state == StudentSubmission::State::kCreated ||
      raw_state == StudentSubmission::State::kReclaimedByStudent) {
    return GlanceablesClassroomStudentSubmission::State::kAssigned;
  }

  if (raw_state == StudentSubmission::State::kTurnedIn) {
    return GlanceablesClassroomStudentSubmission::State::kTurnedIn;
  }

  if (raw_state == StudentSubmission::State::kReturned) {
    return raw_student_submission->assigned_grade().has_value()
               ? GlanceablesClassroomStudentSubmission::State::kGraded
               : GlanceablesClassroomStudentSubmission::State::kAssigned;
  }

  return GlanceablesClassroomStudentSubmission::State::kOther;
}

}  // namespace

GlanceablesClassroomClientImpl::GlanceablesClassroomClientImpl(
    const GlanceablesClassroomClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesClassroomClientImpl::~GlanceablesClassroomClientImpl() = default;

void GlanceablesClassroomClientImpl::FetchStudentCourses(
    FetchCoursesCallback callback) {
  CHECK(callback);

  if (student_courses_) {
    // Invoke callback immediately with previously cached student courses.
    std::move(callback).Run(student_courses_.get());
    return;
  }

  student_courses_ =
      std::make_unique<ui::ListModel<GlanceablesClassroomCourse>>();
  FetchCoursesPage(/*student_id=*/kOwnCoursesFilterValue, /*teacher_id=*/"",
                   /*page_token=*/"", student_courses_.get(),
                   std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchTeacherCourses(
    FetchCoursesCallback callback) {
  CHECK(callback);

  if (teacher_courses_) {
    // Invoke callback immediately with previously cached teacher courses.
    std::move(callback).Run(teacher_courses_.get());
    return;
  }

  teacher_courses_ =
      std::make_unique<ui::ListModel<GlanceablesClassroomCourse>>();
  FetchCoursesPage(/*student_id=*/"", /*teacher_id=*/kOwnCoursesFilterValue,
                   /*page_token=*/"", teacher_courses_.get(),
                   std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchCourseWork(
    const std::string& course_id,
    FetchCourseWorkCallback callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const auto [iter, inserted] = course_work_.emplace(
      course_id,
      std::make_unique<ui::ListModel<GlanceablesClassroomCourseWorkItem>>());

  if (!inserted) {
    // Invoke callback immediately with previously cached course work items.
    std::move(callback).Run(iter->second.get());
    return;
  }

  FetchCourseWorkPage(course_id, /*page_token=*/"", std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchStudentSubmissions(
    const std::string& course_id,
    FetchStudentSubmissionsCallback callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const auto [iter, inserted] = student_submissions_.emplace(
      course_id,
      std::make_unique<ui::ListModel<GlanceablesClassroomStudentSubmission>>());

  if (!inserted) {
    // Invoke callback immediately with previously cached student submissions.
    std::move(callback).Run(iter->second.get());
    return;
  }

  FetchStudentSubmissionsPage(course_id, /*page_token=*/"",
                              std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchCoursesPage(
    const std::string& student_id,
    const std::string& teacher_id,
    const std::string& page_token,
    ui::ListModel<GlanceablesClassroomCourse>* courses_container,
    FetchCoursesCallback callback) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(courses_container);
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListCoursesRequest>(
          request_sender, student_id, teacher_id, page_token,
          base::BindOnce(&GlanceablesClassroomClientImpl::OnCoursesPageFetched,
                         weak_factory_.GetWeakPtr(), student_id, teacher_id,
                         courses_container, std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnCoursesPageFetched(
    const std::string& student_id,
    const std::string& teacher_id,
    ui::ListModel<GlanceablesClassroomCourse>* courses_container,
    FetchCoursesCallback callback,
    base::expected<std::unique_ptr<Courses>, ApiErrorCode> result) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(courses_container);
  CHECK(callback);

  if (!result.has_value()) {
    // TODO(b/282013130): handle failures of a single page fetch request more
    // gracefully (retry and/or reflect errors on UI).
    courses_container->DeleteAll();
    std::move(callback).Run(courses_container);
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (item->state() == Course::State::kActive) {
      courses_container->Add(std::make_unique<GlanceablesClassroomCourse>(
          item->id(), item->name()));
      FetchCourseWork(item->id(), base::DoNothing());
      FetchStudentSubmissions(item->id(), base::DoNothing());
    }
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(courses_container);
  } else {
    FetchCoursesPage(student_id, teacher_id, result.value()->next_page_token(),
                     courses_container, std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::FetchCourseWorkPage(
    const std::string& course_id,
    const std::string& page_token,
    FetchCourseWorkCallback callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListCourseWorkRequest>(
          request_sender, course_id, page_token,
          base::BindOnce(
              &GlanceablesClassroomClientImpl::OnCourseWorkPageFetched,
              weak_factory_.GetWeakPtr(), course_id, std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnCourseWorkPageFetched(
    const std::string& course_id,
    FetchCourseWorkCallback callback,
    base::expected<std::unique_ptr<CourseWork>, ApiErrorCode> result) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const auto iter = course_work_.find(course_id);

  if (!result.has_value()) {
    // TODO(b/282013130): handle failures of a single page fetch request more
    // gracefully (retry and/or reflect errors on UI).
    iter->second->DeleteAll();
    std::move(callback).Run(iter->second.get());
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (item->state() == CourseWorkItem::State::kPublished) {
      iter->second->Add(std::make_unique<GlanceablesClassroomCourseWorkItem>(
          item->id(), item->title(), item->alternate_link(),
          ConvertCourseWorkItemDue(item->due_date_time())));
    }
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(iter->second.get());
  } else {
    FetchCourseWorkPage(course_id, result.value()->next_page_token(),
                        std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::FetchStudentSubmissionsPage(
    const std::string& course_id,
    const std::string& page_token,
    FetchStudentSubmissionsCallback callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListStudentSubmissionsRequest>(
          request_sender, course_id, kAllStudentSubmissionsParameterValue,
          page_token,
          base::BindOnce(
              &GlanceablesClassroomClientImpl::OnStudentSubmissionsPageFetched,
              weak_factory_.GetWeakPtr(), course_id, std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnStudentSubmissionsPageFetched(
    const std::string& course_id,
    FetchStudentSubmissionsCallback callback,
    base::expected<std::unique_ptr<StudentSubmissions>, ApiErrorCode> result) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const auto iter = student_submissions_.find(course_id);

  if (!result.has_value()) {
    // TODO(b/282013130): handle failures of a single page fetch request more
    // gracefully (retry and/or reflect errors on UI).
    iter->second->DeleteAll();
    std::move(callback).Run(iter->second.get());
    return;
  }

  for (const auto& item : result.value()->items()) {
    iter->second->Add(std::make_unique<GlanceablesClassroomStudentSubmission>(
        item->id(), item->course_work_id(),
        CalculateStudentSubmissionState(item)));
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(iter->second.get());
  } else {
    FetchStudentSubmissionsPage(course_id, result.value()->next_page_token(),
                                std::move(callback));
  }
}

RequestSender* GlanceablesClassroomClientImpl::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ =
        std::move(create_request_sender_callback_)
            .Run(
                {GaiaConstants::kClassroomReadOnlyCoursesOAuth2Scope,
                 GaiaConstants::kClassroomReadOnlyCourseWorkSelfOAuth2Scope,
                 GaiaConstants::kClassroomReadOnlyCourseWorkStudentsOAuth2Scope,
                 GaiaConstants::
                     kClassroomReadOnlyStudentSubmissionsSelfOAuth2Scope,
                 GaiaConstants::
                     kClassroomReadOnlyStudentSubmissionsStudentsOAuth2Scope},
                kTrafficAnnotationTag);
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

}  // namespace ash

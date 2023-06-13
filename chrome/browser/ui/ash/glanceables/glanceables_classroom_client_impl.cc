// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
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

// TODO(b/283369115): consider doing this only once after fetching all
// submissions.
base::flat_map<std::string, std::vector<GlanceablesClassroomStudentSubmission*>>
GroupStudentSubmissionsByCourseWorkId(
    const std::vector<std::unique_ptr<GlanceablesClassroomStudentSubmission>>&
        student_submissions) {
  base::flat_map<std::string,
                 std::vector<GlanceablesClassroomStudentSubmission*>>
      grouped_submissions;
  for (const auto& submission : student_submissions) {
    grouped_submissions[submission->course_work_id].push_back(submission.get());
  }
  return grouped_submissions;
}

}  // namespace

GlanceablesClassroomClientImpl::GlanceablesClassroomClientImpl(
    const GlanceablesClassroomClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesClassroomClientImpl::~GlanceablesClassroomClientImpl() = default;

void GlanceablesClassroomClientImpl::IsStudentRoleActive(
    IsRoleEnabledCallback callback) {
  CHECK(callback);

  InvokeOnceStudentDataFetched(base::BindOnce(
      [](base::WeakPtr<GlanceablesClassroomClientImpl> self,
         base::OnceCallback<void(bool active)> callback) {
        if (!self) {
          std::move(callback).Run(false);
          return;
        }
        std::move(callback).Run(!self->student_courses_.empty());
      },
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetCompletedStudentAssignments(
    GetStudentAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return true; });
  auto submission_state_predicate = base::BindRepeating(
      [](GlanceablesClassroomStudentSubmission::State state) {
        return state ==
                   GlanceablesClassroomStudentSubmission::State::kTurnedIn ||
               state == GlanceablesClassroomStudentSubmission::State::kGraded;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      weak_factory_.GetWeakPtr(), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(callback)));
}

void GlanceablesClassroomClientImpl::
    GetStudentAssignmentsWithApproachingDueDate(
        GetStudentAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        return due.has_value() && now < due.value();
      },
      base::Time::Now());
  auto submission_state_predicate = base::BindRepeating(
      [](GlanceablesClassroomStudentSubmission::State state) {
        return state == GlanceablesClassroomStudentSubmission::State::kAssigned;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      weak_factory_.GetWeakPtr(), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetStudentAssignmentsWithMissedDueDate(
    GetStudentAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        return due.has_value() && now > due.value();
      },
      base::Time::Now());
  auto submission_state_predicate = base::BindRepeating(
      [](GlanceablesClassroomStudentSubmission::State state) {
        return state == GlanceablesClassroomStudentSubmission::State::kAssigned;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      weak_factory_.GetWeakPtr(), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetStudentAssignmentsWithoutDueDate(
    GetStudentAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return !due.has_value(); });
  auto submission_state_predicate = base::BindRepeating(
      [](GlanceablesClassroomStudentSubmission::State state) {
        return state == GlanceablesClassroomStudentSubmission::State::kAssigned;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      weak_factory_.GetWeakPtr(), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(callback)));
}

void GlanceablesClassroomClientImpl::FetchStudentCourses(
    FetchCoursesCallback callback) {
  CHECK(callback);

  student_courses_.clear();
  FetchCoursesPage(
      /*student_id=*/kOwnCoursesFilterValue, /*teacher_id=*/"",
      /*page_token=*/"", student_courses_, std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchTeacherCourses(
    FetchCoursesCallback callback) {
  CHECK(callback);

  teacher_courses_.clear();
  FetchCoursesPage(
      /*student_id=*/"", /*teacher_id=*/kOwnCoursesFilterValue,
      /*page_token=*/"", teacher_courses_, std::move(callback));
}

void GlanceablesClassroomClientImpl::FetchCourseWork(
    const std::string& course_id,
    FetchCourseWorkCallback callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const auto [iter, inserted] = course_work_.emplace(
      course_id,
      std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>());
  if (!inserted) {
    iter->second.clear();
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
      std::vector<std::unique_ptr<GlanceablesClassroomStudentSubmission>>());
  if (!inserted) {
    iter->second.clear();
  }

  FetchStudentSubmissionsPage(course_id, /*page_token=*/"",
                              std::move(callback));
}

void GlanceablesClassroomClientImpl::InvokeOnceStudentDataFetched(
    base::OnceClosure callback) {
  CHECK(callback);

  if (student_data_fetch_status_ == FetchStatus::kFetched) {
    std::move(callback).Run();
    return;
  }

  callbacks_waiting_for_student_data_.push_back(std::move(callback));

  if (student_data_fetch_status_ == FetchStatus::kNotFetched) {
    student_data_fetch_status_ = FetchStatus::kFetching;
    FetchStudentCourses(base::BindOnce(
        &GlanceablesClassroomClientImpl::OnCoursesFetched,
        weak_factory_.GetWeakPtr(),
        base::BindOnce(&GlanceablesClassroomClientImpl::OnStudentDataFetched,
                       weak_factory_.GetWeakPtr())));
  }
}

void GlanceablesClassroomClientImpl::FetchCoursesPage(
    const std::string& student_id,
    const std::string& teacher_id,
    const std::string& page_token,
    std::vector<std::unique_ptr<GlanceablesClassroomCourse>>& courses_container,
    FetchCoursesCallback callback) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListCoursesRequest>(
          request_sender, student_id, teacher_id, page_token,
          base::BindOnce(&GlanceablesClassroomClientImpl::OnCoursesPageFetched,
                         weak_factory_.GetWeakPtr(), student_id, teacher_id,
                         std::ref(courses_container), std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnCoursesPageFetched(
    const std::string& student_id,
    const std::string& teacher_id,
    std::vector<std::unique_ptr<GlanceablesClassroomCourse>>& courses_container,
    FetchCoursesCallback callback,
    base::expected<std::unique_ptr<Courses>, ApiErrorCode> result) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(callback);

  if (!result.has_value()) {
    // TODO(b/282013130): handle failures of a single page fetch request more
    // gracefully (retry and/or reflect errors on UI).
    courses_container.clear();
    std::move(callback).Run(courses_container);
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (item->state() == Course::State::kActive) {
      courses_container.push_back(std::make_unique<GlanceablesClassroomCourse>(
          item->id(), item->name()));
    }
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(courses_container);
  } else {
    FetchCoursesPage(student_id, teacher_id, result.value()->next_page_token(),
                     courses_container, std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::OnCoursesFetched(
    base::OnceClosure on_course_work_and_student_submissions_fetched,
    const std::vector<std::unique_ptr<GlanceablesClassroomCourse>>& courses) {
  CHECK(on_course_work_and_student_submissions_fetched);

  // `FetchCourseWork()` + `FetchStudentSubmissions()` per course.
  const auto expected_callback_calls = courses.size() * 2;
  const auto barrier_closure = base::BarrierClosure(
      expected_callback_calls,
      std::move(on_course_work_and_student_submissions_fetched));

  for (const auto& course : courses) {
    // Helps to prevent the presubmit error. Otherwise it thinks explicit
    // `std::unique_ptr` constructor is called with `barrier_closure` and asks
    // to use `std::make_unique<T>()` or `base::WrapUnique` instead. Looks like
    // a false-positive regular expression match.
    using FetchCourseWorkIgnoredArg =
        const std::vector<std::unique_ptr<GlanceablesClassroomCourseWorkItem>>&;
    using FetchStudentSubmissionsIgnoredArg = const std::vector<
        std::unique_ptr<GlanceablesClassroomStudentSubmission>>&;

    FetchCourseWork(course->id, base::IgnoreArgs<FetchCourseWorkIgnoredArg>(
                                    barrier_closure));
    FetchStudentSubmissions(
        course->id,
        base::IgnoreArgs<FetchStudentSubmissionsIgnoredArg>(barrier_closure));
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
    iter->second.clear();
    std::move(callback).Run(iter->second);
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (item->state() == CourseWorkItem::State::kPublished) {
      iter->second.push_back(
          std::make_unique<GlanceablesClassroomCourseWorkItem>(
              item->id(), item->title(), item->alternate_link(),
              ConvertCourseWorkItemDue(item->due_date_time())));
    }
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(iter->second);
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
    iter->second.clear();
    std::move(callback).Run(iter->second);
    return;
  }

  for (const auto& item : result.value()->items()) {
    iter->second.push_back(
        std::make_unique<GlanceablesClassroomStudentSubmission>(
            item->id(), item->course_work_id(),
            CalculateStudentSubmissionState(item)));
  }

  if (result.value()->next_page_token().empty()) {
    std::move(callback).Run(iter->second);
  } else {
    FetchStudentSubmissionsPage(course_id, result.value()->next_page_token(),
                                std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::OnStudentDataFetched() {
  student_data_fetch_status_ = FetchStatus::kFetched;
  for (auto& cb : callbacks_waiting_for_student_data_) {
    std::move(cb).Run();
  }
}

void GlanceablesClassroomClientImpl::GetFilteredStudentAssignments(
    base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
        due_predicate,
    base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmission::State)>
        submission_state_predicate,
    GetStudentAssignmentsCallback callback) {
  CHECK(due_predicate);
  CHECK(submission_state_predicate);
  CHECK(callback);

  std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
      filtered_assignments;

  for (const auto& course : student_courses_) {
    const auto course_work_iter = course_work_.find(course->id);
    const auto submissions_iter = student_submissions_.find(course->id);
    if (course_work_iter == course_work_.end() ||
        submissions_iter == student_submissions_.end()) {
      continue;
    }

    const auto submissions =
        GroupStudentSubmissionsByCourseWorkId(submissions_iter->second);

    for (const auto& course_work_item : course_work_iter->second) {
      if (!due_predicate.Run(course_work_item->due)) {
        continue;
      }

      const auto submission_iter = submissions.find(course_work_item->id);
      if (submission_iter == submissions.end()) {
        continue;
      }

      // There should be only one iteration, because course work item and
      // student submission have 1:1 relationship for students.
      for (const auto* const submission : submission_iter->second) {
        if (!submission_state_predicate.Run(submission->state)) {
          continue;
        }

        filtered_assignments.push_back(
            std::make_unique<GlanceablesClassroomStudentAssignment>(
                course->name, course_work_item->title, course_work_item->link,
                course_work_item->due));
      }
    }
  }

  std::move(callback).Run(std::move(filtered_assignments));
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

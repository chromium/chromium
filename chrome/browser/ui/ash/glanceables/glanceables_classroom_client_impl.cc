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
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_course_work_item.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "content/public/browser/browser_thread.h"
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
#include "url/gurl.h"

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

// Struct that temporarily holds information needed to create a classroom
// assignment structure for a course work item. Used when creating filtered
// teacher assignment list from the cached teacher course data.
struct TentativeTeacherAssignment {
  TentativeTeacherAssignment(
      const std::string& course_id,
      const std::string& course_work_id,
      const std::string& course_name,
      const GlanceablesClassroomCourseWorkItem* course_work_item)
      : course_id(course_id),
        course_work_id(course_work_id),
        course_name(course_name),
        course_work_item(course_work_item) {}
  TentativeTeacherAssignment(const TentativeTeacherAssignment&) = default;
  TentativeTeacherAssignment& operator=(const TentativeTeacherAssignment&) =
      default;
  ~TentativeTeacherAssignment() = default;

  std::string course_id;
  std::string course_work_id;
  std::string course_name;
  raw_ptr<const GlanceablesClassroomCourseWorkItem, ExperimentalAsh>
      course_work_item;
};

}  // namespace

GlanceablesClassroomClientImpl::CourseWorkRequest::CourseWorkRequest(
    base::OnceClosure callback)
    : callback_(std::move(callback)) {
  CHECK(callback_);
}

GlanceablesClassroomClientImpl::CourseWorkRequest::~CourseWorkRequest() =
    default;

void GlanceablesClassroomClientImpl::CourseWorkRequest::
    IncrementPendingPageCount() {
  ++pending_page_requests_;
}

void GlanceablesClassroomClientImpl::CourseWorkRequest::
    DecrementPendingPageCount() {
  CHECK_GT(pending_page_requests_, 0);
  --pending_page_requests_;
}

bool GlanceablesClassroomClientImpl::CourseWorkRequest::RespondIfComplete() {
  CHECK(callback_);

  if (pending_page_requests_ == 0) {
    std::move(callback_).Run();
    return true;
  }
  return false;
}

GlanceablesClassroomClientImpl::GlanceablesClassroomClientImpl(
    Profile* profile,
    base::Clock* clock,
    const GlanceablesClassroomClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback,
    bool use_best_effort_prefetch_task_runner)
    : profile_(profile),
      clock_(clock),
      create_request_sender_callback_(create_request_sender_callback),
      number_of_assignments_prioritized_for_display_(3) {
  if (use_best_effort_prefetch_task_runner) {
    teacher_data_prefetch_timer_.SetTaskRunner(
        content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT}));
  }

  teacher_data_prefetch_timer_.Start(
      FROM_HERE, base::Seconds(20),
      base::BindOnce(&GlanceablesClassroomClientImpl::PrefetchTeacherData,
                     base::Unretained(this)));
}

GlanceablesClassroomClientImpl::~GlanceablesClassroomClientImpl() = default;

void GlanceablesClassroomClientImpl::IsStudentRoleActive(
    IsRoleEnabledCallback callback) {
  CHECK(callback);

  InvokeOnceStudentDataFetched(base::BindOnce(
      [](GlanceablesClassroomClientImpl* self,
         base::OnceCallback<void(bool active)> callback) {
        std::move(callback).Run(!self->student_courses_.empty());
        return true;
      },
      base::Unretained(this), std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetCompletedStudentAssignments(
    GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return true; });
  auto submission_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state == GlanceablesClassroomStudentSubmissionState::kTurnedIn ||
               state == GlanceablesClassroomStudentSubmissionState::kGraded;
      });
  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // TODO(b/291609743): Order by when a student submission is turned-in
        // in descending order.
        return true;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(sort_comparator),
      std::move(callback)));
}

void GlanceablesClassroomClientImpl::
    GetStudentAssignmentsWithApproachingDueDate(
        GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        return due.has_value() && now < due.value();
      },
      clock_->Now());
  auto submission_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state == GlanceablesClassroomStudentSubmissionState::kAssigned;
      });
  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // `due_predicate` should have filtered out items with no due date.
        CHECK(lhs->due());
        CHECK(rhs->due());

        // Order by due date in ascending order.
        return *lhs->due() < *rhs->due();
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(sort_comparator),
      std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetStudentAssignmentsWithMissedDueDate(
    GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        return due.has_value() && now > due.value();
      },
      clock_->Now());
  auto submission_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state == GlanceablesClassroomStudentSubmissionState::kAssigned;
      });
  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // `due_predicate` should have filtered out items with no due date.
        CHECK(lhs->due());
        CHECK(rhs->due());

        // Order by due date in descending order.
        return *lhs->due() > *rhs->due();
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(sort_comparator),
      std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetStudentAssignmentsWithoutDueDate(
    GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return !due.has_value(); });
  auto submission_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state == GlanceablesClassroomStudentSubmissionState::kAssigned;
      });
  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // TODO(b/291609743): Order by publish date in descending order.
        return true;
      });
  InvokeOnceStudentDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredStudentAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submission_state_predicate), std::move(sort_comparator),
      std::move(callback)));
}

void GlanceablesClassroomClientImpl::IsTeacherRoleActive(
    IsRoleEnabledCallback callback) {
  CHECK(callback);

  InvokeOnceTeacherDataFetched(base::BindOnce(
      [](GlanceablesClassroomClientImpl* self,
         base::OnceCallback<void(bool active)> callback) {
        std::move(callback).Run(!self->teacher_courses_.empty());
        return true;
      },
      base::Unretained(this), std::move(callback)));
}

void GlanceablesClassroomClientImpl::
    GetTeacherAssignmentsWithApproachingDueDate(
        GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        // Only include items which an approaching due date.
        return due.has_value() && now < due.value();
      },
      clock_->Now());

  auto submissions_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state != GlanceablesClassroomStudentSubmissionState::kGraded;
      });

  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // `due_predicate` should have filtered out items with no due date.
        CHECK(lhs->due());
        CHECK(rhs->due());

        // Order by due date in ascending order.
        return *lhs->due() < *rhs->due();
      });

  InvokeOnceTeacherDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submissions_state_predicate), std::move(sort_comparator),
      /*allow_submissions_refresh=*/true, std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetTeacherAssignmentsRecentlyDue(
    GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const base::Time& now, const absl::optional<base::Time>& due) {
        //  Only include items with a due date in the past.
        return due.has_value() && now > due.value();
      },
      clock_->Now());

  auto submissions_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state != GlanceablesClassroomStudentSubmissionState::kGraded;
      });

  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // `due_predicate` should have filtered out items with no due date.
        CHECK(lhs->due());
        CHECK(rhs->due());

        // Order by due date in descending order.
        return *lhs->due() > *rhs->due();
      });

  InvokeOnceTeacherDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submissions_state_predicate), std::move(sort_comparator),
      /*allow_submissions_refresh=*/true, std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetTeacherAssignmentsWithoutDueDate(
    GetAssignmentsCallback callback) {
  CHECK(callback);

  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return !due.has_value(); });

  auto submissions_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state != GlanceablesClassroomStudentSubmissionState::kGraded;
      });

  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // Order by last update time in descending order.
        return lhs->last_update() > rhs->last_update();
      });

  InvokeOnceTeacherDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submissions_state_predicate), std::move(sort_comparator),
      /*allow_submissions_refresh=*/true, std::move(callback)));
}

void GlanceablesClassroomClientImpl::GetGradedTeacherAssignments(
    GetAssignmentsCallback callback) {
  auto due_predicate = base::BindRepeating(
      [](const absl::optional<base::Time>& due) { return true; });

  auto submissions_state_predicate =
      base::BindRepeating([](GlanceablesClassroomStudentSubmissionState state) {
        return state == GlanceablesClassroomStudentSubmissionState::kGraded;
      });

  auto sort_comparator =
      base::BindRepeating([](const GlanceablesClassroomCourseWorkItem* lhs,
                             const GlanceablesClassroomCourseWorkItem* rhs) {
        // TODO(b/291609743): Order by when last student submission was graded,
        // in descending order.
        // For now, order by the assignment's last update time in descending
        // order.
        return lhs->last_update() > rhs->last_update();
      });

  InvokeOnceTeacherDataFetched(base::BindOnce(
      &GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments,
      base::Unretained(this), std::move(due_predicate),
      std::move(submissions_state_predicate), std::move(sort_comparator),
      /*allow_submissions_refresh=*/true, std::move(callback)));
}

void GlanceablesClassroomClientImpl::OpenUrl(const GURL& url) const {
  if (!url.is_valid()) {
    return;
  }

  // TODO(b/283370862): consider opening PWA if installed.
  ShowSingletonTabOverwritingNTP(profile_, url);
}

void GlanceablesClassroomClientImpl::OnGlanceablesBubbleClosed() {
  for (FetchStatus* fetch_status :
       {&student_data_fetch_status_, &teacher_data_fetch_status_}) {
    switch (*fetch_status) {
      case FetchStatus::kNotFetched:
        break;
      case FetchStatus::kFetched:
        *fetch_status = FetchStatus::kNotFetched;
        break;
      case FetchStatus::kFetching:
        *fetch_status = FetchStatus::kFetchingInvalidated;
        break;
      case FetchStatus::kFetchingInvalidated:
        // Do no restart fetch if it's still in progress, which could happen if
        // the user toggles glanceables bubble in quick succession.
        *fetch_status = FetchStatus::kFetching;
        break;
    }
  }
}

bool GlanceablesClassroomClientImpl::
    FireTeacherDataPrefetchTimerIfRunningForTesting(
        base::OnceClosure prefetch_callback) {
  if (!teacher_data_prefetch_timer_.IsRunning()) {
    return false;
  }
  prefetch_callback_ = std::move(prefetch_callback);
  teacher_data_prefetch_timer_.FireNow();
  return true;
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
    bool fetch_submissions,
    CourseWorkPerCourse& course_work,
    base::OnceClosure callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  const int request_id = next_course_work_request_id_++;
  const auto [request_it, request_inserted] = course_work_requests_.emplace(
      request_id, std::make_unique<CourseWorkRequest>(std::move(callback)));
  CHECK(request_inserted);

  for (auto& course_work_info : course_work[course_id]) {
    course_work_info.second.InvalidateCourseWorkItem();
  }

  FetchCourseWorkPage(request_id, course_id, /*page_token=*/"",
                      /*page_number=*/1, fetch_submissions, course_work);
}

void GlanceablesClassroomClientImpl::FetchStudentSubmissions(
    const std::string& course_id,
    const std::string& course_work_id,
    CourseWorkPerCourse& course_work,
    base::OnceClosure callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  // Invalidate any preexisting cached student submissions info for requested
  // course work ID.
  if (course_work_id == kAllStudentSubmissionsParameterValue) {
    for (auto& course_work_info : course_work[course_id]) {
      course_work_info.second.InvalidateStudentSubmissions();
    }
  } else {
    course_work[course_id][course_work_id].InvalidateStudentSubmissions();
  }

  FetchStudentSubmissionsPage(course_id, course_work_id,
                              /*page_token=*/"", /*page_number=*/1, course_work,
                              std::move(callback));
}

void GlanceablesClassroomClientImpl::InvokeOnceStudentDataFetched(
    DataFetchCallback callback) {
  CHECK(callback);

  if (student_data_fetch_status_ == FetchStatus::kFetched) {
    std::move(callback).Run();
    return;
  }

  callbacks_waiting_for_student_data_.push_back(std::move(callback));

  const bool needs_fetch =
      student_data_fetch_status_ != FetchStatus::kFetching &&
      student_data_fetch_status_ != FetchStatus::kFetchingInvalidated;
  student_data_fetch_status_ = FetchStatus::kFetching;

  if (needs_fetch) {
    FetchStudentCourses(base::BindOnce(
        &GlanceablesClassroomClientImpl::OnCoursesFetched,
        weak_factory_.GetWeakPtr(),
        /*fetch_submissions_per_course_work=*/false,
        std::ref(student_course_work_),
        base::BindOnce(&GlanceablesClassroomClientImpl::OnStudentDataFetched,
                       weak_factory_.GetWeakPtr(), clock_->Now())));
  }
}

void GlanceablesClassroomClientImpl::InvokeOnceTeacherDataFetched(
    DataFetchCallback callback) {
  CHECK(callback);

  teacher_data_prefetch_timer_.Stop();

  if (teacher_data_fetch_status_ == FetchStatus::kFetched) {
    std::move(callback).Run();
    return;
  }

  callbacks_waiting_for_teacher_data_.push_back(std::move(callback));

  const bool needs_fetch =
      teacher_data_fetch_status_ != FetchStatus::kFetching &&
      teacher_data_fetch_status_ != FetchStatus::kFetchingInvalidated;
  teacher_data_fetch_status_ = FetchStatus::kFetching;

  if (needs_fetch) {
    FetchTeacherCourses(base::BindOnce(
        &GlanceablesClassroomClientImpl::OnCoursesFetched,
        weak_factory_.GetWeakPtr(),
        /*fetch_submissions_per_course_work=*/true,
        std::ref(teacher_course_work_),
        base::BindOnce(&GlanceablesClassroomClientImpl::OnTeacherDataFetched,
                       weak_factory_.GetWeakPtr(), clock_->Now())));
  }
}

void GlanceablesClassroomClientImpl::FetchCoursesPage(
    const std::string& student_id,
    const std::string& teacher_id,
    const std::string& page_token,
    CourseList& courses_container,
    FetchCoursesCallback callback) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListCoursesRequest>(
          request_sender, student_id, teacher_id, page_token,
          base::BindOnce(&GlanceablesClassroomClientImpl::OnCoursesPageFetched,
                         weak_factory_.GetWeakPtr(), student_id, teacher_id,
                         std::ref(courses_container), clock_->Now(),
                         std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnCoursesPageFetched(
    const std::string& student_id,
    const std::string& teacher_id,
    CourseList& courses_container,
    const base::Time& request_start_time,
    FetchCoursesCallback callback,
    base::expected<std::unique_ptr<Courses>, ApiErrorCode> result) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(callback);

  base::UmaHistogramTimes("Ash.Glanceables.Api.Classroom.GetCourses.Latency",
                          clock_->Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Classroom.GetCourses.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

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
    base::UmaHistogramCounts100(
        base::ReplaceStringPlaceholders(
            "Ash.Glanceables.Api.Classroom.$1CoursesCount",
            {student_id == kOwnCoursesFilterValue ? "Student" : "Teacher"},
            nullptr),
        courses_container.size());
    std::move(callback).Run(courses_container);
  } else {
    FetchCoursesPage(student_id, teacher_id, result.value()->next_page_token(),
                     courses_container, std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::OnCoursesFetched(
    bool fetch_submissions_per_course_work,
    CourseWorkPerCourse& course_work,
    base::OnceClosure on_course_work_and_student_submissions_fetched,
    const CourseList& courses) {
  CHECK(on_course_work_and_student_submissions_fetched);

  // `FetchCourseWork()` + `FetchStudentSubmissions()` per course.
  const auto expected_callback_calls =
      courses.size() * (fetch_submissions_per_course_work ? 1 : 2);
  const auto barrier_closure = base::BarrierClosure(
      expected_callback_calls,
      std::move(on_course_work_and_student_submissions_fetched));

  for (const auto& course : courses) {
    FetchCourseWork(course->id, fetch_submissions_per_course_work, course_work,
                    barrier_closure);

    if (!fetch_submissions_per_course_work) {
      FetchStudentSubmissions(course->id, kAllStudentSubmissionsParameterValue,
                              course_work, barrier_closure);
    }
  }
}

void GlanceablesClassroomClientImpl::FetchCourseWorkPage(
    int request_id,
    const std::string& course_id,
    const std::string& page_token,
    int page_number,
    bool fetch_submissions,
    CourseWorkPerCourse& course_work) {
  CHECK(!course_id.empty());

  auto request_it = course_work_requests_.find(request_id);
  if (request_it == course_work_requests_.end() || !request_it->second) {
    return;
  }
  request_it->second->IncrementPendingPageCount();

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListCourseWorkRequest>(
          request_sender, course_id, page_token,
          base::BindOnce(
              &GlanceablesClassroomClientImpl::OnCourseWorkPageFetched,
              weak_factory_.GetWeakPtr(), request_id, course_id,
              fetch_submissions, std::ref(course_work), clock_->Now(),
              page_number)));
}

void GlanceablesClassroomClientImpl::OnCourseWorkPageFetched(
    int request_id,
    const std::string& course_id,
    bool fetch_submissions,
    CourseWorkPerCourse& course_work,
    const base::Time& request_start_time,
    int page_number,
    base::expected<std::unique_ptr<CourseWork>, ApiErrorCode> result) {
  CHECK(!course_id.empty());

  base::UmaHistogramTimes("Ash.Glanceables.Api.Classroom.GetCourseWork.Latency",
                          clock_->Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Classroom.GetCourseWork.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  auto request_it = course_work_requests_.find(request_id);
  if (request_it == course_work_requests_.end() || !request_it->second) {
    return;
  }

  CourseWorkInfo& course_work_for_course = course_work[course_id];

  if (!result.has_value()) {
    request_it->second->DecrementPendingPageCount();
    if (request_it->second->RespondIfComplete()) {
      course_work_requests_.erase(request_it);
    }
    return;
  }

  std::set<std::string> submissions_to_fetch;
  for (const auto& item : result.value()->items()) {
    if (item->state() != CourseWorkItem::State::kPublished) {
      course_work_for_course.erase(item->id());
      continue;
    }

    auto& course_work_item = course_work_for_course[item->id()];
    course_work_item.SetCourseWorkItem(item.get());
    if (fetch_submissions) {
      if (course_work_item.StudentSubmissionsNeedRefetch(clock_->Now())) {
        submissions_to_fetch.insert(item->id());
      } else {
        course_work_item.SetHasFreshSubmissionsState(false, clock_->Now());
      }
    }
  }

  if (!result.value()->next_page_token().empty()) {
    FetchCourseWorkPage(request_id, course_id,
                        result.value()->next_page_token(), page_number + 1,
                        fetch_submissions, course_work);
  } else {
    base::UmaHistogramCounts100(
        "Ash.Glanceables.Api.Classroom.GetCourseWork.PagesCount", page_number);
  }

  // NOTE: If `submissions_to_fetch` is empty, `barrier_closure` will run
  // immediately.
  const auto barrier_closure = base::BarrierClosure(
      submissions_to_fetch.size(),
      base::BindOnce(
          &GlanceablesClassroomClientImpl::OnCourseWorkSubmissionsFetched,
          weak_factory_.GetWeakPtr(), request_id, course_id));
  for (const auto& course_work_id : submissions_to_fetch) {
    FetchStudentSubmissions(course_id, course_work_id, course_work,
                            barrier_closure);
  }
}

void GlanceablesClassroomClientImpl::OnCourseWorkSubmissionsFetched(
    int request_id,
    const std::string& course_id) {
  auto request_it = course_work_requests_.find(request_id);
  if (request_it == course_work_requests_.end() || !request_it->second) {
    return;
  }

  request_it->second->DecrementPendingPageCount();

  if (request_it->second->RespondIfComplete()) {
    course_work_requests_.erase(request_it);
  }
}

void GlanceablesClassroomClientImpl::FetchStudentSubmissionsPage(
    const std::string& course_id,
    const std::string& course_work_id,
    const std::string& page_token,
    int page_number,
    CourseWorkPerCourse& course_work,
    base::OnceClosure callback) {
  CHECK(!course_id.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListStudentSubmissionsRequest>(
          request_sender, course_id, course_work_id, page_token,
          base::BindOnce(
              &GlanceablesClassroomClientImpl::OnStudentSubmissionsPageFetched,
              weak_factory_.GetWeakPtr(), course_id, course_work_id,
              std::ref(course_work), clock_->Now(), page_number,
              std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnStudentSubmissionsPageFetched(
    const std::string& course_id,
    const std::string& course_work_id,
    CourseWorkPerCourse& course_work,
    const base::Time& request_start_time,
    int page_number,
    base::OnceClosure callback,
    base::expected<std::unique_ptr<StudentSubmissions>, ApiErrorCode> result) {
  CHECK(!course_id.empty());
  CHECK(callback);

  base::UmaHistogramTimes(
      "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Latency",
      clock_->Now() - request_start_time);
  base::UmaHistogramSparse(
      "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Status",
      result.error_or(ApiErrorCode::HTTP_SUCCESS));

  auto& course_work_info_map = course_work[course_id];
  GlanceablesClassroomCourseWorkItem* const shared_course_work_info =
      (course_work_id == kAllStudentSubmissionsParameterValue)
          ? nullptr
          : &course_work_info_map[course_work_id];

  if (!result.has_value()) {
    // TODO(b/282013130): handle failures of a single page fetch request more
    // gracefully (retry and/or reflect errors on UI).
    if (course_work_id == kAllStudentSubmissionsParameterValue) {
      for (auto& course_work_info : course_work_info_map) {
        course_work_info.second.InvalidateStudentSubmissions();
      }
    } else {
      shared_course_work_info->InvalidateStudentSubmissions();
    }

    std::move(callback).Run();
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (shared_course_work_info) {
      CHECK_EQ(course_work_id, item->course_work_id());
    }

    GlanceablesClassroomCourseWorkItem* const submission_course_work_info =
        shared_course_work_info ? shared_course_work_info
                                : &course_work_info_map[item->course_work_id()];
    submission_course_work_info->AddStudentSubmission(item.get());
  }

  if (result.value()->next_page_token().empty()) {
    base::UmaHistogramCounts100(
        "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.PagesCount",
        page_number);
    if (shared_course_work_info) {
      shared_course_work_info->SetHasFreshSubmissionsState(true, clock_->Now());
    }
    std::move(callback).Run();
  } else {
    FetchStudentSubmissionsPage(
        course_id, course_work_id, result.value()->next_page_token(),
        page_number + 1, course_work, std::move(callback));
  }
}

void GlanceablesClassroomClientImpl::OnStudentDataFetched(
    const base::Time& sequence_start_time) {
  base::UmaHistogramMediumTimes(
      "Ash.Glanceables.Api.Classroom.StudentDataFetchTime",
      clock_->Now() - sequence_start_time);

  switch (student_data_fetch_status_) {
    case FetchStatus::kNotFetched:
    case FetchStatus::kFetched:
      NOTREACHED();
      break;
    case FetchStatus::kFetching:
      student_data_fetch_status_ = FetchStatus::kFetched;
      break;
    case FetchStatus::kFetchingInvalidated:
      student_data_fetch_status_ = FetchStatus::kNotFetched;
      break;
  }

  PruneInvalidCourseWork(student_courses_, student_course_work_);

  std::list<DataFetchCallback> callbacks;
  callbacks_waiting_for_student_data_.swap(callbacks);

  for (auto& cb : callbacks) {
    CHECK(std::move(cb).Run());
  }
}

void GlanceablesClassroomClientImpl::OnTeacherDataFetched(
    const base::Time& sequence_start_time) {
  base::UmaHistogramMediumTimes(
      "Ash.Glanceables.Api.Classroom.TeacherDataFetchTime",
      clock_->Now() - sequence_start_time);

  switch (teacher_data_fetch_status_) {
    case FetchStatus::kNotFetched:
    case FetchStatus::kFetched:
      NOTREACHED();
      break;
    case FetchStatus::kFetching:
      teacher_data_fetch_status_ = FetchStatus::kFetched;
      break;
    case FetchStatus::kFetchingInvalidated:
      teacher_data_fetch_status_ = FetchStatus::kNotFetched;
      break;
  }

  PruneInvalidCourseWork(teacher_courses_, teacher_course_work_);

  std::list<DataFetchCallback> callbacks;
  callbacks_waiting_for_teacher_data_.swap(callbacks);

  while (!callbacks.empty()) {
    DataFetchCallback callback = std::move(callbacks.front());
    callbacks.pop_front();

    const bool success = std::move(callback).Run();
    if (!success) {
      CHECK_EQ(teacher_data_fetch_status_, FetchStatus::kFetching);
      break;
    }
  }
  for (auto& callback : callbacks) {
    callbacks_waiting_for_teacher_data_.push_back(std::move(callback));
  }
}

bool GlanceablesClassroomClientImpl::GetFilteredStudentAssignments(
    base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
        due_predicate,
    base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
        submission_state_predicate,
    SortComparator sort_comparator,
    GetAssignmentsCallback callback) {
  CHECK(due_predicate);
  CHECK(submission_state_predicate);
  CHECK(callback);

  if (callback.IsCancelled()) {
    return true;
  }

  if (student_data_fetch_status_ == FetchStatus::kNotFetched) {
    std::move(callback).Run({});
    return true;
  }

  using CourseNameAndCourseWork =
      std::pair<std::string, const GlanceablesClassroomCourseWorkItem*>;
  std::vector<CourseNameAndCourseWork> filtered_items;

  for (const auto& course : student_courses_) {
    const auto course_work_iter = student_course_work_.find(course->id);
    if (course_work_iter == student_course_work_.end()) {
      continue;
    }

    for (const auto& course_work_item : course_work_iter->second) {
      if (!course_work_item.second.SatisfiesPredicates(
              due_predicate, submission_state_predicate)) {
        continue;
      }
      filtered_items.push_back(
          std::make_pair(course->name, &course_work_item.second));
    }
  }

  std::sort(filtered_items.begin(), filtered_items.end(),
            [&sort_comparator](const CourseNameAndCourseWork& lhs,
                               const CourseNameAndCourseWork& rhs) {
              return sort_comparator.Run(lhs.second, rhs.second);
            });

  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
      filtered_assignments;
  for (const auto& item : filtered_items) {
    filtered_assignments.push_back(item.second->CreateClassroomAssignment(
        item.first, /*include_aggregated_submissions_state=*/false));
  }
  std::move(callback).Run(std::move(filtered_assignments));
  return true;
}

bool GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments(
    base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
        due_predicate,
    base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
        submission_state_predicate,
    SortComparator sort_comparator,
    bool allow_submissions_refresh,
    GetAssignmentsCallback callback) {
  CHECK(due_predicate);
  CHECK(callback);

  if (callback.IsCancelled()) {
    return true;
  }

  if (teacher_data_fetch_status_ == FetchStatus::kNotFetched) {
    std::move(callback).Run({});
    return true;
  }

  std::vector<TentativeTeacherAssignment> filtered_items;

  for (const auto& course : teacher_courses_) {
    const auto course_work_iter = teacher_course_work_.find(course->id);
    if (course_work_iter == teacher_course_work_.end()) {
      continue;
    }

    for (const auto& course_work_item : course_work_iter->second) {
      if (!course_work_item.second.SatisfiesPredicates(
              due_predicate, submission_state_predicate)) {
        continue;
      }
      filtered_items.emplace_back(course->id, course_work_item.first,
                                  course->name, &course_work_item.second);
    }
  }

  std::sort(filtered_items.begin(), filtered_items.end(),
            [&sort_comparator](const TentativeTeacherAssignment& lhs,
                               const TentativeTeacherAssignment& rhs) {
              return sort_comparator.Run(lhs.course_work_item,
                                         rhs.course_work_item);
            });

  std::vector<std::pair<std::string, std::string>> unfresh_top_items;
  for (size_t i = 0u; i < number_of_assignments_prioritized_for_display_ &&
                      i < filtered_items.size();
       ++i) {
    const TentativeTeacherAssignment& item = filtered_items[i];
    if (!item.course_work_item->has_fresh_submissions_state()) {
      unfresh_top_items.push_back(
          std::make_pair(item.course_id, item.course_work_id));
    }
  }

  // If any of items that are expected to show up in the UI have not been
  // refreshed during the latest fetch cycle, refresh them before returning the
  // filtered assignment list to the callback.
  if (allow_submissions_refresh && !unfresh_top_items.empty()) {
    callbacks_waiting_for_teacher_data_.push_back(base::BindOnce(
        &GlanceablesClassroomClientImpl::GetFilteredTeacherAssignments,
        base::Unretained(this), std::move(due_predicate),
        std::move(submission_state_predicate), std::move(sort_comparator),
        /*allow_submissions_refresh=*/false, std::move(callback)));

    teacher_data_fetch_status_ = FetchStatus::kFetching;

    const auto barrier_closure = base::BarrierClosure(
        unfresh_top_items.size(),
        base::BindOnce(&GlanceablesClassroomClientImpl::OnTeacherDataFetched,
                       weak_factory_.GetWeakPtr(), clock_->Now()));
    for (const auto& [course_id, course_work_id] : unfresh_top_items) {
      FetchStudentSubmissions(course_id, course_work_id, teacher_course_work_,
                              barrier_closure);
    }
    return false;
  }

  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
      filtered_assignments;
  for (const auto& item : filtered_items) {
    filtered_assignments.push_back(
        item.course_work_item->CreateClassroomAssignment(
            item.course_name, /*include_aggregated_submissions_state=*/true));
  }

  std::move(callback).Run(std::move(filtered_assignments));
  return true;
}

void GlanceablesClassroomClientImpl::PruneInvalidCourseWork(
    const CourseList& courses,
    CourseWorkPerCourse& course_work) {
  std::set<std::string> course_ids;
  for (const auto& course : courses) {
    course_ids.insert(course->id);
  }

  base::EraseIf(course_work, [&course_ids](const auto& per_course_info) {
    return !course_ids.contains(per_course_info.first);
  });

  for (const auto& course : courses) {
    const auto course_work_iter = course_work.find(course->id);
    if (course_work_iter != course_work.end()) {
      base::EraseIf(course_work_iter->second, [](const auto& course_work_item) {
        return !course_work_item.second.IsValid();
      });
    }
  }
}

void GlanceablesClassroomClientImpl::PrefetchTeacherData() {
  CHECK_EQ(teacher_data_fetch_status_, FetchStatus::kNotFetched);

  callbacks_waiting_for_teacher_data_.push_back(base::BindOnce(
      [](base::OnceClosure callback) {
        if (callback) {
          std::move(callback).Run();
        }
        return true;
      },
      std::move(prefetch_callback_)));

  // Start in kFetchingInvalidated state so more recent data gets refetched when
  // requested from UI.
  teacher_data_fetch_status_ = FetchStatus::kFetchingInvalidated;

  FetchTeacherCourses(base::BindOnce(
      &GlanceablesClassroomClientImpl::OnCoursesFetched,
      weak_factory_.GetWeakPtr(),
      /*fetch_submissions_per_course_work=*/true,
      std::ref(teacher_course_work_),
      base::BindOnce(&GlanceablesClassroomClientImpl::OnTeacherDataFetched,
                     weak_factory_.GetWeakPtr(), clock_->Now())));
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

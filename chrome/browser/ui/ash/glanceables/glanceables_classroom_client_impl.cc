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
#include "base/types/expected.h"
#include "google_apis/classroom/classroom_api_courses_response_types.h"
#include "google_apis/classroom/classroom_api_list_courses_request.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::RequestSender;
using ::google_apis::classroom::Course;
using ::google_apis::classroom::Courses;
using ::google_apis::classroom::ListCoursesRequest;

// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";

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
                         page_token, courses_container, std::move(callback))));
}

void GlanceablesClassroomClientImpl::OnCoursesPageFetched(
    const std::string& student_id,
    const std::string& teacher_id,
    const std::string& page_token,
    ui::ListModel<GlanceablesClassroomCourse>* courses_container,
    FetchCoursesCallback callback,
    base::expected<std::unique_ptr<Courses>, ApiErrorCode> result) {
  CHECK(!student_id.empty() || !teacher_id.empty());
  CHECK(courses_container);
  CHECK(callback);

  if (!result.has_value()) {
    courses_container->DeleteAll();
    std::move(callback).Run(courses_container);
    return;
  }

  for (const auto& item : result.value()->items()) {
    if (item->state() == Course::State::kActive) {
      courses_container->Add(std::make_unique<GlanceablesClassroomCourse>(
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

RequestSender* GlanceablesClassroomClientImpl::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ =
        std::move(create_request_sender_callback_)
            .Run({GaiaConstants::kClassroomReadOnlyCoursesOAuth2Scope},
                 kTrafficAnnotationTag);
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

}  // namespace ash

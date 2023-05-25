// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/time_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

using ::base::test::RepeatingTestFuture;
using ::base::test::TestFuture;
using ::google_apis::util::FormatTimeAsString;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Return;

// Helper class to simplify mocking `net::EmbeddedTestServer` responses,
// especially useful for subsequent responses when testing pagination logic.
class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse(
      const std::string& content) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

}  // namespace

class GlanceablesClassroomClientImplTest : public testing::Test {
 public:
  void SetUp() override {
    auto create_request_sender_callback = base::BindLambdaForTesting(
        [&](const std::vector<std::string>& scopes,
            const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
          return std::make_unique<google_apis::RequestSender>(
              std::make_unique<google_apis::DummyAuthService>(),
              url_loader_factory_, task_environment_.GetMainThreadTaskRunner(),
              "test-user-agent", TRAFFIC_ANNOTATION_FOR_TESTS);
        });
    client_ = std::make_unique<GlanceablesClassroomClientImpl>(
        create_request_sender_callback);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "classroom_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->classroom_api_origin_url(),
              test_server_.base_url().spec());
  }

  GlanceablesClassroomClientImpl* client() { return client_.get(); }
  TestRequestHandler& request_handler() { return request_handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
  std::unique_ptr<GlanceablesClassroomClientImpl> client_;
};

// ----------------------------------------------------------------------------
// Fetch all courses:

// Fetches and makes sure only "ACTIVE" courses are converted to
// `GlanceablesClassroomCourse`.
TEST_F(GlanceablesClassroomClientImplTest, FetchCourses) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                },
                {
                  "id": "course-id-2",
                  "name": "??? Course 2",
                  "courseState": "???"
                }
              ]
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));

  auto fetch_courses_methods = std::vector<base::RepeatingCallback<void(
      GlanceablesClassroomClientImpl::FetchCoursesCallback)>>{
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                          base::Unretained(client())),
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                          base::Unretained(client()))};

  for (auto fetch_method : fetch_courses_methods) {
    TestFuture<ui::ListModel<GlanceablesClassroomCourse>*> future;
    fetch_method.Run(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    const auto* const courses = future.Get();
    ASSERT_EQ(courses->item_count(), 1u);

    EXPECT_EQ(courses->GetItemAt(0)->id, "course-id-1");
    EXPECT_EQ(courses->GetItemAt(0)->name, "Active Course 1");
  }
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCoursesOnSubsequentCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .Times(
          2 /* 1 for `FetchStudentCourses()` + 1 for `FetchTeacherCourses()` */)
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                },
                {
                  "id": "course-id-2",
                  "name": "??? Course 2",
                  "courseState": "???"
                }
              ]
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));

  auto fetch_courses_methods = std::vector<base::RepeatingCallback<void(
      GlanceablesClassroomClientImpl::FetchCoursesCallback)>>{
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                          base::Unretained(client())),
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                          base::Unretained(client()))};

  for (auto fetch_method : fetch_courses_methods) {
    RepeatingTestFuture<ui::ListModel<GlanceablesClassroomCourse>*> future;
    fetch_method.Run(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    const auto* const courses = future.Take();

    // Subsequent request doesn't trigger another network call and returns a
    // pointer to the same `ui::ListModel`.
    fetch_method.Run(future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(future.Take(), courses);
  }
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCoursesOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_)).WillRepeatedly(Invoke([]() {
    return TestRequestHandler::CreateFailedResponse();
  }));

  auto fetch_courses_methods = std::vector<base::RepeatingCallback<void(
      GlanceablesClassroomClientImpl::FetchCoursesCallback)>>{
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                          base::Unretained(client())),
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                          base::Unretained(client()))};

  for (auto fetch_method : fetch_courses_methods) {
    TestFuture<ui::ListModel<GlanceablesClassroomCourse>*> future;
    fetch_method.Run(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    const auto* const courses = future.Get();
    ASSERT_EQ(courses->item_count(), 0u);
  }
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCoursesMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), Not(HasSubstr("pageToken"))))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-1", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-2-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-2", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-3-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-3", "courseState": "ACTIVE"}
              ]
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillRepeatedly(Invoke(
          []() { return TestRequestHandler::CreateSuccessfulResponse("{}"); }));

  auto fetch_courses_methods = std::vector<base::RepeatingCallback<void(
      GlanceablesClassroomClientImpl::FetchCoursesCallback)>>{
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                          base::Unretained(client())),
      base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                          base::Unretained(client()))};

  for (auto fetch_method : fetch_courses_methods) {
    TestFuture<ui::ListModel<GlanceablesClassroomCourse>*> future;
    fetch_method.Run(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    const auto* const courses = future.Get();
    ASSERT_EQ(courses->item_count(), 3u);

    EXPECT_EQ(courses->GetItemAt(0)->id, "course-id-from-page-1");
    EXPECT_EQ(courses->GetItemAt(1)->id, "course-id-from-page-2");
    EXPECT_EQ(courses->GetItemAt(2)->id, "course-id-from-page-3");
  }
}

// ----------------------------------------------------------------------------
// Fetch all course work:

// Fetches and makes sure only "PUBLISHED" course work items are converted to
// `GlanceablesClassroomCourseWorkItem`.
TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWork) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1"
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math multiple choice question",
                  "state": "DRAFT",
                  "alternateLink": "https://classroom.google.com/test-link-2"
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment with due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })");
      }));

  TestFuture<ui::ListModel<GlanceablesClassroomCourseWorkItem>*> future;
  client()->FetchCourseWork(/*course_id=*/"course-123", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const course_work = future.Get();
  ASSERT_EQ(course_work->item_count(), 2u);

  EXPECT_EQ(course_work->GetItemAt(0)->id, "course-work-item-1");
  EXPECT_EQ(course_work->GetItemAt(0)->title, "Math assignment");
  EXPECT_EQ(course_work->GetItemAt(0)->link,
            "https://classroom.google.com/test-link-1");
  EXPECT_FALSE(course_work->GetItemAt(0)->due);

  EXPECT_EQ(course_work->GetItemAt(1)->id, "course-work-item-3");
  EXPECT_EQ(course_work->GetItemAt(1)->title, "Math assignment with due date");
  EXPECT_EQ(course_work->GetItemAt(1)->link,
            "https://classroom.google.com/test-link-3");
  EXPECT_EQ(FormatTimeAsString(course_work->GetItemAt(1)->due.value()),
            "2023-04-25T15:09:25.250Z");
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkOnSubsequentCalls) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "courseWork": [
              {
                "id": "course-work-item-1",
                "title": "Math assignment",
                "state": "PUBLISHED",
                "alternateLink": "https://classroom.google.com/test-link-1"
              }
            ]
          })"))));

  RepeatingTestFuture<ui::ListModel<GlanceablesClassroomCourseWorkItem>*>
      future;
  client()->FetchCourseWork(/*course_id=*/"course-123", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const course_work = future.Take();

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->FetchCourseWork(/*course_id=*/"course-123", future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), course_work);
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkOnHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillRepeatedly(
          Invoke([]() { return TestRequestHandler::CreateFailedResponse(); }));

  TestFuture<ui::ListModel<GlanceablesClassroomCourseWorkItem>*> future;
  client()->FetchCourseWork(/*course_id=*/"course-123", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const course_work = future.Get();
  ASSERT_EQ(course_work->item_count(), 0u);
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-1", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-2-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-2", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-3-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-3", "state": "PUBLISHED"}
              ]
            })");
      }));

  TestFuture<ui::ListModel<GlanceablesClassroomCourseWorkItem>*> future;
  client()->FetchCourseWork(/*course_id=*/"course-123", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const courses = future.Get();
  ASSERT_EQ(courses->item_count(), 3u);

  EXPECT_EQ(courses->GetItemAt(0)->id, "course-work-item-from-page-1");
  EXPECT_EQ(courses->GetItemAt(1)->id, "course-work-item-from-page-2");
  EXPECT_EQ(courses->GetItemAt(2)->id, "course-work-item-from-page-3");
}

// ----------------------------------------------------------------------------
// Fetch all student submissions:

TEST_F(GlanceablesClassroomClientImplTest, FetchStudentSubmissions) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-1",
                  "state": "CREATED"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-1",
                  "state": "RECLAIMED_BY_STUDENT"
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-1",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-1",
                  "state": "RETURNED"
                },
                {
                  "id": "student-submission-6",
                  "courseWorkId": "course-work-1",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-7",
                  "courseWorkId": "course-work-1",
                  "state": "???"
                }
              ]
            })");
      }));

  TestFuture<ui::ListModel<GlanceablesClassroomStudentSubmission>*> future;
  client()->FetchStudentSubmissions(/*course_id=*/"course-123",
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const student_submissions = future.Get();
  ASSERT_EQ(student_submissions->item_count(), 7u);

  EXPECT_EQ(student_submissions->GetItemAt(0)->id, "student-submission-1");
  EXPECT_EQ(student_submissions->GetItemAt(0)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(0)->state,
            GlanceablesClassroomStudentSubmission::State::kAssigned);

  EXPECT_EQ(student_submissions->GetItemAt(1)->id, "student-submission-2");
  EXPECT_EQ(student_submissions->GetItemAt(1)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(1)->state,
            GlanceablesClassroomStudentSubmission::State::kAssigned);

  EXPECT_EQ(student_submissions->GetItemAt(2)->id, "student-submission-3");
  EXPECT_EQ(student_submissions->GetItemAt(2)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(2)->state,
            GlanceablesClassroomStudentSubmission::State::kAssigned);

  EXPECT_EQ(student_submissions->GetItemAt(3)->id, "student-submission-4");
  EXPECT_EQ(student_submissions->GetItemAt(3)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(3)->state,
            GlanceablesClassroomStudentSubmission::State::kTurnedIn);

  EXPECT_EQ(student_submissions->GetItemAt(4)->id, "student-submission-5");
  EXPECT_EQ(student_submissions->GetItemAt(4)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(4)->state,
            GlanceablesClassroomStudentSubmission::State::kAssigned);

  EXPECT_EQ(student_submissions->GetItemAt(5)->id, "student-submission-6");
  EXPECT_EQ(student_submissions->GetItemAt(5)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(5)->state,
            GlanceablesClassroomStudentSubmission::State::kGraded);

  EXPECT_EQ(student_submissions->GetItemAt(6)->id, "student-submission-7");
  EXPECT_EQ(student_submissions->GetItemAt(6)->course_work_id, "course-work-1");
  EXPECT_EQ(student_submissions->GetItemAt(6)->state,
            GlanceablesClassroomStudentSubmission::State::kOther);
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentSubmissionsOnSubsequentCalls) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "studentSubmissions": [
              {"id": "student-submission-1", "courseWorkId": "course-work-1"}
            ]
          })"))));

  RepeatingTestFuture<ui::ListModel<GlanceablesClassroomStudentSubmission>*>
      future;
  client()->FetchStudentSubmissions(/*course_id=*/"course-123",
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const student_submissions = future.Take();

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->FetchStudentSubmissions(/*course_id=*/"course-123",
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), student_submissions);
}

TEST_F(GlanceablesClassroomClientImplTest, FetchStudentSubmissionsOnHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillRepeatedly(
          Invoke([]() { return TestRequestHandler::CreateFailedResponse(); }));

  TestFuture<ui::ListModel<GlanceablesClassroomStudentSubmission>*> future;
  client()->FetchStudentSubmissions(/*course_id=*/"course-123",
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const student_submissions = future.Get();
  ASSERT_EQ(student_submissions->item_count(), 0u);
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentSubmissionsMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {"id": "student-submission-from-page-1"}
              ],
              "nextPageToken": "page-2-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {"id": "student-submission-from-page-2"}
              ],
              "nextPageToken": "page-3-token"
            })");
      }));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {"id": "student-submission-from-page-3"}
              ]
            })");
      }));

  TestFuture<ui::ListModel<GlanceablesClassroomStudentSubmission>*> future;
  client()->FetchStudentSubmissions(/*course_id=*/"course-123",
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const student_submissions = future.Get();
  ASSERT_EQ(student_submissions->item_count(), 3u);

  EXPECT_EQ(student_submissions->GetItemAt(0)->id,
            "student-submission-from-page-1");
  EXPECT_EQ(student_submissions->GetItemAt(1)->id,
            "student-submission-from-page-2");
  EXPECT_EQ(student_submissions->GetItemAt(2)->id,
            "student-submission-from-page-3");
}

}  // namespace ash

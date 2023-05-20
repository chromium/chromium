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
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::_;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;

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

// Fetches and makes sure only "ACTIVE" courses are converted to
// `GlanceablesClassroomCourse`.
TEST_F(GlanceablesClassroomClientImplTest, FetchCourses) {
  EXPECT_CALL(request_handler(), HandleRequest(_)).WillRepeatedly(Invoke([]() {
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
  EXPECT_CALL(request_handler(), HandleRequest(_))
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
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
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
                                  HasSubstr("pageToken=page-2-token"))))
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
                                  HasSubstr("pageToken=page-3-token"))))
      .WillRepeatedly(Invoke([]() {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-3", "courseState": "ACTIVE"}
              ]
            })");
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
    ASSERT_EQ(courses->item_count(), 3u);

    EXPECT_EQ(courses->GetItemAt(0)->id, "course-id-from-page-1");
    EXPECT_EQ(courses->GetItemAt(1)->id, "course-id-from-page-2");
    EXPECT_EQ(courses->GetItemAt(2)->id, "course-id-from-page-3");
  }
}

}  // namespace ash

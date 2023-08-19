// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
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

namespace ash {
namespace {

using ::base::test::TestFuture;
using ::google_apis::ApiErrorCode;
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

using AssignmentListFuture =
    TestFuture<bool,
               std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>>;

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

std::string CreateSubmissionsListResponse(const std::string& course_work_id,
                                          int total_submissions,
                                          int turned_in_submissions,
                                          int graded_submissions) {
  constexpr char kTemplate[] = R"({"id": "student-submissions-%d",)"
                               R"("courseWorkId": "%s", "state": "%s"%s})";
  std::vector<std::string> submissions;
  for (int i = 0; i < total_submissions; ++i) {
    std::string state = i < graded_submissions
                            ? "RETURNED"
                            : (i < turned_in_submissions ? "TURNED_IN" : "NEW");
    std::string grade = i < graded_submissions ? R"(,"assignedGrade": 20)" : "";
    submissions.push_back(base::StringPrintf(
        kTemplate, i, course_work_id.c_str(), state.c_str(), grade.c_str()));
  }
  std::string submissions_string = base::JoinString(submissions, ",");
  return base::StringPrintf(R"({"studentSubmissions": [%s]})",
                            submissions_string.c_str());
}

}  // namespace

class GlanceablesClassroomClientImplTest : public testing::Test {
 public:
  void SetUp() override {
    // This is the time most of the test expect.
    OverrideTime("10 Apr 2023 00:00 GMT");

    auto create_request_sender_callback = base::BindLambdaForTesting(
        [&](const std::vector<std::string>& scopes,
            const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
          return std::make_unique<google_apis::RequestSender>(
              std::make_unique<google_apis::DummyAuthService>(),
              url_loader_factory_, task_environment_.GetMainThreadTaskRunner(),
              "test-user-agent", TRAFFIC_ANNOTATION_FOR_TESTS);
        });
    client_ = std::make_unique<GlanceablesClassroomClientImpl>(
        /*profile=*/nullptr, &test_clock_, create_request_sender_callback,
        /*use_best_effort_prefetch_task_runner=*/false);

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

  void OverrideTime(const char* now_string) {
    base::Time new_now;
    ASSERT_TRUE(base::Time::FromString(now_string, &new_now));
    test_clock_.SetNow(new_now);
  }

  void ExpectActiveCourse(int call_count = 1) {
    EXPECT_CALL(request_handler(),
                HandleRequest(
                    Field(&HttpRequest::relative_url, HasSubstr("/courses?"))))
        .Times(call_count)
        .WillRepeatedly(Invoke([](const HttpRequest&) {
          return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })");
        }));
  }

  GlanceablesClassroomClientImpl* client() { return client_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  TestRequestHandler& request_handler() { return request_handler_; }

 private:
  base::SimpleTestClock test_clock_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedFeatureList feature_list_{
      features::kGlanceablesV2ClassroomTeacherView};
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
  std::unique_ptr<GlanceablesClassroomClientImpl> client_;
  base::HistogramTester histogram_tester_;
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

  struct {
    base::RepeatingCallback<void(
        GlanceablesClassroomClientImpl::FetchCoursesCallback)>
        fetch_method;
    std::string expected_courses_count_histogram_name;
  } test_cases[] = {
      {base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                           base::Unretained(client())),
       "Ash.Glanceables.Api.Classroom.StudentCoursesCount"},
      {base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                           base::Unretained(client())),
       "Ash.Glanceables.Api.Classroom.TeacherCoursesCount"},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    test_case.fetch_method.Run(base::BindLambdaForTesting(
        [&](bool success,
            const GlanceablesClassroomClientImpl::CourseList& courses) {
          run_loop.Quit();

          EXPECT_TRUE(success);
          ASSERT_EQ(courses.size(), 1u);

          EXPECT_EQ(courses.at(0)->id, "course-id-1");
          EXPECT_EQ(courses.at(0)->name, "Active Course 1");

          histogram_tester.ExpectTotalCount(
              "Ash.Glanceables.Api.Classroom.GetCourses.Latency",
              /*expected_count=*/1);
          histogram_tester.ExpectUniqueSample(
              "Ash.Glanceables.Api.Classroom.GetCourses.Status",
              ApiErrorCode::HTTP_SUCCESS,
              /*expected_bucket_count=*/1);
          histogram_tester.ExpectUniqueSample(
              test_case.expected_courses_count_histogram_name,
              /*sample=*/1,
              /*expected_bucket_count=*/1);
        }));
    run_loop.Run();
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
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    fetch_method.Run(base::BindLambdaForTesting(
        [&](bool success,
            const GlanceablesClassroomClientImpl::CourseList& courses) {
          run_loop.Quit();

          EXPECT_FALSE(success);
          EXPECT_EQ(0u, courses.size());

          histogram_tester.ExpectTotalCount(
              "Ash.Glanceables.Api.Classroom.GetCourses.Latency",
              /*expected_count=*/1);
          histogram_tester.ExpectUniqueSample(
              "Ash.Glanceables.Api.Classroom.GetCourses.Status",
              ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
              /*expected_bucket_count=*/1);
        }));
    run_loop.Run();
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

  struct {
    base::RepeatingCallback<void(
        GlanceablesClassroomClientImpl::FetchCoursesCallback)>
        fetch_method;
    std::string expected_courses_count_histogram_name;
  } test_cases[] = {
      {base::BindRepeating(&GlanceablesClassroomClientImpl::FetchStudentCourses,
                           base::Unretained(client())),
       "Ash.Glanceables.Api.Classroom.StudentCoursesCount"},
      {base::BindRepeating(&GlanceablesClassroomClientImpl::FetchTeacherCourses,
                           base::Unretained(client())),
       "Ash.Glanceables.Api.Classroom.TeacherCoursesCount"},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    test_case.fetch_method.Run(base::BindLambdaForTesting(
        [&](bool success,
            const GlanceablesClassroomClientImpl::CourseList& courses) {
          run_loop.Quit();
          EXPECT_TRUE(success);
          ASSERT_EQ(courses.size(), 3u);

          EXPECT_EQ(courses.at(0)->id, "course-id-from-page-1");
          EXPECT_EQ(courses.at(1)->id, "course-id-from-page-2");
          EXPECT_EQ(courses.at(2)->id, "course-id-from-page-3");

          histogram_tester.ExpectUniqueSample(
              test_case.expected_courses_count_histogram_name,
              /*sample=*/3,
              /*expected_bucket_count=*/1);
        }));
    run_loop.Run();
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
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
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
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];
            ASSERT_EQ(course_work_map.size(), 2u);

            ASSERT_TRUE(course_work_map.contains("course-work-item-1"));
            const GlanceablesClassroomCourseWorkItem& course_work_1 =
                course_work_map.at("course-work-item-1");
            EXPECT_EQ(course_work_1.title(), "Math assignment");
            EXPECT_EQ(course_work_1.link(),
                      "https://classroom.google.com/test-link-1");
            EXPECT_FALSE(course_work_1.due());

            ASSERT_TRUE(course_work_map.contains("course-work-item-3"));
            const GlanceablesClassroomCourseWorkItem& course_work_3 =
                course_work_map.at("course-work-item-3");
            EXPECT_EQ(course_work_3.title(), "Math assignment with due date");
            EXPECT_EQ(course_work_3.link(),
                      "https://classroom.google.com/test-link-3");
            ASSERT_TRUE(course_work_3.due());
            EXPECT_EQ(FormatTimeAsString(course_work_3.due().value()),
                      "2023-04-25T15:09:25.250Z");

            histogram_tester()->ExpectTotalCount(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.Latency",
                /*expected_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.Status",
                ApiErrorCode::HTTP_SUCCESS,
                /*expected_bucket_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.PagesCount",
                /*sample=*/1,
                /*expected_bucket_count=*/1);
          }));
  run_loop.Run();
}

// Fetches and makes sure only "PUBLISHED" course work items are converted to
// `GlanceablesClassroomCourseWorkItem`.
TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkAndSubmissions) {
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
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment with no submissions",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4"
                }
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .Times(0);

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-3",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 90
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": []
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kTeacher;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];
            ASSERT_EQ(course_work_map.size(), 3u);

            ASSERT_TRUE(course_work_map.contains("course-work-item-1"));
            const GlanceablesClassroomCourseWorkItem& course_work_1 =
                course_work_map.at("course-work-item-1");
            EXPECT_EQ(course_work_1.title(), "Math assignment");
            EXPECT_EQ(course_work_1.link(),
                      "https://classroom.google.com/test-link-1");
            EXPECT_FALSE(course_work_1.due());
            EXPECT_EQ(course_work_1.total_submissions(), 1);
            EXPECT_EQ(course_work_1.turned_in_submissions(), 0);
            EXPECT_EQ(course_work_1.graded_submissions(), 0);

            ASSERT_TRUE(course_work_map.contains("course-work-item-3"));
            const GlanceablesClassroomCourseWorkItem& course_work_3 =
                course_work_map.at("course-work-item-3");
            EXPECT_EQ(course_work_3.title(), "Math assignment with due date");
            EXPECT_EQ(course_work_3.link(),
                      "https://classroom.google.com/test-link-3");
            ASSERT_TRUE(course_work_3.due());
            EXPECT_EQ(FormatTimeAsString(course_work_3.due().value()),
                      "2023-04-25T15:09:25.250Z");
            EXPECT_EQ(course_work_3.total_submissions(), 3);
            EXPECT_EQ(course_work_3.turned_in_submissions(), 2);
            EXPECT_EQ(course_work_3.graded_submissions(), 1);

            ASSERT_TRUE(course_work_map.contains("course-work-item-4"));
            const GlanceablesClassroomCourseWorkItem& course_work_4 =
                course_work_map.at("course-work-item-4");
            EXPECT_EQ(course_work_4.title(),
                      "Math assignment with no submissions");
            EXPECT_EQ(course_work_4.link(),
                      "https://classroom.google.com/test-link-4");
            EXPECT_FALSE(course_work_4.due());
            EXPECT_EQ(course_work_4.total_submissions(), 0);
            EXPECT_EQ(course_work_4.turned_in_submissions(), 0);
            EXPECT_EQ(course_work_4.graded_submissions(), 0);
          }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkOnHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            auto& course_work_map = courses_map["course-123"];
            ASSERT_TRUE(course_work_map.empty());

            histogram_tester()->ExpectTotalCount(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.Latency",
                /*expected_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.Status",
                ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
                /*expected_bucket_count=*/1);
          }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchCourseWorkAndSubmissionsMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-1", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-2", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-3-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-3", "state": "PUBLISHED"}
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/course-work-item-from-page-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-from-page-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/course-work-item-from-page-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-from-page-2",
                  "state": "TURNED_IN"
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("courseWork/course-work-item-from-page-3/"
                                  "studentSubmissions?"),
                        Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-from-page-3",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-from-page-3",
                  "state": "TURNED_IN"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("courseWork/course-work-item-from-page-3/"
                                  "studentSubmissions?"),
                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-from-page-3",
                  "state": "RETURNED",
                  "assignedGrade": 10.0
                }
              ]
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kTeacher;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];
            ASSERT_EQ(course_work_map.size(), 3u);

            ASSERT_TRUE(
                course_work_map.contains("course-work-item-from-page-1"));
            const GlanceablesClassroomCourseWorkItem& course_work_1 =
                course_work_map.at("course-work-item-from-page-1");
            EXPECT_EQ(course_work_1.total_submissions(), 1);
            EXPECT_EQ(course_work_1.turned_in_submissions(), 0);
            EXPECT_EQ(course_work_1.graded_submissions(), 0);

            ASSERT_TRUE(
                course_work_map.contains("course-work-item-from-page-2"));
            const GlanceablesClassroomCourseWorkItem& course_work_2 =
                course_work_map.at("course-work-item-from-page-2");
            EXPECT_EQ(course_work_2.total_submissions(), 1);
            EXPECT_EQ(course_work_2.turned_in_submissions(), 1);
            EXPECT_EQ(course_work_2.graded_submissions(), 0);

            ASSERT_TRUE(
                course_work_map.contains("course-work-item-from-page-3"));
            const GlanceablesClassroomCourseWorkItem& course_work_3 =
                course_work_map.at("course-work-item-from-page-3");
            EXPECT_EQ(course_work_3.total_submissions(), 3);
            EXPECT_EQ(course_work_3.turned_in_submissions(), 2);
            EXPECT_EQ(course_work_3.graded_submissions(), 1);

            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.PagesCount",
                /*sample=*/3,
                /*expected_bucket_count=*/1);
          }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCourseWorkMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-1", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-2", "state": "PUBLISHED"}
              ],
              "nextPageToken": "page-3-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {"id": "course-work-item-from-page-3", "state": "PUBLISHED"}
              ]
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting([&]() {
        run_loop.Quit();

        GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
            client()->GetCourseWork(course_work_type);

        ASSERT_TRUE(courses_map.contains("course-123"));
        auto& course_work_map = courses_map["course-123"];

        ASSERT_EQ(course_work_map.size(), 3u);
        ASSERT_TRUE(course_work_map.contains("course-work-item-from-page-1"));
        ASSERT_TRUE(course_work_map.contains("course-work-item-from-page-2"));
        ASSERT_TRUE(course_work_map.contains("course-work-item-from-page-3"));
      }));
  run_loop.Run();
}

// ----------------------------------------------------------------------------
// Fetch all student submissions:

TEST_F(GlanceablesClassroomClientImplTest, FetchStudentSubmissions) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
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
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchStudentSubmissions(
      /*course_id=*/"course-123", /*course_work_id=*/"-", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];

            ASSERT_EQ(course_work_map.size(), 1u);
            ASSERT_TRUE(course_work_map.contains("course-work-1"));

            const auto& course_work = course_work_map.at("course-work-1");
            EXPECT_EQ(course_work.total_submissions(), 7);
            EXPECT_EQ(course_work.turned_in_submissions(), 2);
            EXPECT_EQ(course_work.graded_submissions(), 1);

            histogram_tester()->ExpectTotalCount(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Latency",
                /*expected_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Status",
                ApiErrorCode::HTTP_SUCCESS,
                /*expected_bucket_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions."
                "PagesCount",
                /*sample=*/1,
                /*expected_bucket_count=*/1);
          }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest, FetchStudentSubmissionsOnHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchStudentSubmissions(
      /*course_id=*/"course-123", /*course_work_id=*/"-", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            auto& course_work_map = courses_map["course-123"];
            ASSERT_TRUE(course_work_map.empty());

            histogram_tester()->ExpectTotalCount(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Latency",
                /*expected_count=*/1);
            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Status",
                ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
                /*expected_bucket_count=*/1);
          }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentSubmissionsMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-from-page-1",
                  "courseWorkId" : "courseWork1"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-from-page-2",
                  "courseWorkId": "courseWork1"
                }
              ],
              "nextPageToken": "page-3-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-from-page-3",
                  "courseWorkId": "courseWork2"
                }
              ]
            })"))));

  base::RunLoop run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchStudentSubmissions(
      /*course_id=*/"course-123", /*course_work_id=*/"-", course_work_type,
      base::BindLambdaForTesting([&]() {
        run_loop.Quit();

        GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
            client()->GetCourseWork(course_work_type);

        ASSERT_TRUE(courses_map.contains("course-123"));
        auto& course_work_map = courses_map["course-123"];

        ASSERT_EQ(course_work_map.size(), 2u);

        ASSERT_TRUE(course_work_map.contains("courseWork1"));
        EXPECT_EQ(course_work_map.at("courseWork1").total_submissions(), 2);
        EXPECT_EQ(course_work_map.at("courseWork1").turned_in_submissions(), 0);
        EXPECT_EQ(course_work_map.at("courseWork1").graded_submissions(), 0);

        ASSERT_TRUE(course_work_map.contains("courseWork2"));
        EXPECT_EQ(course_work_map.at("courseWork2").total_submissions(), 1);
        EXPECT_EQ(course_work_map.at("courseWork2").turned_in_submissions(), 0);
        EXPECT_EQ(course_work_map.at("courseWork2").graded_submissions(), 0);

        histogram_tester()->ExpectUniqueSample(
            "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.PagesCount",
            /*sample=*/3,
            /*expected_bucket_count=*/1);
      }));
  run_loop.Run();
}

// Verifies that student submissions can be fetched before course work items,
// and that they don't get overwritten after fetching course work items.
TEST_F(GlanceablesClassroomClientImplTest,
       FetchCourseWorkAfterStudentSubmissions) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-1",
                  "state": "CREATED"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-1",
                  "state": "RECLAIMED_BY_STUDENT"
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-1",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-1",
                  "state": "RETURNED"
                },
                {
                  "id": "student-submission-6",
                  "courseWorkId": "course-work-item-1",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-7",
                  "courseWorkId": "course-work-item-1",
                  "state": "???"
                }
              ]
            })"))));

  base::RunLoop student_submissions_run_loop;
  const auto course_work_type =
      GlanceablesClassroomClientImpl::CourseWorkType::kStudent;
  client()->FetchStudentSubmissions(
      /*course_id=*/"course-123", /*course_work_id=*/"-", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            student_submissions_run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];

            ASSERT_EQ(course_work_map.size(), 1u);
            ASSERT_TRUE(course_work_map.contains("course-work-item-1"));

            const auto& course_work = course_work_map.at("course-work-item-1");
            EXPECT_EQ(course_work.total_submissions(), 7);
            EXPECT_EQ(course_work.turned_in_submissions(), 2);
            EXPECT_EQ(course_work.graded_submissions(), 1);

            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetStudentSubmissions.Status",
                ApiErrorCode::HTTP_SUCCESS,
                /*expected_bucket_count=*/1);
          }));
  student_submissions_run_loop.Run();

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

  base::RunLoop course_work_run_loop;
  client()->FetchCourseWork(
      /*course_id=*/"course-123", course_work_type,
      base::BindLambdaForTesting(
          [&]() {
            course_work_run_loop.Quit();

            GlanceablesClassroomClientImpl::CourseWorkPerCourse& courses_map =
                client()->GetCourseWork(course_work_type);

            ASSERT_TRUE(courses_map.contains("course-123"));
            auto& course_work_map = courses_map["course-123"];

            ASSERT_EQ(course_work_map.size(), 1u);

            ASSERT_TRUE(course_work_map.contains("course-work-item-1"));
            const GlanceablesClassroomCourseWorkItem& course_work =
                course_work_map.at("course-work-item-1");
            EXPECT_EQ(course_work.title(), "Math assignment");
            EXPECT_EQ(course_work.link(),
                      "https://classroom.google.com/test-link-1");
            EXPECT_FALSE(course_work.due());
            EXPECT_EQ(course_work.total_submissions(), 7);
            EXPECT_EQ(course_work.turned_in_submissions(), 2);
            EXPECT_EQ(course_work.graded_submissions(), 1);

            histogram_tester()->ExpectUniqueSample(
                "Ash.Glanceables.Api.Classroom.GetCourseWork.Status",
                ApiErrorCode::HTTP_SUCCESS,
                /*expected_bucket_count=*/1);
          }));
  course_work_run_loop.Run();
}

// ----------------------------------------------------------------------------
// Public interface, student assignments:

TEST_F(GlanceablesClassroomClientImplTest,
       StudentRoleIsActiveWithEnrolledCourses) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url,
                        AllOf(HasSubstr("/courses?"), HasSubstr("studentId=me"),
                              Not(HasSubstr("teacherId=me"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .Times(0);
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .Times(0);

  TestFuture<bool> future;
  client()->IsStudentRoleActive(future.GetCallback());

  const bool active = future.Get();
  ASSERT_TRUE(active);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.GetCourses.Status",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       StudentRoleIsInactiveWithoutEnrolledCourses) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"courses": []})"))));

  TestFuture<bool> future;
  client()->IsStudentRoleActive(future.GetCallback());

  const bool active = future.Get();
  ASSERT_FALSE(active);
}

TEST_F(GlanceablesClassroomClientImplTest, ReturnsCompletedStudentAssignments) {
  ExpectActiveCourse();
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
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - submission graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2"
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - submission turned in",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3"
                },
                {
                  "id": "deleted-course-work-item",
                  "title": "Math assignment - draft",
                  "state": "DELETED"
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - submission graded two",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4"
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "updateTime": "2023-03-10T15:09:25.250Z",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "updateTime": "2023-03-10T15:09:25.250Z",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "updateTime": "2023-04-05T15:09:25.250Z",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "deleted-course-work-item",
                  "updateTime": "2023-03-25T15:09:25.250Z",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-4",
                  "updateTime": "2023-03-25T15:09:25.250Z",
                  "state": "TURNED_IN"
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetCompletedStudentAssignments(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - submission turned in");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-3");
  EXPECT_FALSE(assignments.at(0)->due);
  EXPECT_FALSE(assignments.at(0)->submissions_state);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - submission graded two");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_FALSE(assignments.at(1)->due);
  EXPECT_FALSE(assignments.at(1)->submissions_state);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title,
            "Math assignment - submission graded");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-2");
  EXPECT_FALSE(assignments.at(2)->due);
  EXPECT_FALSE(assignments.at(2)->submissions_state);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.StudentDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnsStudentAssignmentsWithApproachingDueDate) {
  ExpectActiveCourse();
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - approaching due date, completed",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - approaching due date two",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "dueDate": {"year": 2023, "month": 6, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-5",
                  "title": "Math assignment - approaching due date three",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-5",
                  "dueDate": {"year": 2023, "month": 5, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-5",
                  "state": "NEW"
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - approaching due date");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-2");
  EXPECT_EQ(FormatTimeAsString(assignments.at(0)->due.value()),
            "2023-04-25T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(0)->submissions_state);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - approaching due date three");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-5");
  EXPECT_EQ(FormatTimeAsString(assignments.at(1)->due.value()),
            "2023-05-25T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(1)->submissions_state);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title,
            "Math assignment - approaching due date two");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_EQ(FormatTimeAsString(assignments.at(2)->due.value()),
            "2023-06-25T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(2)->submissions_state);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.StudentDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnsStudentAssignmentsWithMissedDueDate) {
  ExpectActiveCourse();
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 3, "day": 20},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - missed due date, completed",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - missed due date, turned in",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-5",
                  "title": "Math assignment - missed due date two",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-5",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-6",
                  "title": "Math assignment - missed due date three",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-6",
                  "dueDate": {"year": 2023, "month": 3, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-5",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-6",
                  "courseWorkId": "course-work-item-6",
                  "state": "NEW"
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetStudentAssignmentsWithMissedDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - missed due date two");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-5");
  EXPECT_EQ(FormatTimeAsString(assignments.at(0)->due.value()),
            "2023-04-05T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(0)->submissions_state);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - missed due date three");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-6");
  EXPECT_EQ(FormatTimeAsString(assignments.at(1)->due.value()),
            "2023-03-25T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(1)->submissions_state);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title,
            "Math assignment - missed due date");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-1");
  EXPECT_EQ(FormatTimeAsString(assignments.at(2)->due.value()),
            "2023-03-20T15:09:25.250Z");
  EXPECT_FALSE(assignments.at(2)->submissions_state);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.StudentDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnsStudentAssignmentsWithoutDueDate) {
  ExpectActiveCourse();
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
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "creationTime": "2023-03-10T15:09:25.250Z"
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - with due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - submission graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3"
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment one",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "creationTime": "2023-03-20T15:09:25.250Z"
                },
                {
                  "id": "course-work-item-5",
                  "title": "Math assignment two",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-5",
                  "creationTime": "2023-03-15T15:09:25.250Z"
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-5",
                  "state": "NEW"
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetStudentAssignmentsWithoutDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title, "Math assignment one");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_FALSE(assignments.at(0)->due);
  EXPECT_FALSE(assignments.at(0)->submissions_state);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title, "Math assignment two");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-5");
  EXPECT_FALSE(assignments.at(1)->due);
  EXPECT_FALSE(assignments.at(1)->submissions_state);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title, "Math assignment");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-1");
  EXPECT_FALSE(assignments.at(2)->due);
  EXPECT_FALSE(assignments.at(2)->submissions_state);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.StudentDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       CourseWorkFailureWhenFetchingStudentAssignments) {
  ExpectActiveCourse();

  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_TRUE(assignments.empty());
}

TEST_F(GlanceablesClassroomClientImplTest,
       SubmissionsFailureWhenFetchingStudentAssignments) {
  ExpectActiveCourse();
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - approaching due date, completed",
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
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  AssignmentListFuture future;
  client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_TRUE(assignments.empty());
}

// ----------------------------------------------------------------------------
// Public interface, teacher assignments:

TEST_F(GlanceablesClassroomClientImplTest, TeacherRoleIsActiveWithCourses) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), Not(HasSubstr("studentId=me")),
                        HasSubstr("teacherId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .Times(0);
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .Times(0);

  TestFuture<bool> future;
  client()->IsTeacherRoleActive(future.GetCallback());

  const bool active = future.Get();
  ASSERT_TRUE(active);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.GetCourses.Status",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       TeacherRoleIsInactiveWithoutCourses) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(
          TestRequestHandler::CreateSuccessfulResponse(R"({"courses": []})"))));

  TestFuture<bool> future;
  client()->IsTeacherRoleActive(future.GetCallback());

  const bool active = future.Get();
  ASSERT_FALSE(active);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnTeacherAssignmentsWithApproachingDueDate) {
  ExpectActiveCourse();
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - approaching due date 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - approaching due date, completed",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - approaching due date 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "dueDate": {"year": 2023, "month": 4, "day": 12},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-5",
                  "title": "Math assignment - approaching due date 3",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-5",
                  "dueDate": {"year": 2023, "month": 7, "day": 15},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
              })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                }
              ]
              })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
              })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-5/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-5",
                  "state": "NEW"
                }
              ]
              })"))));
  AssignmentListFuture future;
  client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - approaching due date 2");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_EQ(FormatTimeAsString(assignments.at(0)->due.value()),
            "2023-04-12T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(0)->submissions_state);
  EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - approaching due date 1");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-2");
  EXPECT_EQ(FormatTimeAsString(assignments.at(1)->due.value()),
            "2023-04-25T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(1)->submissions_state);
  EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_graded, 0);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title,
            "Math assignment - approaching due date 3");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-5");
  EXPECT_EQ(FormatTimeAsString(assignments.at(2)->due.value()),
            "2023-07-15T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(2)->submissions_state);
  EXPECT_EQ(assignments.at(2)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_graded, 0);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.TeacherDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnsTeacherAssignmentsRecentlyDue) {
  ExpectActiveCourse();
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 3, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - missed due date, some completed",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - missed due date, turned in",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "dueDate": {"year": 2023, "month": 4, "day": 1},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-3-2",
                  "courseWorkId": "course-work-item-3",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-3-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "TURNED_IN"
                }
              ]
            })"))));
  AssignmentListFuture future;
  client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - missed due date, some completed");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-3");
  EXPECT_EQ(FormatTimeAsString(assignments.at(0)->due.value()),
            "2023-04-05T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(0)->submissions_state);
  EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 4);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 1);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - missed due date, turned in");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_EQ(FormatTimeAsString(assignments.at(1)->due.value()),
            "2023-04-01T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(1)->submissions_state);
  EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_graded, 0);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title,
            "Math assignment - missed due date");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-1");
  EXPECT_EQ(FormatTimeAsString(assignments.at(2)->due.value()),
            "2023-03-05T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(2)->submissions_state);
  EXPECT_EQ(assignments.at(2)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_graded, 0);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.TeacherDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnsTeacherAssignmentsWithoutDueDate) {
  ExpectActiveCourse();
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
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "updateTime": "2023-03-15T15:09:25.250Z"
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - with due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - submission graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3"
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - for sorting",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4",
                  "updateTime": "2023-04-05T15:09:25.250Z"
                },{
                  "id": "course-work-item-5",
                  "title": "Math assignment - for sorting",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-5",
                  "updateTime": "2023-03-25T15:09:25.250Z"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-5/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-5",
                  "courseWorkId": "course-work-item-5",
                  "state": "NEW"
                }
              ]
            })"))));
  AssignmentListFuture future;
  client()->GetTeacherAssignmentsWithoutDueDate(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 3u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - for sorting");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-4");
  EXPECT_FALSE(assignments.at(0)->due);
  EXPECT_EQ(FormatTimeAsString(assignments.at(0)->last_update),
            "2023-04-05T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(0)->submissions_state);
  EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);

  EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(1)->course_work_title,
            "Math assignment - for sorting");
  EXPECT_EQ(assignments.at(1)->link,
            "https://classroom.google.com/test-link-5");
  EXPECT_FALSE(assignments.at(1)->due);
  EXPECT_EQ(FormatTimeAsString(assignments.at(1)->last_update),
            "2023-03-25T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(1)->submissions_state);
  EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(1)->submissions_state->number_graded, 0);

  EXPECT_EQ(assignments.at(2)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(2)->course_work_title, "Math assignment");
  EXPECT_EQ(assignments.at(2)->link,
            "https://classroom.google.com/test-link-1");
  EXPECT_FALSE(assignments.at(2)->due);
  EXPECT_EQ(FormatTimeAsString(assignments.at(2)->last_update),
            "2023-03-15T15:09:25.250Z");
  ASSERT_TRUE(assignments.at(2)->submissions_state);
  EXPECT_EQ(assignments.at(2)->submissions_state->total_count, 1);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  EXPECT_EQ(assignments.at(2)->submissions_state->number_graded, 0);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.TeacherDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest, ReturnsGradedTeacherAssignments) {
  ExpectActiveCourse();
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
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment - submissions all graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2"
                },
                {
                  "id": "course-work-item-3",
                  "title": "Math assignment - turned in but not all graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3"
                },
                {
                  "id": "course-work-item-4",
                  "title": "Math assignment - turned in none graded",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-4"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "RETURNED",
                  "assignedGrade": 50.0
                },
                {
                  "id": "student-submission-2-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "RETURNED",
                  "assignedGrade": 90.0
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-3-2",
                  "courseWorkId": "course-work-item-3",
                  "state": "RETURNED",
                  "assignedGrade": 74.0
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-4-2",
                  "courseWorkId": "course-work-item-4",
                  "state": "TURNED_IN"
                }
              ]
            })"))));

  AssignmentListFuture future;
  client()->GetGradedTeacherAssignments(future.GetCallback());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 1u);

  EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(assignments.at(0)->course_work_title,
            "Math assignment - submissions all graded");
  EXPECT_EQ(assignments.at(0)->link,
            "https://classroom.google.com/test-link-2");
  EXPECT_FALSE(assignments.at(0)->due);
  ASSERT_TRUE(assignments.at(0)->submissions_state);
  EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 2);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
  EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 2);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Classroom.TeacherDataFetchTime",
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchingTeacherAssignmentsDoesNotClearStudentAssignments) {
  // Mock requests for a course where the user is student.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("studentId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "student-course",
                  "name": "Active Student Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/student-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "student-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "student-course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "student-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // Mock requests where the user is a teacher.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("teacherId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "teacher-course",
                  "name": "Active Teacher Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/teacher-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "teacher-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "teacher-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "teacher-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "teacher-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // Fetch student courses with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  // Fetch recently due teacher courses.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }

  // Fetch student courses with missed due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithMissedDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchingStudentAssignmentsDoesNotClearTeacherAssignments) {
  // Mock requests for a course where the user is student.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("studentId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "student-course",
                  "name": "Active Student Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/student-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "student-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "student-course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "student-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // Mock requests where the user is a teacher.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("teacherId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "teacher-course",
                  "name": "Active Teacher Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/teacher-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "teacher-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "teacher-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "teacher-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "teacher-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // Fetch recently due teacher courses.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }

  // Fetch student courses with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  // Fetch approaching due teacher courses.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       RefetchStudentAssignmentsAfterReshowingBubble) {
  // Mock requests for a course where the user is student - expect two requests,
  // second of which adds an assignment with an approaching due date.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("studentId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "student-course",
                  "name": "Active Student Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "student-course",
                  "name": "Active Student Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/student-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "student-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "student-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "student-course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "student-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // The student has one assignment with missed due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithMissedDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
  }

  // Initially, there are no assignments with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 0u);
  }

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();

  // The response from requests sent after the bubble was closed contains an
  // assignment with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  // No change in assignments with missed due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithMissedDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
  }

  // Simulate another request, to verify that coursework is not refetched if the
  // bubble does not close.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_title, "Active Student Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentCoursesAfterIsActiveCheck) {
  ExpectActiveCourse(/*times=*/2);

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
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
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("student-course-work-item-1", 1, 0,
                                        0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("student-course-work-item-1", 1, 1,
                                        0)))));

  TestFuture<bool> is_active_future;
  client()->IsStudentRoleActive(is_active_future.GetCallback());

  const bool active = is_active_future.Get();
  EXPECT_TRUE(active);

  // The student has one assignment with missed due date.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithMissedDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
  }

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetCompletedStudentAssignments(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentCoursesConcurrentlyWithIsActiveCheck) {
  ExpectActiveCourse(/*times=*/2);

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
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
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("student-course-work-item-1", 1, 0,
                                        0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("student-course-work-item-1", 1, 1,
                                        0)))));

  TestFuture<bool> is_active_future;
  client()->IsStudentRoleActive(is_active_future.GetCallback());

  AssignmentListFuture assignments_future;
  client()->GetStudentAssignmentsWithMissedDueDate(
      assignments_future.GetCallback());

  const bool active = is_active_future.Get();
  EXPECT_TRUE(active);

  const auto [success, assignments] = assignments_future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 1u);
  EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();

  client()->GetCompletedStudentAssignments(assignments_future.GetCallback());

  const auto [refetch_success, refetched_assignments] =
      assignments_future.Take();
  ASSERT_EQ(refetched_assignments.size(), 1u);
  EXPECT_EQ(refetched_assignments.at(0)->course_work_title, "Assignment 1");
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchTeacherCoursesAfterIsActiveCheck) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  ExpectActiveCourse(/*times=*/2);

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
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
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 2, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 2, 2, 0)))));

  TestFuture<bool> is_active_future;
  client()->IsTeacherRoleActive(is_active_future.GetCallback());

  const bool active = is_active_future.Get();
  EXPECT_TRUE(active);

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
  }

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       RefetchTeacherAssignmentsAfterReshowingBubble) {
  // Mock requests for a course where the user is a teacher - expect two
  // requests, second of which adds an assignment with an approaching due date.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("teacherId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "teacher-course",
                  "name": "Active Teacher Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "teacher-course",
                  "name": "Active Teacher Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/teacher-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "teacher-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "teacher-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "teacher-course-work-item-2",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "teacher-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "teacher-course-work-item-1",
                  "state": "TURNED_IN"
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "teacher-course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))));

  // There should be single assignment with missed due date - and the assignment
  // is initially not turned in.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }

  // Initially, there are no assignments with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 0u);
  }

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();

  // The response from requests sent after the bubble was closed contains an
  // assignment with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }

  // The assignment with passed due date is now turned in.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - missed due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }

  // Repeat the request for approaching due date assignments, to verify the data
  // is not refetched.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Teacher Course");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 0);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       RefetchRemovesOldTeacherAssignments) {
  // Mock requests for a course where the user is a teacher - expect two
  // requests, second of which removes an assignment with an approaching due
  // date.
  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Math assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 1, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 1, 0, 0)))));

  // Initially, there are 2 assignments with approaching due date.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Math assignment 1");

    EXPECT_EQ(assignments.at(1)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Math assignment 2");
  }

  // Simulate glanceables bubble closure.
  client()->OnGlanceablesBubbleClosed();

  // The response from requests sent after the bubble was closed removes an
  // assignment.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Math assignment 1");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       DontRefetchStudentAssignmentsIfBubbleReshownWhileStillFetching) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("studentId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "student-course",
                  "name": "Active Student Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {"courses": []}
      )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/student-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "student-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("courseWork/-/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "student-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));

  AssignmentListFuture initial_future;
  client()->GetStudentAssignmentsWithMissedDueDate(
      initial_future.GetCallback());

  // Simulate glanceables bubble closure, and then another assignments request
  // before the first request completes.
  client()->OnGlanceablesBubbleClosed();

  AssignmentListFuture second_future;
  client()->GetStudentAssignmentsWithMissedDueDate(second_future.GetCallback());

  // Verify that both requests return the same result.
  const auto [initial_success, initial_assignments] = initial_future.Take();
  EXPECT_TRUE(initial_success);
  ASSERT_EQ(initial_assignments.size(), 1u);
  EXPECT_EQ(initial_assignments.at(0)->course_title, "Active Student Course");
  EXPECT_EQ(initial_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  const auto [second_success, second_assignments] = second_future.Take();
  EXPECT_TRUE(second_success);
  ASSERT_EQ(second_assignments.size(), 1u);
  EXPECT_EQ(second_assignments.at(0)->course_title, "Active Student Course");
  EXPECT_EQ(second_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  // Getting assignments after initial results have been received does not
  // repeat course work data fetch.
  AssignmentListFuture third_future;
  client()->GetStudentAssignmentsWithMissedDueDate(third_future.GetCallback());

  const auto [third_success, third_assignments] = third_future.Take();
  EXPECT_TRUE(third_success);
  ASSERT_EQ(third_assignments.size(), 1u);
  EXPECT_EQ(third_assignments.at(0)->course_title, "Active Student Course");
  EXPECT_EQ(third_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  // Request after closing the bubble should refetch data, which is in this case
  // empty.
  client()->OnGlanceablesBubbleClosed();

  AssignmentListFuture refetch_future;
  client()->GetStudentAssignmentsWithMissedDueDate(
      refetch_future.GetCallback());

  const auto [refetch_success, refetch_assignments] = refetch_future.Take();
  EXPECT_TRUE(refetch_success);
  EXPECT_EQ(refetch_assignments.size(), 0u);
}

TEST_F(GlanceablesClassroomClientImplTest,
       DontRefetchTeacherAssignmentsIfBubbleReshownWhileStillFetching) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), HasSubstr("teacherId=me")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "teacher-course",
                  "name": "Active Teacher Course",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {"courses": []})"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/teacher-course/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "teacher-course-work-item-1",
                  "title": "Math assignment - missed due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 5},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr(
              "courseWork/teacher-course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "teacher-course-work-item-1",
                  "state": "NEW"
                }
              ]
            })"))));

  AssignmentListFuture initial_future;
  client()->GetTeacherAssignmentsRecentlyDue(initial_future.GetCallback());

  // Simulate glanceables bubble closure, and then another assignments request
  // before the first request completes.
  client()->OnGlanceablesBubbleClosed();

  AssignmentListFuture second_future;
  client()->GetTeacherAssignmentsRecentlyDue(second_future.GetCallback());

  // Verify that both requests return the same result.
  const auto [initial_success, initial_assignments] = initial_future.Take();
  EXPECT_TRUE(initial_success);
  ASSERT_EQ(initial_assignments.size(), 1u);
  EXPECT_EQ(initial_assignments.at(0)->course_title, "Active Teacher Course");
  EXPECT_EQ(initial_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  const auto [second_success, second_assignments] = second_future.Take();
  EXPECT_TRUE(second_success);
  ASSERT_EQ(second_assignments.size(), 1u);
  EXPECT_EQ(second_assignments.at(0)->course_title, "Active Teacher Course");
  EXPECT_EQ(second_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  // Getting assignments after initial results have been received does not
  // repeat course work data fetch.
  AssignmentListFuture third_future;
  client()->GetTeacherAssignmentsRecentlyDue(third_future.GetCallback());

  const auto [third_success, third_assignments] = third_future.Take();
  EXPECT_TRUE(third_success);
  ASSERT_EQ(third_assignments.size(), 1u);
  EXPECT_EQ(third_assignments.at(0)->course_title, "Active Teacher Course");
  EXPECT_EQ(third_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  // Request after closing the bubble should refetch data, which is in this case
  // empty.
  client()->OnGlanceablesBubbleClosed();

  AssignmentListFuture refetch_future;
  client()->GetTeacherAssignmentsRecentlyDue(refetch_future.GetCallback());

  const auto [refetch_success, refetch_assignments] = refetch_future.Take();
  EXPECT_TRUE(refetch_success);
  EXPECT_EQ(refetch_assignments.size(), 0u);
}

TEST_F(GlanceablesClassroomClientImplTest,
       SubmissionsRefetchFrequencyForTeachers_RecentDueDate) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/5);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(5)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Recently past due",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 3,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Past due within a week",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 8},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Long past due",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 3, "day": 20},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 3, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 4, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 3, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 5, 5)))));
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();
  // Advance time 5 minutes, and verify only assignments with a recent due date
  // gets refetched.
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by a day - this removes second assignment due date from "very
  // recent" interval, but the assignment should still be fetched, as enough
  // time has passed from the last fetch.
  OverrideTime("11 Apr 2023 09:00 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by 5 minutes - the second assignment should now not be
  // fetched.
  OverrideTime("11 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 3);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by 7 days, which is enough time to refresh all items, even the
  // old, ungraded one. With this request, the last assignment state changed to
  // graded, so it should be removed from the returned assignments.
  OverrideTime("18 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 4);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 3);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       SubmissionsRefetchFrequencyForTeachers_UpcomingDueDate) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/4);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(4)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due within a couple of days",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Due within a week",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Course Work 4",
                  "description": "Due in future",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 5, "day": 29},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 3, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 3, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 1, 0)))));
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 4u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(3)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(3)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time 5 minutes, and verify only assignments with due date within a
  // couple of days are re-fetched.
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 4u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(3)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(3)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by a day - assignments with a due date within a week should be
  // refetched, given it's been more than a week from the last fetch.
  OverrideTime("11 Apr 2023 09:00 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by 7 days, which is enough time to refresh all items, even the
  // ones with due date farther along. With this request, the last assignment
  // state changed to graded, so it should be removed from the returned
  // assignments.
  OverrideTime("18 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
  }

  client()->OnGlanceablesBubbleClosed();
}

TEST_F(GlanceablesClassroomClientImplTest,
       SubmissionsRefetchFrequencyForTeachers_RecentlyUpdated) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/4);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(4)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Recently updated",
                  "state": "PUBLISHED",
                  "updateTime": "2023-04-10T08:09:25.250Z"
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Updated within a couple of days",
                  "state": "PUBLISHED",
                  "updateTime": "2023-04-09T15:09:25.250Z"
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Updated long ago",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-15T15:09:25.250Z"
                },
                {
                  "id": "course-work-item-4",
                  "title": "Course Work 4",
                  "description": "Due long ago, recently updated",
                  "state": "PUBLISHED",
                  "updateTime": "2023-04-10T08:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 3, "day": 1},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-5",
                  "title": "Course Work 5",
                  "description": "Due long ago, no recent updates",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-15T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 3, "day": 1},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 3, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 2, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 3, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 5, 5)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 5, 5)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 5, 4)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 5, 5)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-5/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-5", 5, 5, 4)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-5", 5, 5, 5)))));

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithoutDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 5);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time 5 minutes, and verify only recently updated assignments get
  // refetched.
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithoutDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 0u);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by a day - recently updated assignments should be refetched
  OverrideTime("11 Apr 2023 09:00 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithoutDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
  }

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 5);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time by 7 days, which is enough time to refresh all items, even the
  // ones with update time further away.
  OverrideTime("18 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithoutDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 3);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 3);
  }

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 3u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_graded, 5);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_graded, 5);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 5");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_graded, 5);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       EnsureFreshnessOfTeacherItemsExpectedToBeDisplayed) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  ExpectActiveCourse(/*call_count=*/3);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(3)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due soon 1",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 11},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Course Work 4",
                  "description": "Due even longer into future",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-5",
                  "title": "Course Work 5",
                  "description": "Past due recently",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-6",
                  "title": "Course Work 6",
                  "description": "Past due long ago",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 2, "day": 20},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 1, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-4/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-4", 5, 0, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-5/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-5", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-5", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-5", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-6/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-6", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-6", 5, 1, 0)))));

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 4u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 0);
    EXPECT_EQ(assignments.at(3)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(3)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time 5 minutes, and verify 3 assignments with closes approaching
  // due date are refetched, even if one of them is further away in future .
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 4u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(2)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(2)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(3)->course_work_title, "Course Work 4");
    EXPECT_EQ(assignments.at(3)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  // Fetch items with passed due date - there are only 2, so verify each one's
  // information is refreshed.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 5");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 6");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
  }
}

TEST_F(GlanceablesClassroomClientImplTest, DontRefetchTopItemMoreThanOnce) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(2u);

  ExpectActiveCourse(/*call_count=*/3);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(3)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Due longer into future",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 5, 5)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 1)))));

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    EXPECT_EQ(assignments.size(), 0u);
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time 5 minutes, then fetch assignments with approaching due date.
  // Second and third assignments are far away in the future, and would not be
  // refetched by default. The second item gets refetched because it ends up
  // being an item for display, but refetch reclassifies it as graded. The third
  // item will not be refetched, even though it ends up getting displayed, to
  // prevent further delay in when the assignment list becomes available to the
  // user.
  OverrideTime("10 Apr 2023 09:05 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  OverrideTime("10 Apr 2023 09:10 GMT");

  // Course Work 3 should be refetched this time.
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 1);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       RefetchingtopItemsWithOtherCallbacksPending) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(2u);

  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-08T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Graded",
                  "state": "PUBLISHED",
                  "updateTime": "2023-04-09T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 5, 5)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 5, 5)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 5, 5)))));

  {
    AssignmentListFuture future;
    client()->GetGradedTeacherAssignments(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 3");
  }

  client()->OnGlanceablesBubbleClosed();

  // Advance time 5 minutes, then synchronously trigger 3 requests, second of
  // which will require extra refresh for a top item (the second closes
  // assignment with approaching due day is farther away than a few days).
  OverrideTime("10 Apr 2023 09:05 GMT");

  AssignmentListFuture initial_graded_future;
  client()->GetGradedTeacherAssignments(initial_graded_future.GetCallback());

  AssignmentListFuture approaching_due_date_future;
  client()->GetTeacherAssignmentsWithApproachingDueDate(
      approaching_due_date_future.GetCallback());

  AssignmentListFuture final_graded_future;
  client()->GetGradedTeacherAssignments(final_graded_future.GetCallback());

  // All requests should receive a response.
  ASSERT_TRUE(initial_graded_future.Wait());
  ASSERT_TRUE(approaching_due_date_future.Wait());
  ASSERT_TRUE(final_graded_future.Wait());

  const auto [initial_graded_success, initial_graded_assignments] =
      initial_graded_future.Take();
  EXPECT_TRUE(initial_graded_success);
  ASSERT_EQ(initial_graded_assignments.size(), 1u);
  EXPECT_EQ(initial_graded_assignments.at(0)->course_work_title,
            "Course Work 3");

  // Note: Refreshed state for Course Work 2 has it classified as graded, so it
  // should end up in "graded assignments" list.
  const auto [approaching_due_date_success, approaching_due_date_assignments] =
      approaching_due_date_future.Take();
  EXPECT_TRUE(approaching_due_date_success);
  ASSERT_EQ(approaching_due_date_assignments.size(), 1u);
  EXPECT_EQ(approaching_due_date_assignments.at(0)->course_work_title,
            "Course Work 1");

  const auto [final_graded_success, final_graded_assignments] =
      final_graded_future.Take();
  EXPECT_TRUE(final_graded_success);
  ASSERT_EQ(final_graded_assignments.size(), 2u);
  EXPECT_EQ(final_graded_assignments.at(0)->course_work_title, "Course Work 3");
  EXPECT_EQ(final_graded_assignments.at(1)->course_work_title, "Course Work 2");
}

TEST_F(GlanceablesClassroomClientImplTest, PrefetchTeacherAssignments) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/3);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .Times(3)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-08T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Course Work 3",
                  "description": "Past due long ago",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 2, "day": 20},
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

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 2, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-3/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-3", 5, 0, 0)))));

  base::RunLoop prefetch_waiter;
  ASSERT_TRUE(client()->FireTeacherDataPrefetchTimerIfRunningForTesting(
      prefetch_waiter.QuitClosure()));
  prefetch_waiter.Run();

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
  }

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 3");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 0);
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

    ASSERT_TRUE(future.Wait());
    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
    EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 2);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
    EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       DontPrefetchTeacherAssignmentsIfAlreadyRequested) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/1);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-08T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))));

  AssignmentListFuture future;
  client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

  EXPECT_FALSE(client()->FireTeacherDataPrefetchTimerIfRunningForTesting(
      base::OnceClosure()));

  ASSERT_TRUE(future.Wait());
  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 2u);
  EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
  EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
  EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);

  EXPECT_FALSE(client()->FireTeacherDataPrefetchTimerIfRunningForTesting(
      base::OnceClosure()));
}

TEST_F(GlanceablesClassroomClientImplTest,
       TeacherAssignmentsRequestedDuringPrefetch) {
  OverrideTime("10 Apr 2023 09:00 GMT");

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  ExpectActiveCourse(/*call_count=*/1);
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Course Work 1",
                  "description": "Due soon",
                  "state": "PUBLISHED",
                  "dueDate": {"year": 2023, "month": 4, "day": 10},
                  "dueTime": {
                    "hours": 13,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Course Work 2",
                  "description": "Due long into future",
                  "state": "PUBLISHED",
                  "updateTime": "2023-03-08T15:09:25.250Z",
                  "dueDate": {"year": 2023, "month": 5, "day": 13},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 5, 1, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 5, 0, 0)))));

  base::RunLoop prefetch_waiter;
  ASSERT_TRUE(client()->FireTeacherDataPrefetchTimerIfRunningForTesting(
      prefetch_waiter.QuitClosure()));

  AssignmentListFuture future;
  client()->GetTeacherAssignmentsWithApproachingDueDate(future.GetCallback());

  prefetch_waiter.Run();
  ASSERT_TRUE(future.Wait());

  const auto [success, assignments] = future.Take();
  EXPECT_TRUE(success);
  ASSERT_EQ(assignments.size(), 2u);
  EXPECT_EQ(assignments.at(0)->course_work_title, "Course Work 1");
  EXPECT_EQ(assignments.at(0)->submissions_state->number_turned_in, 1);
  EXPECT_EQ(assignments.at(1)->course_work_title, "Course Work 2");
  EXPECT_EQ(assignments.at(1)->submissions_state->number_turned_in, 0);
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReusePreviousStudentDataOnCourseFetchError) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-2",
                  "name": "Active Course 2",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("course-id-1/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
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
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("course-id-2/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Final assignment",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .Times(3)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })");
      }));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 2");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Final assignment");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReusePreviousStudentDataOnCourseSecondPageFetchError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-2",
                  "name": "Active Course 2",
                  "courseState": "ACTIVE"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-2",
                  "name": "Active Course 2",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("course-id-1/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Math assignment - approaching due date",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
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
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("course-id-2/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Final assignment",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("studentSubmissions?"))))
      .Times(3)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ]
            })");
      }));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 2");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Final assignment");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedDataIfCourseWorkFetchFailsForStudents) {
  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 16,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-4",
                  "title": "Assignment 4",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 16,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "TURNED_IN"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
            })"))));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 4");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedDataIfCourseWorkSecondPageFetchFailsForStudents) {
  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 16,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-3",
                  "title": "Assignment 3",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 17,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ],
              "nextPageToken": "page-2-token"
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-4",
                  "title": "Assignment 4",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
            })"))));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 2");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 3");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 4");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedDataIfSubmissionsFetchFailsForStudents) {
  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 16,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Assignment 3",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 17,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-4",
                  "title": "Assignment 4",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
            })"))));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 4");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedDataIfSubmissionsSecondPageFetchFailsForStudents) {
  ExpectActiveCourse(/*call_count=*/2);
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 16,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-3",
                  "title": "Assignment 3",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 17,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-3",
                  "title": "Assignment 3",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-4",
                  "title": "Assignment 4",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-3",
                  "dueDate": {"year": 2023, "month": 4, "day": 25},
                  "dueTime": {
                    "hours": 17,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-2",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "studentSubmissions": [
                {
                  "id": "student-submission-3",
                  "courseWorkId": "course-work-item-3",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-4",
                  "courseWorkId": "course-work-item-4",
                  "state": "NEW"
                }
              ]
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/studentSubmissions?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
  }

  client()->OnGlanceablesBubbleClosed();

  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 3");
  }

  // Make sure assignments can be refetched after a failure.
  {
    AssignmentListFuture future;
    client()->GetStudentAssignmentsWithApproachingDueDate(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 3");
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 4");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedTeacherAssignmentsOnCoursesFetchFailure) {
  OverrideTime("10 Apr 2023 09:05 GMT");

  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Active Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
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
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-1/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 2, 0, 0)))));
  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
  }

  client()->OnGlanceablesBubbleClosed();
  OverrideTime("10 Apr 2023 09:15 GMT");

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 1u);

    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 2);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedTeacherAssignmentsOnCourseWorkFetchFailure) {
  OverrideTime("10 Apr 2023 09:05 GMT");

  ExpectActiveCourse(/*call_count=*/2);

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 14,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          AllOf(HasSubstr("courseWork/course-work-item-1/studentSubmissions?"),
                Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          R"({
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          AllOf(HasSubstr("courseWork/course-work-item-1/studentSubmissions?"),
                HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 0, 0)))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 1, 0, 0)))));

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
    ASSERT_TRUE(assignments.at(1)->submissions_state);
    EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  }

  client()->OnGlanceablesBubbleClosed();

  OverrideTime("10 Apr 2023 09:05 GMT");
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 2);

    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
    ASSERT_TRUE(assignments.at(1)->submissions_state);
    EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       ReturnCachedTeacherAssignmentsOnSubmissionsFetchFailure) {
  OverrideTime("10 Apr 2023 09:05 GMT");

  ExpectActiveCourse(/*call_count=*/2);

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                },
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-2",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 14,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-1",
                  "title": "Assignment 1",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 15,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "course-work-item-2",
                  "title": "Assignment 2",
                  "state": "PUBLISHED",
                  "alternateLink": "https://classroom.google.com/test-link-1",
                  "dueDate": {"year": 2023, "month": 4, "day": 9},
                  "dueTime": {
                    "hours": 14,
                    "minutes": 9,
                    "seconds": 25,
                    "nanos": 250000000
                  }
                }
              ]
            })"))));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          AllOf(HasSubstr("courseWork/course-work-item-1/studentSubmissions?"),
                Not(HasSubstr("pageToken="))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-1", 1, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          R"({
              "studentSubmissions": [
                {
                  "id": "student-submission-1",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                },
                {
                  "id": "student-submission-2",
                  "courseWorkId": "course-work-item-1",
                  "state": "NEW"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          AllOf(HasSubstr("courseWork/course-work-item-1/studentSubmissions?"),
                HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(
          &HttpRequest::relative_url,
          HasSubstr("courseWork/course-work-item-2/studentSubmissions?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 1, 0, 0)))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          CreateSubmissionsListResponse("course-work-item-2", 2, 0, 0)))));

  client()->set_number_of_assignments_prioritized_for_display_for_testing(0u);

  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_TRUE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);
    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
    ASSERT_TRUE(assignments.at(1)->submissions_state);
    EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 1);
  }

  client()->OnGlanceablesBubbleClosed();

  OverrideTime("10 Apr 2023 09:05 GMT");
  {
    AssignmentListFuture future;
    client()->GetTeacherAssignmentsRecentlyDue(future.GetCallback());

    const auto [success, assignments] = future.Take();
    EXPECT_FALSE(success);
    ASSERT_EQ(assignments.size(), 2u);

    EXPECT_EQ(assignments.at(0)->course_work_title, "Assignment 1");
    ASSERT_TRUE(assignments.at(0)->submissions_state);
    EXPECT_EQ(assignments.at(0)->submissions_state->total_count, 1);

    EXPECT_EQ(assignments.at(1)->course_work_title, "Assignment 2");
    ASSERT_TRUE(assignments.at(1)->submissions_state);
    EXPECT_EQ(assignments.at(1)->submissions_state->total_count, 2);
  }
}

}  // namespace ash

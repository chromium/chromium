// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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

class GlanceablesClassroomClientImplIsDisabledByAdminTest
    : public testing::Test {
 public:
  GlanceablesClassroomClientImplIsDisabledByAdminTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
  GetDefaultPrefs() const {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    return prefs;
  }

  TestingProfile* CreateTestingProfile(
      std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs) {
    return profile_manager_.CreateTestingProfile(
        "profile@example.com", std::move(prefs), u"User Name", /*avatar_id=*/0,
        TestingProfile::TestingFactories());
  }

  GlanceablesClassroomClientImpl CreateClientForProfile(
      Profile* profile) const {
    return GlanceablesClassroomClientImpl(
        profile, base::DefaultClock::GetInstance(),
        base::BindLambdaForTesting(
            [](const std::vector<std::string>& scopes,
               const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)
                -> std::unique_ptr<google_apis::RequestSender> {
              return nullptr;
            }));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(GlanceablesClassroomClientImplIsDisabledByAdminTest, Default) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());
  EXPECT_FALSE(CreateClientForProfile(profile).IsDisabledByAdmin());
}

TEST_F(GlanceablesClassroomClientImplIsDisabledByAdminTest,
       NoClassroomInContextualGoogleIntegrationsPref) {
  auto prefs = GetDefaultPrefs();
  base::Value::List enabled_integrations;
  enabled_integrations.Append(prefs::kGoogleCalendarIntegrationName);
  enabled_integrations.Append(prefs::kGoogleTasksIntegrationName);
  prefs->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                 std::move(enabled_integrations));

  auto* const profile = CreateTestingProfile(std::move(prefs));
  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
}

TEST_F(GlanceablesClassroomClientImplIsDisabledByAdminTest,
       DisabledClassroomApp) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());

  std::vector<apps::AppPtr> app_deltas;
  app_deltas.push_back(apps::AppPublisher::MakeApp(
      apps::AppType::kWeb, web_app::kGoogleClassroomAppId,
      apps::Readiness::kDisabledByPolicy, "Classroom",
      apps::InstallReason::kUser, apps::InstallSource::kBrowser));

  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(app_deltas), apps::AppType::kWeb,
      /*should_notify_initialized=*/true);

  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
}

TEST_F(GlanceablesClassroomClientImplIsDisabledByAdminTest,
       BlockedClassroomUrl) {
  auto prefs = GetDefaultPrefs();
  base::Value::List blocklist;
  blocklist.Append("classroom.google.com");
  prefs->SetManagedPref(policy::policy_prefs::kUrlBlocklist,
                        std::move(blocklist));

  auto* const profile = CreateTestingProfile(std::move(prefs));
  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
}

class GlanceablesClassroomClientImplTest : public testing::Test {
 public:
  GlanceablesClassroomClientImplTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

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
        profile_manager_.CreateTestingProfile("profile@example.com",
                                              /*is_main_profile=*/true,
                                              url_loader_factory_),
        &test_clock_, create_request_sender_callback);

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

  base::SimpleTestClock* clock() { return &test_clock_; }
  GlanceablesClassroomClientImpl* client() { return client_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  TestRequestHandler& request_handler() { return request_handler_; }

 private:
  base::SimpleTestClock test_clock_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TestingProfileManager profile_manager_;
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
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
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
            })"))));

  base::RunLoop run_loop;
  client()->FetchStudentCourses(base::BindLambdaForTesting(
      [&](bool success,
          const GlanceablesClassroomClientImpl::CourseList& courses) {
        run_loop.Quit();

        EXPECT_TRUE(success);
        ASSERT_EQ(courses.size(), 1u);

        EXPECT_EQ(courses.at(0)->id, "course-id-1");
        EXPECT_EQ(courses.at(0)->name, "Active Course 1");

        histogram_tester()->ExpectTotalCount(
            "Ash.Glanceables.Api.Classroom.GetCourses.Latency",
            /*expected_count=*/1);
        histogram_tester()->ExpectUniqueSample(
            "Ash.Glanceables.Api.Classroom.GetCourses.Status",
            ApiErrorCode::HTTP_SUCCESS,
            /*expected_bucket_count=*/1);
        histogram_tester()->ExpectUniqueSample(
            "Ash.Glanceables.Api.Classroom.StudentCoursesCount",
            /*sample=*/1,
            /*expected_bucket_count=*/1);
      }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCoursesOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  base::RunLoop run_loop;
  client()->FetchStudentCourses(base::BindLambdaForTesting(
      [&](bool success,
          const GlanceablesClassroomClientImpl::CourseList& courses) {
        run_loop.Quit();

        EXPECT_FALSE(success);
        EXPECT_EQ(0u, courses.size());

        histogram_tester()->ExpectTotalCount(
            "Ash.Glanceables.Api.Classroom.GetCourses.Latency",
            /*expected_count=*/1);
        histogram_tester()->ExpectUniqueSample(
            "Ash.Glanceables.Api.Classroom.GetCourses.Status",
            ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
            /*expected_bucket_count=*/1);
      }));
  run_loop.Run();
}

TEST_F(GlanceablesClassroomClientImplTest, FetchCoursesMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-1", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-2-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-2", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-3-token"
            })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-3", "courseState": "ACTIVE"}
              ]
            })"))));

  base::RunLoop run_loop;
  client()->FetchStudentCourses(base::BindLambdaForTesting(
      [&](bool success,
          const GlanceablesClassroomClientImpl::CourseList& courses) {
        run_loop.Quit();
        EXPECT_TRUE(success);
        ASSERT_EQ(courses.size(), 3u);

        EXPECT_EQ(courses.at(0)->id, "course-id-from-page-1");
        EXPECT_EQ(courses.at(1)->id, "course-id-from-page-2");
        EXPECT_EQ(courses.at(2)->id, "course-id-from-page-3");

        histogram_tester()->ExpectUniqueSample(
            "Ash.Glanceables.Api.Classroom.StudentCoursesCount",
            /*sample=*/3,
            /*expected_bucket_count=*/1);
      }));
  run_loop.Run();
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
  ExpectActiveCourse();

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
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.IsStudentRoleActiveResult",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
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

  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.IsStudentRoleActiveResult",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
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
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.CourseWorkItemsPerStudentCourseCount",
      /*sample=*/4,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.StudentSubmissionsPerStudentCourseCount",
      /*sample=*/4,
      /*expected_bucket_count=*/1);
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
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.CourseWorkItemsPerStudentCourseCount",
      /*sample=*/5,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.StudentSubmissionsPerStudentCourseCount",
      /*sample=*/5,
      /*expected_bucket_count=*/1);
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
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.CourseWorkItemsPerStudentCourseCount",
      /*sample=*/6,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.StudentSubmissionsPerStudentCourseCount",
      /*sample=*/6,
      /*expected_bucket_count=*/1);
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
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.CourseWorkItemsPerStudentCourseCount",
      /*sample=*/5,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Classroom.StudentSubmissionsPerStudentCourseCount",
      /*sample=*/5,
      /*expected_bucket_count=*/1);
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

TEST_F(GlanceablesClassroomClientImplTest,
       RefetchStudentAssignmentsAfterReshowingBubble) {
  ExpectActiveCourse();

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
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
    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
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
    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
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
    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
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
    EXPECT_EQ(assignments.at(0)->course_title, "Active Course 1");
    EXPECT_EQ(assignments.at(0)->course_work_title,
              "Math assignment - approaching due date");
  }
}

TEST_F(GlanceablesClassroomClientImplTest,
       FetchStudentCoursesAfterIsActiveCheck) {
  ExpectActiveCourse();

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
  ExpectActiveCourse();

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
       DontRefetchStudentAssignmentsIfBubbleReshownWhileStillFetching) {
  ExpectActiveCourse();

  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::relative_url,
                          HasSubstr("/courses/course-id-1/courseWork?"))))
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
  EXPECT_EQ(initial_assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(initial_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  const auto [second_success, second_assignments] = second_future.Take();
  EXPECT_TRUE(second_success);
  ASSERT_EQ(second_assignments.size(), 1u);
  EXPECT_EQ(second_assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(second_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");

  // Getting assignments after initial results have been received does not
  // repeat course work data fetch.
  AssignmentListFuture third_future;
  client()->GetStudentAssignmentsWithMissedDueDate(third_future.GetCallback());

  const auto [third_success, third_assignments] = third_future.Take();
  EXPECT_TRUE(third_success);
  ASSERT_EQ(third_assignments.size(), 1u);
  EXPECT_EQ(third_assignments.at(0)->course_title, "Active Course 1");
  EXPECT_EQ(third_assignments.at(0)->course_work_title,
            "Math assignment - missed due date");
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
    clock()->Advance(base::Hours(4));

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
    clock()->Advance(base::Hours(4));

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
    clock()->Advance(base::Hours(4));

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
    clock()->Advance(base::Hours(4));

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
  ExpectActiveCourse();

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
  ExpectActiveCourse();

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
  ExpectActiveCourse();

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
  ExpectActiveCourse();

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

}  // namespace ash

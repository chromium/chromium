// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_calendar_fetcher.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Handles an HTTP request by returning a response from a json file in
// google_apis/test/data/.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  return google_apis::test_util::CreateHttpResponseFromFile(
      google_apis::test_util::GetTestFilePath("calendar/events.json"));
}

class BirchCalendarFetcherTest : public testing::Test {
 public:
  BirchCalendarFetcherTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true)) {}

  void SetUp() override {
    test_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(test_server_.Start());

    profile_ = std::make_unique<TestingProfile>();
    fetcher_ = std::make_unique<BirchCalendarFetcher>(profile_.get());

    // Configure the fetcher to use the test server.
    fetcher_->SetSenderForTest(MakeRequestSender());
    fetcher_->SetBaseUrlForTest(test_server_.base_url().spec());
  }

  void TearDown() override {
    fetcher_->Shutdown();
    fetcher_.reset();
    profile_.reset();

    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  // Makes a request sender configured for testing.
  std::unique_ptr<google_apis::RequestSender> MakeRequestSender() {
    return std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Makes the primary account available, which generates a refresh token.
  void MakePrimaryAccountAvailable() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BirchCalendarFetcher> fetcher_;
};

TEST_F(BirchCalendarFetcherTest, GetCalendarEvents_WithRefreshToken) {
  // Make the primary account available, which generates a refresh token.
  MakePrimaryAccountAvailable();

  // Use arbitrary start and end times. The returned response data is always the
  // same fixed test data.
  base::Time start_time;
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start_time));
  ASSERT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end_time));

  // These will be overwritten by the copy result callback.
  google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<google_apis::calendar::EventList> events;

  // Fetch the calendar events.
  base::RunLoop run_loop;
  fetcher_->GetCalendarEvents(
      start_time, end_time,
      google_apis::test_util::CreateQuitCallback(
          &run_loop,
          google_apis::test_util::CreateCopyResultCallback(&error, &events)));
  run_loop.Run();

  // Verify events were returned.
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(events);
  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
  const std::vector<std::unique_ptr<google_apis::calendar::CalendarEvent>>&
      items = events->items();
  ASSERT_EQ(items.size(), 3u);
  EXPECT_EQ(items[0]->summary(), "Mobile weekly team meeting ");
  EXPECT_EQ(items[1]->summary(), "Calendar view weekly meeting");
  EXPECT_EQ(items[2]->summary(), "System UI sync");
}

TEST_F(BirchCalendarFetcherTest, GetCalendarEvents_NoRefreshToken) {
  // Use arbitrary start and end times. The returned response data is always the
  // same fixed test data.
  base::Time start_time;
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start_time));
  ASSERT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end_time));

  // These will be overwritten by the copy result callback.
  google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<google_apis::calendar::EventList> events;

  // Fetch the calendar events with no refresh token available.
  base::RunLoop run_loop;
  fetcher_->GetCalendarEvents(
      start_time, end_time,
      google_apis::test_util::CreateQuitCallback(
          &run_loop,
          google_apis::test_util::CreateCopyResultCallback(&error, &events)));

  // Make the primary account available, which generates a refresh token.
  MakePrimaryAccountAvailable();
  run_loop.Run();

  // Verify events were returned.
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(events);
  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
  const std::vector<std::unique_ptr<google_apis::calendar::CalendarEvent>>&
      items = events->items();
  ASSERT_EQ(items.size(), 3u);
  EXPECT_EQ(items[0]->summary(), "Mobile weekly team meeting ");
  EXPECT_EQ(items[1]->summary(), "Calendar view weekly meeting");
  EXPECT_EQ(items[2]->summary(), "System UI sync");
}

}  // namespace
}  // namespace ash

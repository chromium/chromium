// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/calendar/calendar_keyed_service.h"

#include <memory>
#include <vector>

#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kPrimaryProfileName[] = "primary_profile";
const char kSecondaryProfileName[] = "secondary_profile";
const char kTestUserAgent[] = "test-user-agent";

}  // namespace

class CalendarKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  CalendarKeyedServiceTest()
      : fake_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  CalendarKeyedServiceTest(const CalendarKeyedServiceTest& other) = delete;
  CalendarKeyedServiceTest& operator=(const CalendarKeyedServiceTest& other) =
      delete;
  ~CalendarKeyedServiceTest() override = default;

  TestingProfile* CreateProfile() override {
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    GetSessionControllerClient()->AddUserSession(kPrimaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);

    return profile_manager()->CreateTestingProfile(kPrimaryProfileName,
                                                   /*testing_factories=*/{});
  }

  TestingProfile* CreateSecondaryProfile() {
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    return profile_manager()->CreateTestingProfile(
        kSecondaryProfileName,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        u"Test profile",
        /*avatar_id=*/1,
        /*testing_factories=*/{});
  }

  void ActivateSecondaryProfile() {
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    GetSessionControllerClient()->AddUserSession(kSecondaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client();
  }

 private:
  std::unique_ptr<FakeChromeUserManager> fake_user_manager_;
};

class CalendarKeyedServiceIOTest : public testing::Test {
 public:
  CalendarKeyedServiceIOTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true)) {}

  CalendarKeyedServiceIOTest(const CalendarKeyedServiceIOTest& other) = delete;
  CalendarKeyedServiceIOTest& operator=(
      const CalendarKeyedServiceIOTest& other) = delete;
  ~CalendarKeyedServiceIOTest() override = default;

  void SetUp() override {
    request_sender_ = std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &CalendarKeyedServiceIOTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Deleting the sender here will delete all request objects.
    request_sender_.reset();

    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;

  // Returns the mock calendar event list.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Return the response from the event json file. The json file path is:
    // google_apis/test/data/calendar/events.json
    return google_apis::test_util::CreateHttpResponseFromFile(
        google_apis::test_util::GetTestFilePath("calendar/events.json"));
  }
};

// Calendar service does not support guest user.
TEST_F(CalendarKeyedServiceTest, GuestUserProfile) {
  // Construct a guest session profile.
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  guest_profile_builder.SetProfileName("guest_profile");
  guest_profile_builder.AddTestingFactories({});
  std::unique_ptr<TestingProfile> guest_profile = guest_profile_builder.Build();

  // Profile should be created for guest sessions.
  ASSERT_TRUE(guest_profile);
  CalendarKeyedService* const guest_profile_service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(
          guest_profile.get());
  EXPECT_FALSE(guest_profile_service);
}

// Calendar service does not support incognito user.
TEST_F(CalendarKeyedServiceTest, OffTheRecordProfile) {
  // Service instances should be created for on the record profiles.
  CalendarKeyedService* const primary_profile_service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  EXPECT_TRUE(primary_profile_service);

  // Construct an incognito profile from the primary profile.
  TestingProfile::Builder incognito_primary_profile_builder;
  incognito_primary_profile_builder.SetProfileName(
      GetProfile()->GetProfileUserName());
  Profile* const incognito_primary_profile =
      incognito_primary_profile_builder.BuildIncognito(GetProfile());
  ASSERT_TRUE(incognito_primary_profile);
  EXPECT_TRUE(incognito_primary_profile->IsOffTheRecord());

  // Service instances should *not* typically be created for OTR profiles.
  CalendarKeyedService* const incognito_primary_profile_service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(
          incognito_primary_profile);
  EXPECT_FALSE(incognito_primary_profile_service);
}

// Calendar service should support switching users.
TEST_F(CalendarKeyedServiceTest, SecondaryUserProfile) {
  CalendarKeyedService* const primary_calendar_service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  TestingProfile* const second_profile = CreateSecondaryProfile();

  CalendarKeyedService* const secondary_calendar_service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(second_profile);
  // Just creating a secondary profile shouldn't change the active client.
  EXPECT_EQ(ash::Shell::Get()->calendar_controller()->GetClient(),
            primary_calendar_service->client());

  // Switching the active user should change the active client (multi-user
  // support).
  ActivateSecondaryProfile();
  EXPECT_EQ(ash::Shell::Get()->calendar_controller()->GetClient(),
            secondary_calendar_service->client());
}

TEST_F(CalendarKeyedServiceIOTest, GetEventList) {
  // Creating the service with some testing profile and account id. Since in
  // this test we are using the IO thread, the service can not be created from
  // the factory.
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  auto calendar_service = std::make_unique<CalendarKeyedService>(
      profile_.get(), AccountId::FromUserEmail("test@email.com"));

  calendar_service->set_sender_for_testing(std::move(request_sender_));
  calendar_service->SetUrlForTesting(test_server_.base_url().spec());

  // The error code should be overwritten by `HTTP_SUCCESS` after the
  // `GetEventList` call.
  google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;

  // This events should be the mock response after the `GetEventList` call.
  std::unique_ptr<google_apis::calendar::EventList> events;

  base::Time start;
  base::Time end;
  ASSERT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start));
  ASSERT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end));

  {
    base::RunLoop run_loop;
    calendar_service->GetEventList(
        google_apis::test_util::CreateQuitCallback(
            &run_loop,
            google_apis::test_util::CreateCopyResultCallback(&error, &events)),
        start, end);
    run_loop.Run();
  }

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  const google_apis::calendar::CalendarEvent& event = *events->items()[0];
  EXPECT_EQ(event.summary(), "Mobile weekly team meeting ");
  EXPECT_EQ(event.id(), "or8221sirt4ogftest");
  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
}

}  // namespace ash

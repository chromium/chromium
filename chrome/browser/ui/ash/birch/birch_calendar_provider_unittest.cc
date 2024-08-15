// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_calendar_provider.h"

#include <vector>

#include "ash/birch/birch_model.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/birch/birch_calendar_fetcher.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {
namespace {

using google_apis::ApiErrorCode;

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  CHECK(base::Time::FromString(time_string, &time));
  return time;
}

// A fake fetcher that provides arbitrary error codes and events.
class TestCalendarFetcher : public BirchCalendarFetcher {
 public:
  explicit TestCalendarFetcher(Profile* profile)
      : BirchCalendarFetcher(profile) {}
  ~TestCalendarFetcher() override = default;

  // BirchCalendarFetcher:
  void GetCalendarEvents(
      base::Time start_time,
      base::Time end_time,
      google_apis::calendar::CalendarEventListCallback callback) override {
    std::move(callback).Run(error_code_, std::move(events_));
  }

  ApiErrorCode error_code_ = ApiErrorCode::HTTP_SUCCESS;
  std::unique_ptr<google_apis::calendar::EventList> events_;
};

// A fetcher that counts how many times GetCalendarEvents() was called.
class CountingCalendarFetcher : public BirchCalendarFetcher {
 public:
  explicit CountingCalendarFetcher(Profile* profile)
      : BirchCalendarFetcher(profile) {}
  ~CountingCalendarFetcher() override = default;

  // BirchCalendarFetcher:
  void GetCalendarEvents(
      base::Time start_time,
      base::Time end_time,
      google_apis::calendar::CalendarEventListCallback callback) override {
    ++get_calendar_events_count_;
    // Intentionally don't run the callback.
  }

  int get_calendar_events_count_ = 0;
};

// BrowserWithTestWindowTest provides a Profile and ash::Shell (which provides
// a BirchModel) needed by the test.
class BirchCalendarProviderTest : public BrowserWithTestWindowTest {
 public:
  BirchCalendarProviderTest() = default;
  ~BirchCalendarProviderTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();

    AccountId account_id = AccountId::FromUserEmail("user1@email.com");
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};

  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
};

TEST_F(BirchCalendarProviderTest, GetCalendarEvents) {
  BirchCalendarProvider provider(profile());

  // Set up a custom fetcher with known events.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  auto events = std::make_unique<google_apis::calendar::EventList>();
  events->set_time_zone("Greenwich Mean Time");
  events->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "title_0", "10 Jan 2010 10:00 GMT", "10 Jan 2010 11:00 GMT"));
  events->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "title_1", "11 Jan 2010 10:00 GMT", "11 Jan 2010 11:00 GMT"));
  fetcher->events_ = std::move(events);
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // Verify the events were inserted into the birch model.
  const auto& items = Shell::Get()->birch_model()->GetCalendarItemsForTest();
  ASSERT_EQ(2u, items.size());
  EXPECT_EQ(items[0].title(), u"title_0");
  EXPECT_EQ(items[0].start_time(), TimeFromString("10 Jan 2010 10:00 GMT"));
  EXPECT_EQ(items[0].end_time(), TimeFromString("10 Jan 2010 11:00 GMT"));
  EXPECT_EQ(items[1].title(), u"title_1");
  EXPECT_EQ(items[1].start_time(), TimeFromString("11 Jan 2010 10:00 GMT"));
  EXPECT_EQ(items[1].end_time(), TimeFromString("11 Jan 2010 11:00 GMT"));
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_WithNoSummary) {
  BirchCalendarProvider provider(profile());

  // Set up a custom fetcher with known events.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  auto events = std::make_unique<google_apis::calendar::EventList>();
  events->set_time_zone("Greenwich Mean Time");
  events->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", /*summary=*/"", "10 Jan 2010 10:00 GMT",
      "10 Jan 2010 11:00 GMT"));
  fetcher->events_ = std::move(events);
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // The title contains "(No title)".
  const auto& items = Shell::Get()->birch_model()->GetCalendarItemsForTest();
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(items[0].title(), u"(No title)");
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_WithAttachments) {
  BirchCalendarProvider provider(profile());

  // Set up a custom fetcher with an event with attachments.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  auto events = std::make_unique<google_apis::calendar::EventList>();
  events->set_time_zone("Greenwich Mean Time");
  auto event = calendar_test_utils::CreateEvent(
      "id_0", "title_0", "10 Jan 2010 10:00 GMT", "10 Jan 2010 11:00 GMT");
  event->set_conference_data_uri(GURL("http://meet.com/"));
  google_apis::calendar::Attachment attachment0;
  attachment0.set_title("attachment0");
  attachment0.set_file_url(GURL("http://file0.com/"));
  attachment0.set_icon_link(GURL("http://icon0.com/"));
  attachment0.set_file_id("file_id_0");
  google_apis::calendar::Attachment attachment1;
  attachment1.set_title("attachment1");
  attachment1.set_file_url(GURL("http://file1.com/"));
  attachment1.set_icon_link(GURL("http://icon1.com/"));
  attachment1.set_file_id("file_id_1");
  event->set_attachments({attachment0, attachment1});
  events->InjectItemForTesting(std::move(event));
  fetcher->events_ = std::move(events);
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // Verify the event was converted correctly to Birch data types.
  auto* birch_model = Shell::Get()->birch_model();
  const auto& items = birch_model->GetCalendarItemsForTest();
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(items[0].title(), u"title_0");
  EXPECT_EQ(items[0].start_time(), TimeFromString("10 Jan 2010 10:00 GMT"));
  EXPECT_EQ(items[0].end_time(), TimeFromString("10 Jan 2010 11:00 GMT"));
  EXPECT_EQ(items[0].conference_url().spec(), "http://meet.com/");

  // Verify the attachments were converted correctly to Birch data types.
  const auto& attachments = birch_model->GetAttachmentItemsForTest();
  EXPECT_EQ(attachments[0].title(), u"attachment0");
  EXPECT_EQ(attachments[0].file_url().spec(), "http://file0.com/");
  EXPECT_EQ(attachments[0].icon_url().spec(), "http://icon0.com/");
  EXPECT_EQ(attachments[0].file_id(), "file_id_0");
  EXPECT_EQ(attachments[1].title(), u"attachment1");
  EXPECT_EQ(attachments[1].file_url().spec(), "http://file1.com/");
  EXPECT_EQ(attachments[1].icon_url().spec(), "http://icon1.com/");
  EXPECT_EQ(attachments[1].file_id(), "file_id_1");
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_DeclinedEventAttachment) {
  BirchCalendarProvider provider(profile());

  // Set up a custom fetcher with an event with attachments.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  auto events = std::make_unique<google_apis::calendar::EventList>();
  events->set_time_zone("Greenwich Mean Time");

  // Create a declined event with an attachment.
  auto event = calendar_test_utils::CreateEvent(
      "id_0", "title_0", "10 Jan 2010 10:00 GMT", "10 Jan 2010 11:00 GMT",
      google_apis::calendar::CalendarEvent::EventStatus::kConfirmed,
      google_apis::calendar::CalendarEvent::ResponseStatus::kDeclined);
  event->set_conference_data_uri(GURL("http://meet.com/"));
  google_apis::calendar::Attachment attachment0;
  attachment0.set_title("attachment0");
  attachment0.set_file_url(GURL("http://file0.com/"));
  attachment0.set_icon_link(GURL("http://icon0.com/"));
  attachment0.set_file_id("file_id_0");
  event->set_attachments({attachment0});
  events->InjectItemForTesting(std::move(event));
  fetcher->events_ = std::move(events);
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // Verify the declined event is not added to the model.
  auto* birch_model = Shell::Get()->birch_model();
  const auto& items = birch_model->GetCalendarItemsForTest();
  ASSERT_EQ(0u, items.size());

  // Verify the declined event attachment is not added to the model.
  const auto& attachments = birch_model->GetAttachmentItemsForTest();
  ASSERT_EQ(0u, attachments.size());
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_HttpError) {
  BirchCalendarProvider provider(profile());

  // Populate the birch model with an event so the test can sense when the
  // model is cleared later.
  std::vector<BirchCalendarItem> items;
  items.emplace_back(u"Event 1", /*start_time=*/base::Time(),
                     /*end_time=*/base::Time(), /*calendar_url=*/GURL(),
                     /*conference_url=*/GURL(), /*event_id=*/"",
                     /*all_day_event=*/false);
  Shell::Get()->birch_model()->SetCalendarItems(std::move(items));

  // Set up a customer fetcher that returns an error.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  fetcher->error_code_ = ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR;
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // Verify the birch model is empty.
  EXPECT_TRUE(Shell::Get()->birch_model()->GetCalendarItemsForTest().empty());
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_NullEventList) {
  BirchCalendarProvider provider(profile());

  // Populate the birch model with an event so the test can sense when the
  // model is cleared later.
  std::vector<BirchCalendarItem> items;
  items.emplace_back(u"Event 1", /*start_time=*/base::Time(),
                     /*end_time=*/base::Time(), /*calendar_url=*/GURL(),
                     /*conference_url=*/GURL(), /*event_id=*/"",
                     /*all_day_event=*/false);
  Shell::Get()->birch_model()->SetCalendarItems(std::move(items));

  // Set up a customer fetcher that returns a null event list.
  auto fetcher = std::make_unique<TestCalendarFetcher>(profile());
  fetcher->events_ = nullptr;
  provider.SetFetcherForTest(std::move(fetcher));

  // Get the calendar events.
  provider.RequestBirchDataFetch();

  // Verify the birch model is empty.
  EXPECT_TRUE(Shell::Get()->birch_model()->GetCalendarItemsForTest().empty());
}

TEST_F(BirchCalendarProviderTest, GetCalendarEvents_MultipleRequests) {
  BirchCalendarProvider provider(profile());

  // Set up a customer fetcher.
  auto fetcher = std::make_unique<CountingCalendarFetcher>(profile());
  auto* fetcher_ptr = fetcher.get();
  provider.SetFetcherForTest(std::move(fetcher));
  ASSERT_EQ(fetcher_ptr->get_calendar_events_count_, 0);

  // Request calendar events twice in a row.
  provider.RequestBirchDataFetch();
  provider.RequestBirchDataFetch();

  // The fetcher was only triggered once.
  EXPECT_EQ(fetcher_ptr->get_calendar_events_count_, 1);
}

}  // namespace
}  // namespace ash

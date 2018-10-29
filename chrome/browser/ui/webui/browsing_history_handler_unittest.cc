// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_history_handler.h"

#include <stdint.h>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/sync/base/model_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

base::Time PretendNow() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 2;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  base::Time out_time;
  EXPECT_TRUE(
      base::Time::FromLocalExploded(exploded_reference_time, &out_time));
  return out_time;
}

class TestSyncService : public browser_sync::TestProfileSyncService {
 public:
  explicit TestSyncService(Profile* profile)
      : browser_sync::TestProfileSyncService(
            CreateProfileSyncServiceParamsForTest(profile)),
        state_(TransportState::ACTIVE) {}

  TransportState GetTransportState() const override { return state_; }

  int GetDisableReasons() const override { return DISABLE_REASON_NONE; }

  bool IsFirstSetupComplete() const override { return true; }

  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return syncer::ModelTypeSet::All();
  }

  void SetTransportState(TransportState state) {
    state_ = state;
    NotifyObservers();
  }

 private:
  TransportState state_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

class BrowsingHistoryHandlerWithWebUIForTesting
    : public BrowsingHistoryHandler {
 public:
  explicit BrowsingHistoryHandlerWithWebUIForTesting(content::WebUI* web_ui) {
    set_clock(&test_clock_);
    set_web_ui(web_ui);
    test_clock_.SetNow(PretendNow());
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  base::SimpleTestClock test_clock_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandlerWithWebUIForTesting);
};

}  // namespace

class BrowsingHistoryHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeSyncService));
    builder.AddTestingFactory(WebHistoryServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeWebHistoryService));
    profile_ = builder.Build();
    profile_->CreateBookmarkModel(false);

    sync_service_ = static_cast<TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile_.get()));
    web_history_service_ = static_cast<history::FakeWebHistoryService*>(
        WebHistoryServiceFactory::GetForProfile(profile_.get()));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    web_ui_.reset(new content::TestWebUI);
    web_ui_->set_web_contents(web_contents_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    web_ui_.reset();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }
  TestSyncService* sync_service() { return sync_service_; }
  history::WebHistoryService* web_history_service() {
    return web_history_service_;
  }
  content::TestWebUI* web_ui() { return web_ui_.get(); }

 private:
  static std::unique_ptr<KeyedService> BuildFakeSyncService(
      content::BrowserContext* context) {
    return std::make_unique<TestSyncService>(
        static_cast<TestingProfile*>(context));
  }

  static std::unique_ptr<KeyedService> BuildFakeWebHistoryService(
      content::BrowserContext* context) {
    std::unique_ptr<history::FakeWebHistoryService> service =
        std::make_unique<history::FakeWebHistoryService>();
    service->SetupFakeResponse(true /* success */, net::HTTP_OK);
    return std::move(service);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  TestSyncService* sync_service_;
  history::FakeWebHistoryService* web_history_service_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Tests that BrowsingHistoryHandler is informed about WebHistoryService
// deletions.
TEST_F(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions) {
  base::Callback<void(bool)> callback = base::DoNothing();

  // BrowsingHistoryHandler is informed about WebHistoryService history
  // deletions.
  {
    sync_service()->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(1U, web_ui()->call_data().size());
    EXPECT_EQ("historyDeleted", web_ui()->call_data().back()->function_name());
  }

  // BrowsingHistoryHandler will be informed about WebHistoryService deletions
  // even if history sync is activated later.
  {
    sync_service()->SetTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    sync_service()->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(2U, web_ui()->call_data().size());
    EXPECT_EQ("historyDeleted", web_ui()->call_data().back()->function_name());
  }

  // BrowsingHistoryHandler does not fire historyDeleted while a web history
  // delete request is happening.
  {
    sync_service()->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    // Simulate an ongoing delete request.
    handler.browsing_history_service_->has_pending_delete_request_ = true;

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(),
        base::Bind(
            &history::BrowsingHistoryService::RemoveWebHistoryComplete,
            handler.browsing_history_service_->weak_factory_.GetWeakPtr()),
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(3U, web_ui()->call_data().size());
    EXPECT_EQ("deleteComplete", web_ui()->call_data().back()->function_name());
  }

  // When history sync is not active, we don't listen to WebHistoryService
  // deletions. The WebHistoryService object still exists (because it's a
  // BrowserContextKeyedService), but is not visible to BrowsingHistoryHandler.
  {
    sync_service()->SetTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(), callback,
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

    // No additional WebUI calls were made.
    EXPECT_EQ(3U, web_ui()->call_data().size());
  }
}

#if !defined(OS_ANDROID)
TEST_F(BrowsingHistoryHandlerTest, MdTruncatesTitles) {
  history::BrowsingHistoryService::HistoryEntry long_url_entry;
  long_url_entry.url = GURL(
      "http://loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ngurlislong.com");
  ASSERT_GT(long_url_entry.url.spec().size(), 300U);

  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  ASSERT_TRUE(web_ui()->call_data().empty());

  handler.OnQueryComplete({long_url_entry},
                          history::BrowsingHistoryService::QueryResultsInfo(),
                          base::OnceClosure());
  ASSERT_FALSE(web_ui()->call_data().empty());

  const base::ListValue* arg2;
  ASSERT_TRUE(web_ui()->call_data().front()->arg2()->GetAsList(&arg2));

  const base::DictionaryValue* first_entry;
  ASSERT_TRUE(arg2->GetDictionary(0, &first_entry));

  base::string16 title;
  ASSERT_TRUE(first_entry->GetString("title", &title));

  ASSERT_EQ(0u, title.find(base::ASCIIToUTF16("http://loooo")));
  EXPECT_EQ(300u, title.size());
}
#endif

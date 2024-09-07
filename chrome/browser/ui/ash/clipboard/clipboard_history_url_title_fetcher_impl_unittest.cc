// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_history_url_title_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::Optional;

const std::u16string kUrlTitle = u"Title";
constexpr char kIsPrimaryProfileActiveHistogramName[] =
    "Ash.ClipboardHistory.UrlTitleFetcher.IsPrimaryProfileActive";
constexpr char kNumProfilesHistogramName[] =
    "Ash.ClipboardHistory.UrlTitleFetcher.NumProfiles";
constexpr char kUrlFoundHistogramName[] =
    "Ash.ClipboardHistory.UrlTitleFetcher.UrlFound";

// MockHistoryService ----------------------------------------------------------

class MockHistoryService : public history::HistoryService {
 public:
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryURL,
              (const GURL& url,
               bool want_visits,
               QueryURLCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};

// Helpers ---------------------------------------------------------------------

base::CancelableTaskTracker::TaskId RunHistoryEntryNotFoundCallback(
    const GURL& url,
    bool want_visits,
    history::HistoryService::QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::QueryURLResult result;
  result.success = false;
  std::move(callback).Run(result);
  return base::CancelableTaskTracker::TaskId();
}

base::CancelableTaskTracker::TaskId RunHistoryEntryFoundCallback(
    const GURL& url,
    bool want_visits,
    history::HistoryService::QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::QueryURLResult result;
  result.success = true;
  result.row.set_url(url);
  result.row.set_title(kUrlTitle);
  std::move(callback).Run(result);
  return base::CancelableTaskTracker::TaskId();
}

}  // namespace

// ClipboardHistoryUrlTitleFetcherTest -----------------------------------------

class ClipboardHistoryUrlTitleFetcherTest : public BrowserWithTestWindowTest {
 public:
  ClipboardHistoryUrlTitleFetcherTest() = default;

 protected:
  void CreateAndSwitchToSecondaryProfile() {
    const std::string kSecondaryProfileName = "secondary_profile@test";
    LogIn(kSecondaryProfileName);
    CreateProfile(kSecondaryProfileName);
    user_manager()->SwitchActiveUser(
        AccountId::FromUserEmail(kSecondaryProfileName));
  }

  ClipboardHistoryUrlTitleFetcherImpl& fetcher() { return fetcher_; }
  MockHistoryService* history_service() { return history_service_; }

 private:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(true);
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override {
    // Reset `history_service_` so that it does not dangle after the keyed
    // service is destroyed during `BrowserWithTestWindowTest::TearDown()`.
    history_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(false);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(
            &ClipboardHistoryUrlTitleFetcherTest::BuildHistoryService,
            base::Unretained(this))}};
  }

  std::unique_ptr<KeyedService> BuildHistoryService(
      content::BrowserContext* context) {
    auto history_service =
        std::make_unique<testing::StrictMock<MockHistoryService>>();
    history_service_ = history_service.get();
    return std::move(history_service);
  }

  ClipboardHistoryUrlTitleFetcherImpl fetcher_;
  // Owned by `KeyedServiceFactory`. Must be cleaned up before `TearDown()`.
  raw_ptr<MockHistoryService> history_service_ = nullptr;
};

// Tests -----------------------------------------------------------------------

TEST_F(ClipboardHistoryUrlTitleFetcherTest,
       QueriedTitleReflectsBrowsingHistory) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl("https://www.url.com");
  base::test::TestFuture<std::optional<std::u16string>> title_future;

  {
    SCOPED_TRACE("Query a title found in the browsing history.");
    EXPECT_CALL(*history_service(), QueryURL(kTestUrl, false, _, _))
        .WillOnce(&RunHistoryEntryFoundCallback);

    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    EXPECT_THAT(title_future.Take(), Optional(kUrlTitle));
    histogram_tester.ExpectTotalCount(kUrlFoundHistogramName,
                                      /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(kUrlFoundHistogramName, true,
                                       /*expected_count=*/1);
  }

  {
    SCOPED_TRACE("Query a title not found in the browsing history.");
    EXPECT_CALL(*history_service(), QueryURL(kTestUrl, false, _, _))
        .WillOnce(&RunHistoryEntryNotFoundCallback);

    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    EXPECT_FALSE(title_future.Take());
    histogram_tester.ExpectTotalCount(kUrlFoundHistogramName,
                                      /*expected_count=*/2);
    histogram_tester.ExpectBucketCount(kUrlFoundHistogramName, false,
                                       /*expected_count=*/1);
  }
}

TEST_F(ClipboardHistoryUrlTitleFetcherTest, HistoryQueryFailsWithMultiProfile) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl("https://www.url.com");
  EXPECT_CALL(*history_service(), QueryURL(kTestUrl, false, _, _))
      .WillRepeatedly(&RunHistoryEntryFoundCallback);
  base::test::TestFuture<std::optional<std::u16string>> title_future;

  {
    SCOPED_TRACE("Query a title while the primary profile is active.");
    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    // Querying the browsing history is allowed when exactly one profile has
    // been added to the session.
    EXPECT_THAT(title_future.Take(), Optional(kUrlTitle));

    histogram_tester.ExpectTotalCount(kIsPrimaryProfileActiveHistogramName,
                                      /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(kIsPrimaryProfileActiveHistogramName,
                                       true, /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(kNumProfilesHistogramName,
                                      /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(kNumProfilesHistogramName, 1,
                                       /*expected_count=*/1);
  }

  CreateAndSwitchToSecondaryProfile();

  {
    SCOPED_TRACE("Query a title after switching to a new profile.");
    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    // Querying the browsing history is not allowed when more than one profile
    // has been added to the session.
    EXPECT_FALSE(title_future.Take());

    histogram_tester.ExpectTotalCount(kIsPrimaryProfileActiveHistogramName,
                                      /*expected_count=*/2);
    histogram_tester.ExpectBucketCount(kIsPrimaryProfileActiveHistogramName,
                                       false, /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(kNumProfilesHistogramName,
                                      /*expected_count=*/2);
    histogram_tester.ExpectBucketCount(kNumProfilesHistogramName, 2,
                                       /*expected_count=*/1);
  }
}

}  // namespace ash

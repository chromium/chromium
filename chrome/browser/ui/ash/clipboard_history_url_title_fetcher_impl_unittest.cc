// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_history_url_title_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::Optional;

const std::u16string kUrlTitle = u"Title";

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
  ClipboardHistoryUrlTitleFetcherTest() {
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

 protected:
  ClipboardHistoryUrlTitleFetcherImpl& fetcher() { return fetcher_; }
  MockHistoryService* history_service() { return history_service_; }

 private:
  // BrowserWithTestWindowTest:
  void TearDown() override {
    // Reset `history_service_` so that it does not dangle after the keyed
    // service is destroyed during `BrowserWithTestWindowTest::TearDown()`.
    history_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{HistoryServiceFactory::GetInstance(),
             base::BindRepeating(
                 &ClipboardHistoryUrlTitleFetcherTest::BuildHistoryService,
                 base::Unretained(this))}};
  }

  TestingProfile* CreateProfile() override {
    const std::string kPrimaryProfileName = "primary_profile@test";
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    return profile_manager()->CreateTestingProfile(kPrimaryProfileName,
                                                   GetTestingFactories());
  }

  std::unique_ptr<KeyedService> BuildHistoryService(
      content::BrowserContext* context) {
    auto history_service =
        std::make_unique<testing::StrictMock<MockHistoryService>>();
    history_service_ = history_service.get();
    return std::move(history_service);
  }

  ClipboardHistoryUrlTitleFetcherImpl fetcher_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // Owned by, and therefore must be cleaned up before, `user_manager_enabler_`.
  raw_ptr<FakeChromeUserManager> fake_user_manager_ = nullptr;
  // Owned by `KeyedServiceFactory`. Must be cleaned up before `TearDown()`.
  raw_ptr<MockHistoryService> history_service_ = nullptr;
};

// Tests -----------------------------------------------------------------------

TEST_F(ClipboardHistoryUrlTitleFetcherTest,
       QueriedTitleReflectsBrowsingHistory) {
  const GURL kTestUrl("https://www.url.com");
  base::test::TestFuture<absl::optional<std::u16string>> title_future;

  {
    SCOPED_TRACE("Query a title found in the browsing history.");
    EXPECT_CALL(*history_service(), QueryURL(kTestUrl, false, _, _))
        .WillOnce(&RunHistoryEntryFoundCallback);

    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    EXPECT_THAT(title_future.Take(), Optional(kUrlTitle));
  }

  {
    SCOPED_TRACE("Query a title not found in the browsing history.");
    EXPECT_CALL(*history_service(), QueryURL(kTestUrl, false, _, _))
        .WillOnce(&RunHistoryEntryNotFoundCallback);

    fetcher().QueryHistory(kTestUrl, title_future.GetRepeatingCallback());
    EXPECT_FALSE(title_future.Take());
  }
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/picker_controller.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

using MockSearchResultsCallback =
    testing::MockFunction<PickerClientImpl::CrosSearchResultsCallback>;

std::unique_ptr<KeyedService> BuildTestHistoryService(
    base::FilePath profile_path,
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(profile_path));
  return std::move(service);
}

void AddSearchToHistory(TestingProfile* profile, GURL url) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->AddPageWithDetails(url, /*title=*/u"", /*visit_count=*/1,
                              /*typed_count=*/1,
                              /*last_visit=*/base::Time::Now(),
                              /*hidden=*/false, history::SOURCE_BROWSED);
  profile->BlockUntilHistoryProcessesPendingRequests();
}

void AddBookmarks(TestingProfile* profile,
                  std::u16string_view title,
                  GURL url) {
  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         std::u16string(title), url);
}

class PickerClientImplTest : public testing::Test {
 public:
  PickerClientImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()),
        fake_user_manager_(std::make_unique<user_manager::FakeUserManager>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  void TearDown() override {
    for (const user_manager::User* user : fake_user_manager_->GetUsers()) {
      fake_user_manager_->OnUserProfileWillBeDestroyed(user->GetAccountId());
    }
  }

  struct LoginState {
    raw_ptr<user_manager::UserManager> user_manager;
    raw_ptr<TestingProfile> profile;
  };

  // Returns the user manager and profile used in this test, logged into a fake
  // user.
  LoginState LogInAsFakeUser() {
    AccountId account_id = AccountId::FromUserEmail("test@test");

    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    TestingProfile* profile = CreateTestingProfileForAccount(account_id);
    fake_user_manager_->OnUserProfileCreated(account_id, profile->GetPrefs());

    return {
        .user_manager = fake_user_manager_.Get(),
        .profile = profile,
    };
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_shared_url_loader_factory_;
  }

 private:
  TestingProfile* CreateTestingProfileForAccount(const AccountId& account_id) {
    return testing_profile_manager_.CreateTestingProfile(
        account_id.GetUserEmail(),
        {{HistoryServiceFactory::GetInstance(),
          base::BindRepeating(&BuildTestHistoryService, temp_dir_.GetPath())},
         {BookmarkModelFactory::GetInstance(),
          BookmarkModelFactory::GetDefaultFactory()},
         {TemplateURLServiceFactory::GetInstance(),
          base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}},
        /*is_main_profile=*/false, test_shared_url_loader_factory_);
  }

  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  // Keep `fake_user_manager_` before `testing_profile_manager_` to match
  // destruction order in production:
  // https://crsrc.org/c/chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1668;drc=c7da8fba0e20c71d61e5c78ecd6a3872c4c56e6c
  // https://crsrc.org/c/chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1719;drc=c7da8fba0e20c71d61e5c78ecd6a3872c4c56e6c
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(PickerClientImplTest, GetsSharedURLLoaderFactory) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, LogInAsFakeUser().user_manager);

  EXPECT_EQ(client.GetSharedURLLoaderFactory(), GetSharedURLLoaderFactory());
}

TEST_F(PickerClientImplTest, StartCrosSearch) {
  ash::PickerController controller;
  LoginState login = LogInAsFakeUser();
  PickerClientImpl client(&controller, login.user_manager);
  AddSearchToHistory(login.profile, GURL("http://foo.com/history"));
  AddBookmarks(login.profile, u"Foobaz", GURL("http://foo.com/bookmarks"));
  base::test::TestFuture<void> test_done;

  NiceMock<MockSearchResultsCallback> mock_search_callback;
  EXPECT_CALL(mock_search_callback, Call(_, _)).Times(AnyNumber());
  EXPECT_CALL(
      mock_search_callback,
      Call(ash::AppListSearchResultType::kOmnibox,
           IsSupersetOf({
               Property(
                   "data", &ash::PickerSearchResult::data,
                   VariantWith<ash::PickerSearchResult::BrowsingHistoryData>(
                       Field("url",
                             &ash::PickerSearchResult::BrowsingHistoryData::url,
                             GURL("http://foo.com/history")))),
               Property(
                   "data", &ash::PickerSearchResult::data,
                   VariantWith<
                       ash::PickerSearchResult::BrowsingHistoryData>(AllOf(
                       Field(
                           "title",
                           &ash::PickerSearchResult::BrowsingHistoryData::title,
                           u"Foobaz"),
                       Field("url",
                             &ash::PickerSearchResult::BrowsingHistoryData::url,
                             GURL("http://foo.com/bookmarks"))))),
           })))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(
      u"foo", /*category=*/std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&mock_search_callback)));

  ASSERT_TRUE(test_done.Wait());
}

TEST_F(PickerClientImplTest, StartCrosSearchFilteredByBrowsingHistory) {
  ash::PickerController controller;
  LoginState login = LogInAsFakeUser();
  PickerClientImpl client(&controller, login.user_manager);
  AddSearchToHistory(login.profile, GURL("http://foo.com/history"));
  AddBookmarks(login.profile, u"Foobaz", GURL("http://foo.com/bookmarks"));
  base::test::TestFuture<void> test_done;

  NiceMock<MockSearchResultsCallback> mock_search_callback;
  EXPECT_CALL(mock_search_callback, Call(_, _)).Times(AnyNumber());
  EXPECT_CALL(
      mock_search_callback,
      Call(ash::AppListSearchResultType::kOmnibox,
           Not(Contains(Property(
               "data", &ash::PickerSearchResult::data,
               VariantWith<ash::PickerSearchResult::BrowsingHistoryData>(Field(
                   "url", &ash::PickerSearchResult::BrowsingHistoryData::url,
                   GURL("http://foo.com/bookmarks"))))))))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(
      u"foo", /*category=*/ash::PickerCategory::kBrowsingHistory,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&mock_search_callback)));

  ASSERT_TRUE(test_done.Wait());
}

TEST_F(PickerClientImplTest, StartCrosSearchFilteredByBookmarks) {
  ash::PickerController controller;
  LoginState login = LogInAsFakeUser();
  PickerClientImpl client(&controller, login.user_manager);
  AddSearchToHistory(login.profile, GURL("http://foo.com/history"));
  AddBookmarks(login.profile, u"Foobaz", GURL("http://foo.com/bookmarks"));
  base::test::TestFuture<void> test_done;

  NiceMock<MockSearchResultsCallback> mock_search_callback;
  EXPECT_CALL(mock_search_callback, Call(_, _)).Times(AnyNumber());
  EXPECT_CALL(
      mock_search_callback,
      Call(ash::AppListSearchResultType::kOmnibox,
           Not(Contains(Property(
               "data", &ash::PickerSearchResult::data,
               VariantWith<ash::PickerSearchResult::BrowsingHistoryData>(Field(
                   "url", &ash::PickerSearchResult::BrowsingHistoryData::url,
                   GURL("http://foo.com/history"))))))))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(
      u"foo", /*category=*/ash::PickerCategory::kBookmarks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&mock_search_callback)));

  ASSERT_TRUE(test_done.Wait());
}

// TODO: b/325540366 - Add PickerClientImpl tests.

}  // namespace

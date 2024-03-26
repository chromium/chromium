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
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/user_manager/fake_user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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

class PickerClientImplTest : public BrowserWithTestWindowTest {
 public:
  PickerClientImplTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    BrowserWithTestWindowTest::SetUp();
  }
  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_shared_url_loader_factory_;
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = profile_manager()->CreateTestingProfile(
        profile_name, GetTestingFactories(), /*is_main_profile=*/false,
        test_shared_url_loader_factory_);
    OnUserProfileCreated(profile_name, profile);
    return profile;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        {HistoryServiceFactory::GetInstance(),
         base::BindRepeating(&BuildTestHistoryService, temp_dir_.GetPath())},
        {BookmarkModelFactory::GetInstance(),
         BookmarkModelFactory::GetDefaultFactory()},
        {TemplateURLServiceFactory::GetInstance(),
         base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}};
  }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(PickerClientImplTest, GetsSharedURLLoaderFactory) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());

  EXPECT_EQ(client.GetSharedURLLoaderFactory(), GetSharedURLLoaderFactory());
}

TEST_F(PickerClientImplTest, StartCrosSearch) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  AddSearchToHistory(profile(), GURL("http://foo.com/history"));
  AddBookmarks(profile(), u"Foobaz", GURL("http://foo.com/bookmarks"));
  AddTab(browser(), GURL("http://foo.com/tab"));
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
                   VariantWith<ash::PickerSearchResult::BrowsingHistoryData>(
                       Field("url",
                             &ash::PickerSearchResult::BrowsingHistoryData::url,
                             GURL("http://foo.com/tab")))),
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
  PickerClientImpl client(&controller, user_manager());
  AddSearchToHistory(profile(), GURL("http://foo.com/history"));
  AddBookmarks(profile(), u"Foobaz", GURL("http://foo.com/bookmarks"));
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
  PickerClientImpl client(&controller, user_manager());
  AddSearchToHistory(profile(), GURL("http://foo.com/history"));
  AddBookmarks(profile(), u"Foobaz", GURL("http://foo.com/bookmarks"));
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

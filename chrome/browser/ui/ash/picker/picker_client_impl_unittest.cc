// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/picker/picker_controller.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/search/test/test_ranker_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/fileapi/test/fake_recent_source.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/test/ax_event_counter.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

namespace fmp = extensions::api::file_manager_private;

class TestFaviconService : public favicon::MockFaviconService {
 public:
  TestFaviconService() = default;
  TestFaviconService(const TestFaviconService&) = delete;
  TestFaviconService& operator=(const TestFaviconService&) = delete;
  ~TestFaviconService() override = default;

  // favicon::FaviconService:
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    page_url_ = page_url;
    std::move(callback).Run(favicon_base::FaviconImageResult());
    return {};
  }

  GURL page_url_;
};

bool CreateTestFile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::WriteFile(path, "test_file")) {
    return false;
  }
  return true;
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(profile->GetPath()));
  return std::move(service);
}

struct Volume {
  fmp::VolumeType type;
  std::vector<ash::RecentFile> files;
};

std::unique_ptr<KeyedService> BuildTestRecentModelFactory(
    std::vector<Volume> volumes,
    content::BrowserContext* /*context*/) {
  std::vector<std::unique_ptr<ash::RecentSource>> sources;
  for (Volume& volume : volumes) {
    auto source = std::make_unique<ash::FakeRecentSource>(volume.type);
    source->AddProducer(std::make_unique<ash::FileProducer>(
        /*lag=*/base::Milliseconds(0), std::move(volume.files)));
    sources.push_back(std::move(source));
  }
  return ash::RecentModel::CreateForTest(std::move(sources));
}

std::unique_ptr<KeyedService> BuildTestDriveIntegrationService(
    std::unique_ptr<drive::FakeDriveFsHelper>& fake_drivefs_helper,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mount_path = profile->GetPath().Append("drivefs");
  static_cast<ash::disks::FakeDiskMountManager*>(
      ash::disks::DiskMountManager::GetInstance())
      ->RegisterMountPointForNetworkStorageScheme("drivefs",
                                                  mount_path.value());
  fake_drivefs_helper =
      std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
  auto service = std::make_unique<drive::DriveIntegrationService>(
      profile, "drivefs", mount_path,
      fake_drivefs_helper->CreateFakeDriveFsListenerFactory());

  // Wait until the DriveIntegrationService is initialized.
  while (!service->IsMounted() || !service->GetDriveFsInterface()) {
    base::RunLoop().RunUntilIdle();
  }
  return service;
}

void AddSearchToHistory(TestingProfile* profile,
                        GURL url,
                        base::Time last_visit = base::Time::Now()) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->AddPageWithDetails(url, /*title=*/u"", /*visit_count=*/1,
                              /*typed_count=*/1,
                              /*last_visit=*/last_visit,
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

ash::RecentFile CreateRecentFile(const base::FilePath& file_path,
                                 storage::FileSystemType type,
                                 base::Time last_modified = base::Time::Now()) {
  CreateTestFile(file_path);
  return ash::RecentFile(storage::FileSystemURL::CreateForTest(
                             blink::StorageKey(), type, file_path),
                         last_modified);
}

void SetRecentFiles(TestingProfile* profile, std::vector<Volume> volumes) {
  ash::RecentModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile,
      base::BindRepeating(BuildTestRecentModelFactory, std::move(volumes)));
}

class PickerClientImplTest : public BrowserWithTestWindowTest {
 public:
  PickerClientImplTest() = default;

  void SetUp() override {
    ash::CrosDisksClient::InitializeFake();
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager());

    BrowserWithTestWindowTest::SetUp();
  }
  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();

    ash::disks::DiskMountManager::Shutdown();
    ash::CrosDisksClient::Shutdown();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_shared_url_loader_factory_;
  }

  drivefs::FakeDriveFs& GetFakeDriveFs() {
    return fake_drivefs_helper_->fake_drivefs();
  }

  // Creates a user and profile for a given `email`. Note that this class will
  // keep the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& email) {
    LogIn(email);
    return CreateProfile(email);
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
        TestingProfile::TestingFactory{
            HistoryServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestHistoryService)},
        TestingProfile::TestingFactory{
            BookmarkModelFactory::GetInstance(),
            BookmarkModelFactory::GetDefaultFactory()},
        TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
        TestingProfile::TestingFactory{
            ash::RecentModelFactory::GetInstance(),
            base::BindRepeating(&BuildTestRecentModelFactory,
                                std::vector<Volume>{})},
        TestingProfile::TestingFactory{
            drive::DriveIntegrationServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestDriveIntegrationService,
                                std::ref(fake_drivefs_helper_))},
        TestingProfile::TestingFactory{
            ash::input_method::EditorMediatorFactory::GetInstance(),
            base::BindRepeating(
                &ash::input_method::EditorMediatorFactory::BuildInstanceFor)}};
  }

  void LogIn(const std::string& email) override {
    // DriveFS needs the account to have an ID.
    const AccountId account_id = AccountId::FromUserEmailGaiaId(email, email);
    user_manager()->AddUser(account_id);
    ash_test_helper()->test_session_controller_client()->AddUserSession(email);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

  void SwitchActiveUser(const std::string& email) override {
    user_manager()->SwitchActiveUser(
        AccountId::FromUserEmailGaiaId(email, email));
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
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

  auto ranker_manager =
      std::make_unique<app_list::TestRankerManager>(profile());
  ranker_manager->SetBestMatchString(u"tab");
  client.set_ranker_manager_for_test(std::move(ranker_manager));

  base::MockCallback<PickerClientImpl::CrosSearchResultsCallback>
      mock_search_callback;
  EXPECT_CALL(mock_search_callback, Run(_, _)).Times(AnyNumber());
  EXPECT_CALL(
      mock_search_callback,
      Run(ash::AppListSearchResultType::kOmnibox,
          IsSupersetOf({
              VariantWith<ash::PickerBrowsingHistoryResult>(AllOf(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("http://foo.com/history")),
                  Field("best_match",
                        &ash::PickerBrowsingHistoryResult::best_match, false))),
              VariantWith<ash::PickerBrowsingHistoryResult>(AllOf(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("http://foo.com/tab")),
                  Field("best_match",
                        &ash::PickerBrowsingHistoryResult::best_match, true))),

              VariantWith<ash::PickerBrowsingHistoryResult>(AllOf(
                  Field("title", &ash::PickerBrowsingHistoryResult::title,
                        u"Foobaz"),
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("http://foo.com/bookmarks")),
                  Field("best_match",
                        &ash::PickerBrowsingHistoryResult::best_match, false))),
          })))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(u"foo", /*category=*/std::nullopt,
                         mock_search_callback.Get());

  ASSERT_TRUE(test_done.Wait());
}

TEST_F(PickerClientImplTest, IgnoresWhatYouTypedResults) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<void> test_done;

  base::MockCallback<PickerClientImpl::CrosSearchResultsCallback>
      mock_search_callback;
  EXPECT_CALL(mock_search_callback, Run(_, _)).Times(AnyNumber());
  EXPECT_CALL(mock_search_callback,
              Run(ash::AppListSearchResultType::kOmnibox, IsEmpty()))
      .WillOnce([&]() { test_done.SetValue(); });

  client.StartCrosSearch(u"a.com", /*category=*/std::nullopt,
                         mock_search_callback.Get());

  ASSERT_TRUE(test_done.Wait());
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesWithNoFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;

  client.GetRecentLocalFileResults(
      /*max_files=*/100, /*now_delta=*/base::Days(30), future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesReturnsOnlyLocalFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  const base::FilePath mount_path = GetFakeDriveFs().mount_path();
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDownloads,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("local.png"),
                                       storage::kFileSystemTypeLocal),
                  },
          },
          Volume{
              .type = fmp::VolumeType::kDrive,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("drive.png"),
                                       storage::kFileSystemTypeDriveFs),
                  },
          },
      });

  client.GetRecentLocalFileResults(
      /*max_files=*/100, /*now_delta=*/base::Days(30), future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(VariantWith<ash::PickerLocalFileResult>(
          Field("title", &ash::PickerLocalFileResult::title, u"local.png"))));
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesDoesNotReturnOldFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDownloads,
              .files =
                  {
                      CreateRecentFile(
                          GetFakeDriveFs().mount_path().AppendASCII("old.png"),
                          storage::kFileSystemTypeLocal,
                          base::Time::Now() - base::Days(31)),
                  },
          },
      });

  client.GetRecentLocalFileResults(
      /*max_files=*/100, /*now_delta=*/base::Days(30), future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesWithNoFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;

  client.GetRecentDriveFileResults(/*max_files=*/100, future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesReturnsOnlyDriveFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  const base::FilePath mount_path = GetFakeDriveFs().mount_path();
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDownloads,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("local.png"),
                                       storage::kFileSystemTypeLocal),
                  },
          },
          Volume{
              .type = fmp::VolumeType::kDrive,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("drive.png"),
                                       storage::kFileSystemTypeDriveFs),
                  },
          },
      });

  client.GetRecentDriveFileResults(/*max_files=*/100, future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(VariantWith<ash::PickerDriveFileResult>(AllOf(
          Field("title", &ash::PickerDriveFileResult::title, u"drive.png"),
          Field("url", &ash::PickerDriveFileResult::url,
                GURL("https://file_alternate_link/drive.png"))))));
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesDoesNotReturnOldFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDrive,
              .files =
                  {
                      CreateRecentFile(
                          GetFakeDriveFs().mount_path().AppendASCII("old.png"),
                          storage::kFileSystemTypeDriveFs,
                          base::Time::Now() - base::Days(31)),
                  },
          },
      });

  client.GetRecentLocalFileResults(
      /*max_files=*/100, /*now_delta=*/base::Days(30), future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesTruncates) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  const base::FilePath mount_path = GetFakeDriveFs().mount_path();
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDownloads,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("1.jpg"),
                                       storage::kFileSystemTypeLocal),
                      CreateRecentFile(mount_path.AppendASCII("2.jpg"),
                                       storage::kFileSystemTypeLocal),
                  },
          },
      });

  client.GetRecentLocalFileResults(
      /*max_files=*/1, /*now_delta=*/base::Days(30), future.GetCallback());

  EXPECT_THAT(future.Get(), SizeIs(1));
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesTruncates) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  const base::FilePath mount_path = GetFakeDriveFs().mount_path();
  SetRecentFiles(
      profile(),
      {
          Volume{
              .type = fmp::VolumeType::kDrive,
              .files =
                  {
                      CreateRecentFile(mount_path.AppendASCII("1"),
                                       storage::kFileSystemTypeDriveFs),
                      CreateRecentFile(mount_path.AppendASCII("2"),
                                       storage::kFileSystemTypeDriveFs),
                  },
          },
      });

  client.GetRecentDriveFileResults(/*max_files=*/1, future.GetCallback());

  EXPECT_THAT(future.Get(), SizeIs(1));
}

TEST_F(PickerClientImplTest, GetSuggestedLinkResultsReturnsLinks) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const base::Time now = base::Time::Now();
  AddSearchToHistory(profile(), GURL("http://a.com/history"),
                     now - base::Seconds(1));
  AddSearchToHistory(profile(), GURL("http://b.com/history"), now);
  TestFaviconService favicon_service;
  client.get_link_suggester_for_test()->set_favicon_service_for_test(
      &favicon_service);

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedLinkResults(100u, future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::PickerBrowsingHistoryResult>(Field(
                              "url", &ash::PickerBrowsingHistoryResult::url,
                              GURL("http://b.com/history"))),
                          VariantWith<ash::PickerBrowsingHistoryResult>(Field(
                              "url", &ash::PickerBrowsingHistoryResult::url,
                              GURL("http://a.com/history")))));
  EXPECT_EQ(favicon_service.page_url_, GURL("http://a.com/history"));
}

TEST_F(PickerClientImplTest, GetSuggestedLinkResultsAreTruncatedToMostRecent) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const base::Time now = base::Time::Now();
  AddSearchToHistory(profile(), GURL("http://a.com/history"),
                     now - base::Seconds(1));
  AddSearchToHistory(profile(), GURL("http://b.com/history"), now);
  TestFaviconService favicon_service;
  client.get_link_suggester_for_test()->set_favicon_service_for_test(
      &favicon_service);

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedLinkResults(1u, future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::PickerBrowsingHistoryResult>(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("http://b.com/history")))));
  EXPECT_EQ(favicon_service.page_url_, GURL("http://b.com/history"));
}

TEST_F(PickerClientImplTest,
       GetSuggestedLinkResultsFiltersOutPersonalizedLinks) {
  base::test::ScopedFeatureList features(ash::features::kPickerFilterLinks);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const base::Time now = base::Time::Now();
  AddSearchToHistory(profile(),
                     GURL("https://mail.google.com/mail/u/0/#inbox/aaa"), now);
  AddSearchToHistory(profile(),
                     GURL("https://mail.google.com/chat/u/0/#chat/aaa"), now);
  AddSearchToHistory(profile(), GURL("https://mail.google.com"), now);
  TestFaviconService favicon_service;
  client.get_link_suggester_for_test()->set_favicon_service_for_test(
      &favicon_service);

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedLinkResults(100u, future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::PickerBrowsingHistoryResult>(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("https://mail.google.com")))));
}

TEST_F(PickerClientImplTest,
       SearchAfterSwitchingActiveUserReturnsResultsFromNewUser) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  TestingProfile* secondary_profile = CreateMultiUserProfile("secondary@test");
  AddSearchToHistory(profile(), GURL("https://foo.com/primary"));
  AddSearchToHistory(secondary_profile, GURL("https://foo.com/secondary"));
  client.StartCrosSearch(u"foo", /*category=*/std::nullopt, base::DoNothing());
  SwitchActiveUser("secondary@test");

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> result_future;
  client.StartCrosSearch(
      u"foo", /*category=*/std::nullopt,
      base::BindLambdaForTesting(
          [&result_future](ash::AppListSearchResultType result_type,
                           std::vector<ash::PickerSearchResult> results) {
            if (result_type == ash::AppListSearchResultType::kOmnibox) {
              result_future.SetValue(std::move(results));
            }
          }));

  const auto& results = result_future.Get();
  ASSERT_THAT(results, Contains(VariantWith<ash::PickerBrowsingHistoryResult>(
                           Field("url", &ash::PickerBrowsingHistoryResult::url,
                                 GURL("https://foo.com/secondary")))));
  ASSERT_THAT(results,
              Not(Contains(VariantWith<ash::PickerBrowsingHistoryResult>(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("https://foo.com/primary"))))));
}

TEST_F(PickerClientImplTest,
       SearchCategoryAfterSwitchingActiveUserReturnsResultsFromNewUser) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  TestingProfile* secondary_profile = CreateMultiUserProfile("secondary@test");
  AddSearchToHistory(profile(), GURL("https://foo.com/primary"));
  AddSearchToHistory(secondary_profile, GURL("https://foo.com/secondary"));
  client.StartCrosSearch(u"foo", ash::PickerCategory::kLinks,
                         base::DoNothing());
  SwitchActiveUser("secondary@test");

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> result_future;
  client.StartCrosSearch(
      u"foo", ash::PickerCategory::kLinks,
      base::BindLambdaForTesting(
          [&result_future](ash::AppListSearchResultType result_type,
                           std::vector<ash::PickerSearchResult> results) {
            if (result_type == ash::AppListSearchResultType::kOmnibox) {
              result_future.SetValue(std::move(results));
            }
          }));

  const auto& results = result_future.Get();
  ASSERT_THAT(results, Contains(VariantWith<ash::PickerBrowsingHistoryResult>(
                           Field("url", &ash::PickerBrowsingHistoryResult::url,
                                 GURL("https://foo.com/secondary")))));
  ASSERT_THAT(results,
              Not(Contains(VariantWith<ash::PickerBrowsingHistoryResult>(
                  Field("url", &ash::PickerBrowsingHistoryResult::url,
                        GURL("https://foo.com/primary"))))));
}

class PickerClientImplEditorTest : public PickerClientImplTest {
 public:
  ash::input_method::EditorMediator& GetEditorMediator(Profile* profile) {
    return *ash::input_method::EditorMediatorFactory::GetForProfile(profile);
  }

  ui::InputMethod& ime() { return ime_; }

 protected:
  void SetUp() override {
    PickerClientImplTest::SetUp();

    ash::IMEBridge::Get()->SetInputContextHandler(&ime_);
  }

  void TearDown() override {
    PickerClientImplTest::TearDown();

    ash::IMEBridge::Get()->SetInputContextHandler(nullptr);
  }

 private:
  ash::InputMethodAsh ime_{nullptr};
};

TEST_F(PickerClientImplEditorTest,
       IsEligibleForEditorReturnsFalseIfEditorDisabled) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kHardBlocked);

  EXPECT_FALSE(client.IsEligibleForEditor());
}

TEST_F(PickerClientImplEditorTest,
       IsEligibleForEditorReturnsFalseIfHardBlocked) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kHardBlocked);

  EXPECT_FALSE(client.IsEligibleForEditor());
}

TEST_F(PickerClientImplEditorTest,
       IsEligibleForEditorReturnsTrueIfSoftBlocked) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kSoftBlocked);

  EXPECT_TRUE(client.IsEligibleForEditor());
}

TEST_F(PickerClientImplEditorTest,
       CacheEditorContextReturnsNullCallbackWhenEditorFlagDisabled) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kHardBlocked);

  EXPECT_TRUE(client.CacheEditorContext().is_null());
}

TEST_F(PickerClientImplEditorTest,
       CacheEditorContextReturnsNullCallbackWhenBlocked) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kSoftBlocked);

  EXPECT_TRUE(client.CacheEditorContext().is_null());
}

TEST_F(PickerClientImplEditorTest,
       CacheEditorContextReturnsCallbackWhenNotBlocked) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kConsentNeeded);

  EXPECT_FALSE(client.CacheEditorContext().is_null());
}

TEST_F(PickerClientImplEditorTest, CacheEditorContextCachesCaretBounds) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kConsentNeeded);
  ui::FakeTextInputClient text_input_client(
      &ime(), {
                  .type = ui::TEXT_INPUT_TYPE_TEXT,
                  .caret_bounds = gfx::Rect(1, 2, 3, 4),
              });
  text_input_client.Focus();

  client.CacheEditorContext();

  EXPECT_EQ(GetEditorMediator(profile())
                .mako_bubble_coordinator_for_testing()
                .context_caret_bounds_for_testing(),
            gfx::Rect(1, 2, 3, 4));
}

TEST_F(PickerClientImplEditorTest, GetSuggestedEditorResults) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kRewrite);
  ui::FakeTextInputClient text_input_client(&ime(),
                                            {.type = ui::TEXT_INPUT_TYPE_TEXT});
  text_input_client.Focus();

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedEditorResults(future.GetCallback());

  EXPECT_TRUE(future.Wait());
  // TODO: b/331286774 - Add expectation for the suggested editor results once
  // EditorServiceConnector is injectable.
}

TEST_F(PickerClientImplEditorTest,
       GetSuggestedEditorResultsReturnsNothingWhenBlocked) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  GetEditorMediator(profile()).OverrideEditorModeForTesting(
      ash::input_method::EditorMode::kSoftBlocked);
  ui::FakeTextInputClient text_input_client(&ime(),
                                            {.type = ui::TEXT_INPUT_TYPE_TEXT});
  text_input_client.Focus();

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedEditorResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplEditorTest, AnnounceSendsLiveRegionChanges) {
  base::test::ScopedFeatureList features(chromeos::features::kOrcaDogfood);
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  client.Announce(u"hello");

  counter.WaitForEvent(ax::mojom::Event::kLiveRegionChanged);
}

TEST_F(PickerClientImplEditorTest, LauncherSearchProviderTypesAll) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const int types =
      client.LauncherSearchProviderTypes(/*bookmarks=*/true, /*history=*/true,
                                         /*open_tabs=*/true);

  EXPECT_TRUE(types & AutocompleteProvider::TYPE_BOOKMARK);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_FUZZY);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_QUICK);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_URL);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_OPEN_TAB);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_SEARCH);
}

TEST_F(PickerClientImplEditorTest, LauncherSearchProviderTypesBookmarks) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const int types =
      client.LauncherSearchProviderTypes(/*bookmarks=*/true, /*history=*/false,
                                         /*open_tabs=*/false);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_BOOKMARK);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_FUZZY);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_QUICK);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_URL);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_OPEN_TAB);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_SEARCH);
}

TEST_F(PickerClientImplEditorTest, LauncherSearchProviderTypesHistory) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const int types =
      client.LauncherSearchProviderTypes(/*bookmarks=*/false, /*history=*/true,
                                         /*open_tabs=*/false);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_BOOKMARK);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_FUZZY);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_QUICK);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_HISTORY_URL);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_OPEN_TAB);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_SEARCH);
}

TEST_F(PickerClientImplEditorTest, LauncherSearchProviderTypesOpenTab) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  const int types =
      client.LauncherSearchProviderTypes(/*bookmarks=*/false, /*history=*/false,
                                         /*open_tabs=*/true);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_BOOKMARK);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_FUZZY);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_QUICK);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_HISTORY_URL);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_OPEN_TAB);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_SEARCH);
}

// TODO: b/325540366 - Add PickerClientImpl tests.

}  // namespace

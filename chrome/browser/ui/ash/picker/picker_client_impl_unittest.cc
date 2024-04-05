// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "ash/picker/picker_controller.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/fileapi/test/fake_recent_source.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_utils.h"
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
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockSearchResultsCallback =
    testing::MockFunction<PickerClientImpl::CrosSearchResultsCallback>;

bool CreateTestFile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::WriteFile(path, "test_file")) {
    return false;
  }
  return true;
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    base::FilePath profile_path,
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(profile_path));
  return std::move(service);
}

std::unique_ptr<KeyedService> BuildTestRecentModelFactory(
    std::vector<ash::RecentFile> files,
    content::BrowserContext* context) {
  const size_t max_files = files.size();

  auto source = std::make_unique<ash::FakeRecentSource>();
  source->AddProducer(std::make_unique<ash::FileProducer>(
      /*lag=*/base::Milliseconds(0), std::move(files)));

  std::vector<std::unique_ptr<ash::RecentSource>> sources;
  sources.push_back(std::move(source));
  return ash::RecentModel::CreateForTest(std::move(sources), max_files);
}

std::unique_ptr<KeyedService> BuildTestDriveIntegrationService(
    const base::FilePath& profile_path,
    std::unique_ptr<drive::FakeDriveFsHelper>& fake_drivefs_helper,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mount_path = profile_path.Append("drivefs");
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

ash::RecentFile CreateRecentFile(const base::FilePath& path,
                                 storage::FileSystemType type,
                                 base::Time last_modified = base::Time::Now()) {
  storage::FileSystemURL url =
      storage::FileSystemURL::CreateForTest(blink::StorageKey(), type, path);
  return ash::RecentFile(url, last_modified);
}

void SetRecentFiles(TestingProfile* profile,
                    std::vector<ash::RecentFile> files) {
  ash::RecentModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile,
      base::BindRepeating(BuildTestRecentModelFactory, std::move(files)));
}

drivefs::FakeMetadata CreateFakeDriveFsMetadata(const base::FilePath& path) {
  drivefs::FakeMetadata metadata;
  metadata.path = path;
  return metadata;
}

class PickerClientImplTest : public BrowserWithTestWindowTest {
 public:
  PickerClientImplTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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
         base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
        {ash::RecentModelFactory::GetInstance(),
         base::BindRepeating(&BuildTestRecentModelFactory,
                             std::vector<ash::RecentFile>{})},
        {drive::DriveIntegrationServiceFactory::GetInstance(),
         base::BindRepeating(&BuildTestDriveIntegrationService,
                             temp_dir_.GetPath(),
                             std::ref(fake_drivefs_helper_))}};
  }

  void LogIn(const std::string& email) override {
    // DriveFS needs the account to have an ID.
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(email, "test gaia");
    user_manager()->AddUser(account_id);
    ash_test_helper()->test_session_controller_client()->AddUserSession(email);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

 private:
  base::ScopedTempDir temp_dir_;
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

TEST_F(PickerClientImplTest, GetRecentLocalFilesWithNoFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;

  client.GetRecentLocalFileResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesReturnsFilteredFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(profile(),
                 {
                     CreateRecentFile(base::FilePath("aaa.jpg"),
                                      storage::kFileSystemTypeLocal),
                     CreateRecentFile(base::FilePath("bbb.mp4"),
                                      storage::kFileSystemTypeLocal),
                     CreateRecentFile(base::FilePath("ccc.png"),
                                      storage::kFileSystemTypeLocal),
                     CreateRecentFile(base::FilePath("ddd.png"),
                                      storage::kFileSystemTypeDriveFs),
                 });

  client.GetRecentLocalFileResults(future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(
          Property(
              "data", &ash::PickerSearchResult::data,
              VariantWith<ash::PickerSearchResult::LocalFileData>(AllOf(
                  Field("title", &ash::PickerSearchResult::LocalFileData::title,
                        u"aaa.jpg"),
                  Field("file_path",
                        &ash::PickerSearchResult::LocalFileData::file_path,
                        base::FilePath("aaa.jpg"))))),
          Property(
              "data", &ash::PickerSearchResult::data,
              VariantWith<ash::PickerSearchResult::LocalFileData>(AllOf(
                  Field("title", &ash::PickerSearchResult::LocalFileData::title,
                        u"ccc.png"),
                  Field("file_path",
                        &ash::PickerSearchResult::LocalFileData::file_path,
                        base::FilePath("ccc.png")))))));
}

TEST_F(PickerClientImplTest, GetRecentLocalFilesDoesNotReturnOldFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(profile(),
                 {
                     CreateRecentFile(base::FilePath("abc.jpg"),
                                      storage::kFileSystemTypeLocal,
                                      base::Time::Now() - base::Days(31)),
                 });

  client.GetRecentDriveFileResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesWithNoFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;

  client.GetRecentLocalFileResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesReturnsDriveFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  const base::FilePath mount_path = GetFakeDriveFs().mount_path();
  ASSERT_TRUE(CreateTestFile(mount_path.AppendASCII("aaa.jpg")));
  ASSERT_TRUE(CreateTestFile(mount_path.AppendASCII("bbb.mp4")));
  ASSERT_TRUE(CreateTestFile(mount_path.AppendASCII("ccc.png")));
  GetFakeDriveFs().SetMetadata(
      CreateFakeDriveFsMetadata(base::FilePath("aaa.jpg")));
  GetFakeDriveFs().SetMetadata(
      CreateFakeDriveFsMetadata(base::FilePath("bbb.mp4")));
  GetFakeDriveFs().SetMetadata(
      CreateFakeDriveFsMetadata(base::FilePath("ccc.png")));
  SetRecentFiles(profile(),
                 {
                     CreateRecentFile(mount_path.AppendASCII("aaa.jpg"),
                                      storage::kFileSystemTypeDriveFs),
                     CreateRecentFile(mount_path.AppendASCII("bbb.mp4"),
                                      storage::kFileSystemTypeDriveFs),
                     CreateRecentFile(mount_path.AppendASCII("ccc.png"),
                                      storage::kFileSystemTypeLocal),
                 });

  client.GetRecentDriveFileResults(future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      UnorderedElementsAre(
          Property(
              "data", &ash::PickerSearchResult::data,
              VariantWith<ash::PickerSearchResult::DriveFileData>(AllOf(
                  Field("title", &ash::PickerSearchResult::DriveFileData::title,
                        u"aaa.jpg"),
                  Field("url", &ash::PickerSearchResult::DriveFileData::url,
                        GURL("https://file_alternate_link/aaa.jpg"))))),
          Property(
              "data", &ash::PickerSearchResult::data,
              VariantWith<ash::PickerSearchResult::DriveFileData>(AllOf(
                  Field("title", &ash::PickerSearchResult::DriveFileData::title,
                        u"bbb.mp4"),
                  Field("url", &ash::PickerSearchResult::DriveFileData::url,
                        GURL("https://file_alternate_link/bbb.mp4")))))));
}

TEST_F(PickerClientImplTest,
       GetRecentDriveFilesDoesNotReturnFilesWithNoMetadata) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(profile(),
                 {
                     CreateRecentFile(base::FilePath("abc.jpg"),
                                      storage::kFileSystemTypeDriveFs),
                 });

  client.GetRecentLocalFileResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetRecentDriveFilesDoesNotReturnOldFiles) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  SetRecentFiles(profile(),
                 {
                     CreateRecentFile(base::FilePath("abc.jpg"),
                                      storage::kFileSystemTypeDriveFs,
                                      base::Time::Now() - base::Days(31)),
                 });

  client.GetRecentLocalFileResults(future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClientImplTest, GetSuggestedLinkResultsReturnsLinks) {
  ash::PickerController controller;
  PickerClientImpl client(&controller, user_manager());
  AddSearchToHistory(profile(), GURL("http://foo.com/history"));

  base::test::TestFuture<std::vector<ash::PickerSearchResult>> future;
  client.GetSuggestedLinkResults(future.GetRepeatingCallback());

  EXPECT_THAT(
      future.Get(),
      IsSupersetOf({Property(
          "data", &ash::PickerSearchResult::data,
          VariantWith<ash::PickerSearchResult::BrowsingHistoryData>(
              Field("url", &ash::PickerSearchResult::BrowsingHistoryData::url,
                    GURL("http://foo.com/history"))))}));
}

// TODO: b/325540366 - Add PickerClientImpl tests.

}  // namespace

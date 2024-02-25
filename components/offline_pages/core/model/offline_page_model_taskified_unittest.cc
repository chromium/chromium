// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/clear_storage_task.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "components/offline_pages/core/model/persistent_page_consistency_check_task.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archive_publisher.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::A;
using testing::An;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Pointee;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;
using ClearStorageResult = ClearStorageTask::ClearStorageResult;

namespace {

const ClientId kTestClientId1(kDefaultNamespace, "1234");
const ClientId kTestClientId2(kDefaultNamespace, "5678");
const ClientId kTestUserRequestedClientId(kDownloadNamespace, "714");
const ClientId kTestBrowserActionsClientId(kBrowserActionsNamespace, "999");
const ClientId kTestLastNClientId(kLastNNamespace, "8989");
const int64_t kTestFileSize = 876543LL;
const std::u16string kTestTitle = u"a title";
const char kTestRequestOrigin[] = "abc.xyz";
const char kEmptyRequestOrigin[] = "";
const char kTestDigest[] = "test digest";
const int64_t kDownloadId = 42LL;

}  // namespace

class OfflinePageModelTaskifiedTest : public testing::Test,
                                      public OfflinePageModel::Observer,
                                      public OfflinePageTestArchiver::Observer {
 public:
  OfflinePageModelTaskifiedTest();
  ~OfflinePageModelTaskifiedTest() override;

  void SetUp() override;
  void TearDown() override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(const base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }
  void BuildStore();
  void BuildModel();
  void ResetModel();
  void ResetResults();

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(const OfflinePageItem& item) override;
  MOCK_METHOD3(ThumbnailAdded,
               void(OfflinePageModel* model,
                    int64_t offline_id,
                    const std::string& thumbnail));
  MOCK_METHOD3(FaviconAdded,
               void(OfflinePageModel* model,
                    int64_t offline_id,
                    const std::string& favicon));

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // Saves a page which will create the file and insert the corresponding
  // metadata into store. It relies on the implementation of
  // OfflinePageModel::SavePage.
  void SavePageWithCallback(const GURL& url,
                            const ClientId& client_id,
                            const GURL& original_url,
                            const std::string& request_origin,
                            std::unique_ptr<OfflinePageArchiver> archiver,
                            SavePageCallback callback);

  int64_t SavePageWithExpectedResult(
      const GURL& url,
      const ClientId& client_id,
      const GURL& original_url,
      const std::string& request_origin,
      std::unique_ptr<OfflinePageArchiver> archiver,
      SavePageResult expected_result);

  // Insert an offline page in to store, it does not rely on the model
  // implementation.
  void InsertPageIntoStore(const OfflinePageItem& offline_page);

  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(const GURL& url,
                                                         ArchiverResult result);

  void CheckTaskQueueIdle();

  void SetTestArchivePublisher(
      std::unique_ptr<OfflinePageTestArchivePublisher> publisher) {
    publisher_ = publisher.get();
    model()->archive_publisher_ = std::move(publisher);
  }

  // Getters for private fields.
  const base::Clock* clock() { return task_environment_.GetMockClock(); }
  OfflinePageModelTaskified* model() { return model_.get(); }
  OfflinePageMetadataStore* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }

  ArchiveManager* archive_manager() { return archive_manager_; }
  OfflinePageItemGenerator* page_generator() { return &generator_; }
  TaskQueue* task_queue() { return &model_->task_queue_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  const base::FilePath& temporary_dir_path() {
    return temporary_dir_.GetPath();
  }
  const base::FilePath& private_archive_dir_path() {
    return private_archive_dir_.GetPath();
  }
  const base::FilePath& public_archive_dir_path() {
    return public_archive_dir_.GetPath();
  }

  const base::FilePath& last_path_created_by_archiver() {
    return last_path_created_by_archiver_;
  }
  bool observer_add_page_called() { return observer_add_page_called_; }
  const OfflinePageItem& last_added_page() { return last_added_page_; }
  bool observer_delete_page_called() { return observer_delete_page_called_; }
  const OfflinePageItem& last_deleted_page() { return last_deleted_page_; }
  base::Time last_maintenance_tasks_schedule_time() {
    return model_->last_maintenance_tasks_schedule_time_;
  }
  OfflinePageTestArchivePublisher* publisher() { return publisher_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<OfflinePageModelTaskified> model_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  raw_ptr<ArchiveManager> archive_manager_;
  OfflinePageItemGenerator generator_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<OfflinePageTestArchivePublisher> publisher_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir private_archive_dir_;
  base::ScopedTempDir public_archive_dir_;

  TestScopedOfflineClockOverride clock_;

  base::FilePath last_path_created_by_archiver_;
  bool observer_add_page_called_;
  OfflinePageItem last_added_page_;
  bool observer_delete_page_called_;
  OfflinePageItem last_deleted_page_;
};

OfflinePageModelTaskifiedTest::OfflinePageModelTaskifiedTest()
    : clock_(task_environment_.GetMockClock()) {}

OfflinePageModelTaskifiedTest::~OfflinePageModelTaskifiedTest() = default;

void OfflinePageModelTaskifiedTest::SetUp() {
  BuildStore();
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(private_archive_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(public_archive_dir_.CreateUniqueTempDir());
  BuildModel();
  PumpLoop();
  CheckTaskQueueIdle();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void OfflinePageModelTaskifiedTest::TearDown() {
  SCOPED_TRACE("in TearDown");
  CheckTaskQueueIdle();
  store_test_util_.DeleteStore();
  if (temporary_dir_.IsValid()) {
    if (!temporary_dir_.Delete()) {
      DLOG(ERROR) << "temporary_dir_ not created";
    }
  }
  if (private_archive_dir_.IsValid()) {
    if (!private_archive_dir_.Delete()) {
      DLOG(ERROR) << "private_persistent_dir not created";
    }
  }
  if (public_archive_dir_.IsValid()) {
    if (!public_archive_dir_.Delete()) {
      DLOG(ERROR) << "public_archive_dir not created";
    }
  }
  archive_manager_ = nullptr;
  publisher_ = nullptr;
  model_->RemoveObserver(this);
  model_.reset();
  PumpLoop();
}

void OfflinePageModelTaskifiedTest::BuildStore() {
  store_test_util()->BuildStore();
}

void OfflinePageModelTaskifiedTest::BuildModel() {
  ASSERT_TRUE(store_test_util_.store());
  // Keep a copy of the system download manager stub to test against.
  auto archive_manager = std::make_unique<ArchiveManager>(
      temporary_dir_path(), private_archive_dir_path(),
      public_archive_dir_path(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  archive_manager_ = archive_manager.get();

  auto publisher = std::make_unique<OfflinePageTestArchivePublisher>(
      archive_manager.get(), kDownloadId);
  publisher_ = publisher.get();

  model_ = std::make_unique<OfflinePageModelTaskified>(
      store_test_util()->ReleaseStore(), std::move(archive_manager),
      std::move(publisher), base::SingleThreadTaskRunner::GetCurrentDefault());
  model_->AddObserver(this);
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  ResetResults();
}

void OfflinePageModelTaskifiedTest::ResetModel() {
  model_.reset();
  PumpLoop();
}

void OfflinePageModelTaskifiedTest::ResetResults() {
  last_path_created_by_archiver_.clear();
  observer_add_page_called_ = false;
  observer_delete_page_called_ = false;
}

void OfflinePageModelTaskifiedTest::OfflinePageModelLoaded(
    OfflinePageModel* model) {}

void OfflinePageModelTaskifiedTest::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {
  observer_add_page_called_ = true;
  last_added_page_ = added_page;
}

void OfflinePageModelTaskifiedTest::OfflinePageDeleted(
    const OfflinePageItem& item) {
  observer_delete_page_called_ = true;
  last_deleted_page_ = item;
}

void OfflinePageModelTaskifiedTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_path_created_by_archiver_ = file_path;
}

void OfflinePageModelTaskifiedTest::SavePageWithCallback(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    std::unique_ptr<OfflinePageArchiver> archiver,
    SavePageCallback callback) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  save_page_params.request_origin = request_origin;
  save_page_params.is_background = false;

  model()->SavePage(save_page_params, std::move(archiver), nullptr,
                    std::move(callback));
  PumpLoop();
}

int64_t OfflinePageModelTaskifiedTest::SavePageWithExpectedResult(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    std::unique_ptr<OfflinePageArchiver> archiver,
    SavePageResult expected_result) {
  int64_t offline_id = OfflinePageModel::kInvalidOfflineId;
  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(expected_result), A<int64_t>()))
      .WillOnce(SaveArg<1>(&offline_id));
  SavePageWithCallback(url, client_id, original_url, request_origin,
                       std::move(archiver), callback.Get());
  if (expected_result == SavePageResult::SUCCESS) {
    EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_id);
  }
  return offline_id;
}

void OfflinePageModelTaskifiedTest::InsertPageIntoStore(
    const OfflinePageItem& offline_page) {
  store_test_util()->InsertItem(offline_page);
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageModelTaskifiedTest::BuildArchiver(const GURL& url,
                                             ArchiverResult result) {
  return std::make_unique<OfflinePageTestArchiver>(
      this, url, result, kTestTitle, kTestFileSize, kTestDigest,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void OfflinePageModelTaskifiedTest::CheckTaskQueueIdle() {
  EXPECT_FALSE(task_queue()->HasPendingTasks());
  EXPECT_FALSE(task_queue()->HasRunningTask());
}

// Tests saving successfully a non-user-requested offline page.
TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessful) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);

  const GURL kTestUrl2("http://other.page.com");
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl2, kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_path_created_by_archiver(), saved_page_ptr->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr->file_size);
  EXPECT_EQ(0, saved_page_ptr->access_count);
  EXPECT_EQ(0, saved_page_ptr->flags);
  EXPECT_EQ(kTestTitle, saved_page_ptr->title);
  EXPECT_EQ(kTestUrl2, saved_page_ptr->original_url_if_different);
  EXPECT_EQ("", saved_page_ptr->request_origin);
  EXPECT_EQ(kTestDigest, saved_page_ptr->digest);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessfulWithSameOriginalUrl) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);

  // Pass the original URL same as the final URL.
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl, kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  // The original URL should be empty.
  EXPECT_TRUE(saved_page_ptr->original_url_if_different.is_empty());
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessfulWithRequestOrigin) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);

  const GURL kTestUrl2("http://other.page.com");
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl2, kTestRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_path_created_by_archiver(), saved_page_ptr->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr->file_size);
  EXPECT_EQ(0, saved_page_ptr->access_count);
  EXPECT_EQ(0, saved_page_ptr->flags);
  EXPECT_EQ(kTestTitle, saved_page_ptr->title);
  EXPECT_EQ(kTestUrl2, saved_page_ptr->original_url_if_different);
  EXPECT_EQ(kTestRequestOrigin, saved_page_ptr->request_origin);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverCancelled) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::ERROR_CANCELED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::CANCELLED);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverDeviceFull) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::ERROR_DEVICE_FULL);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::DEVICE_FULL);
}

TEST_F(OfflinePageModelTaskifiedTest,
       SavePageOfflineArchiverContentUnavailable) {
  const GURL kTestUrl("http://example.com");
  auto archiver =
      BuildArchiver(kTestUrl, ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver),
                             SavePageResult::CONTENT_UNAVAILABLE);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineCreationFailed) {
  const GURL kTestUrl("http://example.com");
  auto archiver =
      BuildArchiver(kTestUrl, ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver),
                             SavePageResult::ARCHIVE_CREATION_FAILED);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverReturnedWrongUrl) {
  auto archiver = BuildArchiver(GURL("http://other.random.url.com"),
                                ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithExpectedResult(
      GURL("http://example.com"), kTestClientId1, GURL("http://other.page.com"),
      kEmptyRequestOrigin, std::move(archiver), SavePageResult::INCORRECT_URL);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageLocalFileFailed) {
  SavePageWithExpectedResult(GURL("file:///foo"), kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::unique_ptr<OfflinePageTestArchiver>(),
                             SavePageResult::SKIPPED);
}

// This test case is for the scenario that there are two save page requests but
// the first one is slower during archive creation (set_delayed in the test
// case) so the second request will finish first.
// offline_id1 will be id of the first completed request.
// offline_id2 will be id of the second completed request.
TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverTwoPages) {
  ASSERT_EQ(kTestClientId1.name_space, kTestClientId2.name_space)
      << "Checks below assume both pages share the same namespace";

  int64_t offline_id1;
  int64_t offline_id2;

  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(SavePageResult::SUCCESS), A<int64_t>()))
      .Times(2)
      .WillOnce(SaveArg<1>(&offline_id1))
      .WillOnce(SaveArg<1>(&offline_id2));

  // delayed_archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called. Keeping the raw pointer because the
  // ownership is transferring to the model.
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  OfflinePageTestArchiver* delayed_archiver_ptr = archiver.get();
  delayed_archiver_ptr->set_delayed(true);
  SavePageWithCallback(kTestUrl, kTestClientId1, GURL(), kEmptyRequestOrigin,
                       std::move(archiver), callback.Get());

  // Request to save another page, with request origin.
  const GURL kTestUrl2("http://other.page.com");
  archiver = BuildArchiver(kTestUrl2, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithCallback(kTestUrl2, kTestClientId2, GURL(), kTestRequestOrigin,
                       std::move(archiver), callback.Get());

  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path1 = last_path_created_by_archiver();

  ResetResults();

  delayed_archiver_ptr->CompleteCreateArchive();
  // Pump loop so the first request can finish saving.
  PumpLoop();

  // Check that offline_id1 refers to the second save page request.
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path2 = last_path_created_by_archiver();

  auto saved_page_ptr1 = store_test_util()->GetPageByOfflineId(offline_id1);
  auto saved_page_ptr2 = store_test_util()->GetPageByOfflineId(offline_id2);
  ASSERT_TRUE(saved_page_ptr1);
  ASSERT_TRUE(saved_page_ptr2);

  EXPECT_EQ(kTestUrl2, saved_page_ptr1->url);
  EXPECT_EQ(kTestClientId2, saved_page_ptr1->client_id);
  EXPECT_EQ(saved_file_path1, saved_page_ptr1->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr1->file_size);
  EXPECT_EQ(kTestRequestOrigin, saved_page_ptr1->request_origin);

  EXPECT_EQ(GURL("http://example.com"), saved_page_ptr2->url);
  EXPECT_EQ(kTestClientId1, saved_page_ptr2->client_id);
  EXPECT_EQ(saved_file_path2, saved_page_ptr2->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr2->file_size);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOnBackground) {
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  OfflinePageTestArchiver* archiver_ptr = archiver.get();

  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = kTestUrl;
  save_page_params.client_id = kTestClientId1;
  save_page_params.original_url = GURL("http://other.page.com");
  save_page_params.is_background = true;

  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(SavePageResult::SUCCESS), A<int64_t>()));
  model()->SavePage(save_page_params, std::move(archiver), nullptr,
                    callback.Get());
  EXPECT_TRUE(archiver_ptr->create_archive_called());
  // |remove_popup_overlay| should be turned on on background mode.
  EXPECT_TRUE(archiver_ptr->create_archive_params().remove_popup_overlay);

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageWithNullArchiver) {
  SavePageWithExpectedResult(GURL("http://example.com"), kTestClientId1, GURL(),
                             kEmptyRequestOrigin, nullptr,
                             SavePageResult::CONTENT_UNAVAILABLE);
}

TEST_F(OfflinePageModelTaskifiedTest, AddPage) {
  // Creates a fresh page.
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  OfflinePageItem page = page_generator()->CreateItemWithTempFile();

  base::MockCallback<AddPageCallback> callback;
  EXPECT_CALL(callback, Run(An<AddPageResult>(), Eq(page.offline_id)));

  model()->AddPage(page, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_add_page_called());
  EXPECT_EQ(last_added_page(), page);
}

TEST_F(OfflinePageModelTaskifiedTest, MarkPageAccessed) {
  OfflinePageItem page = page_generator()->CreateItem();
  InsertPageIntoStore(page);

  model()->MarkPageAccessed(page.offline_id);
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();

  auto accessed_page_ptr =
      store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(accessed_page_ptr);
  EXPECT_EQ(1LL, accessed_page_ptr->access_count);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(page.client_id.name_space)),
      1);
}

TEST_F(OfflinePageModelTaskifiedTest, GetAllPagesWhenStoreEmpty) {
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty()));

  model()->GetAllPages(callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

// These newly added tests are testing the API instead of results, which
// should be covered in DeletePagesTaskTest.

TEST_F(OfflinePageModelTaskifiedTest, DeletePagesWithCriteria) {
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  page_generator()->SetNamespace(kDefaultNamespace);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  page1.system_download_id = kDownloadId;
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());

  base::MockCallback<DeletePageCallback> callback;
  EXPECT_CALL(callback, Run(A<DeletePageResult>()));
  CheckTaskQueueIdle();

  PageCriteria criteria;
  criteria.offline_ids = std::vector<int64_t>{page1.offline_id};
  model()->DeletePagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_delete_page_called());
  EXPECT_EQ(last_deleted_page().offline_id, page1.offline_id);
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(page1.system_download_id,
            publisher()->last_removed_id().download_id);
}

TEST_F(OfflinePageModelTaskifiedTest, DeletePagesByUrlPredicate) {
  const GURL kTestUrl("http://example.com");
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  page_generator()->SetUrl(GURL("http://other.page.com"));
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());

  base::MockCallback<DeletePageCallback> callback;
  EXPECT_CALL(callback, Run(testing::A<DeletePageResult>()));
  CheckTaskQueueIdle();

  UrlPredicate predicate = base::BindRepeating(
      [](const GURL& expected_url, const GURL& url) -> bool {
        return url == expected_url;
      },
      kTestUrl);

  model()->DeleteCachedPagesByURLPredicate(predicate, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_delete_page_called());
  EXPECT_EQ(last_deleted_page().offline_id, page1.offline_id);
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
}

TEST_F(OfflinePageModelTaskifiedTest, GetPageByOfflineId) {
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(GURL("http://example.com"));
  OfflinePageItem page = page_generator()->CreateItem();
  InsertPageIntoStore(page);

  base::MockCallback<SingleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(Pointee(Eq(page))));

  model()->GetPageByOfflineId(page.offline_id, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesWithCriteria_FinalUrl) {
  const GURL kTestUrl("http://example.com");
  const GURL kTestUrl2("http://other.page.com");
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2);
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  // Search by kTestUrl.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(ElementsAre(page1)));
  PageCriteria criteria;
  criteria.url = kTestUrl;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  criteria.url = kTestUrl2;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  PumpLoop();

  // Search by random url, which should return no pages.
  EXPECT_CALL(callback, Run(IsEmpty()));
  criteria.url = GURL("http://foo");
  model()->GetPagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest,
       GetPagesByUrl_FinalUrlWithFragmentStripped) {
  const GURL kTestUrl("http://example.com");
  const GURL kTestUrlWithFragment("http://example.com#frag");
  const GURL kTestUrl2("http://other.page.com");
  const GURL kTestUrl2WithFragment("http://other.page.com#frag");
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2WithFragment);
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  // Search by kTestUrlWithFragment.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(ElementsAre(page1)));
  PageCriteria criteria;
  criteria.url = kTestUrlWithFragment;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  criteria.url = kTestUrl2;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2WithFragment.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  criteria.url = kTestUrl2WithFragment;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesWithCriteria_AllUrls) {
  const GURL kTestUrl2("http://other.page.com");
  page_generator()->SetUrl(GURL("http://example.com"));
  page_generator()->SetOriginalUrl(kTestUrl2);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2);
  page_generator()->SetOriginalUrl(GURL());
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));
  PageCriteria criteria;
  criteria.url = kTestUrl2;
  model()->GetPagesWithCriteria(criteria, callback.Get());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, CanSaveURL) {
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("http://foo")));
  EXPECT_TRUE(OfflinePageModel::CanSaveURL(GURL("https://foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("file:///foo")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("data:image/png;base64,ab")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome://version")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("chrome-native://newtab/")));
  EXPECT_FALSE(OfflinePageModel::CanSaveURL(GURL("/invalid/url.mhtml")));
}

TEST_F(OfflinePageModelTaskifiedTest, GetOfflineIdsForClientId) {
  page_generator()->SetNamespace(kTestClientId1.name_space);
  page_generator()->SetId(kTestClientId1.id);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflineIdCallback> callback;
  EXPECT_CALL(callback,
              Run(UnorderedElementsAre(page1.offline_id, page2.offline_id)));

  model()->GetOfflineIdsForClientId(kTestClientId1, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CheckTempPagesSavedInCorrectDir \
  DISABLED_CheckTempPagesSavedInCorrectDir
#else
#define MAYBE_CheckTempPagesSavedInCorrectDir CheckTempPagesSavedInCorrectDir
#endif
TEST_F(OfflinePageModelTaskifiedTest, MAYBE_CheckTempPagesSavedInCorrectDir) {
  // Save a temporary page.
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t temporary_id = SavePageWithExpectedResult(
      kTestUrl, kTestLastNClientId, GURL(), kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  std::unique_ptr<OfflinePageItem> temporary_page =
      store_test_util()->GetPageByOfflineId(temporary_id);
  ASSERT_TRUE(temporary_page);

  EXPECT_TRUE(temporary_dir_path().IsParent(temporary_page->file_path));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CheckPersistenPagesSavedInCorrectDir \
  DISABLED_CheckPersistenPagesSavedInCorrectDir
#else
#define MAYBE_CheckPersistenPagesSavedInCorrectDir \
  CheckPersistenPagesSavedInCorrectDir
#endif
TEST_F(OfflinePageModelTaskifiedTest,
       MAYBE_CheckPersistenPagesSavedInCorrectDir) {
  // Save a persistent page that will be published to the public folder.
  const GURL kTestUrl("http://other.page.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t persistent_id = SavePageWithExpectedResult(
      kTestUrl, kTestUserRequestedClientId, GURL(), kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  std::unique_ptr<OfflinePageItem> persistent_page =
      store_test_util()->GetPageByOfflineId(persistent_id);
  ASSERT_TRUE(persistent_page);

  EXPECT_TRUE(public_archive_dir_path().IsParent(persistent_page->file_path));
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PublishPageFailure DISABLED_PublishPageFailure
#else
#define MAYBE_PublishPageFailure PublishPageFailure
#endif
TEST_F(OfflinePageModelTaskifiedTest, MAYBE_PublishPageFailure) {
  // Save a persistent page that will report failure to be copied to a public
  // dir.
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);

  // Expect that PublishArchive is called and force returning FILE_MOVE_FAILED.
  auto publisher = std::make_unique<OfflinePageTestArchivePublisher>(
      archive_manager(), kDownloadId);
  publisher->expect_publish_archive_called(true);
  publisher->set_archive_attempt_failure(true);
  SetTestArchivePublisher(std::move(publisher));

  SavePageWithExpectedResult(kTestUrl, kTestUserRequestedClientId, GURL(),
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::FILE_MOVE_FAILED);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CheckPublishInternalArchive DISABLED_CheckPublishInternalArchive
#else
#define MAYBE_CheckPublishInternalArchive CheckPublishInternalArchive
#endif
TEST_F(OfflinePageModelTaskifiedTest, MAYBE_CheckPublishInternalArchive) {
  // Save a persistent page into our internal directory that will not be
  // published. We use a "browser actions" page for this purpose.
  const GURL kTestUrl("http://other.page.com");
  std::unique_ptr<OfflinePageTestArchiver> test_archiver =
      BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t persistent_id = SavePageWithExpectedResult(
      kTestUrl, kTestBrowserActionsClientId, GURL(), kEmptyRequestOrigin,
      std::move(test_archiver), SavePageResult::SUCCESS);

  std::unique_ptr<OfflinePageItem> persistent_page =
      store_test_util()->GetPageByOfflineId(persistent_id);
  ASSERT_TRUE(persistent_page);

  // For a page in the browser actions namespace, it gets moved to the
  // a public downloads directory.
  EXPECT_TRUE(public_archive_dir_path().IsParent(persistent_page->file_path));

  // Publish the page from our internal store.
  base::MockCallback<PublishPageCallback> callback;
  EXPECT_CALL(callback, Run(A<const base::FilePath&>(), A<SavePageResult>()));

  model()->PublishInternalArchive(*persistent_page, callback.Get());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, ExtraActionTriggeredWhenSaveSuccess) {
  // After a save successfully saved, both RemovePagesWithSameUrlInSameNamespace
  // and PostClearCachedPagesTask will be triggered.
  // Add pages that have the same namespace and url directly into store, in
  // order to avoid triggering the removal.
  // The 'default' namespace has a limit of 1 per url.
  const GURL kTestUrl("http://example.com");
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  ResetResults();

  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_TRUE(observer_add_page_called());
  EXPECT_TRUE(observer_delete_page_called());
}

TEST_F(OfflinePageModelTaskifiedTest, GetArchiveDirectory) {
  base::FilePath temporary_dir =
      model()->GetArchiveDirectory(kDefaultNamespace);
  EXPECT_EQ(temporary_dir_path(), temporary_dir);
  base::FilePath persistent_dir =
      model()->GetArchiveDirectory(kDownloadNamespace);
  EXPECT_EQ(private_archive_dir_path(), persistent_dir);
}

TEST_F(OfflinePageModelTaskifiedTest, GetAllPages) {
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));
  model()->GetAllPages(callback.Get());
  PumpLoop();
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_StartupMaintenanceTaskExecuted \
  DISABLED_StartupMaintenanceTaskExecuted
#else
#define MAYBE_StartupMaintenanceTaskExecuted StartupMaintenanceTaskExecuted
#endif
TEST_F(OfflinePageModelTaskifiedTest, MAYBE_StartupMaintenanceTaskExecuted) {
  // Insert temporary pages
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  page_generator()->SetNamespace(kDefaultNamespace);
  // Page missing archive file in temporary directory.
  OfflinePageItem temp_page1 = page_generator()->CreateItem();
  // Page missing metadata entry in database since it's not inserted into store.
  OfflinePageItem temp_page2 = page_generator()->CreateItemWithTempFile();
  // Page in temporary namespace saved in persistent directory to simulate pages
  // saved in legacy directory.
  page_generator()->SetArchiveDirectory(private_archive_dir_path());
  OfflinePageItem temp_page3 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(temp_page1);
  InsertPageIntoStore(temp_page3);

  // Insert persistent pages.
  page_generator()->SetNamespace(kDownloadNamespace);
  // Page missing archive file in private directory.
  OfflinePageItem persistent_page1 = page_generator()->CreateItem();
  // Page missing metadata entry in database since it's not inserted into store.
  OfflinePageItem persistent_page2 = page_generator()->CreateItemWithTempFile();
  // Page in persistent namespace saved in private directory.
  OfflinePageItem persistent_page3 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(persistent_page1);
  InsertPageIntoStore(persistent_page3);

  PumpLoop();

  EXPECT_EQ(4LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(3UL,
            test_utils::GetFileCountInDirectory(private_archive_dir_path()));

  // Execute GetAllPages and move the clock forward to cover the delay, in order
  // to trigger StartupMaintenanceTask execution.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  model()->GetAllPages(callback.Get());
  FastForwardBy(OfflinePageModelTaskified::kMaintenanceTasksDelay +
                base::Milliseconds(1));

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(0UL, test_utils::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1UL,
            test_utils::GetFileCountInDirectory(private_archive_dir_path()));
}

TEST_F(OfflinePageModelTaskifiedTest, ClearStorage) {
  // The ClearStorage task should not be executed based on time delays after
  // launch (aka the model being built).
  FastForwardBy(base::Days(1));
  EXPECT_EQ(base::Time(), last_maintenance_tasks_schedule_time());

  // GetAllPages should schedule a delayed task that will eventually run
  // ClearStorage.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  model()->GetAllPages(callback.Get());
  PumpLoop();
  EXPECT_EQ(clock()->Now(), last_maintenance_tasks_schedule_time());
  base::Time last_scheduling_time = clock()->Now();

  // After the delay (plus 1 millisecond just in case) ClearStorage should be
  // enqueued and executed.
  const base::TimeDelta run_delay =
      OfflinePageModelTaskified::kMaintenanceTasksDelay + base::Milliseconds(1);
  FastForwardBy(run_delay);
  EXPECT_EQ(last_scheduling_time, last_maintenance_tasks_schedule_time());
  // Check that CleanupVisualsTask ran.

  // Calling GetAllPages after only half of the enforced interval between
  // ClearStorage runs should not schedule ClearStorage.
  // Note: The previous elapsed delay is discounted from the clock advance here.
  FastForwardBy(OfflinePageModelTaskified::kClearStorageInterval / 2 -
                run_delay);
  ASSERT_GT(clock()->Now(), last_scheduling_time);
  model()->GetAllPages(callback.Get());
  // And advance the delay too.
  FastForwardBy(run_delay);
  EXPECT_EQ(last_scheduling_time, last_maintenance_tasks_schedule_time());

  // Forwarding by the full interval (plus 1 second just in case) should allow
  // the task to be enqueued again.
  FastForwardBy(OfflinePageModelTaskified::kClearStorageInterval / 2 +
                base::Seconds(1));
  // Saving a page should also immediately enqueue the ClearStorage task.
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::SUCCESS);
  last_scheduling_time = clock()->Now();
  // Advance the delay again.
  FastForwardBy(run_delay);
  EXPECT_EQ(last_scheduling_time, last_maintenance_tasks_schedule_time());

}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PersistentPageConsistencyCheckExecuted \
  DISABLED_PersistentPageConsistencyCheckExecuted
#else
#define MAYBE_PersistentPageConsistencyCheckExecuted \
  PersistentPageConsistencyCheckExecuted
#endif
TEST_F(OfflinePageModelTaskifiedTest,
       MAYBE_PersistentPageConsistencyCheckExecuted) {
  // The PersistentPageConsistencyCheckTask should not be executed based on time
  // delays after launch (aka the model being built).
  FastForwardBy(base::Days(1));

  // GetAllPages should schedule a delayed task that will eventually run
  // PersistentPageConsistencyCheck.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  model()->GetAllPages(callback.Get());
  PumpLoop();

  // Add a persistent page with file.
  page_generator()->SetNamespace(kDownloadNamespace);
  page_generator()->SetArchiveDirectory(public_archive_dir_path());
  OfflinePageItem page = page_generator()->CreateItemWithTempFile();
  page.system_download_id = kDownloadId;
  InsertPageIntoStore(page);
  EXPECT_EQ(1UL,
            test_utils::GetFileCountInDirectory(public_archive_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());

  // After the delay (plus 1 millisecond just in case), the consistency check
  // should be enqueued and executed.
  const base::TimeDelta run_delay =
      OfflinePageModelTaskified::kMaintenanceTasksDelay + base::Milliseconds(1);
  FastForwardBy(run_delay);
  // But nothing should change.
  EXPECT_EQ(1UL,
            test_utils::GetFileCountInDirectory(public_archive_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());

  // Delete the file associated with |page|, so the next time when the
  // consistency check is executed, the page will be marked as hidden.
  base::DeleteFile(page.file_path);

  // Calling GetAllPages after only half of the enforced interval between
  // consistency check runs should not schedule the task.
  // Note: The previous elapsed delay is discounted from the clock advance here.
  FastForwardBy(OfflinePageModelTaskified::kClearStorageInterval / 2 -
                run_delay);
  model()->GetAllPages(callback.Get());
  // And advance the delay too.
  FastForwardBy(run_delay);

  // Forwarding by the full interval (plus 1 second just in case) should allow
  // the task to be enqueued again and call GetAllPages again to enqueue the
  // task.
  FastForwardBy(OfflinePageModelTaskified::kClearStorageInterval / 2 +
                base::Seconds(1));
  model()->GetAllPages(callback.Get());
  // And advance the delay too.
  FastForwardBy(run_delay);
  // Confirm persistent page consistency check is executed, and the page is
  // marked as missing file.
  EXPECT_EQ(0UL,
            test_utils::GetFileCountInDirectory(public_archive_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto actual_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(actual_page);
  EXPECT_NE(base::Time(), actual_page->file_missing_time);

  // Forwarding by a long time that is enough for the page with missing file to
  // get expired.
  FastForwardBy(base::Days(400));
  // Saving a page should also immediately enqueue the consistency check task.
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::SUCCESS);
  // Advance the delay to activate task execution.
  FastForwardBy(run_delay);
  // Confirm persistent page consistency check is executed, and the page is
  // deleted from database, also notified system download manager.
  EXPECT_EQ(0UL,
            test_utils::GetFileCountInDirectory(public_archive_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(page.system_download_id,
            publisher()->last_removed_id().download_id);
}

TEST_F(OfflinePageModelTaskifiedTest, MaintenanceTasksAreDisabled) {
  // The maintenance tasks should not be executed when disabled by tests.
  model()->DoNotRunMaintenanceTasksForTesting();

  // With that setting GetAllPages and saving a page should not schedule
  // maintenance tasks.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  model()->GetAllPages(callback.Get());
  const GURL kTestUrl("http://example.com");
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1,
                             GURL("http://other.page.com"), kEmptyRequestOrigin,
                             std::move(archiver), SavePageResult::SUCCESS);
  PumpLoop();
  EXPECT_EQ(base::Time(), last_maintenance_tasks_schedule_time());

  // Advance the clock considerably and confirm no runs happened.
  FastForwardBy(base::Days(1));
  EXPECT_EQ(base::Time(), last_maintenance_tasks_schedule_time());
}

TEST_F(OfflinePageModelTaskifiedTest, StoreAndCheckThumbnail) {
  // Store a thumbnail.
  OfflinePageVisuals visuals;
  visuals.offline_id = 1;
  visuals.expiration = clock()->Now();
  visuals.thumbnail = "abc123";
  model()->StoreThumbnail(visuals.offline_id, visuals.thumbnail);
  EXPECT_CALL(*this, ThumbnailAdded(_, visuals.offline_id, visuals.thumbnail));
  PumpLoop();

  // Check it exists
  std::optional<VisualsAvailability> availability;
  auto exists_callback = base::BindLambdaForTesting(
      [&](VisualsAvailability value) { availability = value; });
  model()->GetVisualsAvailability(visuals.offline_id, exists_callback);
  PumpLoop();
  EXPECT_TRUE(availability.value().has_thumbnail);
  EXPECT_FALSE(availability.value().has_favicon);

  // Obtain its data.
  std::unique_ptr<OfflinePageVisuals> result_visuals;
  auto data_callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        result_visuals = std::move(result);
      });
  model()->GetVisualsByOfflineId(visuals.offline_id, data_callback);
  PumpLoop();
  EXPECT_EQ(visuals.thumbnail, result_visuals->thumbnail);
}

TEST_F(OfflinePageModelTaskifiedTest, StoreAndCheckFavicon) {
  // Store a thumbnail.
  OfflinePageVisuals visuals;
  visuals.offline_id = 1;
  visuals.expiration = clock()->Now();
  visuals.favicon = "abc123";
  model()->StoreFavicon(visuals.offline_id, visuals.favicon);
  EXPECT_CALL(*this, FaviconAdded(_, visuals.offline_id, visuals.favicon));
  PumpLoop();

  // Check if it exists.
  std::optional<VisualsAvailability> availability;
  auto exists_callback = base::BindLambdaForTesting(
      [&](VisualsAvailability value) { availability = value; });
  model()->GetVisualsAvailability(visuals.offline_id, exists_callback);
  PumpLoop();
  EXPECT_FALSE(availability.value().has_thumbnail);
  EXPECT_TRUE(availability.value().has_favicon);

  // Obtain its data.
  std::unique_ptr<OfflinePageVisuals> result_visuals;
  auto data_callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageVisuals> result) {
        result_visuals = std::move(result);
      });
  model()->GetVisualsByOfflineId(visuals.offline_id, data_callback);
  PumpLoop();
  EXPECT_EQ(visuals.favicon, result_visuals->favicon);
}

}  // namespace offline_pages

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_client_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

password_manager::PasswordForm CreateTestPassword(
    const std::string& url,
    password_manager::PasswordForm::Store store =
        password_manager::PasswordForm::Store::kProfileStore,
    base::Time last_used_time = base::Time::UnixEpoch()) {
  password_manager::PasswordForm form;
  form.signon_realm = url;
  form.url = GURL(url);
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  form.in_store = store;
  form.date_last_used = last_used_time;
  return form;
}

syncer::LocalDataDescription CreateLocalDataDescription(
    syncer::ModelType type,
    int item_count,
    const std::vector<std::string>& domains,
    int domain_count) {
  syncer::LocalDataDescription desc;
  desc.type = type;
  desc.item_count = item_count;
  desc.domains = domains;
  desc.domain_count = domain_count;
  return desc;
}

class LocalDataQueryHelperTest : public testing::Test {
 public:
  LocalDataQueryHelperTest()
      : local_bookmark_sync_service_(
            &bookmark_undo_service_,
            syncer::WipeModelUponSyncDisabledBehavior::kNever),
        account_bookmark_sync_service_(
            &bookmark_undo_service_,
            syncer::WipeModelUponSyncDisabledBehavior::kNever) {
    local_password_store_->Init(/*prefs=*/nullptr,
                                /*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);

    auto local_bookmark_client =
        std::make_unique<bookmarks::TestBookmarkClient>();
    local_bookmark_client_ = local_bookmark_client.get();
    local_bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(
            std::move(local_bookmark_client));

    // Make sure BookmarkSyncService is aware of bookmarks having been loaded.
    local_bookmark_sync_service_.DecodeBookmarkSyncMetadata(
        /*metadata_str=*/"",
        /*schedule_save_closure=*/base::DoNothing(),
        std::make_unique<
            sync_bookmarks::BookmarkModelViewUsingLocalOrSyncableNodes>(
            local_bookmark_model_.get()));
    account_bookmark_sync_service_.DecodeBookmarkSyncMetadata(
        /*metadata_str=*/"",
        /*schedule_save_closure=*/base::DoNothing(),
        std::make_unique<
            sync_bookmarks::BookmarkModelViewUsingLocalOrSyncableNodes>(
            account_bookmark_model_.get()));

    // TODO(crbug.com/1451508): Simplify by wrapping into a helper.
    auto local_reading_list_storage =
        std::make_unique<FakeReadingListModelStorage>();
    auto* local_reading_list_storage_ptr = local_reading_list_storage.get();
    auto local_reading_list_model = std::make_unique<ReadingListModelImpl>(
        std::move(local_reading_list_storage),
        syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    local_reading_list_model_ = local_reading_list_model.get();

    auto account_reading_list_storage =
        std::make_unique<FakeReadingListModelStorage>();
    auto* account_reading_list_storage_ptr = account_reading_list_storage.get();
    auto account_reading_list_model = ReadingListModelImpl::BuildNewForTest(
        std::move(account_reading_list_storage), syncer::StorageType::kAccount,
        syncer::WipeModelUponSyncDisabledBehavior::kAlways, &clock_,
        processor_.CreateForwardingProcessor());
    account_reading_list_model_ = account_reading_list_model.get();

    dual_reading_list_model_ =
        std::make_unique<reading_list::DualReadingListModel>(
            std::move(local_reading_list_model),
            std::move(account_reading_list_model));
    local_reading_list_storage_ptr->TriggerLoadCompletion();
    account_reading_list_storage_ptr->TriggerLoadCompletion();

    local_data_query_helper_ =
        std::make_unique<browser_sync::LocalDataQueryHelper>(
            /*profile_password_store=*/local_password_store_.get(),
            /*account_password_store=*/account_password_store_.get(),
            /*local_bookmark_sync_service=*/&local_bookmark_sync_service_,
            /*account_bookmark_sync_service=*/&account_bookmark_sync_service_,
            /*dual_reading_list_model=*/dual_reading_list_model_.get());

    // Make sure PasswordStore is fully initialized.
    RunAllPendingTasks();
  }

  ~LocalDataQueryHelperTest() override {
    local_password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;

  scoped_refptr<password_manager::TestPasswordStore> local_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore{true});

  std::unique_ptr<bookmarks::BookmarkModel> local_bookmark_model_;
  raw_ptr<bookmarks::TestBookmarkClient> local_bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> account_bookmark_model_ =
      bookmarks::TestBookmarkClient::CreateModel();

  BookmarkUndoService bookmark_undo_service_;  // Needed by BookmarkSyncService.
  sync_bookmarks::BookmarkSyncService local_bookmark_sync_service_;
  sync_bookmarks::BookmarkSyncService account_bookmark_sync_service_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<reading_list::DualReadingListModel> dual_reading_list_model_;
  raw_ptr<ReadingListModel> local_reading_list_model_;
  raw_ptr<ReadingListModel> account_reading_list_model_;

  std::unique_ptr<LocalDataQueryHelper> local_data_query_helper_;
};

TEST_F(LocalDataQueryHelperTest, ShouldHandleZeroTypes) {
  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  EXPECT_CALL(callback, Run(::testing::IsEmpty()));

  local_data_query_helper_->Run(syncer::ModelTypeSet(), callback.Get());
}

TEST_F(LocalDataQueryHelperTest, ShouldHandleUnusableTypes) {
  base::HistogramTester histogram_tester;

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  EXPECT_CALL(callback, Run(::testing::IsEmpty()));

  ASSERT_TRUE(task_environment_.MainThreadIsIdle());

  LocalDataQueryHelper helper(
      /*profile_password_store=*/nullptr,
      /*account_password_store=*/nullptr,
      /*local_bookmark_sync_service=*/nullptr,
      /*account_bookmark_sync_service=*/nullptr,
      /*dual_reading_list_model=*/nullptr);
  helper.Run(syncer::ModelTypeSet(
                 {syncer::PASSWORDS, syncer::BOOKMARKS, syncer::READING_LIST}),
             callback.Get());

  EXPECT_TRUE(task_environment_.MainThreadIsIdle());
}

TEST_F(LocalDataQueryHelperTest, ShouldReturnLocalPasswordsViaCallback) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       CreateLocalDataDescription(syncer::PASSWORDS, 2,
                                  {"amazon.de", "facebook.com"}, 2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldReturnCountOfDistinctDomains) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));
  // Another password with the same domain as an existing password.
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.amazon.de/login"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS, CreateLocalDataDescription(
                              syncer::PASSWORDS,
                              // Total passwords = 3.
                              /*item_count=*/3, {"amazon.de", "facebook.com"},
                              // Total distinct domains = 2.
                              /*domain_count=*/2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldHandleMultipleRequests) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_password_store_->AddLogin(
      CreateTestPassword("https://www.facebook.com"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback1;

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback2;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       CreateLocalDataDescription(syncer::PASSWORDS, 2,
                                  {"amazon.de", "facebook.com"}, 2)}};

  // Request #1.
  EXPECT_CALL(callback1, Run(expected));
  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback1.Get());

  // Request #2.
  EXPECT_CALL(callback2, Run(expected));
  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback2.Get());

  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldReturnLocalBookmarksViaCallback) {
  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(https://www.amazon.de)
  //    |- url2(http://www.facebook.com)
  //  |- folder 2
  //    |- url3(http://www.amazon.de/faq)
  const bookmarks::BookmarkNode* bookmark_bar_node =
      local_bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = local_bookmark_model_->AddFolder(
      bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(std::string("folder1")));
  const bookmarks::BookmarkNode* folder2 = local_bookmark_model_->AddFolder(
      bookmark_bar_node, /*index=*/1,
      base::UTF8ToUTF16(std::string("folder2")));
  ASSERT_EQ(2u, bookmark_bar_node->children().size());
  local_bookmark_model_->AddURL(folder1, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url1")),
                                GURL("https://www.amazon.de"));
  local_bookmark_model_->AddURL(folder1, /*index=*/1,
                                base::UTF8ToUTF16(std::string("url2")),
                                GURL("https://www.facebook.com"));
  ASSERT_EQ(2u, folder1->children().size());
  local_bookmark_model_->AddURL(folder2, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url3")),
                                GURL("https://www.amazon.de/faq"));
  ASSERT_EQ(1u, folder2->children().size());

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::BOOKMARKS,
       CreateLocalDataDescription(syncer::BOOKMARKS, 3,
                                  {"amazon.de", "facebook.com"}, 2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}),
                                callback.Get());

  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldIgnoreManagedBookmarks) {
  // -------- The local model --------
  // bookmark_bar
  //    |- url1(https://www.amazon.de)
  // managed_bookmarks
  //    |- url2(http://www.facebook.com)
  const bookmarks::BookmarkNode* bookmark_bar_node =
      local_bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* managed_node =
      local_bookmark_client_->EnableManagedNode();

  local_bookmark_model_->AddURL(bookmark_bar_node, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url1")),
                                GURL("https://www.amazon.de"));
  local_bookmark_model_->AddURL(managed_node, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url2")),
                                GURL("https://www.facebook.com"));
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  ASSERT_EQ(1u, managed_node->children().size());

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::BOOKMARKS,
       CreateLocalDataDescription(syncer::BOOKMARKS, 1, {"amazon.de"}, 1)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}),
                                callback.Get());

  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest,
       ShouldOnlyTriggerCallbackWhenAllTypesHaveReturned) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_bookmark_model_->AddURL(
      local_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.facebook.com"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       CreateLocalDataDescription(syncer::PASSWORDS, 1, {"amazon.de"}, 1)},
      {syncer::BOOKMARKS,
       CreateLocalDataDescription(syncer::BOOKMARKS, 1, {"facebook.com"}, 1)},
  };

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(
      syncer::ModelTypeSet({syncer::PASSWORDS, syncer::BOOKMARKS}),
      callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest,
       ShouldHandleMultipleRequestsForDifferentTypes) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("https://www.amazon.de"));
  local_bookmark_model_->AddURL(
      local_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.facebook.com"));

  RunAllPendingTasks();

  // Request #1.
  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback1;
  std::map<syncer::ModelType, syncer::LocalDataDescription> expected1 = {
      {syncer::PASSWORDS,
       CreateLocalDataDescription(syncer::PASSWORDS, 1, {"amazon.de"}, 1)},
  };
  EXPECT_CALL(callback1, Run(expected1));

  // Request #2.
  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback2;
  std::map<syncer::ModelType, syncer::LocalDataDescription> expected2 = {
      {syncer::BOOKMARKS,
       CreateLocalDataDescription(syncer::BOOKMARKS, 1, {"facebook.com"}, 1)},
  };
  EXPECT_CALL(callback2, Run(expected2));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback1.Get());
  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}),
                                callback2.Get());

  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldReturnLocalReadingListViaCallback) {
  // Add test data to local model.
  local_reading_list_model_->AddOrReplaceEntry(
      GURL("https://www.amazon.de"), "url1",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  local_reading_list_model_->AddOrReplaceEntry(
      GURL("https://www.facebook.com"), "url2",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  local_reading_list_model_->AddOrReplaceEntry(
      GURL("https://www.amazon.de/faq"), "url3",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  ASSERT_TRUE(local_reading_list_model_->loaded());

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::READING_LIST,
       CreateLocalDataDescription(syncer::READING_LIST, 3,
                                  {"amazon.de", "facebook.com"}, 2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::READING_LIST}),
                                callback.Get());
  RunAllPendingTasks();
}

TEST_F(LocalDataQueryHelperTest, ShouldWorkForUrlsWithNoTLD) {
  // Add test data to local store.
  local_password_store_->AddLogin(CreateTestPassword("chrome://flags"));
  local_password_store_->AddLogin(CreateTestPassword("https://test"));

  RunAllPendingTasks();

  base::MockOnceCallback<void(
      std::map<syncer::ModelType, syncer::LocalDataDescription>)>
      callback;

  std::map<syncer::ModelType, syncer::LocalDataDescription> expected = {
      {syncer::PASSWORDS,
       CreateLocalDataDescription(syncer::PASSWORDS, 2,
                                  {"chrome://flags", "test"}, 2)}};

  EXPECT_CALL(callback, Run(expected));

  local_data_query_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}),
                                callback.Get());
  RunAllPendingTasks();
}

class LocalDataMigrationHelperTest : public testing::Test {
 public:
  LocalDataMigrationHelperTest()
      : local_bookmark_sync_service_(
            &bookmark_undo_service_,
            syncer::WipeModelUponSyncDisabledBehavior::kNever),
        account_bookmark_sync_service_(
            &bookmark_undo_service_,
            syncer::WipeModelUponSyncDisabledBehavior::kNever) {
    local_password_store_->Init(/*prefs=*/nullptr,
                                /*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);

    auto local_bookmark_client =
        std::make_unique<bookmarks::TestBookmarkClient>();
    local_bookmark_client_ = local_bookmark_client.get();
    local_bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(
            std::move(local_bookmark_client));

    // Make sure BookmarkSyncService is aware of bookmarks having been loaded.
    local_bookmark_sync_service_.DecodeBookmarkSyncMetadata(
        /*metadata_str=*/"",
        /*schedule_save_closure=*/base::DoNothing(),
        std::make_unique<
            sync_bookmarks::BookmarkModelViewUsingLocalOrSyncableNodes>(
            local_bookmark_model_.get()));
    account_bookmark_sync_service_.DecodeBookmarkSyncMetadata(
        /*metadata_str=*/"",
        /*schedule_save_closure=*/base::DoNothing(),
        std::make_unique<
            sync_bookmarks::BookmarkModelViewUsingLocalOrSyncableNodes>(
            account_bookmark_model_.get()));

    // TODO(crbug.com/1451508): Simplify by wrapping into a helper.
    auto local_reading_list_storage =
        std::make_unique<FakeReadingListModelStorage>();
    auto* local_reading_list_storage_ptr = local_reading_list_storage.get();
    auto local_reading_list_model = std::make_unique<ReadingListModelImpl>(
        std::move(local_reading_list_storage),
        syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    local_reading_list_model_ = local_reading_list_model.get();

    auto account_reading_list_storage =
        std::make_unique<FakeReadingListModelStorage>();
    auto* account_reading_list_storage_ptr = account_reading_list_storage.get();
    auto account_reading_list_model = ReadingListModelImpl::BuildNewForTest(
        std::move(account_reading_list_storage), syncer::StorageType::kAccount,
        syncer::WipeModelUponSyncDisabledBehavior::kAlways, &clock_,
        processor_.CreateForwardingProcessor());
    account_reading_list_model_ = account_reading_list_model.get();

    dual_reading_list_model_ =
        std::make_unique<reading_list::DualReadingListModel>(
            std::move(local_reading_list_model),
            std::move(account_reading_list_model));
    local_reading_list_storage_ptr->TriggerLoadCompletion();
    account_reading_list_storage_ptr->TriggerLoadCompletion();

    local_data_migration_helper_ =
        std::make_unique<browser_sync::LocalDataMigrationHelper>(
            /*profile_password_store=*/local_password_store_.get(),
            /*account_password_store=*/account_password_store_.get(),
            /*local_bookmark_sync_service=*/&local_bookmark_sync_service_,
            /*account_bookmark_sync_service=*/&account_bookmark_sync_service_,
            /*dual_reading_list_model=*/dual_reading_list_model_.get());

    // Make sure PasswordStore is fully initialized.
    RunAllPendingTasks();
  }

  ~LocalDataMigrationHelperTest() override {
    local_password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;

  scoped_refptr<password_manager::TestPasswordStore> local_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore{true});

  std::unique_ptr<bookmarks::BookmarkModel> local_bookmark_model_;
  raw_ptr<bookmarks::TestBookmarkClient> local_bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> account_bookmark_model_ =
      bookmarks::TestBookmarkClient::CreateModel();

  BookmarkUndoService bookmark_undo_service_;  // Needed by BookmarkSyncService.
  sync_bookmarks::BookmarkSyncService local_bookmark_sync_service_;
  sync_bookmarks::BookmarkSyncService account_bookmark_sync_service_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<reading_list::DualReadingListModel> dual_reading_list_model_;
  raw_ptr<ReadingListModel> local_reading_list_model_;
  raw_ptr<ReadingListModel> account_reading_list_model_;

  std::unique_ptr<LocalDataMigrationHelper> local_data_migration_helper_;
};

TEST_F(LocalDataMigrationHelperTest, ShouldLogRequestsToHistogram) {
  {
    base::HistogramTester histogram_tester;
    local_data_migration_helper_->Run(syncer::ModelTypeSet());

    // Nothing logged to histogram.
    histogram_tester.ExpectTotalCount("Sync.BatchUpload.Requests2", 0);
  }
  {
    base::HistogramTester histogram_tester;
    local_data_migration_helper_->Run(
        syncer::ModelTypeSet({syncer::PASSWORDS}));

    histogram_tester.ExpectUniqueSample(
        "Sync.BatchUpload.Requests2",
        syncer::ModelTypeForHistograms::kPasswords, 1);
  }
  {
    base::HistogramTester histogram_tester;

    // Required by MarkAllForUploadToSyncServerIfNeeded() to work.
    ON_CALL(processor_, IsTrackingMetadata)
        .WillByDefault(::testing::Return(true));

    local_data_migration_helper_->Run(syncer::ModelTypeSet(
        {syncer::PASSWORDS, syncer::BOOKMARKS, syncer::READING_LIST}));

    histogram_tester.ExpectTotalCount("Sync.BatchUpload.Requests2", 3);
    histogram_tester.ExpectBucketCount(
        "Sync.BatchUpload.Requests2",
        syncer::ModelTypeForHistograms::kPasswords, 1);
    histogram_tester.ExpectBucketCount(
        "Sync.BatchUpload.Requests2",
        syncer::ModelTypeForHistograms::kBookmarks, 1);
    histogram_tester.ExpectBucketCount(
        "Sync.BatchUpload.Requests2",
        syncer::ModelTypeForHistograms::kReadingList, 1);
  }
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldNotLogUnsupportedDataTypesRequestToHistogram) {
  base::HistogramTester histogram_tester;
  local_data_migration_helper_->Run(
      syncer::ModelTypeSet({syncer::PASSWORDS, syncer::DEVICE_INFO}));

  // Only the request for PASSWORDS is logged.
  histogram_tester.ExpectUniqueSample(
      "Sync.BatchUpload.Requests2", syncer::ModelTypeForHistograms::kPasswords,
      1);
}

TEST_F(LocalDataMigrationHelperTest, ShouldHandleZeroTypes) {
  // Just checks that there's no crash.
  local_data_migration_helper_->Run(syncer::ModelTypeSet());
}

TEST_F(LocalDataMigrationHelperTest, ShouldHandleUnusableTypes) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(task_environment_.MainThreadIsIdle());

  LocalDataMigrationHelper helper(
      /*profile_password_store=*/nullptr,
      /*account_password_store=*/nullptr,
      /*local_bookmark_model=*/nullptr,
      /*account_bookmark_model=*/nullptr,
      /*dual_reading_list_model=*/nullptr);
  helper.Run(syncer::ModelTypeSet(
      {syncer::PASSWORDS, syncer::BOOKMARKS, syncer::READING_LIST}));

  EXPECT_TRUE(task_environment_.MainThreadIsIdle());
}

TEST_F(LocalDataMigrationHelperTest, ShouldMovePasswordsToAccountStore) {
  // Add test data to local store.
  auto form1 = CreateTestPassword("https://www.amazon.de");
  auto form2 = CreateTestPassword("https://www.facebook.com");
  local_password_store_->AddLogin(form1);
  local_password_store_->AddLogin(form2);

  RunAllPendingTasks();

  ASSERT_EQ(
      local_password_store_->stored_passwords(),
      password_manager::TestPasswordStore::PasswordMap(
          {{form1.signon_realm, {form1}}, {form2.signon_realm, {form2}}}));

  base::HistogramTester histogram_tester;

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  EXPECT_EQ(2, histogram_tester.GetTotalSum("Sync.PasswordsBatchUpload.Count"));

  // Passwords have been moved to the account store.
  form1.in_store = password_manager::PasswordForm::Store::kAccountStore;
  form2.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(
      account_password_store_->stored_passwords(),
      password_manager::TestPasswordStore::PasswordMap(
          {{form1.signon_realm, {form1}}, {form2.signon_realm, {form2}}}));
  // Local password store is empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest, ShouldNotUploadSamePassword) {
  // Add test password to local store.
  auto local_form = CreateTestPassword("https://www.amazon.de");
  local_form.times_used_in_html_form = 10;
  local_password_store_->AddLogin(local_form);

  // Add the same password to the account store, with slight different
  // non-identifying details.
  auto account_form = local_form;
  account_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  account_form.times_used_in_html_form = 5;
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  base::HistogramTester histogram_tester;

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  EXPECT_EQ(0, histogram_tester.GetTotalSum("Sync.PasswordsBatchUpload.Count"));

  // No new password is added to the account store.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));
  // The password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldUploadConflictingPasswordIfMoreRecentlyUsed) {
  // Add test password to local store, with last used time set to (time for
  // epoch in Unix + 1 second).
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore,
                         base::Time::UnixEpoch() + base::Seconds(1));
  local_form.password_value = u"local_value";
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last used time set to time for epoch in Unix.
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore,
                         base::Time::UnixEpoch());
  account_form.password_value = u"account_value";
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  base::HistogramTester histogram_tester;

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  EXPECT_EQ(1, histogram_tester.GetTotalSum("Sync.PasswordsBatchUpload.Count"));

  // Since local password has a more recent last used date, it is moved to the
  // account store.
  local_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  // Local password store is now empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldNotUploadConflictingPasswordIfLessRecentlyUsed) {
  // Add test password to local store, with last used time set to time for epoch
  // in Unix.
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore,
                         base::Time::UnixEpoch());
  local_form.password_value = u"local_value";
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last used time set to (time for epoch in Unix + 1 second).
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore,
                         base::Time::UnixEpoch() + base::Seconds(1));
  account_form.password_value = u"account_value";
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  base::HistogramTester histogram_tester;

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  EXPECT_EQ(0, histogram_tester.GetTotalSum("Sync.PasswordsBatchUpload.Count"));

  // Since account password has a more recent last used date, it wins over the
  // local password.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {account_form}}}));
  // Local password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldUploadConflictingPasswordIfMoreRecentlyUpdated) {
  // Add test password to local store, with last updated time set to (time for
  // epoch in Linux + 1 second).
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore);
  local_form.password_value = u"local_value";
  local_form.date_password_modified =
      base::Time::UnixEpoch() + base::Seconds(1);
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last updated time set to time for epoch in Unix.
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore);
  account_form.password_value = u"account_value";
  account_form.date_password_modified = base::Time::UnixEpoch();
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since local password has a more recent last modified date, it is moved to
  // the account store.
  local_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  // Local password store is now empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldNotUploadConflictingPasswordIfLessRecentlyUpdated) {
  // Add test password to local store, with last updated time set to time for
  // epoch in Unix.
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore);
  local_form.password_value = u"local_value";
  local_form.date_password_modified = base::Time::UnixEpoch();
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // last updated time set to (time for epoch in Unix + 1 second).
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore);
  account_form.password_value = u"account_value";
  account_form.date_password_modified =
      base::Time::UnixEpoch() + base::Seconds(1);
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since account password has a more recent last modified date, it wins over
  // the local password.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {account_form}}}));
  // Local password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldUploadConflictingPasswordIfMoreRecentlyCreated) {
  // Add test password to local store, with creation time set to (time for
  // epoch in Unix + 1 second).
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore);
  local_form.password_value = u"local_value";
  local_form.date_created = base::Time::UnixEpoch() + base::Seconds(1);
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // creation time set to time for epoch in Unix.
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore);
  account_form.password_value = u"account_value";
  account_form.date_created = base::Time::UnixEpoch();
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since local password has a more recent creation time, it is moved to the
  // account store.
  local_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  // Local password store is now empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldNotUploadConflictingPasswordIfLessRecentlyCreated) {
  // Add test password to local store, with creation time set to time for epoch
  // in Unix.
  auto local_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore);
  local_form.password_value = u"local_value";
  local_form.date_created = base::Time::UnixEpoch();
  local_password_store_->AddLogin(local_form);

  // Add same credential with a different password to the account store, with
  // creation time set to (time for epoch in Unix + 1 second).
  auto account_form =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore);
  account_form.password_value = u"account_value";
  account_form.date_created = base::Time::UnixEpoch() + base::Seconds(1);
  account_password_store_->AddLogin(account_form);

  RunAllPendingTasks();

  ASSERT_EQ(local_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {local_form}}}));
  ASSERT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{account_form.signon_realm, {account_form}}}));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Since account password has a more recent creation time, it wins over the
  // local password.
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{local_form.signon_realm, {account_form}}}));
  // Local password is removed from the local store.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest, ShouldMoveBookmarksToAccountStore) {
  // -------- The local model --------
  // bookmark_bar
  //    |- url1(https://www.amazon.de)
  local_bookmark_model_->AddURL(
      local_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.amazon.de"));

  // -------- The account model --------
  // bookmark_bar
  //    |- url2(http://www.google.com)
  account_bookmark_model_->AddURL(
      account_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url2")), GURL("https://www.google.com"));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //    |- url1(https://www.amazon.de)
  //    |- url2(http://www.google.com)
  EXPECT_EQ(2u,
            account_bookmark_model_->bookmark_bar_node()->children().size());

  EXPECT_FALSE(local_bookmark_model_->HasBookmarks());
}

TEST_F(LocalDataMigrationHelperTest, ShouldClearBookmarksFromLocalStore) {
  // -------- The local model --------
  // bookmark_bar
  //    |- url1(https://www.google.com)
  local_bookmark_model_->AddURL(
      local_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.google.com"));

  // -------- The account model --------
  // bookmark_bar
  //    |- url1(https://www.google.com)
  account_bookmark_model_->AddURL(
      account_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.google.com"));

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}));

  // No actual move happens since the data already exists in the account store.
  // -------- The expected merge outcome --------
  // bookmark_bar
  //    |- url1(https://www.google.com)
  EXPECT_EQ(1u,
            account_bookmark_model_->bookmark_bar_node()->children().size());

  // The data is still cleared from the local store.
  EXPECT_FALSE(local_bookmark_model_->HasBookmarks());
}

TEST_F(LocalDataMigrationHelperTest, ShouldIgnoreManagedBookmarks) {
  // -------- The local model --------
  // bookmark_bar
  //    |- url1(https://www.amazon.de)
  // managed_bookmarks
  //    |- url2(http://www.facebook.com)
  const bookmarks::BookmarkNode* bookmark_bar_node =
      local_bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* managed_node =
      local_bookmark_client_->EnableManagedNode();

  local_bookmark_model_->AddURL(bookmark_bar_node, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url1")),
                                GURL("https://www.amazon.de"));
  local_bookmark_model_->AddURL(managed_node, /*index=*/0,
                                base::UTF8ToUTF16(std::string("url2")),
                                GURL("https://www.facebook.com"));

  // The account model is empty.
  EXPECT_FALSE(account_bookmark_model_->HasBookmarks());

  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //    |- url1(https://www.amazon.de)
  EXPECT_EQ(1u,
            account_bookmark_model_->bookmark_bar_node()->children().size());

  // Managed nodes should be ignored.
  std::vector<const bookmarks::BookmarkNode*> nodes =
      account_bookmark_model_->GetNodesByURL(GURL("https://www.facebook.com"));
  EXPECT_TRUE(nodes.empty());

  // The local bookmark is not empty since managed bookmarks were not moved.
  EXPECT_TRUE(local_bookmark_model_->HasBookmarks());
}

TEST_F(LocalDataMigrationHelperTest,
       ShouldHandleMultipleRequestsForDifferentTypes) {
  // Add test data to local store.
  auto form = CreateTestPassword("https://amazon.de");
  local_password_store_->AddLogin(form);
  local_bookmark_model_->AddURL(
      local_bookmark_model_->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16(std::string("url1")), GURL("https://www.facebook.com"));

  RunAllPendingTasks();

  // Account stores/models are empty.
  ASSERT_TRUE(account_password_store_->IsEmpty());
  ASSERT_FALSE(account_bookmark_model_->HasBookmarks());

  // Request #1.
  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  // Request #2.
  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::BOOKMARKS}));

  RunAllPendingTasks();

  // The local data has been moved to the account store/model.
  form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap(
                {{form.signon_realm, {form}}}));
  EXPECT_TRUE(local_password_store_->IsEmpty());
  EXPECT_EQ(1u,
            account_bookmark_model_->bookmark_bar_node()->children().size());
  EXPECT_FALSE(local_bookmark_model_->HasBookmarks());
}

TEST_F(LocalDataMigrationHelperTest, ShouldHandleMultipleRequestsForPasswords) {
  // Add test data to local store.
  auto local_form1 =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kProfileStore,
                         base::Time::UnixEpoch() + base::Seconds(1));
  local_form1.password_value = u"local_value";
  auto local_form2 = CreateTestPassword("https://www.facebook.com");
  local_password_store_->AddLogin(local_form1);
  local_password_store_->AddLogin(local_form2);

  // Add test data to account store.
  auto account_form1 =
      CreateTestPassword("https://www.amazon.de",
                         password_manager::PasswordForm::Store::kAccountStore,
                         base::Time::UnixEpoch());
  account_form1.password_value = u"account_value";

  RunAllPendingTasks();

  // Request #1.
  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  // Request #2.
  local_data_migration_helper_->Run(syncer::ModelTypeSet({syncer::PASSWORDS}));

  RunAllPendingTasks();

  // Passwords have been moved to the account store.
  local_form1.in_store = password_manager::PasswordForm::Store::kAccountStore;
  local_form2.in_store = password_manager::PasswordForm::Store::kAccountStore;
  EXPECT_EQ(account_password_store_->stored_passwords(),
            password_manager::TestPasswordStore::PasswordMap({
                {local_form1.signon_realm, {local_form1}},
                {local_form2.signon_realm, {local_form2}},
            }));

  // Local password store is empty.
  EXPECT_TRUE(local_password_store_->IsEmpty());
}

TEST_F(LocalDataMigrationHelperTest, ShouldMoveReadingListToAccountStore) {
  // Required by MarkAllForUploadToSyncServerIfNeeded() to work.
  ON_CALL(processor_, IsTrackingMetadata)
      .WillByDefault(::testing::Return(true));

  // Add test data.
  local_reading_list_model_->AddOrReplaceEntry(
      GURL("https://www.amazon.de"), "url1",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  account_reading_list_model_->AddOrReplaceEntry(
      GURL("https://www.facebook.com"), "url2",
      reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  local_data_migration_helper_->Run(
      syncer::ModelTypeSet({syncer::READING_LIST}));

  RunAllPendingTasks();

  EXPECT_EQ(2u, account_reading_list_model_->size());
  EXPECT_EQ(0u, local_reading_list_model_->size());
}

TEST_F(LocalDataMigrationHelperTest, ShouldClearReadingListFromLocalStore) {
  // Required by MarkAllForUploadToSyncServerIfNeeded() to work.
  ON_CALL(processor_, IsTrackingMetadata)
      .WillByDefault(::testing::Return(true));

  const GURL kCommonUrl("http://common_url.com/");

  // Add test data.
  local_reading_list_model_->AddOrReplaceEntry(
      kCommonUrl, "url1", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());
  // Same data exists in the account store.
  account_reading_list_model_->AddOrReplaceEntry(
      kCommonUrl, "url1", reading_list::ADDED_VIA_CURRENT_APP,
      /*estimated_read_time=*/base::TimeDelta());

  ASSERT_EQ(
      dual_reading_list_model_->GetStorageStateForURLForTesting(kCommonUrl),
      reading_list::DualReadingListModel::StorageStateForTesting::
          kExistsInBothModels);

  local_data_migration_helper_->Run(
      syncer::ModelTypeSet({syncer::READING_LIST}));

  RunAllPendingTasks();

  // No new data in the account reading list since it already existed.
  EXPECT_EQ(1u, account_reading_list_model_->size());
  // Local store is still cleared.
  EXPECT_EQ(0u, local_reading_list_model_->size());
}

}  // namespace
}  // namespace browser_sync

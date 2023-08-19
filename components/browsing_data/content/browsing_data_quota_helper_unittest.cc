// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "components/browsing_data/content/browsing_data_quota_helper_impl.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::StorageType;

namespace {

struct ClientDefaultBucketData {
  const char* origin;
  StorageType type;
  int64_t usage;
};

}  // namespace

class BrowsingDataQuotaHelperTest : public testing::Test {
 public:
  typedef BrowsingDataQuotaHelper::QuotaInfo QuotaInfo;
  typedef BrowsingDataQuotaHelper::QuotaInfoArray QuotaInfoArray;

  BrowsingDataQuotaHelperTest() = default;

  BrowsingDataQuotaHelperTest(const BrowsingDataQuotaHelperTest&) = delete;
  BrowsingDataQuotaHelperTest& operator=(const BrowsingDataQuotaHelperTest&) =
      delete;

  ~BrowsingDataQuotaHelperTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::QuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        content::GetIOThreadTaskRunner({}).get(),
        /*quota_change_callback=*/base::DoNothing(),
        /*special_storage_policy=*/nullptr, storage::GetQuotaSettingsFunc());
    helper_ =
        base::MakeRefCounted<BrowsingDataQuotaHelperImpl>(quota_manager_.get());
  }

  void TearDown() override {
    helper_ = nullptr;
    quota_manager_ = nullptr;
    quota_info_.clear();
    content::RunAllTasksUntilIdle();
  }

 protected:
  const QuotaInfoArray& quota_info() const { return quota_info_; }

  bool fetching_completed() const { return fetching_completed_; }

  void StartFetching() {
    fetching_completed_ = false;
    helper_->StartFetching(
        base::BindOnce(&BrowsingDataQuotaHelperTest::FetchCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  void RegisterClient(
      base::span<const ClientDefaultBucketData> storage_key_data) {
    auto mock_quota_client = std::make_unique<storage::MockQuotaClient>(
        quota_manager_->proxy(), storage::QuotaClientType::kFileSystem);
    storage::MockQuotaClient* mock_quota_client_ptr = mock_quota_client.get();

    mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
    mojo::MakeSelfOwnedReceiver(std::move(mock_quota_client),
                                quota_client.InitWithNewPipeAndPassReceiver());
    // Database bootstrapping tries to cache bucket usage in quota before bucket
    // usage can be set in MockQuotaClient. So we disable bootstrapping so
    // bucket usage retrieval happens after MockQuotaClient is ready.
    quota_manager_->SetBootstrapDisabledForTesting(true);
    quota_manager_->proxy()->RegisterClient(
        std::move(quota_client), storage::QuotaClientType::kFileSystem,
        {blink::mojom::StorageType::kTemporary,
         blink::mojom::StorageType::kSyncable});

    std::map<storage::BucketLocator, int64_t> buckets_data;
    for (const ClientDefaultBucketData& data : storage_key_data) {
      base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
      quota_manager_->GetOrCreateBucketDeprecated(
          storage::BucketInitParams::ForDefaultBucket(
              blink::StorageKey::CreateFromStringForTesting(data.origin)),
          data.type, future.GetCallback());
      ASSERT_OK_AND_ASSIGN(auto bucket, future.Take());
      buckets_data.insert(std::pair<storage::BucketLocator, int64_t>(
          bucket.ToBucketLocator(), data.usage));
    }
    mock_quota_client_ptr->AddBucketsData(buckets_data);
  }

  void DeleteHostData(const std::string& host, blink::mojom::StorageType type) {
    helper_->DeleteHostData(host, type);
  }

  int64_t quota() { return quota_; }

 private:
  void FetchCompleted(const QuotaInfoArray& quota_info) {
    quota_info_ = quota_info;
    fetching_completed_ = true;
  }

  base::ScopedTempDir temp_dir_;

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::QuotaManager> quota_manager_;

  scoped_refptr<BrowsingDataQuotaHelper> helper_;

  bool fetching_completed_ = true;
  QuotaInfoArray quota_info_;
  int64_t quota_ = -1;
  base::WeakPtrFactory<BrowsingDataQuotaHelperTest> weak_factory_{this};
};

TEST_F(BrowsingDataQuotaHelperTest, Empty) {
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());
  EXPECT_TRUE(quota_info().empty());
}

TEST_F(BrowsingDataQuotaHelperTest, FetchData) {
  static const ClientDefaultBucketData kStorageKeys[] = {
      {"http://example.com/", StorageType::kTemporary, 1},
      {"https://example.com/", StorageType::kTemporary, 10},
      {"https://example.com/", StorageType::kSyncable, 1},
      {"http://example2.com/", StorageType::kTemporary, 1000},
  };

  RegisterClient(kStorageKeys);
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  std::set<QuotaInfo> expected, actual;
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"), 1,
      0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example2.com"),
      1000, 0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"), 10,
      1));
  EXPECT_TRUE(expected == actual);
}

TEST_F(BrowsingDataQuotaHelperTest, IgnoreExtensionsAndDevTools) {
  static const ClientDefaultBucketData kStorageKeys[] = {
    {"http://example.com/", StorageType::kTemporary, 1},
    {"https://example.com/", StorageType::kTemporary, 10},
    {"https://example.com/", StorageType::kSyncable, 1},
    {"http://example2.com/", StorageType::kTemporary, 1000},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"chrome-extension://abcdefghijklmnopqrstuvwxyz/", StorageType::kTemporary,
     10000},
#endif
    {"devtools://abcdefghijklmnopqrstuvwxyz/", StorageType::kTemporary, 10000},
  };

  RegisterClient(kStorageKeys);
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  std::set<QuotaInfo> expected, actual;
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"), 1,
      0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example2.com"),
      1000, 0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"), 10,
      1));
  EXPECT_TRUE(expected == actual);
}

TEST_F(BrowsingDataQuotaHelperTest, DeleteHostData) {
  static const ClientDefaultBucketData kStorageKeys[] = {
      {"http://example.com/", StorageType::kTemporary, 1},
      {"https://example.com/", StorageType::kTemporary, 10},
      {"https://example.com/", StorageType::kSyncable, 1},
      {"http://example2.com/", StorageType::kTemporary, 1000},
  };
  RegisterClient(kStorageKeys);

  DeleteHostData("example.com", StorageType::kSyncable);

  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  std::set<QuotaInfo> expected, actual;
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"), 1,
      0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example2.com"),
      1000, 0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"), 10,
      0));
  EXPECT_TRUE(expected == actual);

  DeleteHostData("example2.com", StorageType::kTemporary);
  content::RunAllTasksUntilIdle();

  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  expected.clear();
  actual.clear();
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"), 1,
      0));
  expected.insert(QuotaInfo(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"), 10,
      0));

  EXPECT_TRUE(expected == actual);
}

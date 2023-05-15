// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/browsing_data/content/test_browsing_data_model_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browsing_data_model_test_util::BrowsingDataEntry;
using browsing_data_model_test_util::ValidateBrowsingDataEntries;

namespace {

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : receiver_(this, std::move(receiver)) {}
  MOCK_METHOD(void,
              GetStoredTrustTokenCounts,
              (GetStoredTrustTokenCountsCallback),
              (override));
  MOCK_METHOD(void,
              DeleteStoredTrustTokens,
              (const url::Origin&, DeleteStoredTrustTokensCallback),
              (override));

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};
}  // namespace

class BrowsingDataModelTest : public testing::Test {
 public:
  BrowsingDataModelTest() {
    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    mock_network_context_ = std::make_unique<MockNetworkContext>(
        network_context_remote.InitWithNewPipeAndPassReceiver());
    storage_partition()->SetNetworkContextForTesting(
        std::move(network_context_remote));
    model_ = BrowsingDataModel::BuildEmpty(
        storage_partition(),
        std::make_unique<browsing_data::TestBrowsingDataModelDelegate>());
  }
  ~BrowsingDataModelTest() override = default;

  void TearDown() override { mock_network_context_.reset(); }

 protected:
  void BuildModel(base::OnceClosure completed) {
    model()->PopulateFromDisk(std::move(completed));
  }

  void DeleteModel() { model_.reset(); }

  content::BrowserContext* browser_context() { return &browser_context_; }
  BrowsingDataModel* model() { return model_.get(); }
  content::StoragePartition* storage_partition() {
    return browser_context()->GetDefaultStoragePartition();
  }
  MockNetworkContext* mock_network_context() {
    return mock_network_context_.get();
  }

  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://subsite.example.com"));
  const std::string kSubdomainOriginHost = "subsite.example.com";
  const std::string kSubdomainOriginSite = "example.com";

  const url::Origin kSiteOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const std::string kSiteOriginHost = "example.com";

  const url::Origin kAnotherSiteOrigin =
      url::Origin::Create(GURL("https://another-example.com"));
  const std::string kAnotherSiteOriginHost = "another-example.com";

  const url::Origin kTestOrigin = url::Origin::Create(GURL("https://a.test"));
  const std::string kTestOriginHost = "a.test";

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<BrowsingDataModel> model_;
  content::TestStoragePartition storage_partition_;
  std::unique_ptr<MockNetworkContext> mock_network_context_;
};

TEST_F(BrowsingDataModelTest, PrimaryHostMapping) {
  model()->AddBrowsingData(kSubdomainOrigin,
                           BrowsingDataModel::StorageType::kTrustTokens, 0, 1);
  model()->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(kSubdomainOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 123, 0);
  model()->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(kSubdomainOrigin),
      BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage, 456, 0);

  ValidateBrowsingDataEntries(
      model(),
      {{kSubdomainOriginHost,
        kSubdomainOrigin,
        {{BrowsingDataModel::StorageType::kTrustTokens}, 0, 1}},
       {kSubdomainOriginSite,
        blink::StorageKey::CreateFirstParty(kSubdomainOrigin),
        {{BrowsingDataModel::StorageType::kPartitionedQuotaStorage}, 123, 0}},
       {kSubdomainOriginHost,
        blink::StorageKey::CreateFirstParty(kSubdomainOrigin),
        {{BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage},
         456,
         0}}});
}

TEST_F(BrowsingDataModelTest, EntryCoalescense) {
  // Check that multiple entries are correctly coalesced.
  // Browsing data with the same owner + data_key pair should update the
  // same entry's details.
  model()->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(kSiteOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 123, 0);
  model()->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(kSiteOrigin),
      BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage, 234, 5);

  auto expected_entries = std::vector<BrowsingDataEntry>(
      {{kSiteOriginHost,
        blink::StorageKey::CreateFirstParty(kSiteOrigin),
        {{BrowsingDataModel::StorageType::kPartitionedQuotaStorage,
          BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage},
         123 + 234,
         5}}});

  ValidateBrowsingDataEntries(model(), expected_entries);

  // Entries related to the same owner, but different data_keys, should
  // create a new entry.
  model()->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(kAnotherSiteOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 345, 0);
  model()->AddBrowsingData(
      kAnotherSiteOrigin, BrowsingDataModel::StorageType::kTrustTokens, 456, 6);

  expected_entries.push_back(
      {kAnotherSiteOriginHost,
       blink::StorageKey::CreateFirstParty(kAnotherSiteOrigin),
       {{BrowsingDataModel::StorageType::kPartitionedQuotaStorage}, 345}});
  expected_entries.push_back(
      {kAnotherSiteOriginHost,
       kAnotherSiteOrigin,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 456, 6}});

  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);
}

TEST_F(BrowsingDataModelTest, ConcurrentDeletions) {
  // Check that the model is able to support multiple deletion operations in
  // flight at the same time, even if the backends finish out-of-order.
  std::vector<::network::mojom::StoredTrustTokensForIssuerPtr> tokens;
  tokens.emplace_back(absl::in_place, kSubdomainOrigin, 10);
  tokens.emplace_back(absl::in_place, kAnotherSiteOrigin, 20);

  EXPECT_CALL(*mock_network_context(), GetStoredTrustTokenCounts(testing::_))
      .WillOnce(
          [&](network::TestNetworkContext::GetStoredTrustTokenCountsCallback
                  callback) { std::move(callback).Run(std::move(tokens)); });

  base::RunLoop run_loop;
  BuildModel(run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  // The size of trust token storage is aliased to a small amount of data, 100B.
  auto expected_entries = std::vector<BrowsingDataEntry>{
      {kSubdomainOriginHost,
       kSubdomainOrigin,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
      {kAnotherSiteOriginHost,
       kAnotherSiteOrigin,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
      {kTestOriginHost,
       kTestOrigin,
       {{static_cast<BrowsingDataModel::StorageType>(
            browsing_data::TestBrowsingDataModelDelegate::StorageType::
                kTestDelegateType)},
        0,
        0}}};

  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);

  // Save the deletion callbacks provided to the mock network context, so we
  // can later fulfil them out-of-order. The mock lives on the other side of a
  // mojo interface, so the callbacks can only be populated async.
  base::RunLoop delete_run_loop;
  base::RepeatingClosure delete_barrier = base::BarrierClosure(
      2, base::BindLambdaForTesting([&]() { delete_run_loop.QuitWhenIdle(); }));
  network::TestNetworkContext::DeleteStoredTrustTokensCallback
      delete_tokens_complete_1;
  network::TestNetworkContext::DeleteStoredTrustTokensCallback
      delete_tokens_complete_2;

  {
    testing::InSequence sequence;
    EXPECT_CALL(*mock_network_context(),
                DeleteStoredTrustTokens(kSubdomainOrigin, testing::_))
        .WillOnce(
            [&](const url::Origin& origin,
                network::TestNetworkContext::DeleteStoredTrustTokensCallback
                    callback) {
              delete_tokens_complete_1 = std::move(callback);
              delete_barrier.Run();
            });
    EXPECT_CALL(*mock_network_context(),
                DeleteStoredTrustTokens(kAnotherSiteOrigin, testing::_))
        .WillOnce(
            [&](const url::Origin& origin,
                network::TestNetworkContext::DeleteStoredTrustTokensCallback
                    callback) {
              delete_tokens_complete_2 = std::move(callback);
              delete_barrier.Run();
            });
  }

  // As running the saved callbacks is also async, we need more run loops to
  // confirm that the model receives the deletion completed callback.
  base::RunLoop delete_callback_1_runloop;
  base::RunLoop delete_callback_2_runloop;

  model()->RemoveBrowsingData(kSubdomainOriginHost,
                              delete_callback_1_runloop.QuitWhenIdleClosure());

  // Removal from the model should be synchronous.
  expected_entries.erase(expected_entries.begin());
  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);

  model()->RemoveBrowsingData(kAnotherSiteOriginHost,
                              delete_callback_2_runloop.QuitWhenIdleClosure());

  expected_entries.erase(expected_entries.begin());
  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);
  delete_run_loop.Run();

  // We're now holding two unfulfilled backend deletion callbacks. Delete the
  // model (for extra test coverage) and then fire them in the opposite order
  // they were called. The callbacks provided with RemoveBrowsingData should
  // still be fired in matching order.
  DeleteModel();

  std::move(delete_tokens_complete_2)
      .Run(
          network::mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted);
  delete_callback_2_runloop.Run();
  EXPECT_FALSE(delete_callback_1_runloop.AnyQuitCalled());
  EXPECT_TRUE(delete_callback_2_runloop.AnyQuitCalled());

  std::move(delete_tokens_complete_1)
      .Run(
          network::mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted);
  delete_callback_1_runloop.Run();

  // The test not timing out is sufficient coverage, but this is easier to grok.
  EXPECT_TRUE(delete_callback_1_runloop.AnyQuitCalled());
  EXPECT_TRUE(delete_callback_2_runloop.AnyQuitCalled());
}

TEST_F(BrowsingDataModelTest, DelegateDataDeleted) {
  // Needed to when building model from disk, returning an empty list as it's
  // not needed for this test.
  EXPECT_CALL(*mock_network_context(), GetStoredTrustTokenCounts(testing::_))
      .WillOnce(
          [&](network::TestNetworkContext::GetStoredTrustTokenCountsCallback
                  callback) { std::move(callback).Run({}); });

  base::RunLoop run_loop;
  BuildModel(run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  auto expected_entries = std::vector<BrowsingDataEntry>{
      {kTestOriginHost,
       kTestOrigin,
       {{static_cast<BrowsingDataModel::StorageType>(
            browsing_data::TestBrowsingDataModelDelegate::StorageType::
                kTestDelegateType)},
        0,
        0}}};

  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);
  expected_entries.erase(expected_entries.begin());
  EXPECT_TRUE(expected_entries.empty());
  model()->RemoveBrowsingData(kTestOriginHost, base::DoNothing());

  // Model should be empty after deleting delegated data.
  browsing_data_model_test_util::ValidateBrowsingDataEntries(model(),
                                                             expected_entries);
}

// A BrowsingDataModel::Delegate that marks all Origin-keyed data belonging
// to a given host as being owned by its origin rather than its host.
class OriginOwnershipDelegate : public BrowsingDataModel::Delegate {
 public:
  explicit OriginOwnershipDelegate(const std::string& origin_owned_host)
      : origin_owned_host_(origin_owned_host) {}

  // BrowsingDataModel::Delegate:
  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DelegateEntry>)> callback) override {
    std::move(callback).Run({});
  }

  void RemoveDataKey(BrowsingDataModel::DataKey data_key,
                     BrowsingDataModel::StorageTypeSet storage_types,
                     base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  absl::optional<BrowsingDataModel::DataOwner> GetDataOwner(
      BrowsingDataModel::DataKey data_key,
      BrowsingDataModel::StorageType storage_type) const override {
    url::Origin* origin = absl::get_if<url::Origin>(&data_key);
    if (origin && origin->host() == origin_owned_host_) {
      return *origin;
    }
    return absl::nullopt;
  }

 private:
  std::string origin_owned_host_;
};

TEST_F(BrowsingDataModelTest, DelegateDataCanBeOriginOwned) {
  std::string origin_owned_host = "origin.owned.com";
  std::unique_ptr<BrowsingDataModel> model = BrowsingDataModel::BuildEmpty(
      storage_partition(),
      std::make_unique<OriginOwnershipDelegate>(origin_owned_host));

  auto httpOriginOwned = url::Origin::Create(GURL("http://origin.owned.com"));
  model->AddBrowsingData(httpOriginOwned,
                         BrowsingDataModel::StorageType::kTrustTokens, 100);
  auto httpsOriginOwned = url::Origin::Create(GURL("https://origin.owned.com"));
  model->AddBrowsingData(httpsOriginOwned,
                         BrowsingDataModel::StorageType::kTrustTokens, 100);
  auto httpHostOwned = url::Origin::Create(GURL("http://host.owned.com"));
  model->AddBrowsingData(httpHostOwned,
                         BrowsingDataModel::StorageType::kTrustTokens, 100);
  auto httpsHostOwned = url::Origin::Create(GURL("https://host.owned.com"));
  model->AddBrowsingData(httpsHostOwned,
                         BrowsingDataModel::StorageType::kTrustTokens, 100);

  auto expected_entries = std::vector<BrowsingDataEntry>{
      {httpOriginOwned,
       httpOriginOwned,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
      {httpsOriginOwned,
       httpsOriginOwned,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
      {"host.owned.com",
       httpHostOwned,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
      {"host.owned.com",
       httpsHostOwned,
       {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}},
  };

  browsing_data_model_test_util::ValidateBrowsingDataEntries(model.get(),
                                                             expected_entries);
}

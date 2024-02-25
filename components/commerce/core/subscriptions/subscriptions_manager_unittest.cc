// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

// Build a subscription list consisting of only one subscription.
std::unique_ptr<std::vector<commerce::CommerceSubscription>> BuildSubscriptions(
    std::string subscription_id) {
  auto subscriptions =
      std::make_unique<std::vector<commerce::CommerceSubscription>>();
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, subscription_id,
      commerce::ManagementType::kUserManaged));
  return subscriptions;
}

// Return one subscription with given id.
commerce::CommerceSubscription BuildSubscription(std::string subscription_id) {
  return commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, subscription_id,
      commerce::ManagementType::kUserManaged);
}

// Check whether the passing subscription list contains exactly one subscription
// with |expected_id|.
MATCHER_P(AreExpectedSubscriptions, expected_id, "") {
  return (*arg).size() == 1 && (*arg)[0].id == expected_id;
}

}  // namespace

namespace commerce {

class MockSubscriptionsServerProxy : public SubscriptionsServerProxy {
 public:
  MockSubscriptionsServerProxy() : SubscriptionsServerProxy(nullptr, nullptr) {}
  MockSubscriptionsServerProxy(const MockSubscriptionsServerProxy&) = delete;
  MockSubscriptionsServerProxy operator=(const MockSubscriptionsServerProxy&) =
      delete;
  ~MockSubscriptionsServerProxy() override = default;

  MOCK_METHOD(void,
              Create,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               ManageSubscriptionsFetcherCallback callback),
              (override));
  MOCK_METHOD(void,
              Delete,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               ManageSubscriptionsFetcherCallback callback),
              (override));
  MOCK_METHOD(void,
              Get,
              (SubscriptionType type, GetSubscriptionsFetcherCallback callback),
              (override));

  // Mock the server responses for Create and Delete requests.
  void MockManageResponses(bool succeeded,
                           std::string subscription_id = "111") {
    ON_CALL(*this, Create)
        .WillByDefault([succeeded, subscription_id](
                           std::unique_ptr<std::vector<CommerceSubscription>>
                               subscriptions,
                           ManageSubscriptionsFetcherCallback callback) {
          std::move(callback).Run(
              succeeded ? SubscriptionsRequestStatus::kSuccess
                        : SubscriptionsRequestStatus::kServerParseError,
              BuildSubscriptions(subscription_id));
        });
    ON_CALL(*this, Delete)
        .WillByDefault([succeeded, subscription_id](
                           std::unique_ptr<std::vector<CommerceSubscription>>
                               subscriptions,
                           ManageSubscriptionsFetcherCallback callback) {
          std::move(callback).Run(
              succeeded ? SubscriptionsRequestStatus::kSuccess
                        : SubscriptionsRequestStatus::kServerParseError,
              BuildSubscriptions(subscription_id));
        });
  }

  // Mock the server fetch responses for Get requests. |subscription_id| is used
  // to generate a CommerceSubscription to be returned.
  void MockGetResponses(std::string subscription_id, bool succeeded = true) {
    ON_CALL(*this, Get)
        .WillByDefault([subscription_id, succeeded](
                           SubscriptionType type,
                           GetSubscriptionsFetcherCallback callback) {
          std::move(callback).Run(
              succeeded ? SubscriptionsRequestStatus::kSuccess
                        : SubscriptionsRequestStatus::kServerParseError,
              BuildSubscriptions(subscription_id));
        });
  }
};

class MockSubscriptionsStorage : public SubscriptionsStorage {
 public:
  MockSubscriptionsStorage() = default;
  MockSubscriptionsStorage(const MockSubscriptionsStorage&) = delete;
  MockSubscriptionsStorage operator=(const MockSubscriptionsStorage&) = delete;
  ~MockSubscriptionsStorage() override = default;

  MOCK_METHOD(void,
              GetUniqueNonExistingSubscriptions,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               GetLocalSubscriptionsCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUniqueExistingSubscriptions,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               GetLocalSubscriptionsCallback callback),
              (override));
  MOCK_METHOD(
      void,
      UpdateStorage,
      (SubscriptionType type,
       StorageOperationCallback callback,
       std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions),
      (override));
  MOCK_METHOD(
      void,
      UpdateStorageAndNotifyModifiedSubscriptions,
      (SubscriptionType type,
       StorageUpdateCallback callback,
       std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions),
      (override));
  MOCK_METHOD(void, DeleteAll, (), (override));
  MOCK_METHOD(void,
              IsSubscribed,
              (CommerceSubscription subscription,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              LoadAllSubscriptionsForType,
              (SubscriptionType type, GetLocalSubscriptionsCallback callback),
              (override));

  // Mock the local fetch responses for Get* requests. |subscription_id| is used
  // to generate a CommerceSubscription to be returned.
  void MockGetResponses(std::string subscription_id) {
    ON_CALL(*this, GetUniqueNonExistingSubscriptions)
        .WillByDefault(
            [subscription_id](std::unique_ptr<std::vector<CommerceSubscription>>
                                  subscriptions,
                              GetLocalSubscriptionsCallback callback) {
              std::move(callback).Run(BuildSubscriptions(subscription_id));
            });
    ON_CALL(*this, GetUniqueExistingSubscriptions)
        .WillByDefault(
            [subscription_id](std::unique_ptr<std::vector<CommerceSubscription>>
                                  subscriptions,
                              GetLocalSubscriptionsCallback callback) {
              std::move(callback).Run(BuildSubscriptions(subscription_id));
            });
  }

  // Mock empty local fetch responses for Get* requests.
  void MockEmptyGetResponses() {
    ON_CALL(*this, GetUniqueNonExistingSubscriptions)
        .WillByDefault([](std::unique_ptr<std::vector<CommerceSubscription>>
                              subscriptions,
                          GetLocalSubscriptionsCallback callback) {
          std::move(callback).Run(
              std::make_unique<std::vector<commerce::CommerceSubscription>>());
        });
    ON_CALL(*this, GetUniqueExistingSubscriptions)
        .WillByDefault([](std::unique_ptr<std::vector<CommerceSubscription>>
                              subscriptions,
                          GetLocalSubscriptionsCallback callback) {
          std::move(callback).Run(
              std::make_unique<std::vector<commerce::CommerceSubscription>>());
        });
  }

  // Mock the responses for UpdateStorage requests.
  void MockUpdateResponses(bool succeeded) {
    ON_CALL(*this, UpdateStorage)
        .WillByDefault(
            [succeeded](SubscriptionType type,
                        StorageOperationCallback callback,
                        std::unique_ptr<std::vector<CommerceSubscription>>
                            remote_subscriptions) {
              std::move(callback).Run(
                  succeeded ? SubscriptionsRequestStatus::kSuccess
                            : SubscriptionsRequestStatus::kStorageError);
            });
    ON_CALL(*this, UpdateStorageAndNotifyModifiedSubscriptions)
        .WillByDefault(
            [succeeded](SubscriptionType type, StorageUpdateCallback callback,
                        std::unique_ptr<std::vector<CommerceSubscription>>
                            remote_subscriptions) {
              std::move(callback).Run(
                  succeeded ? SubscriptionsRequestStatus::kSuccess
                            : SubscriptionsRequestStatus::kStorageError,
                  std::vector<CommerceSubscription>{BuildSubscription("333")},
                  std::vector<CommerceSubscription>{BuildSubscription("444")});
            });
  }

  void MockIsSubscribedResponses(bool is_subscribed) {
    ON_CALL(*this, IsSubscribed)
        .WillByDefault(
            [is_subscribed](CommerceSubscription subscription,
                            base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(is_subscribed);
            });
  }

  void MockLoadAllSubscriptionsResponses(std::string subscription_id) {
    ON_CALL(*this, LoadAllSubscriptionsForType)
        .WillByDefault(
            [subscription_id](SubscriptionType type,
                              GetLocalSubscriptionsCallback callback) {
              std::move(callback).Run(BuildSubscriptions(subscription_id));
            });
  }
};

class SubscriptionsManagerTest : public testing::Test,
                                 public commerce::SubscriptionsObserver {
 public:
  SubscriptionsManagerTest()
      : mock_server_proxy_(std::make_unique<MockSubscriptionsServerProxy>()),
        mock_storage_(std::make_unique<MockSubscriptionsStorage>()) {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
  }
  ~SubscriptionsManagerTest() override = default;

  void CreateManagerAndVerify(bool last_sync_succeeded) {
    subscriptions_manager_ = std::make_unique<SubscriptionsManager>(
        identity_test_env_.identity_manager(), std::move(mock_server_proxy_),
        std::move(mock_storage_), &account_checker_);
    ASSERT_EQ(last_sync_succeeded,
              subscriptions_manager_->GetLastSyncSucceededForTesting());
  }

  void MockHasRequestRunning(bool has_request_running) {
    subscriptions_manager_->SetHasRequestRunningForTesting(has_request_running);
  }

  void VerifyHasPendingRequests(bool has_pending_requests) {
    ASSERT_EQ(has_pending_requests,
              subscriptions_manager_->HasPendingRequestsForTesting());
  }

  void SetAccountStatus(bool signed_in, bool msbb_enabled) {
    account_checker_.SetSignedIn(signed_in);
    account_checker_.SetAnonymizedUrlDataCollectionEnabled(msbb_enabled);
  }

  void OnSubscribe(const CommerceSubscription& subscription,
                   bool succeeded) override {
    ASSERT_EQ("333", subscription.id);
    ASSERT_EQ(true, succeeded);
    on_subscribe_run_loop_.Quit();
  }

  void OnUnsubscribe(const CommerceSubscription& subscription,
                     bool succeeded) override {
    ASSERT_EQ("444", subscription.id);
    ASSERT_EQ(true, succeeded);
    on_unsubscribe_run_loop_.Quit();
  }

  void AddObserver() { subscriptions_manager_->AddObserver(this); }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList test_features_;
  MockAccountChecker account_checker_;
  std::unique_ptr<MockSubscriptionsServerProxy> mock_server_proxy_;
  std::unique_ptr<MockSubscriptionsStorage> mock_storage_;
  std::unique_ptr<SubscriptionsManager> subscriptions_manager_;
  base::HistogramTester histogram_tester;
  base::RunLoop on_subscribe_run_loop_;
  base::RunLoop on_unsubscribe_run_loop_;
};

TEST_F(SubscriptionsManagerTest, TestSyncSucceeded) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_,
              UpdateStorage(_, _, AreExpectedSubscriptions("111")))
      .Times(1);

  CreateManagerAndVerify(true);
  ASSERT_LT(0L, subscriptions_manager_->GetLastSyncTimeForTesting());
}

TEST_F(SubscriptionsManagerTest, TestSyncFailedDueToStorage) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(false);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_,
              UpdateStorage(_, _, AreExpectedSubscriptions("111")))
      .Times(1);

  CreateManagerAndVerify(false);
  ASSERT_EQ(0L, subscriptions_manager_->GetLastSyncTimeForTesting());
}

TEST_F(SubscriptionsManagerTest, TestSyncFailedDueToServer) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111", false);
  mock_storage_->MockUpdateResponses(true);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_, UpdateStorage).Times(0);

  CreateManagerAndVerify(false);
  ASSERT_EQ(0L, subscriptions_manager_->GetLastSyncTimeForTesting());
}

TEST_F(SubscriptionsManagerTest, TestNotSignedIn) {
  SetAccountStatus(false, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(0);
  EXPECT_CALL(*mock_storage_, UpdateStorage).Times(0);

  CreateManagerAndVerify(false);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  int64_t last_sync_time = subscriptions_manager_->GetLastSyncTimeForTesting();
  base::RunLoop run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();
  EXPECT_LT(last_sync_time,
            subscriptions_manager_->GetLastSyncTimeForTesting());

  histogram_tester.ExpectTotalCount(kTrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kTrackResultHistogramName, 0, 1);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_ServerManageFailed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(false);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_, UpdateStorage).Times(0);
  }

  CreateManagerAndVerify(true);
  int64_t last_sync_time = subscriptions_manager_->GetLastSyncTimeForTesting();
  base::RunLoop run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();
  EXPECT_EQ(last_sync_time,
            subscriptions_manager_->GetLastSyncTimeForTesting());

  histogram_tester.ExpectTotalCount(kTrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kTrackResultHistogramName, 1, 1);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_LastSyncFailed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(false);

  {
    InSequence s;
    // First sync.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Re-try the sync when a subscribe request comes.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(false);
  base::RunLoop run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kTrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kTrackResultHistogramName, 4, 1);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_HasRequestRunning) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")))
        .Times(1);
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(true);
  MockHasRequestRunning(true);
  bool callback_executed = false;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce([](bool* callback_executed,
                        bool succeeded) { *callback_executed = true; },
                     &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(false, callback_executed);

  histogram_tester.ExpectTotalCount(kTrackResultHistogramName, 0);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_HasStuckRequestRunning) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  MockHasRequestRunning(true);
  subscriptions_manager_->SetLastRequestStartedTimeForTesting(
      base::Time::Now() - base::Hours(1));
  base::RunLoop run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_HasPendingUnsubscribeRequest) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // Sync calls.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Unsubscribe calls.
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Delete(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Subscribe calls.
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("444"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  VerifyHasPendingRequests(false);
  // First, we set has_request_running_ as true to hold on coming Unsubscribe
  // request.
  MockHasRequestRunning(true);
  bool unsubscribe_callback_executed = false;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            *callback_executed = true;
          },
          &unsubscribe_callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  // This request won't be processed and will be held in pending_requests_.
  ASSERT_EQ(false, unsubscribe_callback_executed);
  VerifyHasPendingRequests(true);

  // Next, we set has_request_running_ as false and Subscribe. The previous
  // Unsubscribe request should be processed first and once it finishes, this
  // Subscribe request will be processed automatically.
  MockHasRequestRunning(false);
  bool subscribe_callback_executed = false;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("444"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            *callback_executed = true;
          },
          &subscribe_callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, unsubscribe_callback_executed);
  ASSERT_EQ(true, subscribe_callback_executed);
  VerifyHasPendingRequests(false);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_ExistingSubscriptions) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockEmptyGetResponses();
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_, Create).Times(0);
  }

  CreateManagerAndVerify(true);
  base::RunLoop run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kTrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kTrackResultHistogramName, 7, 1);
}

TEST_F(SubscriptionsManagerTest, TestUnsubscribe) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Delete(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  int64_t last_sync_time = subscriptions_manager_->GetLastSyncTimeForTesting();
  base::RunLoop run_loop;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  EXPECT_LT(last_sync_time,
            subscriptions_manager_->GetLastSyncTimeForTesting());

  histogram_tester.ExpectTotalCount(kUntrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kUntrackResultHistogramName, 0, 1);
}

TEST_F(SubscriptionsManagerTest, TestUnsubscribe_LastSyncFailed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(false);

  {
    InSequence s;
    // First sync.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Re-try the sync when an unsubscribe request comes.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(false);
  base::RunLoop run_loop;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kUntrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kUntrackResultHistogramName, 4, 1);
}

TEST_F(SubscriptionsManagerTest,
       TestUnsubscribe_LastSyncFailedWithRequestRunning) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(false);

  // Don't retry the sync if there is any request running.
  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")))
        .Times(1);
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(false);
  MockHasRequestRunning(true);
  bool callback_executed = false;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce([](bool* callback_executed,
                        bool succeeded) { *callback_executed = true; },
                     &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(false, callback_executed);

  histogram_tester.ExpectTotalCount(kUntrackResultHistogramName, 0);
}

TEST_F(SubscriptionsManagerTest, TestUnsubscribe_NonExistingSubscriptions) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockEmptyGetResponses();
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_, Delete).Times(0);
  }

  CreateManagerAndVerify(true);
  base::RunLoop run_loop;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            run_loop->Quit();
          },
          &run_loop));
  // The callback should eventually quit the run loop.
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kUntrackResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kUntrackResultHistogramName, 7, 1);
}

TEST_F(SubscriptionsManagerTest, TestIdentityChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // First sync on manager instantiation.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Second sync on primary account change.
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSync);
}

TEST_F(SubscriptionsManagerTest,
       TestIdentityChange_ReplaceSyncPromosWithSignInPromosEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // First sync on manager instantiation.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Second sync on primary account change.
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSignin);
}

TEST_F(SubscriptionsManagerTest, TestIsSubscribed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);
  mock_storage_->MockIsSubscribedResponses(true);

  EXPECT_CALL(*mock_storage_, IsSubscribed);

  CreateManagerAndVerify(true);
  base::RunLoop run_loop;
  subscriptions_manager_->IsSubscribed(
      BuildSubscription("111"),
      base::BindOnce(
          [](base::RunLoop* looper, bool is_subscribed) {
            EXPECT_TRUE(is_subscribed);
            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsManagerTest, TestIsSubscribed_LastSyncFailed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(false);
  mock_storage_->MockIsSubscribedResponses(true);

  {
    InSequence s;
    // First sync.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Re-try the sync when a look up request comes.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, IsSubscribed);
  }

  CreateManagerAndVerify(false);
  base::RunLoop run_loop;
  subscriptions_manager_->IsSubscribed(
      BuildSubscription("111"),
      base::BindOnce(
          [](base::RunLoop* looper, bool is_subscribed) {
            EXPECT_TRUE(is_subscribed);
            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsManagerTest, TestGetAllSubscriptions) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);
  mock_storage_->MockLoadAllSubscriptionsResponses("222");

  EXPECT_CALL(*mock_storage_, LoadAllSubscriptionsForType);

  CreateManagerAndVerify(true);
  base::RunLoop run_loop;
  subscriptions_manager_->GetAllSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* looper,
             std::vector<CommerceSubscription> subscriptions) {
            AreExpectedSubscriptions("222");
            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsManagerTest, TestGetAllSubscriptions_LastSyncFailed) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(false);
  mock_storage_->MockLoadAllSubscriptionsResponses("222");

  {
    InSequence s;
    // First sync.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Re-try the sync when a look up request comes.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, LoadAllSubscriptionsForType);
  }

  CreateManagerAndVerify(false);
  base::RunLoop run_loop;
  subscriptions_manager_->GetAllSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* looper,
             std::vector<CommerceSubscription> subscriptions) {
            AreExpectedSubscriptions("222");
            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsManagerTest,
       TestCheckTimestampOnBookmarkChange_NeedToSync) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // First sync.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Re-try the sync when local cache is outdated.
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_, UpdateStorageAndNotifyModifiedSubscriptions(
                                    _, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  AddObserver();

  int64_t last_sync_time = subscriptions_manager_->GetLastSyncTimeForTesting();
  subscriptions_manager_->CheckTimestampOnBookmarkChange(
      subscriptions_manager_->GetLastSyncTimeForTesting() + 100);

  on_subscribe_run_loop_.Run();
  on_unsubscribe_run_loop_.Run();

  EXPECT_LT(last_sync_time,
            subscriptions_manager_->GetLastSyncTimeForTesting());
}

TEST_F(SubscriptionsManagerTest,
       TestCheckTimestampOnBookmarkChange_NoNeedToSync) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);

  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_,
              UpdateStorage(_, _, AreExpectedSubscriptions("111")))
      .Times(1);
  EXPECT_CALL(*mock_storage_, UpdateStorageAndNotifyModifiedSubscriptions)
      .Times(0);

  CreateManagerAndVerify(true);
  int64_t last_sync_time = subscriptions_manager_->GetLastSyncTimeForTesting();
  subscriptions_manager_->CheckTimestampOnBookmarkChange(
      subscriptions_manager_->GetLastSyncTimeForTesting() - 100);
  EXPECT_EQ(last_sync_time,
            subscriptions_manager_->GetLastSyncTimeForTesting());
}

TEST_F(SubscriptionsManagerTest, TestSubscriptionsObserver) {
  SetAccountStatus(true, true);
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  CreateManagerAndVerify(true);
  AddObserver();

  base::RunLoop subscribe_run_loop;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](base::RunLoop* subscribe_run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            subscribe_run_loop->Quit();
          },
          &subscribe_run_loop));
  subscribe_run_loop.Run();
  on_subscribe_run_loop_.Run();

  base::RunLoop unsubscribe_run_loop;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("444"),
      base::BindOnce(
          [](base::RunLoop* unsubscribe_run_loop, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            unsubscribe_run_loop->Quit();
          },
          &unsubscribe_run_loop));
  unsubscribe_run_loop.Run();
  on_unsubscribe_run_loop_.Run();
}

}  // namespace commerce

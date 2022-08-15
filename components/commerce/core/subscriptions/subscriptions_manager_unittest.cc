// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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
  void MockManageResponses(bool succeeded) {
    ON_CALL(*this, Create)
        .WillByDefault(
            [succeeded](std::unique_ptr<std::vector<CommerceSubscription>>
                            subscriptions,
                        ManageSubscriptionsFetcherCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, Delete)
        .WillByDefault(
            [succeeded](std::unique_ptr<std::vector<CommerceSubscription>>
                            subscriptions,
                        ManageSubscriptionsFetcherCallback callback) {
              std::move(callback).Run(succeeded);
            });
  }

  // Mock the server fetch responses for Get requests. |subscription_id| is used
  // to generate a CommerceSubscription to be returned.
  void MockGetResponses(std::string subscription_id) {
    ON_CALL(*this, Get)
        .WillByDefault(
            [subscription_id](SubscriptionType type,
                              GetSubscriptionsFetcherCallback callback) {
              std::move(callback).Run(BuildSubscriptions(subscription_id));
            });
  }
};

class MockSubscriptionsStorage : public SubscriptionsStorage {
 public:
  MockSubscriptionsStorage() : SubscriptionsStorage(nullptr) {}
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
       base::OnceCallback<void(bool)> callback,
       std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions),
      (override));
  MOCK_METHOD(void, DeleteAll, (), (override));

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

  // Mock the responses for UpdateStorage requests.
  void MockUpdateResponses(bool succeeded) {
    ON_CALL(*this, UpdateStorage)
        .WillByDefault(
            [succeeded](SubscriptionType type,
                        base::OnceCallback<void(bool)> callback,
                        std::unique_ptr<std::vector<CommerceSubscription>>
                            remote_subscriptions) {
              std::move(callback).Run(succeeded);
            });
  }
};

class SubscriptionsManagerTest : public testing::Test {
 public:
  SubscriptionsManagerTest()
      : mock_server_proxy_(std::make_unique<MockSubscriptionsServerProxy>()),
        mock_storage_(std::make_unique<MockSubscriptionsStorage>()) {
    test_features_.InitAndEnableFeature(commerce::kShoppingList);
  }
  ~SubscriptionsManagerTest() override = default;

  void CreateManagerAndVerify(bool init_succeeded) {
    subscriptions_manager_ = std::make_unique<SubscriptionsManager>(
        identity_test_env_.identity_manager(), std::move(mock_server_proxy_),
        std::move(mock_storage_));
    ASSERT_EQ(init_succeeded,
              subscriptions_manager_->GetInitSucceededForTesting());
  }

  void MockHasRequestRunning(bool has_request_running) {
    subscriptions_manager_->SetHasRequestRunningForTesting(has_request_running);
  }

  void VerifyHasPendingRequests(bool has_pending_requests) {
    ASSERT_EQ(has_pending_requests,
              subscriptions_manager_->HasPendingRequestsForTesting());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList test_features_;
  std::unique_ptr<MockSubscriptionsServerProxy> mock_server_proxy_;
  std::unique_ptr<MockSubscriptionsStorage> mock_storage_;
  std::unique_ptr<SubscriptionsManager> subscriptions_manager_;
};

TEST_F(SubscriptionsManagerTest, TestInitSucceeded) {
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);
  EXPECT_CALL(*mock_storage_, DeleteAll).Times(1);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_,
              UpdateStorage(_, _, AreExpectedSubscriptions("111")))
      .Times(1);

  CreateManagerAndVerify(true);
}

TEST_F(SubscriptionsManagerTest, TestInitFailed) {
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(false);
  EXPECT_CALL(*mock_storage_, DeleteAll).Times(1);
  EXPECT_CALL(*mock_server_proxy_, Get).Times(1);
  EXPECT_CALL(*mock_storage_,
              UpdateStorage(_, _, AreExpectedSubscriptions("111")))
      .Times(1);

  CreateManagerAndVerify(false);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  bool callback_executed = false;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            *callback_executed = true;
          },
          &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_ServerManageFailed) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(false);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_server_proxy_, Get).Times(0);
    EXPECT_CALL(*mock_storage_, UpdateStorage).Times(0);
  }

  CreateManagerAndVerify(true);
  bool callback_executed = false;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            *callback_executed = true;
          },
          &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_InitFailed) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(false);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(false);
  bool callback_executed = false;
  subscriptions_manager_->Subscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            *callback_executed = true;
          },
          &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_HasRequestRunning) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
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
}

TEST_F(SubscriptionsManagerTest, TestSubscribe_HasPendingUnsubscribeRequest) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // Init calls.
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Unsubscribe calls.
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Delete(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Subscribe calls.
    EXPECT_CALL(*mock_storage_, GetUniqueNonExistingSubscriptions(
                                    AreExpectedSubscriptions("444"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Create(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_server_proxy_, Get);
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

TEST_F(SubscriptionsManagerTest, TestUnsubscribe) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions(
                                    AreExpectedSubscriptions("333"), _));
    EXPECT_CALL(*mock_server_proxy_,
                Delete(AreExpectedSubscriptions("222"), _));
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  bool callback_executed = false;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(true, succeeded);
            *callback_executed = true;
          },
          &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsManagerTest, TestUnsubscribe_InitFailed) {
  mock_server_proxy_->MockGetResponses("111");
  mock_server_proxy_->MockManageResponses(true);
  mock_storage_->MockGetResponses("222");
  mock_storage_->MockUpdateResponses(false);

  {
    InSequence s;
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    EXPECT_CALL(*mock_storage_, GetUniqueExistingSubscriptions).Times(0);
  }

  CreateManagerAndVerify(false);
  bool callback_executed = false;
  subscriptions_manager_->Unsubscribe(
      BuildSubscriptions("333"),
      base::BindOnce(
          [](bool* callback_executed, bool succeeded) {
            ASSERT_EQ(false, succeeded);
            *callback_executed = true;
          },
          &callback_executed));
  // Use a RunLoop in case the callback is posted on a different thread.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsManagerTest, TestIdentityChange) {
  mock_server_proxy_->MockGetResponses("111");
  mock_storage_->MockUpdateResponses(true);

  {
    InSequence s;
    // First init on manager instantiation.
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
    // Second init on primary account change.
    EXPECT_CALL(*mock_storage_, DeleteAll);
    EXPECT_CALL(*mock_server_proxy_, Get);
    EXPECT_CALL(*mock_storage_,
                UpdateStorage(_, _, AreExpectedSubscriptions("111")));
  }

  CreateManagerAndVerify(true);
  identity_test_env_.MakePrimaryAccountAvailable("mock_email@gmail.com",
                                                 signin::ConsentLevel::kSync);
}

}  // namespace commerce

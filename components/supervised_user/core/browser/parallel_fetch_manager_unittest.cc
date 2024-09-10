// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/parallel_fetch_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

using ::kidsmanagement::ClassifyUrlRequest;
using ::kidsmanagement::ClassifyUrlResponse;

class ParallelFetchManagerTest : public testing::Test {
 public:
  MOCK_METHOD2(Done,
               void(const ProtoFetcherStatus&,
                    std::unique_ptr<ClassifyUrlResponse>));

 protected:
  void SetUp() override {
    // Fetch process is two-phase (access token and then rpc). The test flow
    // will be controlled by releasing pending requests.
    identity_test_env_.MakePrimaryAccountAvailable(
        "bob@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  // Flips order of arguments so that the unbound arguments will be the
  // request and callback.
  static std::unique_ptr<ClassifyUrlFetcher> ClassifyURL(
      signin::IdentityManager* identity_manager,
      network::TestURLLoaderFactory& url_loader_factory,
      const ClassifyUrlRequest& request,
      ClassifyUrlFetcher::Callback callback) {
    return supervised_user::CreateClassifyURLFetcher(
        *identity_manager, url_loader_factory.GetSafeWeakWrapper(), request,
        std::move(callback), kClassifyUrlConfig);
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  base::RepeatingCallback<std::unique_ptr<ClassifyUrlFetcher>(
      const ClassifyUrlRequest&,
      ClassifyUrlFetcher::Callback)>
      factory_{base::BindRepeating(&ParallelFetchManagerTest::ClassifyURL,
                                   identity_test_env_.identity_manager(),
                                   std::ref(test_url_loader_factory_))};
  ClassifyUrlRequest request_;
  ClassifyUrlResponse response_;
};

// Tests whether two requests can be handled "in parallel" from the observer's
// point of view.
TEST_F(ParallelFetchManagerTest, HandlesMultipleRequests) {
  // Receiver's callbacks will be executed two times, once for every scheduled
  // fetch,
  EXPECT_CALL(*this, Done(::testing::_, ::testing::_)).Times(2);

  ParallelFetchManager<ClassifyUrlRequest, ClassifyUrlResponse> under_test(
      factory_);

  under_test.Fetch(request_, base::BindOnce(&ParallelFetchManagerTest::Done,
                                            base::Unretained(this)));
  under_test.Fetch(request_, base::BindOnce(&ParallelFetchManagerTest::Done,
                                            base::Unretained(this)));

  // task_environment_.RunUntilIdle() would be called from simulations.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 2L);

  // This is unblocking the pending network traffic so that EXPECT_CALL will be
  // fulfilled.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
}

// Tests whether destroying the fetch manager will also terminate all pending
// network operations.
TEST_F(ParallelFetchManagerTest, CancelsRequestsUponDestruction) {
  // Receiver's callbacks will never be executed, because the fetch manager
  // `under_test` will be gone before the responses are received.
  EXPECT_CALL(*this, Done(::testing::_, ::testing::_)).Times(0);

  {
    ParallelFetchManager<ClassifyUrlRequest, ClassifyUrlResponse> under_test(
        factory_);
    under_test.Fetch(request_, base::BindOnce(&ParallelFetchManagerTest::Done,
                                              base::Unretained(this)));
    under_test.Fetch(request_, base::BindOnce(&ParallelFetchManagerTest::Done,
                                              base::Unretained(this)));

    // Callbacks are pending on blocked network traffic.
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 2L);

    // Now under_test will go out of scope.
  }

  // Unblocking network traffic won't help executing callbacks, since their
  // parent manager `under_test` is now gone.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
      response_.SerializeAsString());
}

}  // namespace
}  // namespace supervised_user

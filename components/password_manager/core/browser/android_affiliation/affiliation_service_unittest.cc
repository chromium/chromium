// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This test focuses on functionality implemented in AffiliationService
// itself. More thorough The AffiliationBackend is tested in-depth separarately.

#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/browser/android_affiliation/fake_affiliation_api.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

const char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
const char kTestFacetURIBeta1[] = "https://one.beta.example.com";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  return {
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)},
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)},
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3)},
  };
}

}  // namespace

class AffiliationServiceTest : public testing::Test {
 public:
  AffiliationServiceTest()
      : background_task_runner_(new base::TestMockTimeTaskRunner()),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~AffiliationServiceTest() override {}

 protected:
  void DestroyService() { service_.reset(); }

  AffiliationService* service() { return service_.get(); }
  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  base::TestMockTimeTaskRunner* background_task_runner() {
    return background_task_runner_.get();
  }

  ScopedFakeAffiliationAPI* fake_affiliation_api() {
    return &fake_affiliation_api_;
  }

  base::test::TaskEnvironment task_environment_;

 private:
  // testing::Test:
  void SetUp() override {
    base::FilePath database_path;
    ASSERT_TRUE(CreateTemporaryFile(&database_path));
    network::TestNetworkConnectionTracker* network_connection_tracker =
        network::TestNetworkConnectionTracker::GetInstance();
    network_connection_tracker->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
    service_.reset(new AffiliationService(background_task_runner()));
    service_->Initialize(test_shared_loader_factory_,
                         network_connection_tracker, database_path);
    // Note: the background task runner is purposely not pumped here, so that
    // the tests also verify that the service can be used synchronously right
    // away after having been constructed.
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
  }

  void TearDown() override {
    // The service uses DeleteSoon to asynchronously destroy its backend. Pump
    // the background thread to make sure destruction actually takes place.
    DestroyService();
    background_task_runner_->RunUntilIdle();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> background_task_runner_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  ScopedFakeAffiliationAPI fake_affiliation_api_;
  MockAffiliationConsumer mock_consumer_;

  std::unique_ptr<AffiliationService> service_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationServiceTest);
};

TEST_F(AffiliationServiceTest, GetAffiliationsAndBranding) {
  // The first request allows on-demand fetching, and should trigger a fetch.
  // Then, it should succeed after the fetch is complete.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK,
      mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  fake_affiliation_api()->ServeNextRequest();

  const auto equivalence_class_alpha(GetTestEquivalenceClassAlpha());
  mock_consumer()->ExpectSuccessWithResult(equivalence_class_alpha);
  EXPECT_THAT(
      equivalence_class_alpha,
      testing::Contains(testing::Field(
          &Facet::uri, FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))));

  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // The second request should be (and can be) served from cache.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectSuccessWithResult(equivalence_class_alpha);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // The third request is also restricted to the cache, but cannot be served
  // from cache, thus it should fail.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      StrategyOnCacheMiss::FAIL, mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectFailure();
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceTest, ShutdownWhileTasksArePosted) {
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK,
      mock_consumer()->GetResultCallback());
  EXPECT_TRUE(background_task_runner()->HasPendingTask());

  DestroyService();
  background_task_runner()->RunUntilIdle();

  mock_consumer()->ExpectFailure();
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

}  // namespace password_manager

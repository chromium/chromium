// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class KidsChromeManagementClientTest : public testing::Test {
 public:
  KidsChromeManagementClientTest() = default;
  ~KidsChromeManagementClientTest() override = default;

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

 protected:
  std::unique_ptr<KidsChromeManagementClient> CreateClient() {
    return std::make_unique<KidsChromeManagementClient>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager());
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Regression test for https://crbug.com/1444748 to ensure that the destruction
// of a KidsChromeManagementClient while a loader is running does not cause a
// UAF from the callback.
TEST_F(KidsChromeManagementClientTest, CallbackDoesNotUAF) {
  std::unique_ptr<KidsChromeManagementClient> client = CreateClient();

  // Start a ClassifyURL request.
  std::unique_ptr<kids_chrome_management::ClassifyUrlRequest>
      classify_url_request =
          std::make_unique<kids_chrome_management::ClassifyUrlRequest>();
  base::MockCallback<KidsChromeManagementClient::KidsChromeManagementCallback>
      callback;
  client->ClassifyURL(std::move(classify_url_request), callback.Get());

  // Run tasks to get the signin token to begin the loader.
  task_environment().RunUntilIdle();

  ASSERT_EQ(test_url_loader_factory().NumPending(), 1)
      << "Expected one pending request";
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory().GetPendingRequest(0);

  // Destroy the client before the request is completed.
  client.reset();

  // Then finish the load request. This should not cause a UAF. Note that the
  // response doesn't matter and can therefore be incorrect.
  test_url_loader_factory().SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
}

}  // namespace

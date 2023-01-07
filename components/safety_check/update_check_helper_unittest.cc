// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/update_check_helper.h"

#include <memory>

#include "components/safety_check/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safety_check {

class UpdateCheckHelperTest : public testing::Test {
 public:
  UpdateCheckHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    update_helper_ =
        std::make_unique<UpdateCheckHelper>(shared_url_loader_factory_);
  }

  void SetExpectedResult(bool connected) { expected_ = connected; }

  void VerifyResult(bool connected) {
    EXPECT_EQ(expected_, connected);
    callback_invoked_ = true;
  }

  void VerifyCallbackInvoked(bool expected = true) {
    EXPECT_EQ(expected, callback_invoked_);
  }

  void ResetCallbackInvoked() { callback_invoked_ = false; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<UpdateCheckHelper> update_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

 private:
  bool expected_ = true;
  bool callback_invoked_ = false;
};

TEST_F(UpdateCheckHelperTest, ConnectionSuccessful) {
  SetExpectedResult(true);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, WrongHTTPCode) {
  SetExpectedResult(false);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_BAD_REQUEST));
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, TimeoutExceeded) {
  SetExpectedResult(false);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  task_environment_.FastForwardBy(base::Seconds(6));
  // Request should timeout after 5 seconds.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, Retry) {
  SetExpectedResult(true);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR));
  // Should be retried on HTTP 500.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, MultipleConnectivityChecks_AtOnce) {
  SetExpectedResult(true);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  task_environment_.FastForwardBy(base::Seconds(1));
  // Start another check 1 second later.
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  // Still only one request as the old one is replaced.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Respond to the request.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, MultipleConnectivityChecks_Sequential) {
  SetExpectedResult(false);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  // Request should timeout after 5 seconds.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_BAD_REQUEST));
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  VerifyCallbackInvoked();

  // Another check after 10 seconds - this time successful.
  ResetCallbackInvoked();
  task_environment_.FastForwardBy(base::Seconds(10));
  SetExpectedResult(true);
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kConnectivityCheckUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  VerifyCallbackInvoked();
}

TEST_F(UpdateCheckHelperTest, DestroyedWhenPending) {
  update_helper_->CheckConnectivity(base::BindOnce(
      &UpdateCheckHelperTest::VerifyResult, base::Unretained(this)));
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  update_helper_.reset();
  // Request canceled on destruction.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  VerifyCallbackInvoked(false);
}

}  // namespace safety_check

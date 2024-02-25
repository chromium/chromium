// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/psm_claim_verifier_unittest.h"
#include "chromeos/ash/components/carrier_lock/psm_claim_verifier_impl.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::carrier_lock {

namespace psm_rlwe = private_membership::rlwe;

namespace {
#define kPrivateSetBaseUrl "https://privatemembership-pa.googleapis.com"
const char kPrivateSetOprfUrl[] = kPrivateSetBaseUrl "/v1/membership:oprf";
const char kPrivateSetQueryUrl[] = kPrivateSetBaseUrl "/v1/membership:query";

const char kManufacturer[] = "Google";
const char kModel[] = "Pixel 20";
const char kSerial[] = "5CD3152701";
}  // namespace

class PsmClaimVerifierTest : public testing::Test {
 public:
  PsmClaimVerifierTest() = default;
  PsmClaimVerifierTest(const PsmClaimVerifierTest&) = delete;
  PsmClaimVerifierTest& operator=(const PsmClaimVerifierTest&) = delete;
  ~PsmClaimVerifierTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    psm_ = std::make_unique<PsmClaimVerifierImpl>(shared_factory_);
    psm_->set_testing(true);
  }

  void TearDown() override { psm_.reset(); }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<PsmClaimVerifier> psm_;
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure async_operation_completed_callback_;
};

TEST_F(PsmClaimVerifierTest, CarrierLockCheckPsmClaimSuccess) {
  std::string response;

  // Send Oprf request
  base::test::TestFuture<Result> future;
  psm_->CheckPsmClaim(kSerial, kManufacturer, kModel, future.GetCallback());

  // Send fake response to Oprf request
  base::Base64Decode(kResponseOprf, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kPrivateSetOprfUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));
  task_environment_.RunUntilIdle();

  // Send fake response to Query request
  base::Base64Decode(kResponseQuery, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kPrivateSetQueryUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Get());
  EXPECT_TRUE(psm_->GetMembership());
}

TEST_F(PsmClaimVerifierTest, CarrierLockCheckPsmClaimTwice) {
  std::string response;

  // Send Oprf request
  base::test::TestFuture<Result> future;
  psm_->CheckPsmClaim(kSerial, kManufacturer, kModel, future.GetCallback());
  psm_->CheckPsmClaim(kSerial, kManufacturer, kModel, future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kHandlerBusy, future.Get());
  EXPECT_FALSE(psm_->GetMembership());
}

TEST_F(PsmClaimVerifierTest, CarrierLockCheckPsmClaimFailedOprf) {
  // Send Oprf request
  base::test::TestFuture<Result> future;
  psm_->CheckPsmClaim(kSerial, kManufacturer, kModel, future.GetCallback());

  // Send empty response to Oprf request
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kPrivateSetOprfUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string()));

  // Wait for callback
  EXPECT_EQ(Result::kCreateQueryRequestFailed, future.Get());
  EXPECT_FALSE(psm_->GetMembership());
}

TEST_F(PsmClaimVerifierTest, CarrierLockCheckPsmClaimFailedQuery) {
  std::string response;

  // Send Oprf request
  base::test::TestFuture<Result> future;
  psm_->CheckPsmClaim(kSerial, kManufacturer, kModel, future.GetCallback());

  // Send fake response to Oprf request
  base::Base64Decode(kResponseOprf, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kPrivateSetOprfUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));
  task_environment_.RunUntilIdle();

  // Send empty response to Query request
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kPrivateSetQueryUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string()));

  // Wait for callback
  EXPECT_EQ(Result::kInvalidQueryReply, future.Get());
  EXPECT_FALSE(psm_->GetMembership());
}

}  // namespace ash::carrier_lock

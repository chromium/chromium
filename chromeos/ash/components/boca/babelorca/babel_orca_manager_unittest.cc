// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

const std::string kTachyonToken = "tachyon-token";
const std::string kSessionId = "session-id";
const std::string kSenderEmail = "user@email.com";

class BabelOrcaManagerTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSignin);
  }

  void AddSuccessfulSigninGaiaResponse() {
    babelorca::SignInGaiaResponse response;
    response.mutable_auth_token()->set_payload(kTachyonToken);
    url_loader_factory_.AddResponse(babelorca::kSigninGaiaUrl,
                                    response.SerializeAsString());
  }

  void AddFailedSigninGaiaResponse() {
    url_loader_factory_.AddResponse(
        GURL(babelorca::kSigninGaiaUrl), network::mojom::URLResponseHead::New(),
        /*content=*/"",
        network::URLLoaderCompletionStatus(net::Error::ERR_NETWORK_CHANGED));
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo account_info_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(BabelOrcaManagerTest, SigninToTachyonAndRespondWithSuccess) {
  base::test::TestFuture<bool> test_future;
  BabelOrcaManager manager(/*translation_dispatcher=*/nullptr,
                           identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper());
  AddSuccessfulSigninGaiaResponse();

  manager.SigninToTachyonAndRespond(test_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(test_future.Get());
  ASSERT_TRUE(manager.tachyon_token().has_value());
  EXPECT_EQ(manager.tachyon_token().value(), kTachyonToken);

  manager.OnSessionEnded(kSessionId);
  EXPECT_FALSE(manager.tachyon_token().has_value());
}

TEST_F(BabelOrcaManagerTest, SigninToTachyonAndRespondWithFailure) {
  base::test::TestFuture<bool> test_future;
  BabelOrcaManager manager(/*translation_dispatcher=*/nullptr,
                           identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper());
  AddFailedSigninGaiaResponse();

  manager.SigninToTachyonAndRespond(test_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  EXPECT_FALSE(test_future.Get());
  EXPECT_FALSE(manager.tachyon_token().has_value());
}

TEST_F(BabelOrcaManagerTest, DataSetAtSessionStart) {
  base::test::TestFuture<bool> test_future;
  BabelOrcaManager manager(/*translation_dispatcher=*/nullptr,
                           identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper());
  ::boca::UserIdentity producer;
  producer.set_email(kSenderEmail);
  manager.OnSessionStarted(kSessionId, producer);

  ASSERT_TRUE(manager.session_id().has_value());
  EXPECT_EQ(manager.session_id().value(), kSessionId);
  ASSERT_TRUE(manager.sender_email().has_value());
  EXPECT_EQ(manager.sender_email().value(), kSenderEmail);

  manager.OnSessionEnded(kSessionId);
  EXPECT_FALSE(manager.session_id().has_value());
  EXPECT_FALSE(manager.sender_email().has_value());
}

}  // namespace
}  // namespace ash::boca

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

const std::string kTachyonToken = "tachyon-token";
const std::string kSessionId = "session-id";
const std::string kSenderEmail = "user@email.com";
const std::string kGroupId = "tachyon-group-id";

class MockBabelOrcaController : public babelorca::BabelOrcaController {
 public:
  MockBabelOrcaController() = default;
  ~MockBabelOrcaController() override = default;
  MOCK_METHOD(void, OnSessionStarted, (), (override));
  MOCK_METHOD(void, OnSessionEnded, (), (override));
  MOCK_METHOD(void, OnSessionCaptionConfigUpdated, (bool, bool), (override));
  MOCK_METHOD(void, OnLocalCaptionConfigUpdated, (bool), (override));
};

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
  babelorca::TachyonRequestDataProvider* request_data_provider;
  auto controller_factory = base::BindLambdaForTesting(
      [&request_data_provider](
          babelorca::TokenManager*,
          babelorca::TachyonRequestDataProvider* data_provider)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        request_data_provider = data_provider;
        return std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));
  AddSuccessfulSigninGaiaResponse();

  manager.SigninToTachyonAndRespond(test_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(test_future.Get());
  ASSERT_TRUE(manager.tachyon_token().has_value());
  EXPECT_EQ(request_data_provider, &manager);
  EXPECT_EQ(manager.tachyon_token().value(), kTachyonToken);
}

TEST_F(BabelOrcaManagerTest, SigninToTachyonAndRespondWithFailure) {
  base::test::TestFuture<bool> test_future;
  auto controller_factory = base::BindLambdaForTesting(
      [](babelorca::TokenManager*, babelorca::TachyonRequestDataProvider*)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        return std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));

  AddFailedSigninGaiaResponse();

  manager.SigninToTachyonAndRespond(test_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  EXPECT_FALSE(test_future.Get());
  EXPECT_FALSE(manager.tachyon_token().has_value());
}

TEST_F(BabelOrcaManagerTest, OnSessionStarted) {
  base::test::TestFuture<bool> test_future;
  MockBabelOrcaController* controller_ptr;
  auto controller_factory = base::BindLambdaForTesting(
      [&controller_ptr](babelorca::TokenManager*,
                        babelorca::TachyonRequestDataProvider*)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        auto controller =
            std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
        controller_ptr = controller.get();
        return controller;
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));

  ::boca::UserIdentity producer;
  producer.set_email(kSenderEmail);
  EXPECT_CALL(*controller_ptr, OnSessionStarted).Times(1);
  manager.OnSessionStarted(kSessionId, producer);

  ASSERT_TRUE(manager.session_id().has_value());
  EXPECT_EQ(manager.session_id().value(), kSessionId);
  ASSERT_TRUE(manager.sender_email().has_value());
  EXPECT_EQ(manager.sender_email().value(), kSenderEmail);
}

TEST_F(BabelOrcaManagerTest, OnSessionEnded) {
  base::test::TestFuture<bool> test_future;
  MockBabelOrcaController* controller_ptr;
  auto controller_factory = base::BindLambdaForTesting(
      [&controller_ptr](babelorca::TokenManager*,
                        babelorca::TachyonRequestDataProvider* data_provider)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        auto controller =
            std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
        controller_ptr = controller.get();
        return controller;
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));
  ::boca::UserIdentity producer;
  // Set email and session id.
  producer.set_email(kSenderEmail);
  manager.OnSessionStarted(kSessionId, producer);

  // Set Tachyon token.
  AddSuccessfulSigninGaiaResponse();
  manager.SigninToTachyonAndRespond(test_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  // Set group id.
  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  manager.OnSessionCaptionConfigUpdated("boca group", captions_config,
                                        kGroupId);

  EXPECT_CALL(*controller_ptr, OnSessionEnded).Times(1);
  manager.OnSessionEnded(kSessionId);

  EXPECT_FALSE(manager.session_id().has_value());
  EXPECT_FALSE(manager.sender_email().has_value());
  EXPECT_FALSE(manager.group_id().has_value());
  EXPECT_FALSE(manager.tachyon_token().has_value());
}

TEST_F(BabelOrcaManagerTest, OnSessionCaptionConfigUpdated) {
  base::test::TestFuture<bool> test_future;
  MockBabelOrcaController* controller_ptr;
  auto controller_factory = base::BindLambdaForTesting(
      [&controller_ptr](babelorca::TokenManager*,
                        babelorca::TachyonRequestDataProvider* data_provider)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        auto controller =
            std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
        controller_ptr = controller.get();
        return controller;
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));
  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  EXPECT_CALL(*controller_ptr, OnSessionCaptionConfigUpdated(true, false))
      .Times(1);
  manager.OnSessionCaptionConfigUpdated("boca group", captions_config,
                                        kGroupId);

  ASSERT_TRUE(manager.group_id().has_value());
  EXPECT_EQ(manager.group_id().value(), kGroupId);

  captions_config.set_captions_enabled(false);
  EXPECT_CALL(*controller_ptr, OnSessionCaptionConfigUpdated(false, false))
      .Times(1);
  manager.OnSessionCaptionConfigUpdated("boca group", captions_config,
                                        kGroupId);
}

TEST_F(BabelOrcaManagerTest, OnLocalCaptionConfigUpdated) {
  base::test::TestFuture<bool> test_future;
  MockBabelOrcaController* controller_ptr;
  auto controller_factory = base::BindLambdaForTesting(
      [&controller_ptr](babelorca::TokenManager*,
                        babelorca::TachyonRequestDataProvider* data_provider)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        auto controller =
            std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
        controller_ptr = controller.get();
        return controller;
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));
  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  EXPECT_CALL(*controller_ptr, OnLocalCaptionConfigUpdated(true)).Times(1);
  manager.OnLocalCaptionConfigUpdated(captions_config);

  captions_config.set_captions_enabled(false);
  EXPECT_CALL(*controller_ptr, OnLocalCaptionConfigUpdated(false)).Times(1);
  manager.OnLocalCaptionConfigUpdated(captions_config);
}
TEST_F(BabelOrcaManagerTest, RequestDataProviderIsTheManager) {
  base::test::TestFuture<bool> test_future;
  babelorca::TachyonRequestDataProvider* request_data_provider;
  auto controller_factory = base::BindLambdaForTesting(
      [&request_data_provider](
          babelorca::TokenManager*,
          babelorca::TachyonRequestDataProvider* data_provider)
          -> std::unique_ptr<babelorca::BabelOrcaController> {
        request_data_provider = data_provider;
        return std::make_unique<testing::NiceMock<MockBabelOrcaController>>();
      });
  BabelOrcaManager manager(identity_test_env_.identity_manager(),
                           url_loader_factory_.GetSafeWeakWrapper(),
                           std::move(controller_factory));

  EXPECT_EQ(&manager, request_data_provider);
}

}  // namespace
}  // namespace ash::boca

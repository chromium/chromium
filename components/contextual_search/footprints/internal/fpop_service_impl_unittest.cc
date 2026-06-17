// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/footprints/internal/fpop_service_impl.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_search {

namespace {

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kTestAppId[] = "test_app";
constexpr char kTestToken[] = "token";
constexpr char kGetFacsUrl[] =
    "https://footprints-pa.googleapis.com/v1/get_facs";
constexpr char kUpdateActivityControlsSettingsUrl[] =
    "https://footprints-pa.googleapis.com/v1/"
    "update_activity_controls_settings";

class FpopServiceImplTest : public testing::Test {
 protected:
  FpopServiceImplTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(FpopServiceImplTest, SendsRequests) {
  FpopServiceImpl service(identity_test_env_.identity_manager(),
                          shared_url_loader_factory_);

  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSignin);

  {
    footprints::oneplatform::GetFacsRequest request;
    request.mutable_header()->set_application_id(kTestAppId);

    bool callback_called = false;
    service.GetFacs(
        request,
        base::BindOnce(
            [](bool* called, bool success,
               const footprints::oneplatform::GetFacsResponse& response) {
              *called = true;
            },
            &callback_called));

    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        kTestToken, base::Time::Max());

    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(pending_request->request.url, kGetFacsUrl);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url.spec(), "");

    EXPECT_TRUE(callback_called);
  }

  {
    footprints::oneplatform::UpdateActivityControlsSettingsRequest request;

    bool callback_called = false;
    service.UpdateActivityControlsSettings(
        request, base::BindOnce(
                     [](bool* called, bool success,
                        const footprints::oneplatform::
                            UpdateActivityControlsSettingsResponse& response) {
                       *called = true;
                     },
                     &callback_called));

    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        kTestToken, base::Time::Max());

    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(pending_request->request.url, kUpdateActivityControlsSettingsUrl);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url.spec(), "");

    EXPECT_TRUE(callback_called);
  }
}

TEST_F(FpopServiceImplTest, GetFacs_AuthError) {
  FpopServiceImpl service(identity_test_env_.identity_manager(),
                          shared_url_loader_factory_);

  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSignin);

  footprints::oneplatform::GetFacsRequest request;
  request.mutable_header()->set_application_id(kTestAppId);

  bool callback_called = false;
  bool result_success = true;
  service.GetFacs(
      request,
      base::BindOnce(
          [](bool* called, bool* success_out, bool success,
             const footprints::oneplatform::GetFacsResponse& response) {
            *called = true;
            *success_out = success;
          },
          &callback_called, &result_success));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_success);
}

TEST_F(FpopServiceImplTest, GetFacs_NetworkError) {
  FpopServiceImpl service(identity_test_env_.identity_manager(),
                          shared_url_loader_factory_);

  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSignin);

  footprints::oneplatform::GetFacsRequest request;
  request.mutable_header()->set_application_id(kTestAppId);

  bool callback_called = false;
  bool result_success = true;
  service.GetFacs(
      request,
      base::BindOnce(
          [](bool* called, bool* success_out, bool success,
             const footprints::oneplatform::GetFacsResponse& response) {
            *called = true;
            *success_out = success;
          },
          &callback_called, &result_success));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestToken, base::Time::Max());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_BAD_REQUEST);

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_success);
}

}  // namespace

}  // namespace contextual_search

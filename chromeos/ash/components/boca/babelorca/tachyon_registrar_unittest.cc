// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {

constexpr char kClientUuid[] = "client-uuid";
constexpr char kTachyonToken[] = "tachyon-token";

TEST(TachyonRegistrarTest, SuccessfulRegistration) {
  base::test::TaskEnvironment task_env;
  base::test::TestFuture<bool> test_future;
  FakeTachyonAuthedClient authed_client;
  TachyonRegistrar registrar(&authed_client, TRAFFIC_ANNOTATION_FOR_TESTS);

  registrar.Register(kClientUuid, test_future.GetCallback());
  SignInGaiaResponse signin_response;
  signin_response.mutable_auth_token()->set_payload(kTachyonToken);
  authed_client.ExecuteResponseCallback(TachyonResponse(
      net::OK, net::HttpStatusCode::HTTP_OK,
      std::make_unique<std::string>(signin_response.SerializeAsString())));

  EXPECT_TRUE(test_future.Get());
  std::optional<std::string> tachyon_token = registrar.GetTachyonToken();
  ASSERT_TRUE(tachyon_token.has_value());
  EXPECT_THAT(tachyon_token.value(), testing::StrEq(kTachyonToken));
}

TEST(TachyonRegistrarTest, FailedRegistration) {
  base::test::TaskEnvironment task_env;
  base::test::TestFuture<bool> test_future;
  FakeTachyonAuthedClient authed_client;
  TachyonRegistrar registrar(&authed_client, TRAFFIC_ANNOTATION_FOR_TESTS);

  registrar.Register(kClientUuid, test_future.GetCallback());
  authed_client.ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  EXPECT_FALSE(test_future.Get());
  std::optional<std::string> tachyon_token = registrar.GetTachyonToken();
  EXPECT_FALSE(tachyon_token.has_value());
}

}  // namespace ash::babelorca

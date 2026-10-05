// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/request_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

namespace {

std::unique_ptr<PermissionRequest> CreateRequest(
    RequestType request_type,
    const GURL& requesting_origin,
    std::optional<GeolocationPromptType> prompt_type = std::nullopt) {
  auto data = std::make_unique<PermissionRequestData>(
      request_type, /*user_gesture=*/true, requesting_origin);
  if (prompt_type.has_value()) {
    data->WithGeolocationPromptType(*prompt_type);
  }
  return std::make_unique<PermissionRequest>(std::move(data),
                                             base::DoNothing());
}

}  // namespace

class PermissionRequestTest : public testing::Test {};

TEST_F(PermissionRequestTest, IsDuplicateOf_SameOriginAndType) {
  const GURL origin("https://example.com");
  auto request1 = CreateRequest(RequestType::kNotifications, origin);
  auto request2 = CreateRequest(RequestType::kNotifications, origin);

  EXPECT_TRUE(request1->IsDuplicateOf(request2.get()));
  EXPECT_TRUE(request2->IsDuplicateOf(request1.get()));
}

TEST_F(PermissionRequestTest, IsDuplicateOf_DifferentOrigin) {
  auto request1 =
      CreateRequest(RequestType::kGeolocation, GURL("https://example.com"),
                    GeolocationPromptType::kApproximateOnly);
  auto request2 =
      CreateRequest(RequestType::kGeolocation, GURL("https://other.com"),
                    GeolocationPromptType::kApproximateOnly);

  EXPECT_FALSE(request1->IsDuplicateOf(request2.get()));
  EXPECT_FALSE(request2->IsDuplicateOf(request1.get()));
}

TEST_F(PermissionRequestTest, IsDuplicateOf_DifferentRequestType) {
  const GURL origin("https://example.com");
  auto request1 = CreateRequest(RequestType::kGeolocation, origin);
  auto request2 = CreateRequest(RequestType::kNotifications, origin);

  EXPECT_FALSE(request1->IsDuplicateOf(request2.get()));
  EXPECT_FALSE(request2->IsDuplicateOf(request1.get()));
}

TEST_F(PermissionRequestTest, IsDuplicateOf_GeolocationSamePromptType) {
  const GURL origin("https://example.com");

  for (const auto geolocation_prompt_type :
       std::initializer_list<std::optional<GeolocationPromptType>>{
           GeolocationPromptType::kApproximateOnly,
           GeolocationPromptType::kUpgradeToPrecise,
           GeolocationPromptType::kApproximateOrPrecise, std::nullopt}) {
    auto request1 = CreateRequest(RequestType::kGeolocation, origin,
                                  geolocation_prompt_type);
    auto request2 = CreateRequest(RequestType::kGeolocation, origin,
                                  geolocation_prompt_type);
    EXPECT_TRUE(request1->IsDuplicateOf(request2.get()));
    EXPECT_TRUE(request2->IsDuplicateOf(request1.get()));
  }
}

TEST_F(PermissionRequestTest, IsDuplicateOf_GeolocationDifferentPromptType) {
  const GURL origin("https://example.com");

  auto req_approx = CreateRequest(RequestType::kGeolocation, origin,
                                  GeolocationPromptType::kApproximateOnly);
  auto req_upgrade = CreateRequest(RequestType::kGeolocation, origin,
                                   GeolocationPromptType::kUpgradeToPrecise);
  auto req_both = CreateRequest(RequestType::kGeolocation, origin,
                                GeolocationPromptType::kApproximateOrPrecise);
  auto req_none =
      CreateRequest(RequestType::kGeolocation, origin, std::nullopt);

  // A precise/upgrade request must not be coalesced with an approximate-only
  // request (https://issues.chromium.org/issues/569012366).
  EXPECT_FALSE(req_approx->IsDuplicateOf(req_upgrade.get()));
  EXPECT_FALSE(req_upgrade->IsDuplicateOf(req_approx.get()));

  EXPECT_FALSE(req_approx->IsDuplicateOf(req_both.get()));
  EXPECT_FALSE(req_both->IsDuplicateOf(req_approx.get()));

  EXPECT_FALSE(req_upgrade->IsDuplicateOf(req_both.get()));
  EXPECT_FALSE(req_both->IsDuplicateOf(req_upgrade.get()));

  EXPECT_FALSE(req_approx->IsDuplicateOf(req_none.get()));
  EXPECT_FALSE(req_none->IsDuplicateOf(req_approx.get()));

  EXPECT_FALSE(req_upgrade->IsDuplicateOf(req_none.get()));
  EXPECT_FALSE(req_none->IsDuplicateOf(req_upgrade.get()));

  EXPECT_FALSE(req_both->IsDuplicateOf(req_none.get()));
  EXPECT_FALSE(req_none->IsDuplicateOf(req_both.get()));
}

}  // namespace permissions

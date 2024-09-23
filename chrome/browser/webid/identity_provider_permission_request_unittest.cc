// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_permission_request.h"

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class IdentityProviderPermissionRequestTest : public testing::Test {
 public:
  IdentityProviderPermissionRequestTest() = default;
  ~IdentityProviderPermissionRequestTest() override = default;
  IdentityProviderPermissionRequestTest(
      IdentityProviderPermissionRequestTest&) = delete;
  IdentityProviderPermissionRequestTest& operator=(
      IdentityProviderPermissionRequestTest&) = delete;

  void SetUp() override {}
};

TEST_F(IdentityProviderPermissionRequestTest, PermissionGranted) {
  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(true)).WillOnce(testing::Return());
  auto* request = new IdentityProviderPermissionRequest(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());
  request->PermissionGranted(/**is_one_time=*/false);
  request->RequestFinished();
}

TEST_F(IdentityProviderPermissionRequestTest, PermissionDenied) {
  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  auto* request = new IdentityProviderPermissionRequest(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());
  request->PermissionDenied();
  request->RequestFinished();
}

TEST_F(IdentityProviderPermissionRequestTest, PermissionCancelled) {
  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  auto* request = new IdentityProviderPermissionRequest(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());
  request->Cancelled();
  request->RequestFinished();
}

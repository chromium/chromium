// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class FederatedIdentityPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityPermissionContextTest() = default;
  ~FederatedIdentityPermissionContextTest() override = default;
  FederatedIdentityPermissionContextTest(
      FederatedIdentityPermissionContextTest&) = delete;
  FederatedIdentityPermissionContextTest& operator=(
      FederatedIdentityPermissionContextTest&) = delete;

  void SetUp() override {
    context_ =
        FederatedIdentityPermissionContextFactory::GetForProfile(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  raw_ptr<FederatedIdentityPermissionContext, DanglingUntriaged> context_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentityPermissionContextTest, RevokeSharingPermission) {
  const url::Origin kRequester =
      url::Origin::Create(GURL("https://requester.com"));
  const url::Origin kEmbedder =
      url::Origin::Create(GURL("https://embedder.com"));
  const url::Origin kIdentityProvider =
      url::Origin::Create(GURL("https://idp.com"));
  constexpr std::string kAccountId = "accountId";

  EXPECT_FALSE(context_->GetLastUsedTimestamp(kRequester, kEmbedder,
                                              kIdentityProvider, kAccountId));
  EXPECT_FALSE(context_->HasSharingPermission(kRequester));

  context_->GrantSharingPermission(kRequester, kEmbedder, kIdentityProvider,
                                   kAccountId);
  EXPECT_TRUE(context_->GetLastUsedTimestamp(kRequester, kEmbedder,
                                             kIdentityProvider, kAccountId));
  EXPECT_TRUE(context_->HasSharingPermission(kRequester));
  EXPECT_FALSE(context_->HasSharingPermission(kEmbedder));

  context_->RevokeSharingPermission(kRequester, kEmbedder, kIdentityProvider,
                                    kAccountId);
  EXPECT_FALSE(context_->GetLastUsedTimestamp(kRequester, kEmbedder,
                                              kIdentityProvider, kAccountId));
  EXPECT_FALSE(context_->HasSharingPermission(kRequester));
}

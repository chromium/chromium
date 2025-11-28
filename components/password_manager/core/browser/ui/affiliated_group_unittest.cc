// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/affiliated_group.h"

#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class AffiliatedGroupTest : public testing::Test {
 public:
  AffiliatedGroupTest() = default;

  void EnableSyncForTestAccount() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
  }

  void DisableSyncFeature() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
  }

 protected:
  syncer::TestSyncService sync_service_;
};

TEST_F(AffiliatedGroupTest,
       GetGroupIconUrl_Syncing_ReturnsAffiliationBrandingIcon) {
  EnableSyncForTestAccount();
  PasswordForm form;
  form.url = GURL("https://url.com");
  affiliations::FacetBrandingInfo branding_info("Group name",
                                                GURL("https://icon_url.com"));
  AffiliatedGroup group({password_manager::CredentialUIEntry(form)},
                        branding_info);

  EXPECT_EQ(group.GetAllowedIconUrl(&sync_service_),
            GURL("https://icon_url.com"));
}

TEST_F(AffiliatedGroupTest, GetGroupIconUrl_NotSyncing_ReturnsFallbackIcon) {
  DisableSyncFeature();
  PasswordForm form;
  form.url = GURL("https://url.com");
  affiliations::FacetBrandingInfo branding_info("Group name",
                                                GURL("https://icon_url.com"));
  AffiliatedGroup group({password_manager::CredentialUIEntry(form)},
                        branding_info);

  EXPECT_EQ(group.GetAllowedIconUrl(&sync_service_),
            GURL("https://url.com/favicon.ico"));
}

TEST_F(AffiliatedGroupTest,
       GetGroupIconUrl_CustonPassphrase_ReturnsFallbackIcon) {
  sync_service_.SetIsUsingExplicitPassphrase(true);
  PasswordForm form;
  form.url = GURL("https://url.com");
  affiliations::FacetBrandingInfo branding_info("Group name",
                                                GURL("https://icon_url.com"));
  AffiliatedGroup group({password_manager::CredentialUIEntry(form)},
                        branding_info);

  EXPECT_EQ(group.GetAllowedIconUrl(&sync_service_),
            GURL("https://url.com/favicon.ico"));
}

}  // namespace password_manager

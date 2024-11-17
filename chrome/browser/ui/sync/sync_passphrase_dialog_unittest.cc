// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"

#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SyncPassphraseDialog, SyncPassphraseDialogDecryptData) {
  constexpr char kPassphraseUTF8[] = "passphrase";
  // `base::UTF8ToUTF16()` cannot be called on string literals, so a duplicate
  // string is needed.
  constexpr char16_t kPassphraseUTF16[] = u"passphrase";
  syncer::TestSyncService sync_service;
  syncer::TestSyncUserSettings* sync_user_settings =
      sync_service.GetUserSettings();
  sync_user_settings->SetPassphraseRequired(kPassphraseUTF8);
  ASSERT_TRUE(sync_user_settings->IsPassphraseRequired());

  // Wrong passphrase.
  EXPECT_FALSE(SyncPassphraseDialogDecryptData(&sync_service, u"wrong"));
  EXPECT_TRUE(sync_user_settings->IsPassphraseRequired());

  // Correct passphrase.
  EXPECT_TRUE(SyncPassphraseDialogDecryptData(&sync_service, kPassphraseUTF16));
  EXPECT_FALSE(sync_user_settings->IsPassphraseRequired());
}

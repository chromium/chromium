// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_test_utils.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "ui/gfx/image/image_unittest_util.h"

void SignInAndEnableAccountBookmarkNodes(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "foo@gmail.com", signin::ConsentLevel::kSignin);
  signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                    "https://avatar.com/avatar.png",
                                    gfx::test::CreateImage(/*size=*/32));
  // Normally done by sync, but sync is not fully up in this test.
  BookmarkModelFactory::GetForBrowserContext(profile)
      ->CreateAccountPermanentFolders();
}

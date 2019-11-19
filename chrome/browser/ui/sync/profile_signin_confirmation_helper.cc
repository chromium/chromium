// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/signin_confirmation_helper.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

using bookmarks::BookmarkModel;

namespace {

const int kHistoryEntriesBeforeNewProfilePrompt = 10;

bool HasBookmarks(Profile* profile) {
  BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bool has_bookmarks = bookmarks && bookmarks->HasBookmarks();
  if (has_bookmarks)
    DVLOG(1) << "SigninConfirmationHelper: profile contains bookmarks";
  return has_bookmarks;
}

}  // namespace

namespace ui {

SkColor GetSigninConfirmationPromptBarColor(ui::NativeTheme* theme,
                                            SkAlpha alpha) {
  static const SkColor kBackgroundColor =
      theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);
  return color_utils::BlendTowardMaxContrast(kBackgroundColor, alpha);
}

bool HasBeenShutdown(Profile* profile) {
  bool has_been_shutdown = !profile->IsNewProfile();
  if (has_been_shutdown)
    DVLOG(1) << "ProfileSigninConfirmationHelper: profile is not new";
  return has_been_shutdown;
}

bool HasSyncedExtensions(Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (registry) {
    for (const scoped_refptr<const extensions::Extension>& extension :
         registry->enabled_extensions()) {
      // The webstore is synced so that it stays put on the new tab
      // page, but since it's installed by default we don't want to
      // consider it when determining if the profile is dirty.
      if (extensions::sync_helper::IsSyncable(extension.get()) &&
          !extensions::sync_helper::IsSyncableComponentExtension(
              extension.get())) {
        DVLOG(1) << "ProfileSigninConfirmationHelper: "
                 << "profile contains a synced extension: " << extension->id();
        return true;
      }
    }
  }
#endif
  return false;
}

void CheckShouldPromptForNewProfile(
    Profile* profile,
    base::OnceCallback<void(bool)> return_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (HasBeenShutdown(profile) ||
      HasBookmarks(profile) ||
      HasSyncedExtensions(profile)) {
    std::move(return_result).Run(true);
    return;
  }
  history::HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  // Fire asynchronous queries for profile data.
  browser_sync::SigninConfirmationHelper* helper =
      new browser_sync::SigninConfirmationHelper(service,
                                                 std::move(return_result));
  helper->CheckHasHistory(kHistoryEntriesBeforeNewProfilePrompt);
  helper->CheckHasTypedURLs();
}

ProfileSigninConfirmationDelegate::~ProfileSigninConfirmationDelegate() {}

}  // namespace ui

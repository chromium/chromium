// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "extensions/common/constants.h"

base::span<StaticAppId> GetDefaultPinnedApps() {
  constexpr const char* kDefaultPinnedApps[] = {
      extension_misc::kGMailAppId,
      web_app::kGmailAppId,

      web_app::kGoogleCalendarAppId,

      extension_misc::kFilesManagerAppId,

      web_app::kMessagesAppId,

      arc::kPlayStoreAppId,

      extension_misc::kYoutubeAppId,
      web_app::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };
  return base::span<StaticAppId>(kDefaultPinnedApps,
                                 base::size(kDefaultPinnedApps));
}

base::span<StaticAppId> GetTabletFormFactorDefaultPinnedApps() {
  constexpr const char* kTabletFormFactorDefaultPinnedApps[] = {
      arc::kGmailAppId,

      arc::kGoogleCalendarAppId,

      arc::kPlayStoreAppId,

      arc::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };
  return base::span<StaticAppId>(
      kTabletFormFactorDefaultPinnedApps,
      base::size(kTabletFormFactorDefaultPinnedApps));
}

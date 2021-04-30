// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

base::span<StaticAppId> GetDefaultPinnedApps() {
  if (!base::FeatureList::IsEnabled(features::kDefaultPinnedAppsUpdate2021Q2)) {
    constexpr const char* kLegacyDefaultPinnedApps[] = {
        extension_misc::kFilesManagerAppId,

        extension_misc::kGmailAppId,
        web_app::kGmailAppId,

        extension_misc::kGoogleDocAppId,
        web_app::kGoogleDocsAppId,

        extension_misc::kYoutubeAppId,
        web_app::kYoutubeAppId,

        arc::kPlayStoreAppId,
    };
    return base::span<StaticAppId>(kLegacyDefaultPinnedApps,
                                   base::size(kLegacyDefaultPinnedApps));
  }

  constexpr const char* kDefaultPinnedApps[] = {
      extension_misc::kGmailAppId,
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
  if (!base::FeatureList::IsEnabled(features::kDefaultPinnedAppsUpdate2021Q2)) {
    constexpr const char* kLegacyTabletFormFactorDefaultPinnedApps[] = {
        extension_misc::kFilesManagerAppId,

        arc::kGmailAppId,

        extension_misc::kGoogleDocAppId,

        arc::kYoutubeAppId,

        arc::kPlayStoreAppId,
    };
    return base::span<StaticAppId>(
        kLegacyTabletFormFactorDefaultPinnedApps,
        base::size(kLegacyTabletFormFactorDefaultPinnedApps));
  }

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

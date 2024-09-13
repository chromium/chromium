// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_utils.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "url/gurl.h"

namespace growth {

namespace {

// The mapping of `app_id` or URL `domain` to `app_group_id`.
const auto& GetAppGroupIdMap() {
  // TODO: b/341721256 - Get the app ids from their constants files.
  // PWA:
  static constexpr char kGoogleDocsAppIdPwa[] =
      "mpnpojknpmmopombnjdcgaaiekajbnjb";
  static constexpr char kGoogleDriveAppIdPwa[] =
      "aghbiahbpaijignceidepookljebhfak";
  static constexpr char kGmailAppIdPwa[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";
  static constexpr char kGooglePhotosAppIdPwa[] =
      "ncmjhecbjeaamljdfahankockkkdmedg";

  // ARC:
  static constexpr char kGoogleDocsAppIdArc[] =
      "cgiadblnmjkjbhignimpegeiplgoidhe";
  static constexpr char kGoogleDriveAppIdArc[] =
      "ljmhbofhbaapdhebeafbhlcapoiipfbi";
  static constexpr char kGmailAppIdArc[] = "hhkfkjpmacfncmbapfohfocpjpdnobjg";
  static constexpr char kGooglePhotosAppIdArc[] =
      "fdbkkojdbojonckghlanfaopfakedeca";

  // Domain name:
  static constexpr char kGoogleDocsAppDomain[] = "docs.google.com";
  static constexpr char kGoogleDriveAppDomain[] = "drive.google.com";
  static constexpr char kGmailAppDomain[] = "mail.google.com";
  static constexpr char kGooglePhotosAppDomain[] = "photos.google.com";

  // A list of supported apps group events.
  // NOTE: An app can be grouped in multiple groups.
  static constexpr char kGoogleDocsOpenedEvent[] = "DocsOpened";
  static constexpr char kGoogleDriveOpenedEvent[] = "DriveOpened";
  static constexpr char kGmailOpenedEvent[] = "GmailOpened";
  static constexpr char kGooglePhotosOpenedEvent[] = "PhotosOpened";

  static constexpr auto kAppGroupIdMap =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          // Docs:
          {kGoogleDocsAppIdPwa, kGoogleDocsOpenedEvent},
          {kGoogleDocsAppIdArc, kGoogleDocsOpenedEvent},
          {kGoogleDocsAppDomain, kGoogleDocsOpenedEvent},
          // Drive:
          {kGoogleDriveAppIdPwa, kGoogleDriveOpenedEvent},
          {kGoogleDriveAppIdArc, kGoogleDriveOpenedEvent},
          {kGoogleDriveAppDomain, kGoogleDriveOpenedEvent},
          // Gmail:
          {kGmailAppIdPwa, kGmailOpenedEvent},
          {kGmailAppIdArc, kGmailOpenedEvent},
          {kGmailAppDomain, kGmailOpenedEvent},
          // Photos:
          {kGooglePhotosAppIdPwa, kGooglePhotosOpenedEvent},
          {kGooglePhotosAppIdArc, kGooglePhotosOpenedEvent},
          {kGooglePhotosAppDomain, kGooglePhotosOpenedEvent},
      });
  return kAppGroupIdMap;
}

}  // namespace

std::string_view GetGrowthCampaignsEventNamePrefix() {
  // Only event name with this prefix can be processed by the Feature Engagement
  // framework.
  return "ChromeOSAshGrowthCampaigns_";
}

std::string GetEventName(CampaignEvent event, std::string_view id) {
  switch (event) {
    case CampaignEvent::kImpression:
      return base::StrCat({"Campaign", id, "_Impression"});
    case CampaignEvent::kDismissed:
      return base::StrCat({"Campaign", id, "_Dismissed"});
    case CampaignEvent::kAppOpened:
      // TODO: b/342282901 - Migrate `CampaignEvent::kAppOpened` to
      // `CampaignEvent::kEvent`, which can be used instead for similar case.
      return base::StrCat({"AppOpened_AppId_", id});
    case CampaignEvent::kEvent:
      return base::StrCat({"Event_", id});
    case CampaignEvent::kGroupImpression:
      return base::StrCat({"Group", id, "_Impression"});
    case CampaignEvent::kGroupDismissed:
      return base::StrCat({"Group", id, "_Dismissed"});
  }
  NOTREACHED();
}

std::string_view GetAppGroupId(std::string_view app_id) {
  const auto& map = GetAppGroupIdMap();
  const auto it = map.find(app_id);
  return (it == map.end()) ? std::string_view() : it->second;
}

std::string_view GetAppGroupId(const GURL& url) {
  const auto& map = GetAppGroupIdMap();
  const auto it = std::ranges::find_if(
      map, [&](const auto& elem) { return url.DomainIs(elem.first); });
  return (it == map.end()) ? std::string_view() : it->second;
}

std::string ToString(bool value) {
  return value ? "true" : "false";
}

}  // namespace growth

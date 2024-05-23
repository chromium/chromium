// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_constants.h"

#include <cstring>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "url/gurl.h"

namespace growth {

namespace {

// Only event name with this prefix can be processed by the Feature Engagement
// framework.
constexpr char kGrowthCampaignsEventNamePrefix[] = "ChromeOSAshGrowthCampaigns";

// All event names will be prefixed by `kGrowthCampaignsEventNamePrefix`.
// `CampaignEvent::kImpression` and `CampaaignEvent::kDismissed` event names
// will be suffixed by campaign id.
// E.g.
// `ChromeOSAshGrowthCampaigns_CampaignId_Impression`
// `ChromeOSAshGrowthCampaigns_CampaignId_Dismissed`
constexpr char kCampaignEventNameImpressionTemplate[] =
    "_Campaign%s_Impression";
constexpr char kCampaignEventNameDismissedTemplate[] = "_Campaign%s_Dismissed";

// `CampaignEvent::kAppOpened` event names will be suffixed by individual app id
// (e.g. [hash]).
// E.g. `ChromeOSAshGrowthCampaigns_AppOpened_AppId_[hash]`.
// TODO: b/342282901 - Migrate `CampaignEvent::kAppOpened` to
// `CampaignEvent::kEvent`, which can be used instead for similar case.
constexpr char kCampaignEventNameAppOpenedTemplate[] = "_AppOpened_AppId_%s";

// `CampaignEvent::kEvent` event names, which is used for recording the Feature
// Engagement events which then used for event targeting.
// E.g. `ChromeOSAshGrowthCampaigns_Event_DocsOpened`.
constexpr char kCampaignEventNameEventTemplate[] = "_Event_%s";

// TODO: b/341721256 - Get the app ids from their constants files.
// PWA:
constexpr char kGoogleDocsAppIdPwa[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";
constexpr char kGoogleDriveAppIdPwa[] = "aghbiahbpaijignceidepookljebhfak";
constexpr char kGmailAppIdPwa[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";
constexpr char kGooglePhotosAppIdPwa[] = "ncmjhecbjeaamljdfahankockkkdmedg";

// ARC:
constexpr char kGoogleDocsAppIdArc[] = "cgiadblnmjkjbhignimpegeiplgoidhe";
constexpr char kGoogleDriveAppIdArc[] = "ljmhbofhbaapdhebeafbhlcapoiipfbi";
constexpr char kGmailAppIdArc[] = "hhkfkjpmacfncmbapfohfocpjpdnobjg";
constexpr char kGooglePhotosAppIdArc[] = "fdbkkojdbojonckghlanfaopfakedeca";

// Domain name:
constexpr char kGoogleDocsAppDomain[] = "docs.google.com";
constexpr char kGoogleDriveAppDomain[] = "drive.google.com";
constexpr char kGmailAppDomain[] = "mail.google.com";
constexpr char kGooglePhotosAppDomain[] = "photos.google.com";

// A list of supported apps group events.
// NOTE: An app can be grouped in multiple groups.
constexpr char kGoogleDocsOpenedEvent[] = "DocsOpened";
constexpr char kGoogleDriveOpenedEvent[] = "DriveOpened";
constexpr char kGmailOpenedEvent[] = "GmailOpened";
constexpr char kGooglePhotosOpenedEvent[] = "PhotosOpened";

// The mapping of `app_id` or URL `domain` to `app_group_id`.
const base::flat_map<std::string, std::string>& GetAppGroupIdMap() {
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      app_group_id_map({
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
  return *app_group_id_map;
}

}  // namespace

std::string GetEventName(CampaignEvent event, const std::string& id) {
  const char* event_name = nullptr;
  switch (event) {
    case growth::CampaignEvent::kImpression:
      event_name = growth::kCampaignEventNameImpressionTemplate;
      break;
    case growth::CampaignEvent::kDismissed:
      event_name = growth::kCampaignEventNameDismissedTemplate;
      break;
    case growth::CampaignEvent::kAppOpened:
      event_name = growth::kCampaignEventNameAppOpenedTemplate;
      break;
    case growth::CampaignEvent::kEvent:
      event_name = growth::kCampaignEventNameEventTemplate;
      break;
  }

  std::string event_name_with_id = base::StringPrintf(event_name, id.c_str());
  return growth::kGrowthCampaignsEventNamePrefix + event_name_with_id;
}

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<std::string> GetAppGroupId(const std::string& app_id) {
  auto it = GetAppGroupIdMap().find(app_id);
  if (it == GetAppGroupIdMap().end()) {
    return std::nullopt;
  }

  return it->second;
}

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<std::string> GetAppGroupId(const GURL& url) {
  auto it = GetAppGroupIdMap().cbegin();
  for (; it != GetAppGroupIdMap().cend(); ++it) {
    if (url.DomainIs(it->first)) {
      return it->second;
    }
  }

  return std::nullopt;
}

}  // namespace growth

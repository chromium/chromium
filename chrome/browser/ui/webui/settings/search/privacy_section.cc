// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/privacy_section.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"

namespace settings_search {

namespace {

using mojom::SettingId;

constexpr int kPrivacyBreadcrumbs[] = {IDS_SETTINGS_PRIVACY};
constexpr int kCookiesBreadcrumbs[] = {
    IDS_SETTINGS_PRIVACY, IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_TITLE};
constexpr int kCookiesAndSiteDataBreadcrumbs[] = {
    IDS_SETTINGS_PRIVACY,
    IDS_SETTINGS_THIRD_PARTY_COOKIES_AND_SITE_DATA_PAGE_TITLE};

// Makes an item for a row on the Privacy and security page that opens
// `sub_page`.
std::unique_ptr<SettingsSearchItem> MakeItem(SettingId id,
                                             std::string_view sub_page,
                                             int title_id,
                                             int synonyms_id) {
  return MakeSearchItem(id, sub_page, title_id, kPrivacyBreadcrumbs,
                        synonyms_id);
}

// Makes an item for a control on the Third-party cookies subpage.
std::unique_ptr<SettingsSearchItem> MakeCookiesItem(
    SettingId id,
    int title_id,
    base::span<const int> breadcrumb_ids) {
  return MakeSearchItem(id, chrome::kCookieSettingsSubPage, title_id,
                        breadcrumb_ids);
}

}  // namespace

PrivacySection::PrivacySection(SettingsSearchRegistry* registry,
                               Profile* profile)
    : SettingsSection(registry) {
  CHECK(profile);
  RegisterSearchItems(profile);
}

PrivacySection::~PrivacySection() = default;

void PrivacySection::RegisterSearchItems(Profile* profile) {
  // Mirrors page_visibility.ts: guests don't get the Privacy and security
  // page outside ChromeOS, and don't get Safety Check anywhere.
  const bool is_guest = settings_utils::IsGuest(profile);
#if !BUILDFLAG(IS_CHROMEOS)
  if (is_guest) {
    return;
  }
#endif
  registry()->AddItem(MakeSearchItem(
      SettingId::kPrivacy, chrome::kPrivacySubPage, IDS_SETTINGS_PRIVACY,
      /*breadcrumb_ids=*/{}, IDS_SETTINGS_TAG_PRIVACY));
  if (!is_guest) {
    registry()->AddItem(
        MakeItem(SettingId::kSafetyCheck, chrome::kSafetyHubSubPage,
                 IDS_SETTINGS_SAFETY_HUB, IDS_SETTINGS_TAG_SAFETY_HUB));
  }
  // The route opens the Privacy and security page with the dialog showing.
  registry()->AddItem(MakeItem(
      SettingId::kClearBrowsingData, chrome::kClearBrowserDataSubPage,
      IDS_SETTINGS_CLEAR_BROWSING_DATA, IDS_SETTINGS_TAG_CLEAR_BROWSING_DATA));

  // Mirrors loadTimeData "showUniversalOptOutSettings", which also renames
  // the Third-party cookies row and subpage.
  const bool show_universal_opt_out =
      settings_utils::ShouldShowUniversalOptOutSettings(profile);
  // The subpage's allow/block radio group shares this item rather than
  // registering a separate entry.
  registry()->AddItem(MakeItem(
      SettingId::kThirdPartyCookies, chrome::kCookieSettingsSubPage,
      show_universal_opt_out
          ? IDS_SETTINGS_THIRD_PARTY_COOKIES_AND_SITE_DATA_LINK_ROW_LABEL
          : IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_LABEL,
      IDS_SETTINGS_TAG_THIRD_PARTY_COOKIES));
  const base::span<const int> cookies_breadcrumbs =
      show_universal_opt_out ? base::span(kCookiesAndSiteDataBreadcrumbs)
                             : base::span(kCookiesBreadcrumbs);
  registry()->AddItem(MakeCookiesItem(SettingId::kDoNotTrack,
                                      IDS_SETTINGS_ENABLE_DO_NOT_TRACK,
                                      cookies_breadcrumbs));
  registry()->AddItem(MakeCookiesItem(
      SettingId::kThirdPartyCookiesAllowedSites,
      IDS_SETTINGS_TRACKING_PROTECTION_SITES_ALLOWED_COOKIES_TITLE,
      cookies_breadcrumbs));
  if (show_universal_opt_out) {
    registry()->AddItem(MakeCookiesItem(SettingId::kUniversalOptOut,
                                        IDS_SETTINGS_UNIVERSAL_OPT_OUT_LABEL,
                                        cookies_breadcrumbs));
  }
}

}  // namespace settings_search

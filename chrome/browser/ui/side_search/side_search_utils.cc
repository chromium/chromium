// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_utils.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_tab_data.pb.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"

namespace side_search {

const char kSideSearchExtraDataKey[] = "side_search";

std::string SerializeSideSearchTabDataAsString(
    SideSearchTabContentsHelper* tab_contents_helper) {
  SideSearchTabData side_search_tab_data;
  side_search_tab_data.set_last_search_url(
      tab_contents_helper->last_search_url().value().spec());
  side_search_tab_data.set_toggled_open(tab_contents_helper->toggled_open());

  return side_search_tab_data.SerializeAsString();
}

std::optional<std::pair<std::string, std::string>>
MaybeGetSideSearchTabRestoreData(content::WebContents* web_contents) {
  SideSearchTabContentsHelper* helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents);
  if (helper && helper->last_search_url().has_value()) {
    return {std::make_pair(kSideSearchExtraDataKey,
                           SerializeSideSearchTabDataAsString(helper))};
  }

  return std::nullopt;
}

void MaybeSaveSideSearchTabSessionData(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (!session_service)
    return;

  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents);
  if (tab_contents_helper &&
      tab_contents_helper->last_search_url().has_value()) {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(web_contents);
    session_service->AddTabExtraData(
        browser->session_id(), session_tab_helper->session_id(),
        kSideSearchExtraDataKey,
        SerializeSideSearchTabDataAsString(tab_contents_helper));
  }
}

void SetSideSearchTabStateFromRestoreData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  if (base::Contains(extra_data, kSideSearchExtraDataKey)) {
    // Restoration takes place before tab contents helpers are created. Will
    // no-op if there is already a Side Search tab contents helper for the web
    // contents.
    SideSearchTabContentsHelper::CreateForWebContents(web_contents);
    auto* side_search_tab_contents_helper =
        SideSearchTabContentsHelper::FromWebContents(web_contents);
    if (side_search_tab_contents_helper) {
      SideSearchTabData side_search_tab_data;
      side_search_tab_data.ParseFromString(
          extra_data.at(kSideSearchExtraDataKey));
      side_search_tab_contents_helper->LastSearchURLUpdated(
          GURL(side_search_tab_data.last_search_url()));
      side_search_tab_contents_helper->set_toggled_open(
          side_search_tab_data.toggled_open());
    }
  }
}

bool IsSidePanelWebContents(content::WebContents* web_contents) {
  return !!SideSearchSideContentsHelper::FromWebContents(web_contents);
}

bool IsEnabledForBrowser(const Browser* browser) {
  return IsSideSearchEnabled(browser->profile()) && browser->is_type_normal();
}

bool IsSearchWebInSidePanelSupported(const Browser* browser) {
  if (!browser)
    return false;

  const TemplateURL* const default_provider =
      TemplateURLServiceFactory::GetForProfile(browser->profile())
          ->GetDefaultSearchProvider();
  DCHECK(default_provider);
  return IsEnabledForBrowser(browser) &&
         default_provider->IsSideSearchSupported() &&
         base::FeatureList::IsEnabled(features::kSearchWebInSidePanel);
}
}  // namespace side_search

bool IsSideSearchEnabled(const Profile* profile) {
  return !profile->IsOffTheRecord() &&
         base::FeatureList::IsEnabled(features::kSideSearch) &&
         profile->GetPrefs()->GetBoolean(side_search_prefs::kSideSearchEnabled);
}

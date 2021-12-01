// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_utils.h"

#include <string>

#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_tab_data.pb.h"
#include "chrome/browser/ui/side_search/side_search_window_data.pb.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace side_search {

const char kSideSearchExtraDataKey[] = "side_search";

void MaybeAddSideSearchTabRestoreData(
    content::WebContents* web_contents,
    std::map<std::string, std::string>& extra_data) {
  SideSearchTabContentsHelper* helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents);
  if (helper && helper->last_search_url().has_value()) {
    SideSearchTabData side_search_tab_data;
    side_search_tab_data.set_last_search_url(
        helper->last_search_url().value().spec());
    side_search_tab_data.set_toggled_open(helper->toggled_open());

    extra_data[kSideSearchExtraDataKey] =
        side_search_tab_data.SerializeAsString();
  }
}

void MaybeAddSideSearchWindowRestoreData(
    bool toggled_open,
    std::map<std::string, std::string>& extra_data) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab))
    return;

  SideSearchWindowData side_search_window_data;
  side_search_window_data.set_toggled_open(toggled_open);

  extra_data[kSideSearchExtraDataKey] =
      side_search_window_data.SerializeAsString();
}

void MaybeRestoreSideSearchWindowState(
    SideSearchTabContentsHelper::Delegate* delegate,
    const std::map<std::string, std::string>& extra_data) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab))
    return;

  if (base::Contains(extra_data, kSideSearchExtraDataKey)) {
    SideSearchWindowData side_search_window_data;
    side_search_window_data.ParseFromString(
        extra_data.at(kSideSearchExtraDataKey));

    if (side_search_window_data.toggled_open())
      delegate->OpenSidePanel();
  }
}

void SetSideSearchStateFromRestoreData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  if (base::Contains(extra_data, kSideSearchExtraDataKey)) {
    auto* side_search_tab_contents_helper =
        SideSearchTabContentsHelper::FromWebContents(web_contents);

    SideSearchTabData side_search_tab_data;
    side_search_tab_data.ParseFromString(
        extra_data.at(kSideSearchExtraDataKey));
    side_search_tab_contents_helper->set_last_search_url(
        GURL(side_search_tab_data.last_search_url()));
    side_search_tab_contents_helper->set_toggled_open(
        side_search_tab_data.toggled_open());
  }
}

}  // namespace side_search

bool IsSideSearchEnabled(const Profile* profile) {
  return !profile->IsOffTheRecord() &&
         base::FeatureList::IsEnabled(features::kSideSearch) &&
         profile->GetPrefs()->GetBoolean(side_search_prefs::kSideSearchEnabled);
}

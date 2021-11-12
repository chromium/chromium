// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_utils.h"

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace side_search {

const char kSideSearchExtraDataKey[] = "side_search";
const char kLastSearchUrl[] = "last_search_url";
const char kToggledOpen[] = "toggled_open";

void MaybeAddSideSearchTabRestoreData(
    content::WebContents* web_contents,
    std::map<std::string, base::Value>& extra_data) {
  SideSearchTabContentsHelper* helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents);
  if (helper && helper->last_search_url().has_value()) {
    base::Value side_search_tab_data(base::Value::Type::DICTIONARY);
    side_search_tab_data.SetStringKey(kLastSearchUrl,
                                      helper->last_search_url().value().spec());
    side_search_tab_data.SetBoolKey(kToggledOpen, helper->toggled_open());

    extra_data[kSideSearchExtraDataKey] = std::move(side_search_tab_data);
  }
}

void MaybeAddSideSearchWindowRestoreData(
    bool toggled_open,
    std::map<std::string, base::Value>& extra_data) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab))
    return;

  base::Value side_search_window_data(base::Value::Type::DICTIONARY);
  side_search_window_data.SetBoolKey(kToggledOpen, toggled_open);

  extra_data[kSideSearchExtraDataKey] = std::move(side_search_window_data);
}

void MaybeRestoreSideSearchWindowState(
    SideSearchTabContentsHelper::Delegate* delegate,
    const std::map<std::string, base::Value>& extra_data) {
  if (base::FeatureList::IsEnabled(features::kSideSearchStatePerTab))
    return;

  if (base::Contains(extra_data, kSideSearchExtraDataKey)) {
    absl::optional<bool> toggled_open =
        extra_data.at(kSideSearchExtraDataKey).FindBoolKey(kToggledOpen);

    if (toggled_open.has_value() && toggled_open.value())
      delegate->OpenSidePanel();
  }
}

void SetSideSearchStateFromRestoreData(
    content::WebContents* web_contents,
    const std::map<std::string, base::Value>& extra_data) {
  if (base::Contains(extra_data, kSideSearchExtraDataKey)) {
    auto* side_search_tab_contents_helper =
        SideSearchTabContentsHelper::FromWebContents(web_contents);

    const std::string* last_search_url_str =
        extra_data.at(kSideSearchExtraDataKey).FindStringKey(kLastSearchUrl);
    if (last_search_url_str)
      side_search_tab_contents_helper->set_last_search_url(
          GURL(*last_search_url_str));

    absl::optional<bool> toggled_open =
        extra_data.at(kSideSearchExtraDataKey).FindBoolKey(kToggledOpen);
    if (toggled_open.has_value()) {
      side_search_tab_contents_helper->set_toggled_open(toggled_open.value());
    }
  }
}

}  // namespace side_search

bool IsSideSearchEnabled(const Profile* profile) {
  return !profile->IsOffTheRecord() &&
         base::FeatureList::IsEnabled(features::kSideSearch) &&
         profile->GetPrefs()->GetBoolean(side_search_prefs::kSideSearchEnabled);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/utils.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {
namespace {
const char kChromeUINewTabURL[] = "chrome://newtab/";

// Tab title to be shown or synced when tab URL is in an unsupported scheme.
const char* kDefaultTitleOverride = "Unsavable tab";
}  // namespace

bool AreLocalIdsPersisted() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id) {
  return local_tab_group_id.ToString();
}

std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& serialized_local_tab_group_id) {
#if BUILDFLAG(IS_ANDROID)
  return base::Token::FromString(serialized_local_tab_group_id);
#else
  auto token = base::Token::FromString(serialized_local_tab_group_id);
  if (!token.has_value()) {
    return std::nullopt;
  }

  return tab_groups::TabGroupId::FromRawToken(token.value());
#endif
}

bool IsURLValidForSavedTabGroups(const GURL& gurl) {
  return gurl.SchemeIsHTTPOrHTTPS() || gurl == GURL(kChromeUINewTabURL);
}

std::pair<GURL, std::u16string> GetDefaultUrlAndTitle() {
  return std::make_pair(GURL(kChromeUINewTabURL),
                        base::ASCIIToUTF16(kDefaultTitleOverride));
}

}  // namespace tab_groups

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_

#include <variant>

#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"

namespace data_sharing {

// The query params of the urls to internally load webui.
inline std::string_view kQueryParamFlow = "flow";
inline std::string_view kQueryParamGroupId = "group_id";
inline std::string_view kQueryParamTokenSecret = "token_secret";
inline std::string_view kQueryParamTabGroupId = "tab_group_id";

// Possible values of kQueryParamFlow in url.
inline std::string_view kFlowShare = "share";
inline std::string_view kFlowJoin = "join";
inline std::string_view kFlowManage = "manage";

// `request_info` contains the info we want to pass into the loaded WebUI.
std::optional<GURL> GenerateWebUIUrl(
    std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken>
        request_info,
    Profile* profile);

// Associate tab group with `group_id` returned by the Share flow WebUI, i.e.
// make the tab group shared.
void AssociateTabGroupWithGroupId(const std::string& tab_group_id,
                                  const std::string& group_id,
                                  Profile* profile);

// Get share link from data sharing service.
GURL GetShareLink(const std::string& group_id,
                  const std::string& access_token,
                  Profile* profile);

}  // namespace data_sharing

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_

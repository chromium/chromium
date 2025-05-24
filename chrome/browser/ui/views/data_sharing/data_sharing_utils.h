// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_

#include <variant>

#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"

namespace data_sharing {

// The query params of the urls to internally load webui.
inline const std::string_view kQueryParamFlow = "flow";
inline const std::string_view kQueryParamGroupId = "group_id";
inline const std::string_view kQueryParamTokenSecret = "token_secret";
inline const std::string_view kQueryParamTabGroupId = "tab_group_id";
inline const std::string_view kQueryParamTabGroupTitle = "tab_group_title";

// Possible values of kQueryParamFlow in url.
inline const std::string_view kFlowShare = "share";
inline const std::string_view kFlowJoin = "join";
inline const std::string_view kFlowManage = "manage";
inline const std::string_view kFlowDelete = "delete";
inline const std::string_view kFlowLeave = "leave";
inline const std::string_view kFlowClose = "close";

enum FlowType {
  kShare,
  kJoin,
  kManage,
  kDelete,
  kLeave,
  kClose,
  kMaxValue = kClose,
};

// Metadata used to determine the which WebUI we should return when
// GenerateWebUIUrl is called,
struct RequestInfo {
 public:
  RequestInfo(
      std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken> id,
      FlowType type);
  RequestInfo();
  RequestInfo(const RequestInfo& other);
  ~RequestInfo();

  // The id of the request.
  std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken> id;

  // The type of request.
  FlowType type;
};

// `request_info` contains the info we want to pass into the loaded WebUI.
std::optional<GURL> GenerateWebUIUrl(RequestInfo request_info,
                                     Profile* profile);

// Return whether the tab group is shared.
bool IsTabGroupShared(const std::string& tab_group_id, Profile* profile);

// Get share link from data sharing service.
GURL GetShareLink(const std::string& group_id,
                  const std::string& access_token,
                  Profile* profile);

void ProcessPreviewOutcome(
    data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback,
    const data_sharing::DataSharingService::SharedDataPreviewOrFailureOutcome&
        outcome);

void GetTabGroupPreview(
    const std::string& group_id,
    const std::string& access_token,
    Profile* profile,
    data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback);
}  // namespace data_sharing

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_UTILS_H_

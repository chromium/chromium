// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"

#include <optional>
#include <string>
#include <variant>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/url_formatter/elide_url.h"
#include "mojo/public/mojom/base/absl_status.mojom.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace data_sharing {
RequestInfo::RequestInfo(
    std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken> id,
    FlowType type)
    : id(id), type(type) {}
RequestInfo::RequestInfo() : id(GroupToken()) {}
RequestInfo::RequestInfo(const RequestInfo& other) = default;
RequestInfo::~RequestInfo() = default;

GURL CreateShareUrl(const GURL& url,
                    const std::variant<tab_groups::LocalTabGroupID,
                                       data_sharing::GroupToken>& group_id) {
  GURL updated_url = url;
  if (std::holds_alternative<tab_groups::LocalTabGroupID>(group_id)) {
    tab_groups::TabGroupId local_group_id = std::get<0>(group_id);

    // Return share flow url which requires a local group id to later
    // associate with the collaboration_id returned by WebUI.
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowShare);
    updated_url = net::AppendQueryParameter(updated_url, kQueryParamTabGroupId,
                                            local_group_id.ToString());
  } else {
    NOTREACHED();
  }

  return updated_url;
}

GURL CreateJoinUrl(const GURL& url,
                   const std::variant<tab_groups::LocalTabGroupID,
                                      data_sharing::GroupToken>& group_id) {
  GURL updated_url = url;
  if (std::holds_alternative<tab_groups::LocalTabGroupID>(group_id)) {
    NOTREACHED();
  } else {
    // Return join flow url which requires both collaboration_id and
    // access_token for WebUI to fetch people info.
    GroupToken group_token = std::get<1>(group_id);
    CHECK(group_token.IsValid());
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowJoin);
    updated_url = net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                            group_token.group_id.value());
    updated_url = net::AppendQueryParameter(updated_url, kQueryParamTokenSecret,
                                            group_token.access_token);
  }

  return updated_url;
}

GURL CreateManageUrl(
    const GURL& url,
    const std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken>&
        group_id,
    const std::optional<tab_groups::SavedTabGroup> saved_group) {
  GURL updated_url = url;
  CHECK(saved_group->is_shared_tab_group());
  if (std::holds_alternative<tab_groups::LocalTabGroupID>(group_id)) {
    // Return manage flow url which requires a group_id for webui to fetch
    // people info.
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowManage);
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                  saved_group->collaboration_id()->value());
    tab_groups::TabGroupId local_group_id = std::get<0>(group_id);

    updated_url = net::AppendQueryParameter(updated_url, kQueryParamTabGroupId,
                                            local_group_id.ToString());
  } else {
    // Return manage flow url which requires a group_id for webui to fetch
    // people info.
    GroupToken group_token = std::get<1>(group_id);
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowManage);
    updated_url = net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                            group_token.group_id.value());
  }

  return updated_url;
}

GURL CreateLeaveUrl(const GURL& url,
                    const std::variant<tab_groups::LocalTabGroupID,
                                       data_sharing::GroupToken>& group_id) {
  GURL updated_url = url;
  if (std::holds_alternative<tab_groups::LocalTabGroupID>(group_id)) {
    NOTREACHED();
  } else {
    // Return manage flow url which requires a group_id for webui to fetch
    // people info.
    GroupToken group_token = std::get<1>(group_id);
    updated_url =
        net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowLeave);
    updated_url = net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                            group_token.group_id.value());
  }

  return updated_url;
}

GURL CreateDeleteUrl(
    const GURL& url,
    const std::optional<tab_groups::SavedTabGroup> saved_group) {
  CHECK(saved_group);
  CHECK(saved_group->collaboration_id());

  GURL updated_url = url;

  // Both variants will use the collaboration id from the saved_group.
  updated_url =
      net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowDelete);
  updated_url =
      net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                saved_group->collaboration_id()->value());

  return updated_url;
}

GURL CreateCloseUrl(
    const GURL& url,
    const std::optional<tab_groups::SavedTabGroup> saved_group) {
  CHECK(saved_group);
  CHECK(saved_group->collaboration_id());

  GURL updated_url = url;

  // Both variants will use the collaboration id from the saved_group.
  updated_url =
      net::AppendQueryParameter(updated_url, kQueryParamFlow, kFlowClose);
  updated_url =
      net::AppendQueryParameter(updated_url, kQueryParamGroupId,
                                saved_group->collaboration_id()->value());

  return updated_url;
}
}  // namespace data_sharing

std::optional<GURL> data_sharing::GenerateWebUIUrl(RequestInfo request_info,
                                                   Profile* profile) {
  tab_groups::TabGroupSyncService* const tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile);
  if (!tab_group_service) {
    return std::nullopt;
  }

  // Find the saved group for the request.
  std::optional<tab_groups::SavedTabGroup> saved_group = std::nullopt;

  if (std::holds_alternative<tab_groups::LocalTabGroupID>(request_info.id)) {
    saved_group = tab_group_service->GetGroup(std::get<0>(request_info.id));
    if (!saved_group) {
      // Local requests must be associated with a SavedTabGroup.
      return std::nullopt;
    }
  } else {
    // It's okay if `saved_group` is nullopt here since we could be joining a
    // group we don't have yet.
    GroupToken group_token = std::get<1>(request_info.id);
    for (const tab_groups::SavedTabGroup& group :
         tab_group_service->GetAllGroups()) {
      if (group.collaboration_id() &&
          group.collaboration_id()->value() == group_token.group_id.value()) {
        saved_group = group;
        break;
      }
    }
  }

  GURL url = GURL(chrome::kChromeUIUntrustedDataSharingURL);
  switch (request_info.type) {
    case kShare: {
      url = CreateShareUrl(url, request_info.id);
      break;
    }
    case kJoin: {
      url = CreateJoinUrl(url, request_info.id);
      break;
    }
    case kManage: {
      url = CreateManageUrl(url, request_info.id, saved_group);
      break;
    }
    case kDelete: {
      if (!saved_group || !saved_group->collaboration_id()) {
        // A shared group must exist for us to delete it.
        return std::nullopt;
      }

      url = CreateDeleteUrl(url, saved_group);
      break;
    }
    case kLeave:
      if (!saved_group || !saved_group->collaboration_id()) {
        // A shared group must exist for us to leave it.
        return std::nullopt;
      }

      url = CreateLeaveUrl(url, request_info.id);
      break;
    case kClose:
      if (!saved_group || !saved_group->collaboration_id()) {
        // A shared group must exist for us to close it.
        return std::nullopt;
      }

      url = CreateCloseUrl(url, saved_group);
      break;
  }

  if (saved_group && request_info.type != kJoin) {
    // If group is unnamed use default name e.g. "1 tab" / "3 tabs".
    std::string title =
        saved_group->title().empty()
            ? l10n_util::GetPluralStringFUTF8(IDS_SAVED_TAB_GROUP_TABS_COUNT,
                                              saved_group->saved_tabs().size())
            : base::UTF16ToUTF8(saved_group->title());
    url = net::AppendQueryParameter(url, kQueryParamTabGroupTitle, title);
  }

  return std::make_optional(url);
}

GURL data_sharing::GetShareLink(const std::string& group_id,
                                const std::string& access_token,
                                Profile* profile) {
  data_sharing::GroupData group_data;
  group_data.group_token =
      data_sharing::GroupToken(data_sharing::GroupId(group_id), access_token);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  // `group_id` and `access_token` are served by webui and should never be null
  // when they get here. So the sharing url must be valid.
  std::unique_ptr<GURL> url_ptr =
      data_sharing_service->GetDataSharingUrl(group_data);
  CHECK(url_ptr);
  return *url_ptr;
}

void data_sharing::ProcessPreviewOutcome(
    data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback,
    const data_sharing::DataSharingService::SharedDataPreviewOrFailureOutcome&
        outcome) {
  data_sharing::mojom::GroupPreviewPtr group_preview =
      data_sharing::mojom::GroupPreview::New();
  if (outcome.has_value()) {
    if (outcome->shared_tab_group_preview) {
      group_preview->title = outcome->shared_tab_group_preview->title;
      for (const auto& tab : outcome->shared_tab_group_preview->tabs) {
        group_preview->shared_tabs.push_back(
            data_sharing::mojom::SharedTab::New(tab.GetDisplayUrl(), tab.url));
      }
    }
    // If group is unnamed use default name e.g. "1 tab" / "3 tabs".
    if (group_preview->title.empty()) {
      group_preview->title = l10n_util::GetPluralStringFUTF8(
          IDS_SAVED_TAB_GROUP_TABS_COUNT, group_preview->shared_tabs.size());
    }

    group_preview->status_code = mojo_base::mojom::AbslStatusCode::kOk;
  } else {
    // TODO(crbug.com/368634445): Convert returned PeopleGroupActionFailure to
    // mojom::AbslStatusCode and let WebUI handle the errors.
    group_preview->status_code = mojo_base::mojom::AbslStatusCode::kUnknown;
  }
  std::move(callback).Run(std::move(group_preview));
}

void data_sharing::GetTabGroupPreview(
    const std::string& group_id,
    const std::string& access_token,
    Profile* profile,
    data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback) {
  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto group_token =
      data_sharing::GroupToken(data_sharing::GroupId(group_id), access_token);
  CHECK(group_token.IsValid());
  data_sharing_service->GetSharedEntitiesPreview(
      group_token, base::BindOnce(&ProcessPreviewOutcome, std::move(callback)));
}

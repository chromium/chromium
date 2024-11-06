// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/url_formatter/elide_url.h"
#include "mojo/public/mojom/base/absl_status.mojom.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

std::optional<GURL> data_sharing::GenerateWebUIUrl(
    std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken>
        request_info,
    Profile* profile) {
  GURL url = GURL(chrome::kChromeUIUntrustedDataSharingURL);
  if (std::holds_alternative<tab_groups::LocalTabGroupID>(request_info)) {
    tab_groups::TabGroupId local_group_id = std::get<0>(request_info);
    if (local_group_id.is_empty()) {
      return std::nullopt;
    }
    tab_groups::TabGroupSyncService* const tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile);
    if (!tab_group_service) {
      return std::nullopt;
    }
    std::optional<tab_groups::SavedTabGroup> saved_group =
        tab_group_service->GetGroup(local_group_id);
    if (!saved_group) {
      return std::nullopt;
    }
    if (saved_group->is_shared_tab_group()) {
      // Return manage flow url which requires a group_id for webui to fetch
      // people info.
      url = net::AppendQueryParameter(url, kQueryParamFlow, kFlowManage);
      url = net::AppendQueryParameter(url, kQueryParamGroupId,
                                      saved_group->collaboration_id().value());
    } else {
      // Return share flow url which requires a local group id to later
      // associate with the collaboration_id returned by WebUI.
      url = net::AppendQueryParameter(url, kQueryParamFlow, kFlowShare);
      url = net::AppendQueryParameter(url, kQueryParamTabGroupId,
                                      local_group_id.ToString());
    }
  } else {
    // Return join flow url which requires both collaboration_id and
    // access_token for WebUI to fetch people info.
    GroupToken group_token = std::get<1>(request_info);
    CHECK(group_token.IsValid());
    url = net::AppendQueryParameter(url, kQueryParamFlow, kFlowJoin);
    url = net::AppendQueryParameter(url, kQueryParamGroupId,
                                    group_token.group_id.value());
    url = net::AppendQueryParameter(url, kQueryParamTokenSecret,
                                    group_token.access_token);
  }
  return std::make_optional(url);
}

void data_sharing::AssociateTabGroupWithGroupId(const std::string& tab_group_id,
                                                const std::string& group_id,
                                                Profile* profile) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile);
  // `tab_group_id` is served by webui and should never be null when it gets
  // here.
  std::optional<base::Token> token = base::Token::FromString(tab_group_id);
  CHECK(token);
  tab_groups::TabGroupId local_tab_group_id(
      tab_groups::TabGroupId::FromRawToken(token.value()));
  std::optional<tab_groups::SavedTabGroup> group =
      service->GetGroup(local_tab_group_id);
  if (group && !group->is_shared_tab_group()) {
    service->MakeTabGroupShared(local_tab_group_id, group_id);
  }
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
        // TODO(crbug.com/376744402): favicon should be retrieved from the tab
        // url. Changed the favicon URL below to a byte array so the data can be
        // passed.
        group_preview->shared_tabs.push_back(
            data_sharing::mojom::SharedTab::New(tab.GetDisplayUrl(),
                                                GURL("chrome://favicon2")));
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

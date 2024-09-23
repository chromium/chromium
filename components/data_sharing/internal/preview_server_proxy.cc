// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/preview_server_proxy.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/data_sharing/public/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace data_sharing {
namespace {
// RPC timeout duration.
constexpr base::TimeDelta kTimeout = base::Milliseconds(5000);

// Content type for network request.
constexpr char kContentType[] = "application/json; charset=UTF-8";
// OAuth name.
constexpr char kOAuthName[] = "shared_data_preview";
// OAuth scope of the server.
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/chromesync";

// Server address to get preview data.
constexpr char kDefaultServiceBaseUrl[] =
    "https://autopush-chromesyncsharedentities-pa.sandbox.googleapis.com/v1";
constexpr base::FeatureParam<std::string> kServiceBaseUrl{
    &features::kDataSharingFeature, "preview_service_base_url",
    kDefaultServiceBaseUrl};

// How many share entities to retrieve for preview.
constexpr int kDefaultPreviewDataSize = 100;
constexpr base::FeatureParam<int> kPreviewDataSize{
    &features::kDataSharingFeature, "preview_data_size",
    kDefaultPreviewDataSize};

// lowerCamelCase JSON proto message keys.
constexpr char kSharedEntitiesKey[] = "sharedEntities";
constexpr char kClientTagHashKey[] = "clientTagHash";
constexpr char kDeletedKey[] = "deleted";
constexpr char kNameKey[] = "name";
constexpr char kVersionKey[] = "version";
constexpr char kCollaborationKey[] = "collaboration";
constexpr char kCollaborationIdKey[] = "collaborationId";
constexpr char kCreateTimeKey[] = "createTime";
constexpr char kUpdateTimeKey[] = "updateTime";
constexpr char kNanosKey[] = "nanos";
constexpr char kSecondsKey[] = "seconds";
constexpr char kSpecificsKey[] = "specifics";
constexpr char kSharedGroupDataKey[] = "sharedTabGroupData";
constexpr char kGuidKey[] = "guid";
constexpr char kUpdateTimeWindowsEpochMicrosKey[] =
    "updateTimeWindowsEpochMicros";
constexpr char kTabKey[] = "tab";
constexpr char kTabGroupKey[] = "tabGroup";
constexpr char kUrlKey[] = "url";
constexpr char kTitleKey[] = "title";
constexpr char kFaviconUrlKey[] = "faviconUrl";
constexpr char kSharedTabGroupGuidKey[] = "sharedTabGroupGuid";
constexpr char kUniquePositionKey[] = "uniquePosition";
constexpr char kCustomCompressedV1Key[] = "customCompressedV1";
constexpr char kColorKey[] = "color";

// Network annotation for getting preview data from server.
constexpr net::NetworkTrafficAnnotationTag
    kGetSharedDataPreviewTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("chrome_data_sharing_preview",
                                            R"(
          semantics {
            sender: "Chrome Data Sharing"
            description:
              "Ask server for a preview of the data shared to a group."
            trigger:
              "A Chrome-initiated request that requires user enabling the "
              "data sharing feature. The request is sent after user receives "
              "an invitation link to join a group, and click on a button to "
              "get a preview of the data shared to that group."
            user_data {
              type: OTHER
              type: ACCESS_TOKEN
            }
            data:
              "Group ID and access token obtained from the invitation that "
              "the user has received."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts { email: "chrome-data-sharing-eng@google.com" }
            }
            last_reviewed: "2024-08-20"
          }
          policy {
            cookies_allowed: NO
            setting:
              "This fetch is enabled for any non-enterprise user that has "
              "the data sharing feature enabled and is signed in."
            chrome_policy {}
          }
        )");

// Find a a value for a field from a child dictionary in json.
std::optional<std::string> GetFieldValueFromChildDict(
    const base::Value::Dict& parent_dict,
    const std::string& child_dict_name,
    const std::string& field_name) {
  auto* child_dict = parent_dict.FindDict(child_dict_name);
  if (!child_dict) {
    return std::nullopt;
  }
  auto* field_value = child_dict->FindString(field_name);
  if (!field_value) {
    return std::nullopt;
  }
  return *field_value;
}

// Parse the shared tab from the dict.
std::optional<sync_pb::SharedTab> ParseSharedTab(
    const base::Value::Dict& dict) {
  auto* url = dict.FindString(kUrlKey);
  if (!url) {
    return std::nullopt;
  }

  auto* title = dict.FindString(kTitleKey);
  if (!title) {
    return std::nullopt;
  }

  auto* shared_tab_group_guid = dict.FindString(kSharedTabGroupGuidKey);
  if (!shared_tab_group_guid) {
    return std::nullopt;
  }

  auto custom_compressed = GetFieldValueFromChildDict(dict, kUniquePositionKey,
                                                      kCustomCompressedV1Key);

  std::optional<sync_pb::SharedTab> shared_tab =
      std::make_optional<sync_pb::SharedTab>();
  shared_tab->set_url(*url);
  shared_tab->set_title(*title);
  shared_tab->set_shared_tab_group_guid(*shared_tab_group_guid);
  auto* favicon_url = dict.FindString(kFaviconUrlKey);
  if (favicon_url) {
    shared_tab->set_favicon_url(*favicon_url);
  }
  if (custom_compressed) {
    shared_tab->mutable_unique_position()->set_custom_compressed_v1(
        custom_compressed.value());
  }
  return shared_tab;
}

// Parse the entity specifics from the dict.
std::optional<sync_pb::EntitySpecifics> ParseEntitySpecifics(
    const base::Value::Dict& dict) {
  auto* shared_tab_group_dict = dict.FindDict(kSharedGroupDataKey);
  if (!shared_tab_group_dict) {
    return std::nullopt;
  }

  std::optional<sync_pb::EntitySpecifics> specifics =
      std::make_optional<sync_pb::EntitySpecifics>();
  sync_pb::SharedTabGroupDataSpecifics* tab_group_data =
      specifics->mutable_shared_tab_group_data();
  auto* guid = shared_tab_group_dict->FindString(kGuidKey);
  if (!guid) {
    return std::nullopt;
  }
  tab_group_data->set_guid(*guid);

  auto* update_time_str =
      shared_tab_group_dict->FindString(kUpdateTimeWindowsEpochMicrosKey);
  uint64_t update_time;
  if (update_time_str && base::StringToUint64(*update_time_str, &update_time)) {
    tab_group_data->set_update_time_windows_epoch_micros(update_time);
  }

  auto* tab_dict = shared_tab_group_dict->FindDict(kTabKey);
  if (tab_dict) {
    auto shared_tab = ParseSharedTab(*tab_dict);
    if (shared_tab) {
      *(tab_group_data->mutable_tab()) = std::move(shared_tab.value());
    } else {
      return std::nullopt;
    }
  } else {
    auto* tab_group_dict = shared_tab_group_dict->FindDict(kTabGroupKey);
    if (tab_group_dict) {
      auto* title = tab_group_dict->FindString(kTitleKey);
      if (!title) {
        return std::nullopt;
      }
      sync_pb::SharedTabGroup* shared_tab_group =
          tab_group_data->mutable_tab_group();
      shared_tab_group->set_title(*title);
      auto* color = tab_group_dict->FindString(kColorKey);
      if (color) {
        sync_pb::SharedTabGroup::Color group_color;
        SharedTabGroup_Color_Parse(*color, &group_color);
        shared_tab_group->set_color(group_color);
      }
    } else {
      return std::nullopt;
    }
  }

  return specifics;
}

// Returns a time for a timestamp object in a child dict.
std::optional<base::Time> GetTimeFromDict(const base::Value::Dict& dict,
                                          const std::string& timestamp_name) {
  auto* time_stamp_dict = dict.FindDict(timestamp_name);
  if (!time_stamp_dict) {
    return std::nullopt;
  }

  auto* seconds_str = time_stamp_dict->FindString(kSecondsKey);
  uint64_t seconds;
  if (!seconds_str || !base::StringToUint64(*seconds_str, &seconds)) {
    return std::nullopt;
  }
  auto nanos = time_stamp_dict->FindInt(kNanosKey);
  return base::Time::FromSecondsSinceUnixEpoch(
      seconds + (nanos ? (nanos.value() /
                          static_cast<double>(base::Seconds(1).InNanoseconds()))
                       : 0.0));
}

// Deserializes a shared entity ID from JSON.
std::optional<SharedEntity> Deserialize(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& value_dict = value.GetDict();
  // Check if entry is deleted.
  auto deleted = value_dict.FindBool(kDeletedKey);
  if (deleted.has_value() && deleted.value()) {
    return std::nullopt;
  }

  std::optional<SharedEntity> entity = std::make_optional<SharedEntity>();
  // Get group id.
  auto collaboration_id = GetFieldValueFromChildDict(
      value_dict, kCollaborationKey, kCollaborationIdKey);
  if (!collaboration_id) {
    return std::nullopt;
  }
  entity->group_id = GroupId(*collaboration_id);

  // Get entity specifics.
  auto* specifics_dict = value_dict.FindDict(kSpecificsKey);
  if (!specifics_dict) {
    return std::nullopt;
  }
  auto specifics = ParseEntitySpecifics(*specifics_dict);
  if (specifics) {
    entity->specifics = std::move(specifics.value());
  } else {
    return std::nullopt;
  }

  // Get client tag hash.
  auto* type = value_dict.FindString(kClientTagHashKey);
  if (type) {
    entity->client_tag_hash = *type;
  }

  // Get name.
  auto* name = value_dict.FindString(kNameKey);
  if (name) {
    entity->name = *name;
  }

  // Get version.
  auto* version_string = value_dict.FindString(kVersionKey);
  uint64_t version;
  if (version_string && base::StringToUint64(*version_string, &version)) {
    entity->version = version;
  }

  // Get time.
  auto create_time = GetTimeFromDict(value_dict, kCreateTimeKey);
  if (create_time) {
    entity->create_time = *create_time;
  }

  auto update_time = GetTimeFromDict(value_dict, kUpdateTimeKey);
  if (update_time) {
    entity->update_time = *update_time;
  }

  return entity;
}

}  // namespace

PreviewServerProxy::PreviewServerProxy(
    signin::IdentityManager* identity_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

PreviewServerProxy::~PreviewServerProxy() = default;

void PreviewServerProxy::GetSharedDataPreview(
    const GroupToken& group_token,
    base::OnceCallback<
        void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
        callback) {
  if (!group_token.IsValid()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(DataSharingService::PeopleGroupActionFailure::
                                 kPersistentFailure)));
    return;
  }

  // Path in the URL to get shared entnties preview, {collaborationId} needs to
  // be replaced by the caller.
  const char kSharedEntitiesPreviewPath[] =
      "collaborations/{collaborationId}/dataTypes/-/sharedEntities:preview";
  std::string shared_entities_preview_path = kSharedEntitiesPreviewPath;
  base::ReplaceFirstSubstringAfterOffset(
      &shared_entities_preview_path, 0, "{collaborationId}",
      base::Base64Encode(group_token.group_id.value()));
  GURL url = GURL(
      kServiceBaseUrl.Get().append("/").append(shared_entities_preview_path));

  // Query string in the URL to get shared entnties preview. {token} needs to
  // be replaced by the caller. {pageSize} can be configured through finch.
  const std::string kQueryString =
      "accessToken={token}&pageToken=&pageSize={pageSize}";
  std::string query_str = kQueryString;
  base::ReplaceFirstSubstringAfterOffset(&query_str, 0, "{token}",
                                         group_token.access_token);
  base::ReplaceFirstSubstringAfterOffset(
      &query_str, 0, "{pageSize}",
      base::NumberToString(kPreviewDataSize.Get()));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_str);
  url = url.ReplaceComponents(replacements);
  auto fetcher = CreateEndpointFetcher(url);
  auto* const fetcher_ptr = fetcher.get();

  fetcher_ptr->Fetch(base::BindOnce(&PreviewServerProxy::HandleServerResponse,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), std::move(fetcher)));
}

std::unique_ptr<EndpointFetcher> PreviewServerProxy::CreateEndpointFetcher(
    const GURL& url) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, net::HttpRequestHeaders::kGetMethod,
      kContentType, std::vector<std::string>{kOAuthScope}, kTimeout,
      /* post_data= */ std::string(), kGetSharedDataPreviewTrafficAnnotation,
      identity_manager_, signin::ConsentLevel::kSignin);
}

void PreviewServerProxy::HandleServerResponse(
    base::OnceCallback<void(
        const DataSharingService::SharedDataPreviewOrFailureOutcome&)> callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    DLOG(ERROR) << "Got bad response (" << response->http_status_code
                << ") for shared data preview!";
    std::move(callback).Run(base::unexpected(
        DataSharingService::PeopleGroupActionFailure::kTransientFailure));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response->response,
      base::BindOnce(&PreviewServerProxy::OnResponseJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PreviewServerProxy::OnResponseJsonParsed(
    base::OnceCallback<void(
        const DataSharingService::SharedDataPreviewOrFailureOutcome&)> callback,
    data_decoder::DataDecoder::ValueOrError result) {
  SharedDataPreview preview;
  if (result.has_value() && result->is_dict()) {
    if (auto* response_json = result->GetDict().FindList(kSharedEntitiesKey)) {
      for (const auto& shared_entity_json : *response_json) {
        if (auto shared_entity = Deserialize(shared_entity_json)) {
          if (shared_entity) {
            preview.shared_entities.push_back(std::move(*shared_entity));
          }
        }
      }
    }
  }
  if (preview.shared_entities.empty()) {
    std::move(callback).Run(base::unexpected(
        DataSharingService::PeopleGroupActionFailure::kPersistentFailure));
  } else {
    std::move(callback).Run(std::move(preview));
  }
}

}  // namespace data_sharing

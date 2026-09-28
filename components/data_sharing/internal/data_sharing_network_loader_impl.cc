// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include "base/time/time.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/group_data.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointResponse;

namespace data_sharing {

namespace {

constexpr base::TimeDelta kTimeout = base::Milliseconds(10000);
const char kRequestContentType[] = "application/x-protobuf";

constexpr net::NetworkTrafficAnnotationTag kCreateGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_create_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service Create Group (Android)."
    description:
      "All create group calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Create group is called."
    data:
      "Info related to creating a collaboration group."
      "HW_OS_INFO : Info about client device."
      "GAIA_ID : Unique identifier for user. Used as profile id."
      "OTHER: Relation defines relation to the group. Example: The user "
      "creating the group is OWNER."
      "ACCESS_TOKEN : This is to identify if the user calling has access to "
      "the group."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: HW_OS_INFO
      type: GAIA_ID
      type: OTHER
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kReadGroupsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_read_groups", R"(
  semantics {
    sender: "Data Sharing Service Read Groups (Android)."
    description:
      "All read groups calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Read groups is called."
    data:
      "Info related to reading collaboration groups."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is "
      "used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your "
      "GAIA_ID provides access."
      "ACCESS_TOKEN: This is to identify if the user calling has access to the "
      "group"
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"} contacts {
      email:
        "nyquist@chromium.org"
      }
    }
    user_data {
      type: HW_OS_INFO
      type: GAIA_ID
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kDeleteGroupsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_delete_groups",
                                        R"(
  semantics {
    sender: "Data Sharing Service Delete Group (Android)."
    description:
      "All delete group calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Delete groups is called."
    data:
      "Info related to deleting collaboration groups."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is "
      "used to identify the group information that is being fetched."
      "ACCESS_TOKEN : This is to identify if the user calling has access to "
      "the group."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"} contacts {
      email:
        "nyquist@chromium.org"
      }
    }
    user_data {
      type: HW_OS_INFO
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kUpdateGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_update_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service update Group (Android)."
    description:
      "All update group calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Update group is called."
    data:
      "Info related to updating collaboration group."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is "
      "used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your "
      "GAIA_ID provides access."
      "GAIA_ID : Unique identifier for user. Used as profile id."
      "ACCESS_TOKEN: This is to identify if the user calling has access to the "
      "group."

    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"} contacts {
      email:
        "nyquist@chromium.org"
      }
    }
    user_data {
      type: HW_OS_INFO
      type: GAIA_ID
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kLookupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_lookup",
                                        R"(
  semantics {
    sender: "Data Sharing Service Lookup."
    description:
      "All lookup calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
      "This lookup person details about a group member or self based on the ID."
    trigger: "Lookup is called."
    data:
      "Info related to lookup info for the group."
      "GAIA_ID : Unique identifier for user. Used as profile id for lookup."
      "EMAIL : Info about client email."
      "PHONE : Info about client phone."
      "OTHER : Chat Space ID for lookup."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: GAIA_ID
      type: EMAIL
      type: PHONE
      type: OTHER
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kBlockPersonTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_block_person",
                                        R"(
  semantics {
    sender: "Data Sharing Service Block Person."
    description:
      "All block person calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Block person is called."
    data:
      "Info related to blocking a person from the group."
      "HW_OS_INFO : Info about client device."
      "GAIA_ID : Unique identifier for user. Used as profile id."
      "NAME : This is to identify the user name."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: HW_OS_INFO
      type: GAIA_ID
      type: NAME
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kLeaveGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_leave_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service Leave Group (Android)."
    description:
      "All leave group calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Leave group is called."
    data:
      "Info related to leaving a group."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is "
      "used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your "
      "GAIA_ID provides access."
      "ACCESS_TOKEN : This is to identify if the user calling has access to "
      "the group."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: HW_OS_INFO
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

constexpr net::NetworkTrafficAnnotationTag kJoinGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_join_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service Join Group (Android)."
    description:
      "All join group calls to Google DataSharing SDK APIs will use "
      "ChromeNetworkStack."
    trigger: "Join group is called."
    data:
      "Info related to joining a group."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is "
      "used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your "
      "GAIA_ID provides access."
      "ACCESS_TOKEN : This is to identify if the user calling has access to "
      "the group."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: HW_OS_INFO
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2024-05-23"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature cannot be disabled by settings as it is part of the Data "
      "Sharing."
    policy_exception_justification: "Not implemented."
  })");

}  // namespace

DataSharingNetworkLoaderImpl::DataSharingNetworkLoaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

DataSharingNetworkLoaderImpl::~DataSharingNetworkLoaderImpl() = default;

void DataSharingNetworkLoaderImpl::LoadUrl(const GURL& url,
                                           const std::string& post_data,
                                           DataSharingRequestType requestType,
                                           NetworkLoaderCallback callback) {
  std::unique_ptr<EndpointFetcher> endpoint_fetcher = CreateEndpointFetcher(
      url, post_data, GetNetworkTrafficAnnotationTag(requestType));
  auto* const fetcher_ptr = endpoint_fetcher.get();
  fetcher_ptr->Fetch(
      base::BindOnce(&DataSharingNetworkLoaderImpl::OnDownloadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)));
}

std::unique_ptr<EndpointFetcher>
DataSharingNetworkLoaderImpl::CreateEndpointFetcher(
    const GURL& url,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, identity_manager_,
      EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kPost, annotation_tag)
          .SetAuthType(endpoint_fetcher::OAUTH)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .SetContentType(kRequestContentType)
          .SetTimeout(kTimeout)
          .SetUrl(url)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kDataSharingAndroid)
          .SetPostData(post_data)
          .Build());
}

void DataSharingNetworkLoaderImpl::OnDownloadComplete(
    NetworkLoaderCallback callback,
    std::unique_ptr<EndpointFetcher> fetcher,
    std::unique_ptr<EndpointResponse> response) {
  NetworkLoaderStatus status = NetworkLoaderStatus::kSuccess;
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    VLOG(1) << "Data sharing network request failed http status: "
            << response->http_status_code << " "
            << (response->error_type ? static_cast<int>(*response->error_type)
                                     : -1);
    // TODO(ssid): Investigate whether some auth errors are permanent.
    status = NetworkLoaderStatus::kTransientFailure;
  }
  std::move(callback).Run(
      std::make_unique<DataSharingNetworkLoader::LoadResult>(
          std::move(response->response), status, response->http_status_code));
}

const net::NetworkTrafficAnnotationTag&
DataSharingNetworkLoaderImpl::GetNetworkTrafficAnnotationTag(
    DataSharingRequestType request_type) {
  switch (request_type) {
    case DataSharingRequestType::kCreateGroup:
      return kCreateGroupTrafficAnnotation;
    case DataSharingRequestType::kReadGroups:
    case DataSharingRequestType::kReadAllGroups:
      return kReadGroupsTrafficAnnotation;
    case DataSharingRequestType::kDeleteGroups:
      return kDeleteGroupsTrafficAnnotation;
    case DataSharingRequestType::kUpdateGroup:
      return kUpdateGroupTrafficAnnotation;
    case DataSharingRequestType::kLookup:
      return kLookupTrafficAnnotation;
    case DataSharingRequestType::kLeaveGroup:
      return kLeaveGroupTrafficAnnotation;
    case DataSharingRequestType::kBlockPerson:
      return kBlockPersonTrafficAnnotation;
    case DataSharingRequestType::kJoinGroup:
      return kJoinGroupTrafficAnnotation;
    // TODO(crbug.com/375594409): Add specific traffic annotation for request
    // types below.
    case DataSharingRequestType::kWarmup:
    case DataSharingRequestType::kAutocomplete:
    case DataSharingRequestType::kMutateConnectionLabel:
    case DataSharingRequestType::kTestRequest:
    case DataSharingRequestType::kUnknown:
      return kReadGroupsTrafficAnnotation;
  }
}

}  // namespace data_sharing

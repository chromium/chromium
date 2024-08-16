// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace data_sharing {

constexpr net::NetworkTrafficAnnotationTag kCreateGroupTrafficAnnotation =
            net::DefineNetworkTrafficAnnotation("data_sharing_service_create_group", R"(
                    semantics {
                      sender: "Data Sharing Service Create Group (Android)"
                      description:
                        "All create group calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
                      trigger: "Create group is called."
                      data:
                        "Info related to creating a collaboration group."
                        "HW_OS_INFO : Info about client device."
                        "GAID_ID : Unique identifier for user. Used as profile id."
                        "OTHER: Relation defines relation to the group. Example: The user creating the group is OWNER."
                        "ACCESS_TOKEN : This is to identify if the user calling has access to the group."
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
                        type: GAID_ID
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
                      sender: "Data Sharing Service Read Groups (Android)"
                      description:
                        "All read groups calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
                      trigger: "Read groups is called."
                      data:
                        "Info related to reading collaboration groups."
                        "HW_OS_INFO : Info about client device."
                        "OTHER : GroupID is a unique identifier for a collaboration. This is used to identify the group information that is being fetched."
                        "TokenSecret from the link is optionally used to get access before your GAIA_ID provides access."
                        "ACCESS_TOKEN: This is to identify if the user calling has access to the group"
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
                        type: GAID_ID
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
            net::DefineNetworkTrafficAnnotation("data_sharing_service_delete_groups", R"(
                    semantics {
                      sender: "Data Sharing Service Delete Group (Android)"
                      description:
                        "All delete group calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
                      trigger: "Delete groups is called."
                      data:
                        "Info related to deleting collaboration groups."
                        "HW_OS_INFO : Info about client device."
                        "OTHER : GroupID is a unique identifier for a collaboration. This is used to identify the group information that is being fetched."
                        "ACCESS_TOKEN : This is to identify if the user calling has access to the group."
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
            net::DefineNetworkTrafficAnnotation("data_sharing_service_update_group", R"(
                    semantics {
                      sender: "Data Sharing Service update Group (Android)"
                      description:
                        "All update group calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
                      trigger: "Update group is called."
                      data:
                        "Info related to updating collaboration group."
                        "HW_OS_INFO : Info about client device."
                        "OTHER : GroupID is a unique identifier for a collaboration. This is used to identify the group information that is being fetched."
                        "TokenSecret from the link is optionally used to get access before your GAIA_ID provides access."
                        "GAID_ID : Unique identifier for user. Used as profile id."
                        "ACCESS_TOKEN: This is to identify if the user calling has access to the group."

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
                        type: GAID_ID
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

                    }  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_

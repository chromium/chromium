// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace data_sharing {

inline constexpr net::NetworkTrafficAnnotationTag
    kAvatarFetcherTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("data_sharing_avatar_image_fetcher",
                                            R"(
        semantics {
          sender: "Image fetcher for data sharing people profile images"
          description:
            "Data sharing service needs to display the account avatars "
            "of users in a shared tab group on Chrome UI."
          trigger:
            "As required from Chrome UIs (tab group editor bubble "
            "and recent activity chips)."
          data: "No data sent in this request."
          destination: GOOGLE_OWNED_SERVICE
          user_data: {
            type: NONE
          }
          internal {
              contacts { email: "chrome-data-sharing-eng@google.com" }
            }
          last_reviewed: "2024-11-27"
        }
        policy {
          cookies_allowed: NO
          cookies_store: ""
          setting:
            "This feature cannot be disabled by settings as it is part of the "
            "Data Sharing."
          policy_exception_justification:
            "Not implemented."
        })");

inline constexpr net::NetworkTrafficAnnotationTag
    kCreateGroupTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("data_sharing_service_create_group",
                                            R"(
  semantics {
    sender: "Data Sharing Service Create Group (Android)\"
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
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
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

inline constexpr net::NetworkTrafficAnnotationTag kReadGroupsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_read_groups", R"(
  semantics {
    sender: "Data Sharing Service Read Groups (Android)\"
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

inline constexpr net::NetworkTrafficAnnotationTag
    kDeleteGroupsTrafficAnnotation = net::DefineNetworkTrafficAnnotation(
        "data_sharing_service_delete_groups",
        R"(
  semantics {
    sender: "Data Sharing Service Delete Group (Android)\"
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

inline constexpr net::NetworkTrafficAnnotationTag
    kUpdateGroupTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("data_sharing_service_update_group",
                                            R"(
  semantics {
    sender: "Data Sharing Service update Group (Android)\"
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

inline constexpr net::NetworkTrafficAnnotationTag kLookupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_lookup",
                                        R"(
  semantics {
    sender: "Data Sharing Service Lookup."
    description:
      "All lookup calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
      "This lookup person details about a group member or self based on the ID."
    trigger: "Lookup is called."
    data:
      "Info related to lookup info for the group."
      "GAID_ID : Unique identifier for user. Used as profile id for lookup."
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
      type: GAID_ID
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

inline constexpr net::NetworkTrafficAnnotationTag
    kBlockPersonTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("data_sharing_service_block_person",
                                            R"(
  semantics {
    sender: "Data Sharing Service Block Person."
    description:
      "All block person calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
    trigger: "Block person is called."
    data:
      "Info related to blocking a person from the group."
      "HW_OS_INFO : Info about client device."
      "GAID_ID : Unique identifier for user. Used as profile id."
      "NAME : This is to identify the user name."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "chrome-tab-group-eng@google.com"}
      contacts{email : "ritikagup@google.com"}
      contacts{email : "nyquist@chromium.org"}
    }
    user_data {
      type: HW_OS_INFO
      type: GAID_ID
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

inline constexpr net::NetworkTrafficAnnotationTag kLeaveGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_leave_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service Leave Group (Android)\"
    description:
      "All leave group calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
    trigger: "Leave group is called."
    data:
      "Info related to leaving a group."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your GAIA_ID provides access."
      "ACCESS_TOKEN : This is to identify if the user calling has access to the group."
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

inline constexpr net::NetworkTrafficAnnotationTag kJoinGroupTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_service_join_group",
                                        R"(
  semantics {
    sender: "Data Sharing Service Join Group (Android)\"
    description:
      "All join group calls to Google DataSharing SDK APIs will use ChromeNetworkStack."
    trigger: "Join group is called."
    data:
      "Info related to joining a group."
      "HW_OS_INFO : Info about client device."
      "OTHER : GroupID is a unique identifier for a collaboration. This is used to identify the group information that is being fetched."
      "TokenSecret from the link is optionally used to get access before your GAIA_ID provides access."
      "ACCESS_TOKEN : This is to identify if the user calling has access to the group."
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

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_UTILS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.net.NetworkTrafficAnnotationTag;

/** Utility class for NetworkTrafficAnnotationTag for DataSharingService APIs. */
public class DataSharingNetworkUtils {

    // TODO(ritikagup) : Update all NetworkTrafficAnnotationTag with appropriate `user_data`.
    static final NetworkTrafficAnnotationTag CREATE_GROUP_TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "data_sharing_service_create_group",
                    """
                    semantics {
                      sender: "Data Sharing Service Create Group (Android)"
                      description:
                        "All create group calls to PeopleKit APIs will use ChromeNetworkStack."
                      trigger: "Create group is called."
                      data: "Info related to creating group."
                      destination: GOOGLE_OWNED_SERVICE
                      internal {
                        contacts {
                          email: "chrome-multiplayer-eng@google.com"
                        }
                        contacts {
                          email: "ritikagup@google.com"
                        }
                        contacts {
                          email: "nyquist@chromium.org"
                        }
                      }
                      user_data {
                        type: NONE
                      }
                      last_reviewed: "2024-05-23"
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "This feature cannot be disabled by settings as it is part of the Data "
                        "Sharing."
                      policy_exception_justification: "Not implemented."
                    }""");

    static final NetworkTrafficAnnotationTag READ_GROUPS_TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "data_sharing_service_read_groups",
                    """
                    semantics {
                      sender: "Data Sharing Service Read Groups (Android)"
                      description:
                        "All read groups calls to PeopleKit APIs will use ChromeNetworkStack."
                      trigger: "Read groups is called."
                      data: "Info related to reading groups."
                      destination: GOOGLE_OWNED_SERVICE
                      internal {
                        contacts {
                          email: "chrome-multiplayer-eng@google.com"
                        }
                        contacts {
                          email: "ritikagup@google.com"
                        }
                        contacts {
                          email: "nyquist@chromium.org"
                        }
                      }
                      user_data {
                        type: NONE
                      }
                      last_reviewed: "2024-05-23"
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "This feature cannot be disabled by settings as it is part of the Data "
                        "Sharing."
                      policy_exception_justification: "Not implemented."
                    }""");

    static final NetworkTrafficAnnotationTag DELETE_GROUPS_TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "data_sharing_service_delete_groups",
                    """
                    semantics {
                      sender: "Data Sharing Service Delete Group (Android)"
                      description:
                        "All delete group calls to PeopleKit APIs will use ChromeNetworkStack."
                      trigger: "Delete groups is called."
                      data: "Info related to deleting groups."
                      destination: GOOGLE_OWNED_SERVICE
                      internal {
                        contacts {
                          email: "chrome-multiplayer-eng@google.com"
                        }
                        contacts {
                          email: "ritikagup@google.com"
                        }
                        contacts {
                          email: "nyquist@chromium.org"
                        }
                      }
                      user_data {
                        type: NONE
                      }
                      last_reviewed: "2024-05-23"
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "This feature cannot be disabled by settings as it is part of the Data "
                        "Sharing."
                      policy_exception_justification: "Not implemented."
                    }""");

    static final NetworkTrafficAnnotationTag UPDATE_GROUP_TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "data_sharing_service_update_group",
                    """
                    semantics {
                      sender: "Data Sharing Service update Group (Android)"
                      description:
                        "All update group calls to PeopleKit APIs will use ChromeNetworkStack."
                      trigger: "Update group is called."
                      data: "Info related to updating group."
                      destination: GOOGLE_OWNED_SERVICE
                      internal {
                        contacts {
                          email: "chrome-multiplayer-eng@google.com"
                        }
                        contacts {
                          email: "ritikagup@google.com"
                        }
                        contacts {
                          email: "nyquist@chromium.org"
                        }
                      }
                      user_data {
                        type: NONE
                      }
                      last_reviewed: "2024-05-23"
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "This feature cannot be disabled by settings as it is part of the Data "
                        "Sharing."
                      policy_exception_justification: "Not implemented."
                    }""");

    public static NetworkTrafficAnnotationTag getNetworkTrafficAnnotationTag(
            @DataSharingRequestType int type) {
        switch (type) {
            case DataSharingRequestType.CREATE_GROUP:
                return CREATE_GROUP_TRAFFIC_ANNOTATION;
            case DataSharingRequestType.READ_GROUPS:
            case DataSharingRequestType.READ_ALL_GROUPS:
                return READ_GROUPS_TRAFFIC_ANNOTATION;
            case DataSharingRequestType.DELETE_GROUPS:
                return DELETE_GROUPS_TRAFFIC_ANNOTATION;
            case DataSharingRequestType.UPDATE_GROUP:
                return UPDATE_GROUP_TRAFFIC_ANNOTATION;
            default:
                return null;
        }
    }
}

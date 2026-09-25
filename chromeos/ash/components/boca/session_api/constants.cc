// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/constants.h"

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::boca {

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("boca_server_integration", R"(
          semantics: {
            sender: "Boca"
            description: "Provide ChromeOS access to school tools server"
            internal {
              contacts {
                  email: "cros-edu-eng@google.com"
              }
            }
            user_data {
              type: ACCESS_TOKEN
              type: EMAIL
              type: NAME
            }
            trigger: "User opens Boca app and goes through session start flow."
            data: "The request is authenticated with an OAuth2 access token "
                  "identifying the Google account."
            destination: GOOGLE_OWNED_SERVICE
            last_reviewed: "2024-06-26"
          }
          policy: {
            cookies_allowed: NO
            setting: "This feature cannot be disabled by settings yet."
            policy_exception_justification: "Not implemented yet."
          })");

}  // namespace ash::boca


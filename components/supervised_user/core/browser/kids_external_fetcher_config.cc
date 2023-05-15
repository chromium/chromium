// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_external_fetcher_config.h"

#include <string>

#include "base/notreached.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace supervised_user {

namespace annotations {
net::NetworkTrafficAnnotationTag ListFamilyMembersTag() {
  return net::DefineNetworkTrafficAnnotation(
      "kids_chrome_management_list_family_members",
      R"(
semantics {
  sender: "Supervised Users"
  description:
    "Fetches information about the user's family group from the Google "
    "Family API."
  trigger:
    "Triggered in regular intervals to update profile information."
  data:
    "The request is authenticated with an OAuth2 access token "
    "identifying the Google account. No other information is sent."
  destination: GOOGLE_OWNED_SERVICE
  user_data {
    type: NONE
  }
  internal {
    contacts {
      email: "chrome-kids-eng@google.com"
    }
  }
  last_reviewed: "2023-05-02"
}
policy {
  cookies_allowed: NO
  setting:
    "This feature cannot be disabled in settings and is only enabled "
    "for child accounts. If sign-in is restricted to accounts from a "
    "managed domain, those accounts are not going to be child accounts."
  chrome_policy {
    RestrictSigninToPattern {
      policy_options {mode: MANDATORY}
      RestrictSigninToPattern: "*@manageddomain.com"
    }
  }
})");
}
}  // namespace annotations

std::string FetcherConfig::GetHttpMethod() const {
  switch (method) {
    case Method::kGet:
      return "GET";
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace supervised_user

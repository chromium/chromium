// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/fetcher_config.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace supervised_user {

BASE_FEATURE(kSupervisedUserProtoFetcherConfig,
             "SupervisedUserProtoFetcherConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace annotations {

net::NetworkTrafficAnnotationTag ClassifyUrlTag() {
  return net::DefineNetworkTrafficAnnotation("supervised_user_classify_url",
                                             R"(
semantics {
  sender: "Supervised Users"
  description:
    "Checks whether a given URL (or set of URLs) is considered safe by "
    "a Google Family Link web restrictions API."
  trigger:
    "If the parent enabled this feature for the child account, this is "
    "sent for every navigation."
  data:
    "An OAuth2 access token identifying and authenticating the "
    "Google account, and the URL to be checked."
  destination: GOOGLE_OWNED_SERVICE
  internal {
    contacts {
      email: "chrome-kids-eng@google.com"
    }
  }
  user_data {
    type: NONE
  }
  last_reviewed: "2023-05-15"
}
policy {
  cookies_allowed: NO
  setting:
    "This feature is only used in child accounts and cannot be "
    "disabled by settings. Parent accounts can disable it in the "
    "family dashboard."
  policy_exception_justification:
    "Enterprise admins don't have control over this feature "
    "because it can't be enabled on enterprise environments."
  })");
}

net::NetworkTrafficAnnotationTag ListFamilyMembersTag() {
  return net::DefineNetworkTrafficAnnotation(
      "supervised_user_list_family_members",
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

net::NetworkTrafficAnnotationTag CreatePermissionRequestTag() {
  return net::DefineNetworkTrafficAnnotation(
      "supervised_user_request_blocked_site_permission",
      R"(
semantics {
  sender: "Supervised Users"
  description:
    "Requests permission for the user to access a blocked site."
  trigger:
    "Initiated by the user, through the Remote Approval option "
    " from the supervised user intersitial page."
  data:
    "The request is authenticated with an OAuth2 access token "
    "that identifies the Google account and contains the URL that "
    "the user requests access to."
  destination: GOOGLE_OWNED_SERVICE
  user_data {
    type: NONE
  }
  internal {
    contacts {
      email: "chrome-kids-eng@google.com"
    }
  }
  last_reviewed: "2023-06-06"
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
      return net::HttpRequestHeaders::kGetMethod;
    case Method::kPost:
      return net::HttpRequestHeaders::kPostMethod;
    default:
      NOTREACHED();
  }
}

std::string_view FetcherConfig::StaticServicePath() const {
  return absl::get<std::string_view>(service_path);
}

std::string FetcherConfig::ServicePath(const PathArgs& args) const {
  const std::string_view* static_path =
      absl::get_if<std::string_view>(&service_path);
  if (static_path != nullptr) {
    CHECK(args.empty()) << "Args are not empty but service_path type variant "
                           "is not FetcherConfig::PathTemplate.";
    return std::string(*static_path);
  }

  const PathTemplate path_template = absl::get<PathTemplate>(service_path);
  CHECK(!path_template.value().empty()) << "Service path is required";

  // Implementation detail: Placeholders are not substituted, but used to split
  // template and put in between as many args as possible. Outstanding args are
  // concatenated at the end.
  std::vector<std::string_view> pieces = base::SplitStringPieceUsingSubstr(
      path_template.value(), "{}", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  std::vector<std::string_view> target;
  auto piece_it = pieces.begin();
  auto args_it = args.begin();

  for (; piece_it != pieces.end() || args_it != args.end();) {
    if (piece_it != pieces.end()) {
      target.push_back(*piece_it++);
    }
    if (args_it != args.end()) {
      target.push_back(*args_it++);
    }
  }

  return base::StrCat(target);
}

std::unique_ptr<net::BackoffEntry> FetcherConfig::BackoffEntry() const {
  if (!backoff_policy.has_value()) {
    return nullptr;
  }
  return std::make_unique<net::BackoffEntry>(&backoff_policy.value());
}
}  // namespace supervised_user

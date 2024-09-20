// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/types/strong_alias.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/backoff_entry.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace supervised_user {

BASE_DECLARE_FEATURE(kSupervisedUserProtoFetcherConfig);

namespace annotations {
// Traffic annotations can only live in cc/mm files.
net::NetworkTrafficAnnotationTag ClassifyUrlTag();
net::NetworkTrafficAnnotationTag ListFamilyMembersTag();
net::NetworkTrafficAnnotationTag CreatePermissionRequestTag();
}  // namespace annotations

struct AccessTokenConfig {
  enum class CredentialsRequirement {
    // This endpoint requires user credentials. If an access token cannot be
    // obtained, the request will be failed (with no request sent to the
    // server).
    kStrict,

    // End user credentials are preferred, but not required, for this endpoint.
    // If an access token cannot be obtained, the request will be sent to the
    // server with no credentials.
    kBestEffort
  };
  CredentialsRequirement credentials_requirement;

  // Must be set in actual configs. See
  // signin::PrimaryAccountAccessTokenFetcher::Mode docs.
  std::optional<signin::PrimaryAccountAccessTokenFetcher::Mode> mode;

  // The OAuth 2.0 permission scope to request the authorization token.
  std::string_view oauth2_scope;
};

// Configuration bundle for the ProtoFetcher.
struct FetcherConfig {
  using PathArgs = std::vector<std::string>;
  using PathTemplate =
      base::StrongAlias<class PathTemplateTag, std::string_view>;

  enum class Method { kUndefined, kGet, kPost };

  // Primary endpoint of the fetcher. May be overridden with feature flags.
  base::FeatureParam<std::string> service_endpoint{
      &kSupervisedUserProtoFetcherConfig, "service_endpoint",
      "https://kidsmanagement-pa.googleapis.com"};

  // Path of the service or a template of such path.
  //
  // In the path mode, it is used literally. Template is substituted with values
  // from supervised_user::CreateFetcher's `args` argument. Use
  // `::StaticServicePath()` and `::ServicePath(const PathArgs&)` accessors to
  // extract the right value.
  //
  // Example templated path: /path/to/{}/my/{}/resource
  //
  // See the service specification at
  // google3/google/internal/kids/chrome/v1/kidschromemanagement.proto for
  // examples.
  absl::variant<std::string_view, PathTemplate> service_path;

  // HTTP method used to communicate with the service.
  const Method method = Method::kUndefined;

  // Basename for histograms. When unset, metrics won't be emitted.
  std::optional<std::string_view> histogram_basename;

  net::NetworkTrafficAnnotationTag (*const traffic_annotation)() = nullptr;

  // Policy for retrying patterns that will be applied to transient errors.
  std::optional<net::BackoffEntry::Policy> backoff_policy;

  AccessTokenConfig access_token_config;

  net::RequestPriority request_priority;

  std::string GetHttpMethod() const;

  // Returns the non-template service_path or crashes for templated one.
  std::string_view StaticServicePath() const;

  // Returns the static (non-template) service path or interpolated template
  // path.
  // If the `service_path` is static (not `::PathTemplate`), `args` must
  // be empty; otherwise this function crashes.
  // For service_path which is `::PathTemplate`, args are inserted in
  // place of placeholders and then exhausted. If there is no arg to be put, an
  // empty string is used instead.
  //
  // Examples for "/path/{}{}/with/template/" template:
  //
  // ServicePath({}) -> /path//with/template/
  // ServicePath({"a"}) -> /path/a/with/template/
  // ServicePath({"a", "b"}) -> /path/ab/with/template/
  // ServicePath({"a", "b", "c"}) -> /path/ab/with/template/c
  // ServicePath({"a", "b", "c", "d"}) -> /path/ab/with/template/cd
  std::string ServicePath(const PathArgs& args) const;

  // Creates backoff entry based on the exact policy from this config.
  std::unique_ptr<net::BackoffEntry> BackoffEntry() const;
};

inline constexpr FetcherConfig kClassifyUrlConfig = {
    .service_path = "/kidsmanagement/v1/people/me:classifyUrl",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.ClassifyUrlRequest",
    .traffic_annotation = annotations::ClassifyUrlTag,
    .access_token_config =
        {
            .credentials_requirement =
                AccessTokenConfig::CredentialsRequirement::kStrict,
            .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            // TODO(b/284523446): Refer to GaiaConstants rather than literal.
            .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
        },
    .request_priority = net::IDLE,
};

inline constexpr FetcherConfig kClassifyUrlConfigWaitUntilAccessTokenAvailable =
    {
        .service_path = "/kidsmanagement/v1/people/me:classifyUrl",
        .method = FetcherConfig::Method::kPost,
        .histogram_basename = "FamilyLinkUser.ClassifyUrlRequest",
        .traffic_annotation = annotations::ClassifyUrlTag,
        .access_token_config =
            {
                .credentials_requirement =
                    AccessTokenConfig::CredentialsRequirement::kStrict,
                .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::
                    kWaitUntilAvailable,
                // TODO(b/284523446): Refer to GaiaConstants rather than
                // literal.
                .oauth2_scope =
                    "https://www.googleapis.com/auth/kid.permission",
            },
        .request_priority = net::IDLE,
};

inline constexpr FetcherConfig kClassifyUrlConfigBestEffort = {
    .service_path = "/kidsmanagement/v1/people/me:classifyUrl",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.ClassifyUrlRequest",
    .traffic_annotation = annotations::ClassifyUrlTag,
    .access_token_config =
        {
            .credentials_requirement =
                AccessTokenConfig::CredentialsRequirement::kBestEffort,
            .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            // TODO(b/284523446): Refer to GaiaConstants rather than literal.
            .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
        },
    .request_priority = net::IDLE,
};

inline constexpr FetcherConfig kListFamilyMembersConfig{
    .service_path = "/kidsmanagement/v1/families/mine/members",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "Signin.ListFamilyMembersRequest",
    .traffic_annotation = annotations::ListFamilyMembersTag,
    .backoff_policy =
        net::BackoffEntry::Policy{
            // Number of initial errors (in sequence) to ignore before
            // applying exponential back-off rules.
            .num_errors_to_ignore = 0,

            // Initial delay for exponential backoff in ms.
            .initial_delay_ms = 2000,

            // Factor by which the waiting time will be multiplied.
            .multiply_factor = 2,

            // Fuzzing percentage. ex: 10% will spread requests randomly
            // between 90%-100% of the calculated time.
            .jitter_factor = 0.2,  // 20%

            // Maximum amount of time we are willing to delay our request in
            // ms.
            .maximum_backoff_ms = 1000 * 60 * 60 * 4,  // 4 hours.

            // Time to keep an entry from being discarded even when it
            // has no significant state, -1 to never discard.
            .entry_lifetime_ms = -1,

            // Don't use initial delay unless the last request was an error.
            .always_use_initial_delay = false,
        },
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        // Wait for the token to be issued. This fetch is asynchronous and not
        // latency sensitive.
        .mode =
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,

        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope = "https://www.googleapis.com/auth/kid.family.readonly",
    },
    .request_priority = net::IDLE,
};

inline constexpr FetcherConfig kCreatePermissionRequestConfig = {
    .service_path = "/kidsmanagement/v1/people/me/permissionRequests",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.CreatePermissionRequest",
    .traffic_annotation = annotations::CreatePermissionRequestTag,
    .access_token_config{
        .credentials_requirement =
            AccessTokenConfig::CredentialsRequirement::kStrict,
        // Fail the fetch right away when access token is not immediately
        // available.
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
    },
    .request_priority = net::IDLE,
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/backoff_entry.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace supervised_user {

BASE_DECLARE_FEATURE(kSupervisedUserProtoFetcherConfig);

namespace annotations {
// Traffic annotations can only live in cc/mm files.
net::NetworkTrafficAnnotationTag ClassifyUrlTag();
net::NetworkTrafficAnnotationTag ListFamilyMembersTag();
net::NetworkTrafficAnnotationTag CreatePermissionRequestTag();
}  // namespace annotations

struct AccessTokenConfig {
  // Must be set in actual configs. See
  // signin::PrimaryAccountAccessTokenFetcher::Mode docs.
  absl::optional<signin::PrimaryAccountAccessTokenFetcher::Mode> mode;

  // The OAuth 2.0 permission scope to request the authorization token.
  base::StringPiece oauth2_scope;
};

// Configuration bundle for the ProtoFetcher.
struct FetcherConfig {
  enum class Method { kUndefined, kGet, kPost };

  // Primary endpoint of the fetcher. May be overridden with feature flags.
  base::FeatureParam<std::string> service_endpoint{
      &kSupervisedUserProtoFetcherConfig, "service_endpoint",
      "https://kidsmanagement-pa.googleapis.com"};

  // Path of the service. See the service specification at
  // google3/google/internal/kids/chrome/v1/kidschromemanagement.proto for
  // examples.
  base::StringPiece service_path;

  // HTTP method used to communicate with the service.
  const Method method = Method::kUndefined;

  // Basename for histograms
  base::StringPiece histogram_basename;

  net::NetworkTrafficAnnotationTag (*const traffic_annotation)() = nullptr;

  // Policy for retrying patterns that will be applied to transient errors.
  absl::optional<net::BackoffEntry::Policy> backoff_policy;

  AccessTokenConfig access_token_config;

  std::string GetHttpMethod() const;
};

constexpr FetcherConfig kClassifyUrlConfig = {
    .service_path = "/kidsmanagement/v1/people/me:classifyUrl",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.ClassifyUrlRequest",
    .traffic_annotation = annotations::ClassifyUrlTag,
    .access_token_config =
        {
            // Fail the fetch right away when access token is not immediately
            // available.
            // TODO(b/301931929): consider using `kWaitUntilAvailable` to improve
            // reliability.
            .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            // TODO(b/284523446): Refer to GaiaConstants rather than literal.
            .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
        },
};

constexpr FetcherConfig kListFamilyMembersLegacyConfig{
    .service_path = "/kidsmanagement/v1/families/mine/members",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "Signin.ListFamilyMembersRequest",
    .traffic_annotation = annotations::ListFamilyMembersTag,
    .access_token_config{
        // Wait for the token to be issued. This fetch is asynchronous and not
        // latency sensitive.
        .mode =
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,

        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope = "https://www.googleapis.com/auth/kid.family.readonly",
    },
};

constexpr FetcherConfig kListFamilyMembersConfig{
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
        // Wait for the token to be issued. This fetch is asynchronous and not
        // latency sensitive.
        .mode =
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,

        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope = "https://www.googleapis.com/auth/kid.family.readonly",
    },

};

constexpr FetcherConfig kCreatePermissionRequestConfig = {
    .service_path = "/kidsmanagement/v1/people/me/permissionRequests",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.CreatePermissionRequest",
    .traffic_annotation = annotations::CreatePermissionRequestTag,
    .access_token_config{
        // Fail the fetch right away when access token is not immediately
        // available.
        .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        // TODO(b/284523446): Refer to GaiaConstants rather than literal.
        .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
    },
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

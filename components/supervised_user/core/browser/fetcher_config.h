// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace supervised_user {

namespace annotations {
// Traffic annotations can only live in cc/mm files.
net::NetworkTrafficAnnotationTag ClassifyUrlTag();
net::NetworkTrafficAnnotationTag ListFamilyMembersTag();
net::NetworkTrafficAnnotationTag CreatePermissionRequestTag();
}  // namespace annotations

// Configuration bundle for the ProtoFetcher.
struct FetcherConfig {
  enum class Method { kUndefined, kGet, kPost };

  // Primary endpoint of the fetcher.
  base::StringPiece service_endpoint{
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/"};

  // Path of the service. See the service specification at
  // google3/google/internal/kids/chrome/v1/kidschromemanagement.proto for
  // examples.
  base::StringPiece service_path;

  // The OAuth 2.0 permission scope to request the authorization token.
  base::StringPiece oauth2_scope;

  // HTTP method used to communicate with the service.
  const Method method = Method::kUndefined;

  // Basename for histograms
  base::StringPiece histogram_basename;

  net::NetworkTrafficAnnotationTag (*const traffic_annotation)() = nullptr;

  std::string GetHttpMethod() const;
};

constexpr FetcherConfig kClassifyUrlConfig = {
    .service_path = "people/me:classifyUrl",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.ClassifyUrlRequest",
    .traffic_annotation = annotations::ClassifyUrlTag,
};

constexpr FetcherConfig kListFamilyMembersConfig{
    .service_path = "families/mine/members",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope = "https://www.googleapis.com/auth/kid.family.readonly",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "Signin.ListFamilyMembersRequest",
    .traffic_annotation = annotations::ListFamilyMembersTag,
};

constexpr FetcherConfig kCreatePermissionRequestConfig = {
    .service_path = "people/me/permissionRequests",
    // TODO(b/284523446): Refer to GaiaConstants rather than literal.
    .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
    .method = FetcherConfig::Method::kPost,
    .histogram_basename = "FamilyLinkUser.CreatePermissionRequest",
    .traffic_annotation = annotations::CreatePermissionRequestTag,
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FETCHER_CONFIG_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_CONFIG_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_CONFIG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace supervised_user {

namespace annotations {
// Traffic annotations can only live in cc/mm files.
net::NetworkTrafficAnnotationTag ListFamilyMembersTag();
}  // namespace annotations

// Configuration bundle for the KidsExternalFetcher.
struct FetcherConfig {
  // TODO(b/276898959): add kPost option.
  enum class Method { kGet };

  // Primary endpoint of the fetcher.
  base::StringPiece service_endpoint{
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/"};

  // Path of the service. See the service specification at
  // google3/google/internal/kids/chrome/v1/kidschromemanagement.proto for
  // examples.
  base::StringPiece service_path;

  // HTTP method used to communicate with the service.
  Method method;

  // Basename for histograms
  base::StringPiece histogram_basename;

  net::NetworkTrafficAnnotationTag (*traffic_annotation)();

  std::string GetHttpMethod() const;
};

constexpr FetcherConfig kListFamilyMembersConfig{
    .service_path = "families/mine/members",
    .method = FetcherConfig::Method::kGet,
    .histogram_basename = "Signin.ListFamilyMembersRequest",
    .traffic_annotation = annotations::ListFamilyMembersTag,
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_CONFIG_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"
#include "chromecast/common/cors_exempt_headers.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentBrowserClient> CastContentBrowserClient::Create(
    CastFeatureListCreator* feature_list_creator) {
  return std::make_unique<CastRuntimeContentBrowserClient>(
      feature_list_creator);
}

// static
std::vector<std::string> CastContentBrowserClient::GetCorsExemptHeadersList() {
  auto headers = GetLegacyCorsExemptHeaders();
  return std::vector<std::string>(headers.begin(), headers.end());
}

}  // namespace shell
}  // namespace chromecast

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentBrowserClient> CastContentBrowserClient::Create(
    CastFeatureListCreator* feature_list_creator) {
  return CastRuntimeContentBrowserClient::Create(feature_list_creator);
}

// static
std::vector<std::string> CastContentBrowserClient::GetCorsExemptHeadersList() {
  return GetCastCoreCorsExemptHeadersList();
}

}  // namespace shell
}  // namespace chromecast

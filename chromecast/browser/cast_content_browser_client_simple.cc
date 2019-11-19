// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromecast/browser/cast_content_browser_client.h"


namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentBrowserClient> CastContentBrowserClient::Create(
    CastFeatureListCreator* cast_feature_list_creator) {
  return base::WrapUnique(
      new CastContentBrowserClient(cast_feature_list_creator));
}

// static
std::vector<std::string> CastContentBrowserClient::GetCorsExemptHeadersList() {
  return std::vector<std::string>();
}

}  // namespace shell
}  // namespace chromecast

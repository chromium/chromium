// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

namespace chromecast {

// static
std::unique_ptr<CastRuntimeContentBrowserClient>
CastRuntimeContentBrowserClient::Create(
    CastFeatureListCreator* feature_list_creator) {
  return std::make_unique<CastRuntimeContentBrowserClient>(
      feature_list_creator);
}

}  // namespace chromecast

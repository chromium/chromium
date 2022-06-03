// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastBrowserMainParts> CastBrowserMainParts::Create(
    content::MainFunctionParams parameters,
    CastContentBrowserClient* cast_content_browser_client) {
  return std::make_unique<CastBrowserMainParts>(std::move(parameters),
                                                cast_content_browser_client);
}

}  // namespace shell
}  // namespace chromecast

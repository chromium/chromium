// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

namespace extensions {

class CastExtensionsBrowserAPIProvider : public ExtensionsBrowserAPIProvider {
 public:
  CastExtensionsBrowserAPIProvider();

  CastExtensionsBrowserAPIProvider(const CastExtensionsBrowserAPIProvider&) =
      delete;
  CastExtensionsBrowserAPIProvider& operator=(
      const CastExtensionsBrowserAPIProvider&) = delete;

  ~CastExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSIONS_BROWSER_API_PROVIDER_H_

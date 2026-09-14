// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SCOPED_CHROME_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_SCOPED_CHROME_EXTENSIONS_CLIENT_H_

#include <memory>

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ChromeExtensionsClient;

// ScopedChromeExtensionsClient is an RAII helper that creates and initializes
// an extensions::ChromeExtensionsClient instance and sets it as the current
// ExtensionsClient on construction, and resets the ExtensionsClient to null on
// destruction.
//
// Note: ExtensionsClient has process-wide side effects upon initialization
// (such as registering manifest handlers and feature providers). In
// production, ScopedChromeExtensionsClient should only be created once per
// process (e.g. by BrowserProcessImpl, ChromeContentRendererClient, or
// standalone command-line handlers that exit immediately such as
// HandlePackExtensionSwitches).
class ScopedChromeExtensionsClient {
 public:
  ScopedChromeExtensionsClient();
  ScopedChromeExtensionsClient(const ScopedChromeExtensionsClient&) = delete;
  ScopedChromeExtensionsClient& operator=(const ScopedChromeExtensionsClient&) =
      delete;
  ~ScopedChromeExtensionsClient();

 private:
  std::unique_ptr<ChromeExtensionsClient> client_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_SCOPED_CHROME_EXTENSIONS_CLIENT_H_

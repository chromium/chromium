// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SIDEBAR_SIDEBAR_INFO_H_
#define CHROME_COMMON_EXTENSIONS_API_SIDEBAR_SIDEBAR_INFO_H_

#include <string>

#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "url/gurl.h"

namespace extensions {

// Structured contents of the "sidebar" manifest key.
struct SidebarInfo : public Extension::ManifestData {
  static const char* kManifestDataKey;

  SidebarInfo();
  ~SidebarInfo() override;

  // Returns true when the extension declares the "sidebar" key.
  static bool HasSidebar(const Extension& extension);

  // Returns the URL that "sidebar.default_path" resolves to, or an empty URL if
  // the extension does not set it.
  static const GURL& GetDefaultUrl(const Extension& extension);

  // URL of the default sidebar page, or empty when unset.
  // `SidebarManifestHandler::Parse()` resolves "sidebar.default_path" against
  // the extension origin and only stores a valid `chrome-extension://` URL.
  GURL default_url;
};

// Parses the "sidebar" manifest key.
class SidebarManifestHandler : public ManifestHandler {
 public:
  SidebarManifestHandler();
  SidebarManifestHandler(const SidebarManifestHandler&) = delete;
  SidebarManifestHandler& operator=(const SidebarManifestHandler&) = delete;
  ~SidebarManifestHandler() override;

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  // ManifestHandler:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SIDEBAR_SIDEBAR_INFO_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_URL_HANDLERS_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_URL_HANDLERS_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

// Chrome-specific extension manifest URL handlers.

namespace extensions {

namespace chrome_manifest_urls {
const GURL& GetDevToolsPage(const Extension* extension);
}

// Stores Chrome URL overrides specified in extensions' manifests.
struct URLOverrides : public Extension::ManifestData {
  typedef std::map<const std::string, GURL> URLOverrideMap;

  URLOverrides();
  ~URLOverrides() override;

  static const URLOverrideMap& GetChromeURLOverrides(
      const Extension* extension);

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs.
  URLOverrideMap chrome_url_overrides_;
};

// Parses the "devtools_page" manifest key.
class DevToolsPageHandler : public ManifestHandler {
 public:
  DevToolsPageHandler();

  DevToolsPageHandler(const DevToolsPageHandler&) = delete;
  DevToolsPageHandler& operator=(const DevToolsPageHandler&) = delete;

  ~DevToolsPageHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

// Parses the "chrome_url_overrides" manifest key.
class URLOverridesHandler : public ManifestHandler {
 public:
  URLOverridesHandler();

  URLOverridesHandler(const URLOverridesHandler&) = delete;
  URLOverridesHandler& operator=(const URLOverridesHandler&) = delete;

  ~URLOverridesHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_URL_HANDLERS_H_

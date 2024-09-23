// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_INFO_H_
#define CHROME_COMMON_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_INFO_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Structured contents of the "side_panel" key.
struct SidePanelInfo : public Extension::ManifestData {
  SidePanelInfo();
  ~SidePanelInfo() override;

  // Returns true when 'side_panel' is defined for the extension.
  static bool HasSidePanel(const Extension* extension);

  // Get default_path.
  static std::string GetDefaultPath(const Extension* extension);

  // SidePanelService relies on this local extension path only if it wasn't set
  // using `sidePanel` API.
  std::string default_path;
};

// Parses the "side_panel" manifest key.
class SidePanelManifestHandler : public ManifestHandler {
 public:
  SidePanelManifestHandler();
  SidePanelManifestHandler(const SidePanelManifestHandler&) = delete;
  SidePanelManifestHandler& operator=(const SidePanelManifestHandler&) = delete;
  ~SidePanelManifestHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_INFO_H_

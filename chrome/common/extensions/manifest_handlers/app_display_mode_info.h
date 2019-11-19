// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_DISPLAY_MODE_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_DISPLAY_MODE_INFO_H_

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace extensions {

// A structure to hold the parsed app display mode data.
struct AppDisplayModeInfo : public Extension::ManifestData {
  AppDisplayModeInfo();
  ~AppDisplayModeInfo() override;

  static blink::mojom::DisplayMode GetDisplayMode(const Extension* extension);

  // The display mode requested in the web app manifest.
  blink::mojom::DisplayMode display_mode;
};

// Parses the "app.display_mode" manifest key.
class AppDisplayModeHandler : public ManifestHandler {
 public:
  AppDisplayModeHandler();
  ~AppDisplayModeHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(AppDisplayModeHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_DISPLAY_MODE_INFO_H_

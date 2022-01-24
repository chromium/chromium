// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_LINKED_APP_ICONS_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_LINKED_APP_ICONS_H_

#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the parsed linked app icon data.
struct LinkedAppIcons : public Extension::ManifestData {
  static constexpr int kAnySize = 0;

  struct IconInfo {
    IconInfo();
    ~IconInfo();

    GURL url;
    int size;
  };

  LinkedAppIcons();
  LinkedAppIcons(const LinkedAppIcons& other);
  ~LinkedAppIcons() override;

  static const LinkedAppIcons& GetLinkedAppIcons(const Extension* extension);

  std::vector<IconInfo> icons;
};

// Parses the "app.linked_icons" manifest key.
class LinkedAppIconsHandler : public ManifestHandler {
 public:
  LinkedAppIconsHandler();

  LinkedAppIconsHandler(const LinkedAppIconsHandler&) = delete;
  LinkedAppIconsHandler& operator=(const LinkedAppIconsHandler&) = delete;

  ~LinkedAppIconsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_LINKED_APP_ICONS_H_

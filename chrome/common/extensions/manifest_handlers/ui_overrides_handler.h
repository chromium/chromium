// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

class ManifestPermission;

// UIOverrides is associated with "chrome_ui_overrides" manifest key, and
// represents manifest settings to override aspects of the Chrome user
// interface.
struct UIOverrides : public Extension::ManifestData {
  UIOverrides();
  ~UIOverrides() override;

  static const UIOverrides* Get(const Extension* extension);

  static bool RemovesBookmarkButton(const Extension* extension);
  static bool RemovesBookmarkShortcut(const Extension* extension);
  static bool RemovesBookmarkAllTabsShortcut(const Extension* extension);

  std::unique_ptr<api::manifest_types::ChromeUIOverrides::Bookmarks_ui>
      bookmarks_ui;

  std::unique_ptr<ManifestPermission> manifest_permission;

 private:
  DISALLOW_COPY_AND_ASSIGN(UIOverrides);
};

class UIOverridesHandler : public ManifestHandler {
 public:
  UIOverridesHandler();
  ~UIOverridesHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

  ManifestPermission* CreatePermission() override;
  ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) override;

 private:
  class ManifestPermissionImpl;

  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(UIOverridesHandler);
};

}  // namespace extensions
#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_

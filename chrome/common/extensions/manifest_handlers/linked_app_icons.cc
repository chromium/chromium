// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

static base::LazyInstance<LinkedAppIcons>::DestructorAtExit
    g_empty_linked_app_icons = LAZY_INSTANCE_INITIALIZER;

}  // namespace

LinkedAppIcons::IconInfo::IconInfo() {
}

LinkedAppIcons::IconInfo::~IconInfo() {
}

LinkedAppIcons::LinkedAppIcons() {
}

LinkedAppIcons::LinkedAppIcons(const LinkedAppIcons& other) = default;

LinkedAppIcons::~LinkedAppIcons() {
}

// static
const LinkedAppIcons& LinkedAppIcons::GetLinkedAppIcons(
    const Extension* extension) {
  LinkedAppIcons* info = static_cast<LinkedAppIcons*>(
      extension->GetManifestData(keys::kLinkedAppIcons));
  return info ? *info : g_empty_linked_app_icons.Get();
}

LinkedAppIconsHandler::LinkedAppIconsHandler() {
}

LinkedAppIconsHandler::~LinkedAppIconsHandler() {
}

bool LinkedAppIconsHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<LinkedAppIcons> linked_app_icons(new LinkedAppIcons);

  const base::Value* icons_value = nullptr;
  const base::ListValue* icons_list = nullptr;
  if (extension->manifest()->Get(keys::kLinkedAppIcons, &icons_value)) {
    if (!icons_value->GetAsList(&icons_list)) {
      *error = base::UTF8ToUTF16(
          extensions::manifest_errors::kInvalidLinkedAppIcons);
      return false;
    }

    for (const auto& icon_value : *icons_list) {
      const base::DictionaryValue* icon_dict = nullptr;
      if (!icon_value.GetAsDictionary(&icon_dict)) {
        *error = base::UTF8ToUTF16(
            extensions::manifest_errors::kInvalidLinkedAppIcon);
        return false;
      }

      std::string url_string;
      if (!icon_dict->GetString(keys::kLinkedAppIconURL, &url_string)) {
        *error = base::UTF8ToUTF16(
            extensions::manifest_errors::kInvalidLinkedAppIconURL);
        return false;
      }

      LinkedAppIcons::IconInfo info;
      info.url = GURL(url_string);
      if (!info.url.is_valid()) {
        *error = base::UTF8ToUTF16(
            extensions::manifest_errors::kInvalidLinkedAppIconURL);
        return false;
      }

      if (!icon_dict->GetInteger(keys::kLinkedAppIconSize, &info.size)) {
        *error = base::UTF8ToUTF16(
            extensions::manifest_errors::kInvalidLinkedAppIconSize);
        return false;
      }

      linked_app_icons->icons.push_back(info);
    }
  }

  extension->SetManifestData(keys::kLinkedAppIcons,
                             std::move(linked_app_icons));
  return true;
}

base::span<const char* const> LinkedAppIconsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kLinkedAppIcons};
  return kKeys;
}

}  // namespace extensions

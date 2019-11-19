// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace {

struct SystemIndicatorInfo : public Extension::ManifestData {
  SystemIndicatorInfo();
  ~SystemIndicatorInfo() override;

  ExtensionIconSet icon_set;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorInfo);
};

SystemIndicatorInfo::SystemIndicatorInfo() {}
SystemIndicatorInfo::~SystemIndicatorInfo() = default;

}  // namespace

SystemIndicatorHandler::SystemIndicatorHandler() {
}

SystemIndicatorHandler::~SystemIndicatorHandler() {
}

const ExtensionIconSet* SystemIndicatorHandler::GetSystemIndicatorIcon(
    const Extension& extension) {
  const auto* info = static_cast<SystemIndicatorInfo*>(
      extension.GetManifestData(manifest_keys::kSystemIndicator));
  return info ? &info->icon_set : nullptr;
}

bool SystemIndicatorHandler::Parse(Extension* extension,
                                   base::string16* error) {
  const base::DictionaryValue* system_indicator_value = nullptr;
  if (!extension->manifest()->GetDictionary(
          manifest_keys::kSystemIndicator, &system_indicator_value)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidSystemIndicator);
    return false;
  }

  auto set_manifest_data = [extension](const ExtensionIconSet& icon_set) {
    auto info = std::make_unique<SystemIndicatorInfo>();
    info->icon_set = icon_set;
    extension->SetManifestData(manifest_keys::kSystemIndicator,
                               std::move(info));
  };

  const base::Value* icon_value =
      system_indicator_value->FindKey(manifest_keys::kActionDefaultIcon);
  if (!icon_value) {
    // Empty icon set.
    set_manifest_data(ExtensionIconSet());
    return true;
  }

  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or a non-empty string value.
  ExtensionIconSet icons;
  if (icon_value->is_dict()) {
    if (!manifest_handler_helpers::LoadIconsFromDictionary(
            static_cast<const base::DictionaryValue*>(icon_value), &icons,
            error)) {
      return false;
    }
    set_manifest_data(icons);
    return true;
  }

  if (icon_value->is_string()) {
    std::string default_icon = icon_value->GetString();
    if (!manifest_handler_helpers::NormalizeAndValidatePath(&default_icon)) {
      *error = base::ASCIIToUTF16(manifest_errors::kInvalidActionDefaultIcon);
      return false;
    }
    // Choose the most optimistic (highest) icon density regardless of the
    // actual icon resolution, whatever that happens to be. Code elsewhere
    // knows how to scale down to 19.
    icons.Add(extension_misc::EXTENSION_ICON_GIGANTOR, default_icon);
    set_manifest_data(icons);
    return true;
  }

  *error = base::ASCIIToUTF16(manifest_errors::kInvalidActionDefaultIcon);
  return false;
}

base::span<const char* const> SystemIndicatorHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kSystemIndicator};
  return kKeys;
}

}  // namespace extensions

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace chromeos {

ChromeOSSystemExtensionHandler::ChromeOSSystemExtensionHandler() = default;

ChromeOSSystemExtensionHandler::~ChromeOSSystemExtensionHandler() = default;

bool ChromeOSSystemExtensionHandler::Parse(extensions::Extension* extension,
                                           std::u16string* error) {
  const base::DictionaryValue* system_extension_dict = nullptr;
  if (!extension->manifest()->GetDictionary(
          extensions::manifest_keys::kChromeOSSystemExtension,
          &system_extension_dict)) {
    *error = base::ASCIIToUTF16(kInvalidChromeOSSystemExtensionDeclaration);
    return false;
  }

  return true;
}

bool ChromeOSSystemExtensionHandler::AlwaysParseForType(
    extensions::Manifest::Type type) const {
  return type == extensions::Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION;
}

base::span<const char* const> ChromeOSSystemExtensionHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      extensions::manifest_keys::kChromeOSSystemExtension};
  return kKeys;
}

}  // namespace chromeos

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"

namespace chromeos {

namespace {

using extensions::PermissionsParser;
using extensions::mojom::APIPermissionID;

bool VerifyExternallyConnectableDefinition(extensions::Extension* extension) {
  const base::DictionaryValue* externally_connectable = nullptr;
  // chromeos_system_extension's 'externally_connectable' must exist.
  if (!extension->manifest()->GetDictionary(
          extensions::manifest_keys::kExternallyConnectable,
          &externally_connectable))
    return false;

  // chromeos_system_extension's 'externally_connectable' can only specify
  // "matches".
  if (externally_connectable->DictSize() != 1 ||
      !externally_connectable->FindKey("matches"))
    return false;

  auto matches_list = externally_connectable->FindKey("matches")->GetList();
  if (matches_list.size() != 1)
    return false;

  const auto& extension_info = GetChromeOSExtensionInfoForId(extension->id());

  // Verifies allowlisted origins.
  return matches_list.front().GetString() == extension_info.pwa_origin;
}

}  // namespace

ChromeOSSystemExtensionHandler::ChromeOSSystemExtensionHandler() = default;

ChromeOSSystemExtensionHandler::~ChromeOSSystemExtensionHandler() = default;

bool ChromeOSSystemExtensionHandler::Parse(extensions::Extension* extension,
                                           std::u16string* error) {
  if (!IsChromeOSSystemExtension(extension->id())) {
    *error = base::ASCIIToUTF16(kInvalidChromeOSSystemExtensionId);
    return false;
  }

  const base::DictionaryValue* system_extension_dict = nullptr;
  if (!extension->manifest()->GetDictionary(
          extensions::manifest_keys::kChromeOSSystemExtension,
          &system_extension_dict)) {
    *error = base::ASCIIToUTF16(kInvalidChromeOSSystemExtensionDeclaration);
    return false;
  }

  // Verifies that chromeos_system_extension's serial number permission is not
  // declared as a required permission. It can only be declared in the
  // "optional_permissions" key. It is a privacy requirement to prompt the user
  // with a warning the first time the serial number is accessed.
  if (PermissionsParser::HasAPIPermission(
          extension, APIPermissionID::kChromeOSTelemetrySerialNumber)) {
    *error = base::ASCIIToUTF16(kSerialNumberPermissionMustBeOptional);
    return false;
  }

  // Verifies that chromeos_system_extension's externally_connectable key exists
  // and contains one origin only.
  if (!VerifyExternallyConnectableDefinition(extension)) {
    *error = base::ASCIIToUTF16(kInvalidExternallyConnectableDeclaration);
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

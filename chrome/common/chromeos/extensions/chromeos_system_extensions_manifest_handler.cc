// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
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
  const base::Value* externally_connectable_value = nullptr;
  // chromeos_system_extension's 'externally_connectable' must exist.
  if (!extension->manifest()->GetDictionary(
          extensions::manifest_keys::kExternallyConnectable,
          &externally_connectable_value))
    return false;

  const auto* externally_connectable_dict =
      externally_connectable_value->GetIfDict();

  // chromeos_system_extension's 'externally_connectable' can only specify
  // "matches".
  if (!externally_connectable_dict ||
      externally_connectable_dict->size() != 1 ||
      !externally_connectable_dict->Find("matches"))
    return false;

  const auto* matches_list =
      externally_connectable_dict->Find("matches")->GetIfList();
  if (!matches_list || matches_list->size() != 1)
    return false;

  const auto& extension_info = GetChromeOSExtensionInfoForId(extension->id());

  // Verifies allowlisted origins.
  return matches_list->front().GetString() == extension_info.pwa_origin;
}

}  // namespace

ChromeOSSystemExtensionHandler::ChromeOSSystemExtensionHandler() = default;

ChromeOSSystemExtensionHandler::~ChromeOSSystemExtensionHandler() = default;

bool ChromeOSSystemExtensionHandler::Parse(extensions::Extension* extension,
                                           std::u16string* error) {
  const base::Value* system_extension_dict = nullptr;
  if (!extension->manifest()->GetDictionary(
          extensions::manifest_keys::kChromeOSSystemExtension,
          &system_extension_dict)) {
    *error = base::ASCIIToUTF16(kInvalidChromeOSSystemExtensionDeclaration);
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

base::span<const char* const> ChromeOSSystemExtensionHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      extensions::manifest_keys::kChromeOSSystemExtension};
  return kKeys;
}

}  // namespace chromeos

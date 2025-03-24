// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"

namespace chromeos {

namespace {

using extensions::PermissionsParser;
using extensions::mojom::APIPermissionID;

bool VerifyExternallyConnectableDefinition(extensions::Extension* extension) {
  const base::Value::Dict* externally_connectable_dict =
      extension->manifest()->FindDictPath(
          extensions::manifest_keys::kExternallyConnectable);
  // chromeos_system_extension's 'externally_connectable' must exist.
  if (!externally_connectable_dict) {
    return false;
  }

  // chromeos_system_extension's 'externally_connectable' can only specify
  // "matches".
  if (externally_connectable_dict->size() != 1 ||
      !externally_connectable_dict->Find("matches")) {
    return false;
  }

  const auto* matches_list =
      externally_connectable_dict->Find("matches")->GetIfList();
  if (!matches_list || matches_list->empty()) {
    return false;
  }

  const auto& extension_info = GetChromeOSExtensionInfoById(extension->id());

  std::optional<std::string> iwa_origin;
  if (extension_info.iwa_id.has_value()) {
    iwa_origin =
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      extension_info.iwa_id->id(), "/*"});
  }
  for (const auto& match : *matches_list) {
    const auto& match_str = match.GetString();
    if (match_str != extension_info.pwa_origin && match_str != iwa_origin) {
      return false;
    }
  }
  return true;
}

}  // namespace

ChromeOSSystemExtensionHandler::ChromeOSSystemExtensionHandler() = default;

ChromeOSSystemExtensionHandler::~ChromeOSSystemExtensionHandler() = default;

bool ChromeOSSystemExtensionHandler::Parse(extensions::Extension* extension,
                                           std::u16string* error) {
  if (extension->id() == kChromeOSSystemExtensionDevExtensionId &&
      !IsChromeOSSystemExtensionDevExtensionEnabled()) {
    *error = base::ASCIIToUTF16(kInvalidChromeOSSystemExtensionId);
    return false;
  }

  if (!extension->manifest()->FindDictPath(
          extensions::manifest_keys::kChromeOSSystemExtension)) {
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

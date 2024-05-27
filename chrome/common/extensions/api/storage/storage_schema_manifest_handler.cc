// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/policy/core/common/schema.h"
#include "extensions/common/extension.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

using extensions::manifest_keys::kStorageManagedSchema;

namespace extensions {

StorageSchemaManifestHandler::StorageSchemaManifestHandler() {}

StorageSchemaManifestHandler::~StorageSchemaManifestHandler() {}

// static
base::expected<policy::Schema, std::string>
StorageSchemaManifestHandler::GetSchema(const Extension* extension) {
  std::string path;
  if (const std::string* temp =
          extension->manifest()->FindStringPath(kStorageManagedSchema)) {
    path = *temp;
  }
  base::FilePath file = base::FilePath::FromUTF8Unsafe(path);
  if (file.IsAbsolute() || file.ReferencesParent()) {
    return base::unexpected(base::StringPrintf(
        "%s must be a relative path without ..", kStorageManagedSchema));
  }
  file = extension->path().AppendASCII(path);
  if (!base::PathExists(file)) {
    return base::unexpected(base::StringPrintf(
        "File does not exist: %" PRFilePath, file.value().c_str()));
  }
  std::string content;
  if (!base::ReadFileToString(file, &content)) {
    return base::unexpected(
        base::StringPrintf("Can't read %" PRFilePath, file.value().c_str()));
  }
  return policy::Schema::Parse(content);
}

bool StorageSchemaManifestHandler::Parse(Extension* extension,
                                         std::u16string* error) {
  if (extension->manifest()->FindStringPath(kStorageManagedSchema) == nullptr) {
    *error = base::ASCIIToUTF16(
        base::StringPrintf("%s must be a string", kStorageManagedSchema));
    return false;
  }

  // If an extension declares the "storage.managed_schema" key then it gets
  // the "storage" permission implicitly.
  PermissionsParser::AddAPIPermission(extension,
                                      mojom::APIPermissionID::kStorage);

  return true;
}

bool StorageSchemaManifestHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  RETURN_IF_ERROR(GetSchema(extension), [&error](const auto& e) {
    *error = e;
    return false;
  });

  return true;
}

base::span<const char* const> StorageSchemaManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {kStorageManagedSchema};
  return kKeys;
}

}  // namespace extensions

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "url/gurl.h"

namespace extensions {

FileSystemProviderCapabilities::FileSystemProviderCapabilities()
    : configurable_(false),
      watchable_(false),
      multiple_mounts_(false),
      source_(SOURCE_FILE) {
}

FileSystemProviderCapabilities::FileSystemProviderCapabilities(
    bool configurable,
    bool watchable,
    bool multiple_mounts,
    FileSystemProviderSource source)
    : configurable_(configurable),
      watchable_(watchable),
      multiple_mounts_(multiple_mounts),
      source_(source) {
}

FileSystemProviderCapabilities::~FileSystemProviderCapabilities() {
}

FileSystemProviderCapabilitiesHandler::FileSystemProviderCapabilitiesHandler() {
}

FileSystemProviderCapabilitiesHandler::
    ~FileSystemProviderCapabilitiesHandler() {
}

// static
const FileSystemProviderCapabilities* FileSystemProviderCapabilities::Get(
    const Extension* extension) {
  return static_cast<FileSystemProviderCapabilities*>(
      extension->GetManifestData(
          manifest_keys::kFileSystemProviderCapabilities));
}

bool FileSystemProviderCapabilitiesHandler::Parse(Extension* extension,
                                                  std::u16string* error) {
  const bool has_permission = extensions::PermissionsParser::HasAPIPermission(
      extension, mojom::APIPermissionID::kFileSystemProvider);
  const base::Value::Dict* section = extension->manifest()->FindDictPath(
      manifest_keys::kFileSystemProviderCapabilities);

  if (has_permission && !section) {
    *error = manifest_errors::kInvalidFileSystemProviderMissingCapabilities;
    return false;
  }

  if (!has_permission && section) {
    // If the permission is not specified, then the section has simply no
    // effect, hence just a warning.
    extension->AddInstallWarning(extensions::InstallWarning(
        manifest_errors::kInvalidFileSystemProviderMissingPermission));
    return true;
  }

  if (!has_permission && !section) {
    // No permission and no capabilities, so ignore.
    return true;
  }

  auto idl_capabilities =
      api::manifest_types::FileSystemProviderCapabilities::FromValue(*section);
  if (!idl_capabilities.has_value()) {
    *error = std::move(idl_capabilities).error();
    return false;
  }

  FileSystemProviderSource source = SOURCE_FILE;
  switch (idl_capabilities->source) {
    case api::manifest_types::FileSystemProviderSource::kFile:
      source = SOURCE_FILE;
      break;
    case api::manifest_types::FileSystemProviderSource::kDevice:
      source = SOURCE_DEVICE;
      break;
    case api::manifest_types::FileSystemProviderSource::kNetwork:
      source = SOURCE_NETWORK;
      break;
    case api::manifest_types::FileSystemProviderSource::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<FileSystemProviderCapabilities> capabilities(
      new FileSystemProviderCapabilities(
          idl_capabilities->configurable.value_or(false) /* false by default */,
          idl_capabilities->watchable.value_or(false) /* false by default */,
          idl_capabilities->multiple_mounts.value_or(
              false) /* false by default */,
          source));

  extension->SetManifestData(manifest_keys::kFileSystemProviderCapabilities,
                             std::move(capabilities));
  return true;
}

base::span<const char* const> FileSystemProviderCapabilitiesHandler::Keys()
    const {
  static constexpr const char* kKeys[] = {
      manifest_keys::kFileSystemProviderCapabilities};
  return kKeys;
}

bool FileSystemProviderCapabilitiesHandler::AlwaysParseForType(
    Manifest::Type type) const {
  return true;
}

}  // namespace extensions

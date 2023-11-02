// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_FILE_SYSTEM_PROVIDER_CAPABILITIES_FILE_SYSTEM_PROVIDER_CAPABILITIES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_FILE_SYSTEM_PROVIDER_CAPABILITIES_FILE_SYSTEM_PROVIDER_CAPABILITIES_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Source of provider file systems.
enum FileSystemProviderSource { SOURCE_FILE, SOURCE_NETWORK, SOURCE_DEVICE };

// Represents capabilities of a file system provider.
class FileSystemProviderCapabilities : public Extension::ManifestData {
 public:
  FileSystemProviderCapabilities();
  FileSystemProviderCapabilities(bool configurable,
                                 bool watchable,
                                 bool multiple_mounts,
                                 FileSystemProviderSource source);
  ~FileSystemProviderCapabilities() override;

  // Returns capabilities of a providing |extension| if declared in the
  // manifets. Otherwise NULL.
  static const FileSystemProviderCapabilities* Get(const Extension* extension);

  bool configurable() const { return configurable_; }
  bool watchable() const { return watchable_; }
  bool multiple_mounts() const { return multiple_mounts_; }
  FileSystemProviderSource source() const { return source_; }

 private:
  bool configurable_;
  bool watchable_;
  bool multiple_mounts_;
  FileSystemProviderSource source_;
};

// Parses the "file_system_provider_capabilities" manifest key.
class FileSystemProviderCapabilitiesHandler : public ManifestHandler {
 public:
  FileSystemProviderCapabilitiesHandler();

  FileSystemProviderCapabilitiesHandler(
      const FileSystemProviderCapabilitiesHandler&) = delete;
  FileSystemProviderCapabilitiesHandler& operator=(
      const FileSystemProviderCapabilitiesHandler&) = delete;

  ~FileSystemProviderCapabilitiesHandler() override;

  // ManifestHandler overrides.
  bool Parse(Extension* extension, std::u16string* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_FILE_SYSTEM_PROVIDER_CAPABILITIES_FILE_SYSTEM_PROVIDER_CAPABILITIES_HANDLER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/side_panel/side_panel_info.h"

#include "base/files/file_util.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

using SidePanelManifestKeys = api::side_panel::ManifestKeys;

namespace {

const SidePanelInfo* GetResourcesInfo(const Extension* extension) {
  return static_cast<SidePanelInfo*>(
      extension->GetManifestData(SidePanelManifestKeys::kSidePanel));
}

std::unique_ptr<SidePanelInfo> ParseFromDictionary(const Extension& extension,
                                                   std::u16string* error) {
  SidePanelManifestKeys manifest_keys;
  if (!SidePanelManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, *error)) {
    return nullptr;
  }
  auto info = std::make_unique<SidePanelInfo>();
  info->default_path = std::move(manifest_keys.side_panel.default_path);
  return info;
}

bool ExtensionResourceExists(const Extension* extension,
                             const std::string& path) {
  auto resource_path = extension->GetResource(path).GetFilePath();
  return !resource_path.empty() && base::PathExists(resource_path);
}

}  // namespace

SidePanelInfo::SidePanelInfo() = default;

SidePanelInfo::~SidePanelInfo() = default;

// static
bool SidePanelInfo::HasSidePanel(const Extension* extension) {
  const SidePanelInfo* info = GetResourcesInfo(extension);
  return info != nullptr;
}

// static
std::string SidePanelInfo::GetDefaultPath(const Extension* extension) {
  const SidePanelInfo* info = GetResourcesInfo(extension);
  return info ? info->default_path : "";
}

SidePanelManifestHandler::SidePanelManifestHandler() = default;

SidePanelManifestHandler::~SidePanelManifestHandler() = default;

bool SidePanelManifestHandler::Parse(Extension* extension,
                                     std::u16string* error) {
  auto info = ParseFromDictionary(*extension, error);
  if (!info) {
    return false;
  }
  extension->SetManifestData(SidePanelManifestKeys::kSidePanel,
                             std::move(info));
  return true;
}

base::span<const char* const> SidePanelManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {SidePanelManifestKeys::kSidePanel};
  return kKeys;
}

bool SidePanelManifestHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  std::string path = SidePanelInfo::GetDefaultPath(extension);
  GURL side_panel_url = extension->GetResourceURL(path);

  if (!side_panel_url.is_valid() ||
      !ExtensionResourceExists(extension, side_panel_url.path())) {
    *error = errors::kSidePanelManifestDefaultPathError;
    return false;
  }

  return true;
}

}  // namespace extensions

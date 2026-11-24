// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/sidebar/sidebar_info.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/common/extensions/api/sidebar.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"

namespace extensions {

using SidebarManifestKeys = api::sidebar::ManifestKeys;

// static
const char* SidebarInfo::kManifestDataKey = SidebarManifestKeys::kSidebar;

namespace {

std::unique_ptr<SidebarInfo> ParseFromDictionary(const Extension& extension,
                                                 std::u16string* error) {
  SidebarManifestKeys manifest_keys;
  if (!SidebarManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, *error)) {
    return nullptr;
  }

  CHECK(manifest_keys.sidebar.has_value());
  // `default_path` is optional. Return an empty `SidebarInfo` when omitted.
  std::optional<std::string>& default_path =
      manifest_keys.sidebar->default_path;
  if (!default_path.has_value()) {
    return std::make_unique<SidebarInfo>();
  }

  // Reject absolute URLs (`chrome-extension://...`), network-path URLs
  // (`//...`), backslashes (`\`), and parent-directory references (`..`) that
  // `GetResourceURL()` would otherwise resolve within the extension origin.
  if (GURL(*default_path).is_valid() || default_path->starts_with("//") ||
      default_path->contains('\\') ||
      base::FilePath::FromUTF8Unsafe(*default_path).ReferencesParent()) {
    *error = manifest_errors::kSidebarManifestDefaultPathInvalid;
    return nullptr;
  }

  const GURL url = extension.GetResourceURL(*default_path);
  if (!url.is_valid() || !url.SchemeIs(kExtensionScheme)) {
    *error = manifest_errors::kSidebarManifestDefaultPathInvalid;
    return nullptr;
  }

  auto info = std::make_unique<SidebarInfo>();
  info->default_path = std::move(*default_path);
  return info;
}

}  // namespace

SidebarInfo::SidebarInfo() = default;

SidebarInfo::~SidebarInfo() = default;

// static
bool SidebarInfo::HasSidebar(const Extension& extension) {
  return extension.GetManifestData<SidebarInfo>() != nullptr;
}

// static
const std::string& SidebarInfo::GetDefaultPath(const Extension& extension) {
  const SidebarInfo* info = extension.GetManifestData<SidebarInfo>();
  return info ? info->default_path : base::EmptyString();
}

SidebarManifestHandler::SidebarManifestHandler() = default;

SidebarManifestHandler::~SidebarManifestHandler() = default;

bool SidebarManifestHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  std::unique_ptr<SidebarInfo> info = ParseFromDictionary(*extension, error);
  if (!info) {
    return false;
  }
  extension->SetManifestData(std::move(info));
  return true;
}

base::span<const char* const> SidebarManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {SidebarManifestKeys::kSidebar};
  return kKeys;
}

}  // namespace extensions

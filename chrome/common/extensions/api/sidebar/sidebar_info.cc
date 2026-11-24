// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/sidebar/sidebar_info.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
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
  const std::optional<std::string>& default_path =
      manifest_keys.sidebar->default_path;
  if (!default_path.has_value()) {
    return std::make_unique<SidebarInfo>();
  }

  // `GetResourceURL()` returns an invalid URL for a path that resolves outside
  // the extension origin or to an unsafe file path. The scheme check also
  // rejects `blob:` and `filesystem:` URLs, which share the extension origin.
  // Other forms, such as "../panel.html", resolve to a URL inside the
  // extension, so storing the resolved URL keeps them harmless.
  GURL url = extension.GetResourceURL(*default_path);
  if (!url.is_valid() || !url.SchemeIs(kExtensionScheme)) {
    *error = manifest_errors::kSidebarManifestDefaultPathInvalid;
    return nullptr;
  }

  auto info = std::make_unique<SidebarInfo>();
  info->default_url = std::move(url);
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
const GURL& SidebarInfo::GetDefaultUrl(const Extension& extension) {
  const SidebarInfo* info = extension.GetManifestData<SidebarInfo>();
  return info ? info->default_url : GURL::EmptyGURL();
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

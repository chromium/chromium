// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

using ManifestKeys = api::omnibox::ManifestKeys;

}  // namespace

// static
const std::string& OmniboxInfo::GetKeyword(const Extension* extension) {
  OmniboxInfo* info = static_cast<OmniboxInfo*>(
      extension->GetManifestData(ManifestKeys::kOmnibox));
  return info ? info->keyword : base::EmptyString();
}

OmniboxHandler::OmniboxHandler() = default;

OmniboxHandler::~OmniboxHandler() = default;

bool OmniboxHandler::Parse(Extension* extension, std::u16string* error) {
  ManifestKeys manifest_keys;
  if (!ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  auto info = std::make_unique<OmniboxInfo>();
  info->keyword = manifest_keys.omnibox.keyword;
  if (info->keyword.empty()) {
    *error = manifest_errors::kEmptyOmniboxKeyword;
    return false;
  }

  extension->SetManifestData(ManifestKeys::kOmnibox, std::move(info));
  return true;
}

base::span<const char* const> OmniboxHandler::Keys() const {
  static constexpr const char* kKeys[] = {ManifestKeys::kOmnibox};
  return kKeys;
}

}  // namespace extensions

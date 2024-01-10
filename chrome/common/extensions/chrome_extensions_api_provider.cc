// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_api_provider.h"

#include <string_view>

#include "chrome/common/extensions/api/api_features.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/api/manifest_features.h"
#include "chrome/common/extensions/api/permission_features.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {

ChromeExtensionsAPIProvider::ChromeExtensionsAPIProvider() {}
ChromeExtensionsAPIProvider::~ChromeExtensionsAPIProvider() = default;

void ChromeExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddChromeAPIFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddManifestFeatures(
    FeatureProvider* provider) {
  AddChromeManifestFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddChromePermissionFeatures(provider);
}

void ChromeExtensionsAPIProvider::AddBehaviorFeatures(
    FeatureProvider* provider) {
  // Note: No chrome-specific behavior features.
}

void ChromeExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROME_EXTENSION_API_FEATURES);
}

bool ChromeExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return api::ChromeGeneratedSchemas::IsGenerated(name);
}

std::string_view ChromeExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::ChromeGeneratedSchemas::Get(name);
}

void ChromeExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      chrome_api_permissions::GetPermissionInfos(),
      chrome_api_permissions::GetPermissionAliases());
}

void ChromeExtensionsAPIProvider::RegisterManifestHandlers() {
  RegisterChromeManifestHandlers();
}

}  // namespace extensions

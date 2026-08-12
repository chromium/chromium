// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/extensions/chromeos_extensions_api_provider.h"

#include <string_view>

#include "chromeos/ash/experiences/extensions/api/api_features.h"
#include "chromeos/ash/experiences/extensions/api/generated_schemas.h"
#include "chromeos/ash/experiences/extensions/api/manifest_features.h"
#include "chromeos/ash/experiences/extensions/api/permission_features.h"
#include "chromeos/ash/experiences/extensions/chromeos_extensions_api_permissions.h"
#include "chromeos/ash/experiences/extensions/grit/chromeos_extensions_resources.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"

namespace ash {

ChromeOSExtensionsAPIProvider::ChromeOSExtensionsAPIProvider() = default;
ChromeOSExtensionsAPIProvider::~ChromeOSExtensionsAPIProvider() = default;

void ChromeOSExtensionsAPIProvider::AddAPIFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSAPIFeatures(provider);
}

void ChromeOSExtensionsAPIProvider::AddManifestFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSManifestFeatures(provider);
}

void ChromeOSExtensionsAPIProvider::AddPermissionFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSPermissionFeatures(provider);
}

void ChromeOSExtensionsAPIProvider::AddBehaviorFeatures(
    extensions::FeatureProvider* provider) {
  // Note: No chromeos-specific behavior features.
}

void ChromeOSExtensionsAPIProvider::AddAPIJSONSources(
    extensions::JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROMEOS_EXTENSION_API_FEATURES);
}

bool ChromeOSExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return extensions::api::ChromeOSGeneratedSchemas::IsGenerated(name);
}

std::string_view ChromeOSExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return extensions::api::ChromeOSGeneratedSchemas::Get(name);
}

void ChromeOSExtensionsAPIProvider::RegisterPermissions(
    extensions::PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      ash::extensions_api_permissions::GetPermissionInfos(), {});
}

void ChromeOSExtensionsAPIProvider::RegisterManifestHandlers(
    extensions::ManifestHandlerRegistry* registry) {
  // Do nothing.
}

}  // namespace ash

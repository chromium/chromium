// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_provider.h"

#include <memory>
#include <string_view>

#include "base/logging.h"
#include "chrome/common/chromeos/extensions/api/api_features.h"
#include "chrome/common/chromeos/extensions/api/generated_schemas.h"
#include "chrome/common/chromeos/extensions/api/manifest_features.h"
#include "chrome/common/chromeos/extensions/api/permission_features.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_permissions.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"
#include "chrome/common/chromeos/extensions/grit/chromeos_system_extensions_resources.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handler_registry.h"
#include "extensions/common/permissions/permissions_info.h"

namespace chromeos {

ChromeOSSystemExtensionsAPIProvider::ChromeOSSystemExtensionsAPIProvider()
    : registry_(extensions::ManifestHandlerRegistry::Get()) {}

ChromeOSSystemExtensionsAPIProvider::~ChromeOSSystemExtensionsAPIProvider() =
    default;

void ChromeOSSystemExtensionsAPIProvider::AddAPIFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSSystemExtensionsAPIFeatures(provider);
}

void ChromeOSSystemExtensionsAPIProvider::AddManifestFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSSystemExtensionsManifestFeatures(provider);
}

void ChromeOSSystemExtensionsAPIProvider::AddPermissionFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSSystemExtensionsPermissionFeatures(provider);
}

void ChromeOSSystemExtensionsAPIProvider::AddBehaviorFeatures(
    extensions::FeatureProvider* provider) {
  // Note: No chromeOS-specific behavior features.
}

void ChromeOSSystemExtensionsAPIProvider::AddAPIJSONSources(
    extensions::JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROMEOS_SYSTEM_EXTENSIONS_API__API_FEATURES_JSON);
}

bool ChromeOSSystemExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return api::ChromeOSGeneratedSchemas::IsGenerated(name);
}

std::string_view ChromeOSSystemExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::ChromeOSGeneratedSchemas::Get(name);
}

void ChromeOSSystemExtensionsAPIProvider::RegisterPermissions(
    extensions::PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      extensions_api_permissions::GetPermissionInfos(),
      base::span<const extensions::Alias>());
}

void ChromeOSSystemExtensionsAPIProvider::RegisterManifestHandlers() {
  DCHECK(!extensions::ManifestHandler::IsRegistrationFinalized());

  registry_->RegisterHandler(
      std::make_unique<ChromeOSSystemExtensionHandler>());
}

}  // namespace chromeos

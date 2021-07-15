// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_api_provider.h"

#include <memory>

#include "base/logging.h"
#include "chrome/common/chromeos/extensions/api/manifest_features.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_handler.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permissions_info.h"

namespace chromeos {

ChromeOSSystemExtensionsAPIProvider::ChromeOSSystemExtensionsAPIProvider()
    : registry_(extensions::ManifestHandlerRegistry::Get()) {}

ChromeOSSystemExtensionsAPIProvider::~ChromeOSSystemExtensionsAPIProvider() =
    default;

void ChromeOSSystemExtensionsAPIProvider::AddAPIFeatures(
    extensions::FeatureProvider* provider) {}

void ChromeOSSystemExtensionsAPIProvider::AddManifestFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeOSSystemExtensionsManifestFeatures(provider);
}

void ChromeOSSystemExtensionsAPIProvider::AddPermissionFeatures(
    extensions::FeatureProvider* provider) {}

void ChromeOSSystemExtensionsAPIProvider::AddBehaviorFeatures(
    extensions::FeatureProvider* provider) {
  // Note: No chromeOS-specific behavior features.
}

void ChromeOSSystemExtensionsAPIProvider::AddAPIJSONSources(
    extensions::JSONFeatureProviderSource* json_source) {}

bool ChromeOSSystemExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return false;
}

base::StringPiece ChromeOSSystemExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return "";
}

void ChromeOSSystemExtensionsAPIProvider::RegisterPermissions(
    extensions::PermissionsInfo* permissions_info) {}

void ChromeOSSystemExtensionsAPIProvider::RegisterManifestHandlers() {
  DCHECK(!extensions::ManifestHandler::IsRegistrationFinalized());

  registry_->RegisterHandler(
      std::make_unique<ChromeOSSystemExtensionHandler>());
}

}  // namespace chromeos

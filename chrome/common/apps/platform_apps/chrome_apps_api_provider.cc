// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"

#include <string_view>

#include "chrome/common/apps/platform_apps/api/api_features.h"
#include "chrome/common/apps/platform_apps/api/generated_schemas.h"
#include "chrome/common/apps/platform_apps/api/permission_features.h"
#include "chrome/common/apps/platform_apps/chrome_apps_api_permissions.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/alias.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"

namespace chrome_apps {

ChromeAppsAPIProvider::ChromeAppsAPIProvider() = default;
ChromeAppsAPIProvider::~ChromeAppsAPIProvider() = default;

void ChromeAppsAPIProvider::AddAPIFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeAppsAPIFeatures(provider);
}

void ChromeAppsAPIProvider::AddManifestFeatures(
    extensions::FeatureProvider* provider) {
  // No Chrome-apps-specific manifest features (yet).
}

void ChromeAppsAPIProvider::AddPermissionFeatures(
    extensions::FeatureProvider* provider) {
  AddChromeAppsPermissionFeatures(provider);
}

void ChromeAppsAPIProvider::AddBehaviorFeatures(
    extensions::FeatureProvider* provider) {
  // No Chrome-apps-specific manifest features.
}

void ChromeAppsAPIProvider::AddAPIJSONSources(
    extensions::JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROME_APP_API_FEATURES);
}

bool ChromeAppsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return api::ChromeAppsGeneratedSchemas::IsGenerated(name);
}

std::string_view ChromeAppsAPIProvider::GetAPISchema(const std::string& name) {
  return api::ChromeAppsGeneratedSchemas::Get(name);
}

void ChromeAppsAPIProvider::RegisterPermissions(
    extensions::PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      chrome_apps_api_permissions::GetPermissionInfos(),
      base::span<const extensions::Alias>());
}

void ChromeAppsAPIProvider::RegisterManifestHandlers() {
  // No apps-specific manifest handlers (yet).
}

}  // namespace chrome_apps

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_extensions_api_provider.h"

#include <memory>

#include "chromecast/common/cast_redirect_manifest_handler.h"
#include "chromecast/common/extensions_api/cast_api_features.h"
#include "chromecast/common/extensions_api/cast_api_permissions.h"
#include "chromecast/common/extensions_api/cast_manifest_features.h"
#include "chromecast/common/extensions_api/cast_permission_features.h"
#include "chromecast/common/extensions_api/generated_schemas.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/shell/grit/app_shell_resources.h"

namespace extensions {

CastExtensionsAPIProvider::CastExtensionsAPIProvider() {}
CastExtensionsAPIProvider::~CastExtensionsAPIProvider() = default;

void CastExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddCastAPIFeatures(provider);
}

void CastExtensionsAPIProvider::AddManifestFeatures(FeatureProvider* provider) {
  AddCastManifestFeatures(provider);
}

void CastExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddCastPermissionFeatures(provider);
}

void CastExtensionsAPIProvider::AddBehaviorFeatures(FeatureProvider* provider) {
  // Note: No cast-specific behavior features.
}

void CastExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_SHELL_EXTENSION_API_FEATURES);
}

bool CastExtensionsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return cast::api::CastGeneratedSchemas::IsGenerated(name);
}

base::StringPiece CastExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return cast::api::CastGeneratedSchemas::Get(name);
}

void CastExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      cast_api_permissions::GetPermissionInfos(),
      cast_api_permissions::GetPermissionAliases());
}

void CastExtensionsAPIProvider::RegisterManifestHandlers() {
  // TODO(devlin): Pass in |registry| rather than Get()ing it.
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();

  registry->RegisterHandler(
      std::make_unique<chromecast::CastRedirectHandler>());
}

}  // namespace extensions

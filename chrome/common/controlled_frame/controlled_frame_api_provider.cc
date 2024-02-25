// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/controlled_frame/controlled_frame_api_provider.h"

#include <string_view>

#include "chrome/common/controlled_frame/api/api_features.h"
#include "chrome/common/controlled_frame/api/generated_schemas.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/features/json_feature_provider_source.h"

namespace controlled_frame {

ControlledFrameAPIProvider::ControlledFrameAPIProvider() = default;
ControlledFrameAPIProvider::~ControlledFrameAPIProvider() = default;

void ControlledFrameAPIProvider::AddAPIFeatures(
    extensions::FeatureProvider* provider) {
  AddControlledFrameAPIFeatures(provider);
}

void ControlledFrameAPIProvider::AddManifestFeatures(
    extensions::FeatureProvider* provider) {
  // No Controlled Frame specific manifest features.
}

void ControlledFrameAPIProvider::AddPermissionFeatures(
    extensions::FeatureProvider* provider) {
  // No Controlled Frame specific permission features.
}

void ControlledFrameAPIProvider::AddBehaviorFeatures(
    extensions::FeatureProvider* provider) {
  // No Controlled Frame specific manifest features.
}

void ControlledFrameAPIProvider::AddAPIJSONSources(
    extensions::JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_CHROME_CONTROLLED_FRAME_API_FEATURES);
}

bool ControlledFrameAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return api::ControlledFrameGeneratedSchemas::IsGenerated(name);
}

std::string_view ControlledFrameAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::ControlledFrameGeneratedSchemas::Get(name);
}

void ControlledFrameAPIProvider::RegisterPermissions(
    extensions::PermissionsInfo* permissions_info) {
  // No Controlled Frame specific permissions.
}

void ControlledFrameAPIProvider::RegisterManifestHandlers() {
  // No Controlled Frame specific manifest handlers.
}

}  // namespace controlled_frame

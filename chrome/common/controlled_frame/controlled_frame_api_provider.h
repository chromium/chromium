// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_API_PROVIDER_H_
#define CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_API_PROVIDER_H_

#include <string_view>

#include "extensions/common/extensions_api_provider.h"

namespace controlled_frame {

class ControlledFrameAPIProvider : public extensions::ExtensionsAPIProvider {
 public:
  ControlledFrameAPIProvider();
  ControlledFrameAPIProvider(const ControlledFrameAPIProvider&) = delete;
  ControlledFrameAPIProvider& operator=(const ControlledFrameAPIProvider&) =
      delete;
  ~ControlledFrameAPIProvider() override;

  // ExtensionsAPIProvider:
  void AddAPIFeatures(extensions::FeatureProvider* provider) override;
  void AddManifestFeatures(extensions::FeatureProvider* provider) override;
  void AddPermissionFeatures(extensions::FeatureProvider* provider) override;
  void AddBehaviorFeatures(extensions::FeatureProvider* provider) override;
  void AddAPIJSONSources(
      extensions::JSONFeatureProviderSource* json_source) override;
  bool IsAPISchemaGenerated(const std::string& name) override;
  std::string_view GetAPISchema(const std::string& name) override;
  void RegisterPermissions(
      extensions::PermissionsInfo* permissions_info) override;
  void RegisterManifestHandlers() override;
};

}  // namespace controlled_frame

#endif  // CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_API_PROVIDER_H_

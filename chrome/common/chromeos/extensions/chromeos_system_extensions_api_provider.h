// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PROVIDER_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PROVIDER_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extensions_api_provider.h"

namespace extensions {
class FeatureProvider;
class JSONFeatureProviderSource;
class ManifestHandlerRegistry;
class PermissionsInfo;

}  // namespace extensions

namespace chromeos {

class ChromeOSSystemExtensionsAPIProvider
    : public extensions::ExtensionsAPIProvider {
 public:
  ChromeOSSystemExtensionsAPIProvider();
  ChromeOSSystemExtensionsAPIProvider(
      const ChromeOSSystemExtensionsAPIProvider&) = delete;
  ChromeOSSystemExtensionsAPIProvider& operator=(
      const ChromeOSSystemExtensionsAPIProvider&) = delete;
  ~ChromeOSSystemExtensionsAPIProvider() override;

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

 private:
  raw_ptr<extensions::ManifestHandlerRegistry> registry_;  // not owned
};

}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PROVIDER_H_

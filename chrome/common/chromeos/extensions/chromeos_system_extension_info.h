// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace chromeos {

namespace switches {

extern const char kTelemetryExtensionManufacturerOverrideForTesting[];
extern const char kTelemetryExtensionPwaOriginOverrideForTesting[];
extern const char kTelemetryExtensionIwaIdOverrideForTesting[];

}  // namespace switches

// An extension for development. It is only enabled when specific feature flags
// are enabled.
inline constexpr char kChromeOSSystemExtensionDevExtensionId[] =
    "jmalcmbicpnakfkncbgbcmlmgpfkhdca";

// Information related to a ChromeOS system extension.
struct ChromeOSSystemExtensionInfo {
  ChromeOSSystemExtensionInfo(
      const base::flat_set<std::string>& manufacturers,
      const std::optional<std::string>& pwa_origin,
      const std::optional<web_package::SignedWebBundleId>& iwa_id);
  ChromeOSSystemExtensionInfo(const ChromeOSSystemExtensionInfo&);
  ChromeOSSystemExtensionInfo& operator=(const ChromeOSSystemExtensionInfo&);
  ~ChromeOSSystemExtensionInfo();

  // The extension is allowed on devices from these manufacturers.
  base::flat_set<std::string> manufacturers;
  // The connected pwa origin. |nullopt| if no connected pwa.
  std::optional<std::string> pwa_origin;
  // The connected iwa id. |nullopt| if no connected iwa.
  std::optional<web_package::SignedWebBundleId> iwa_id;
};

// Check if |id| is a ChromeOS system extension id.
bool IsChromeOSSystemExtension(const std::string& id);

// Get the information of a ChromeOS system extension by id.
const ChromeOSSystemExtensionInfo& GetChromeOSExtensionInfoById(
    const std::string& id);

// Check if `manufacturer` provides any chromeos system extension.
bool IsChromeOSSystemExtensionProvider(const std::string& manufacturer);

// Returns if dev extension is enable.
bool IsChromeOSSystemExtensionDevExtensionEnabled();

// Returns if the IWA id is a valid 3p diagnostics IWA id.
bool Is3pDiagnosticsIwaId(const web_package::SignedWebBundleId& id);

// Exported for testing.
// A helper class to restore the allowlist after tests. This should be created
// before modifying base::CommandLine to avoid changing the original allowlist.
class ScopedChromeOSSystemExtensionInfo {
 public:
  virtual ~ScopedChromeOSSystemExtensionInfo() = default;

  // Creates a instance.
  static std::unique_ptr<ScopedChromeOSSystemExtensionInfo> CreateForTesting();

  // Applies the change from the related switches in base::CommandLine.
  virtual void ApplyCommandLineSwitchesForTesting() = 0;

 protected:
  ScopedChromeOSSystemExtensionInfo() = default;
};

}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_

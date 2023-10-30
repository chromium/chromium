// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_BROWSER_PARAMS_PROXY_H_
#define CHROMEOS_STARTUP_BROWSER_PARAMS_PROXY_H_

#include "base/no_destructor.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chromeos {

// Provides access to the browser's initialization parameters,
// dispatching to either BrowserInitParams or BrowserPostLoginParams.
class COMPONENT_EXPORT(CHROMEOS_STARTUP) BrowserParamsProxy {
 public:
  static BrowserParamsProxy* Get();

  // Wait for the user to login and post-login parameters to be available.
  // NOTE: This needs to be called before post-login parameters are accessed.
  // Please note that this method is not thread-safe and should be called
  // before any threads are created in the browser process.
  static void WaitForLogin();

  // Init and post-login parameters' accessors are listed starting from here.
  bool IsCrosapiDisabledForTesting() const;

  uint32_t CrosapiVersion() const;

  bool DeprecatedAshMetricsEnabledHasValue() const;

  bool AshMetricsEnabled() const;

  crosapi::mojom::SessionType SessionType() const;

  crosapi::mojom::DeviceMode DeviceMode() const;

  const absl::optional<base::flat_map<base::Token, uint32_t>>&
  InterfaceVersions() const;

  const crosapi::mojom::DefaultPathsPtr& DefaultPaths() const;

  crosapi::mojom::MetricsReportingManaged AshMetricsManaged() const;

  crosapi::mojom::ExoImeSupport ExoImeSupport() const;

  const absl::optional<std::string>& CrosUserIdHash() const;

  const absl::optional<std::vector<uint8_t>>& DeviceAccountPolicy() const;

  uint64_t LastPolicyFetchAttemptTimestamp() const;

  const crosapi::mojom::IdleInfoPtr& IdleInfo() const;

  crosapi::mojom::InitialBrowserAction InitialBrowserAction() const;

  const crosapi::mojom::AccountPtr& DeviceAccount() const;

  bool WebAppsEnabled() const;

  bool StandaloneBrowserIsPrimary() const;

  const crosapi::mojom::NativeThemeInfoPtr& NativeThemeInfo() const;

  const crosapi::mojom::DevicePropertiesPtr& DeviceProperties() const;

  crosapi::mojom::OndeviceHandwritingSupport OndeviceHandwritingSupport() const;

  const absl::optional<std::vector<crosapi::mojom::BuildFlag>>& BuildFlags()
      const;

  crosapi::mojom::OpenUrlFrom StartupUrlsFrom() const;

  const crosapi::mojom::DeviceSettingsPtr& DeviceSettings() const;

  const absl::optional<std::string>& MetricsServiceClientId() const;

  const crosapi::mojom::EntropySourcePtr& EntropySource() const;

  uint64_t UkmClientId() const;

  bool StandaloneBrowserIsOnlyBrowser() const;

  bool PublishChromeApps() const;

  bool PublishHostedApps() const;

  crosapi::mojom::BrowserInitParams::InitialKeepAlive InitialKeepAlive() const;

  bool IsUnfilteredBluetoothDeviceEnabled() const;

  const absl::optional<std::vector<std::string>>& AshCapabilities() const;

  const absl::optional<std::vector<GURL>>& AcceptedInternalAshUrls() const;

  bool IsDeviceEnterprisedManaged() const;

  crosapi::mojom::BrowserInitParams::DeviceType DeviceType() const;

  bool IsOndeviceSpeechSupported() const;

  const absl::optional<base::flat_map<policy::PolicyNamespace, base::Value>>&
  DeviceAccountComponentPolicy() const;

  const absl::optional<std::string>& AshChromeVersion() const;

  bool UseCupsForPrinting() const;

  bool UseFlossBluetooth() const;

  bool IsFlossAvailable() const;

  bool IsFlossAvailabilityCheckNeeded() const;

  bool IsCurrentUserDeviceOwner() const;

  bool IsCurrentUserEphemeral() const;

  bool EnableLacrosTtsSupport() const;

  crosapi::mojom::BrowserInitParams::LacrosSelection LacrosSelection() const;

  bool IsWindowLayoutMenuEnabled() const;

  bool IsCloudGamingDevice() const;

  crosapi::mojom::BrowserInitParams::GpuSandboxStartMode GpuSandboxStartMode()
      const;

  const crosapi::mojom::ExtensionKeepListPtr& ExtensionKeepList() const;

  bool VcControlsUiEnabled() const;

  const crosapi::mojom::StandaloneBrowserAppServiceBlockList*
  StandaloneBrowserAppServiceBlockList() const;

  bool EnableCpuMappableNativeGpuMemoryBuffers() const;

  bool OopVideoDecodingEnabled() const;

  bool IsUploadOfficeToCloudEnabled() const;

  bool EnableClipboardHistoryRefresh() const;

  bool IsVariableRefreshRateAlwaysOn() const;

  bool IsPdfOcrEnabled() const;

  bool IsDriveFsBulkPinningAvailable() const;

  bool IsSysUiDownloadsIntegrationV2Enabled() const;

  bool IsCrosBatterySaverAvailable() const;

  bool IsAppInstallServiceUriEnabled() const;

  bool IsDeskProfilesEnabled() const;

  bool IsCrosWebAppShortcutUiUpdateEnabled() const;

 private:
  friend base::NoDestructor<BrowserParamsProxy>;

  BrowserParamsProxy() = default;
  ~BrowserParamsProxy();
};

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_BROWSER_PARAMS_PROXY_H_

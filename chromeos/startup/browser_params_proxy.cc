// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_params_proxy.h"

#include "chromeos/startup/browser_init_params.h"
#include "chromeos/startup/startup.h"

namespace chromeos {

// static
BrowserParamsProxy* BrowserParamsProxy::Get() {
  static base::NoDestructor<BrowserParamsProxy> browser_params_proxy;
  return browser_params_proxy.get();
}

bool BrowserParamsProxy::IsCrosapiDisabledForTesting() {
  return BrowserInitParams::IsCrosapiDisabledForTesting();
}

void BrowserParamsProxy::DisableCrosapiForTesting() {
  return BrowserInitParams::DisableCrosapiForTesting();
}

uint32_t BrowserParamsProxy::CrosapiVersion() const {
  return BrowserInitParams::Get()->crosapi_version;
}

bool BrowserParamsProxy::DeprecatedAshMetricsEnabledHasValue() const {
  return BrowserInitParams::Get()->deprecated_ash_metrics_enabled_has_value;
}

bool BrowserParamsProxy::AshMetricsEnabled() const {
  return BrowserInitParams::Get()->ash_metrics_enabled;
}

crosapi::mojom::SessionType BrowserParamsProxy::SessionType() const {
  return BrowserInitParams::Get()->session_type;
}

crosapi::mojom::DeviceMode BrowserParamsProxy::DeviceMode() const {
  return BrowserInitParams::Get()->device_mode;
}

const std::optional<base::flat_map<base::Token, uint32_t>>&
BrowserParamsProxy::InterfaceVersions() const {
  return BrowserInitParams::Get()->interface_versions;
}

const crosapi::mojom::DefaultPathsPtr& BrowserParamsProxy::DefaultPaths()
    const {
  return BrowserInitParams::Get()->default_paths;
}

crosapi::mojom::MetricsReportingManaged BrowserParamsProxy::AshMetricsManaged()
    const {
  return BrowserInitParams::Get()->ash_metrics_managed;
}

crosapi::mojom::ExoImeSupport BrowserParamsProxy::ExoImeSupport() const {
  return BrowserInitParams::Get()->exo_ime_support;
}

const std::optional<std::string>& BrowserParamsProxy::CrosUserIdHash() const {
  return BrowserInitParams::Get()->cros_user_id_hash;
}

const std::optional<std::vector<uint8_t>>&
BrowserParamsProxy::DeviceAccountPolicy() const {
  return BrowserInitParams::Get()->device_account_policy;
}

uint64_t BrowserParamsProxy::LastPolicyFetchAttemptTimestamp() const {
  return BrowserInitParams::Get()->last_policy_fetch_attempt_timestamp;
}

const crosapi::mojom::IdleInfoPtr& BrowserParamsProxy::IdleInfo() const {
  return BrowserInitParams::Get()->idle_info;
}

crosapi::mojom::InitialBrowserAction BrowserParamsProxy::InitialBrowserAction()
    const {
  return BrowserInitParams::Get()->initial_browser_action;
}

const crosapi::mojom::AccountPtr& BrowserParamsProxy::DeviceAccount() const {
  return BrowserInitParams::Get()->device_account;
}

const crosapi::mojom::NativeThemeInfoPtr& BrowserParamsProxy::NativeThemeInfo()
    const {
  return BrowserInitParams::Get()->native_theme_info;
}

const crosapi::mojom::DevicePropertiesPtr&
BrowserParamsProxy::DeviceProperties() const {
  return BrowserInitParams::Get()->device_properties;
}

crosapi::mojom::OndeviceHandwritingSupport
BrowserParamsProxy::OndeviceHandwritingSupport() const {
  return BrowserInitParams::Get()->ondevice_handwriting_support;
}

const std::optional<std::vector<crosapi::mojom::BuildFlag>>&
BrowserParamsProxy::BuildFlags() const {
  return BrowserInitParams::Get()->build_flags;
}

crosapi::mojom::OpenUrlFrom BrowserParamsProxy::StartupUrlsFrom() const {
  return BrowserInitParams::Get()->startup_urls_from;
}

const crosapi::mojom::DeviceSettingsPtr& BrowserParamsProxy::DeviceSettings()
    const {
  return BrowserInitParams::Get()->device_settings;
}

const std::optional<std::string>& BrowserParamsProxy::MetricsServiceClientId()
    const {
  return BrowserInitParams::Get()->metrics_service_client_id;
}

const crosapi::mojom::EntropySourcePtr& BrowserParamsProxy::EntropySource()
    const {
  return BrowserInitParams::Get()->entropy_source;
}

uint64_t BrowserParamsProxy::LimitedEntropySyntheticTrialSeed() const {
  return BrowserInitParams::Get()->limited_entropy_synthetic_trial_seed;
}

uint64_t BrowserParamsProxy::UkmClientId() const {
  return BrowserInitParams::Get()->ukm_client_id;
}

bool BrowserParamsProxy::PublishChromeApps() const {
  return BrowserInitParams::Get()->publish_chrome_apps;
}

bool BrowserParamsProxy::PublishHostedApps() const {
  return BrowserInitParams::Get()->publish_hosted_apps;
}

crosapi::mojom::BrowserInitParams::InitialKeepAlive
BrowserParamsProxy::InitialKeepAlive() const {
  return BrowserInitParams::Get()->initial_keep_alive;
}

bool BrowserParamsProxy::IsUnfilteredBluetoothDeviceEnabled() const {
  return BrowserInitParams::Get()->is_unfiltered_bluetooth_device_enabled;
}

const std::optional<std::vector<std::string>>&
BrowserParamsProxy::AshCapabilities() const {
  return BrowserInitParams::Get()->ash_capabilities;
}

const std::optional<std::vector<GURL>>&
BrowserParamsProxy::AcceptedInternalAshUrls() const {
  return BrowserInitParams::Get()->accepted_internal_ash_urls;
}

bool BrowserParamsProxy::IsDeviceEnterprisedManaged() const {
  return BrowserInitParams::Get()->is_device_enterprised_managed;
}

crosapi::mojom::BrowserInitParams::DeviceType BrowserParamsProxy::DeviceType()
    const {
  return BrowserInitParams::Get()->device_type;
}

bool BrowserParamsProxy::IsOndeviceSpeechSupported() const {
  return BrowserInitParams::Get()->is_ondevice_speech_supported;
}

const std::optional<base::flat_map<policy::PolicyNamespace, base::Value>>&
BrowserParamsProxy::DeviceAccountComponentPolicy() const {
  return BrowserInitParams::Get()->device_account_component_policy;
}

const std::optional<std::string>& BrowserParamsProxy::AshChromeVersion() const {
  return BrowserInitParams::Get()->ash_chrome_version;
}

bool BrowserParamsProxy::UseCupsForPrinting() const {
  return BrowserInitParams::Get()->use_cups_for_printing;
}

bool BrowserParamsProxy::UseFlossBluetooth() const {
  return BrowserInitParams::Get()->use_floss_bluetooth;
}

bool BrowserParamsProxy::IsFlossAvailable() const {
  return BrowserInitParams::Get()->is_floss_available;
}

bool BrowserParamsProxy::IsFlossAvailabilityCheckNeeded() const {
  return BrowserInitParams::Get()->is_floss_availability_check_needed;
}

bool BrowserParamsProxy::IsLLPrivacyAvailable() const {
  return BrowserInitParams::Get()->is_llprivacy_available;
}

bool BrowserParamsProxy::IsCurrentUserDeviceOwner() const {
  return BrowserInitParams::Get()->is_current_user_device_owner;
}

bool BrowserParamsProxy::IsCurrentUserEphemeral() const {
  return BrowserInitParams::Get()->is_current_user_ephemeral;
}

bool BrowserParamsProxy::EnableLacrosTtsSupport() const {
  return BrowserInitParams::Get()->enable_lacros_tts_support;
}

crosapi::mojom::BrowserInitParams::LacrosSelection
BrowserParamsProxy::LacrosSelection() const {
  return BrowserInitParams::Get()->lacros_selection;
}

bool BrowserParamsProxy::IsCloudGamingDevice() const {
  return BrowserInitParams::Get()->is_cloud_gaming_device;
}

crosapi::mojom::BrowserInitParams::GpuSandboxStartMode
BrowserParamsProxy::GpuSandboxStartMode() const {
  return BrowserInitParams::Get()->gpu_sandbox_start_mode;
}

const crosapi::mojom::ExtensionKeepListPtr&
BrowserParamsProxy::ExtensionKeepList() const {
  return BrowserInitParams::Get()->extension_keep_list;
}

bool BrowserParamsProxy::VcControlsUiEnabled() const {
  return BrowserInitParams::Get()->vc_controls_ui_enabled;
}

const crosapi::mojom::StandaloneBrowserAppServiceBlockList*
BrowserParamsProxy::StandaloneBrowserAppServiceBlockList() const {
  return BrowserInitParams::Get()
      ->standalone_browser_app_service_blocklist.get();
}

bool BrowserParamsProxy::EnableCpuMappableNativeGpuMemoryBuffers() const {
  return BrowserInitParams::Get()
      ->enable_cpu_mappable_native_gpu_memory_buffers;
}

bool BrowserParamsProxy::OopVideoDecodingEnabled() const {
  return BrowserInitParams::Get()->oop_video_decoding_enabled;
}

bool BrowserParamsProxy::IsUploadOfficeToCloudEnabled() const {
  return BrowserInitParams::Get()->is_upload_office_to_cloud_enabled;
}

bool BrowserParamsProxy::EnableClipboardHistoryRefresh() const {
  return BrowserInitParams::Get()->enable_clipboard_history_refresh;
}

bool BrowserParamsProxy::IsVariableRefreshRateAlwaysOn() const {
  return BrowserInitParams::Get()->is_variable_refresh_rate_always_on;
}

bool BrowserParamsProxy::IsPdfOcrEnabled() const {
  return BrowserInitParams::Get()->is_pdf_ocr_enabled;
}

bool BrowserParamsProxy::IsDriveFsBulkPinningAvailable() const {
  return BrowserInitParams::Get()->is_drivefs_bulk_pinning_available;
}

bool BrowserParamsProxy::IsSysUiDownloadsIntegrationV2Enabled() const {
  return BrowserInitParams::Get()->is_sys_ui_downloads_integration_v2_enabled;
}

bool BrowserParamsProxy::IsCrosBatterySaverAvailable() const {
  return BrowserInitParams::Get()->is_cros_battery_saver_available;
}

bool BrowserParamsProxy::IsAppInstallServiceUriEnabled() const {
  return BrowserInitParams::Get()->is_app_install_service_uri_enabled;
}

bool BrowserParamsProxy::IsDeskProfilesEnabled() const {
  return BrowserInitParams::Get()->is_desk_profiles_enabled;
}

bool BrowserParamsProxy::ShouldDisableChromeComposeOnChromeOS() const {
  return BrowserInitParams::Get()->should_disable_chrome_compose_on_chromeos;
}

bool BrowserParamsProxy::IsCaptivePortalPopupWindowEnabled() const {
  return BrowserInitParams::Get()->is_captive_portal_popup_window_enabled;
}

bool BrowserParamsProxy::IsFileSystemProviderCloudFileSystemEnabled() const {
  return BrowserInitParams::Get()
      ->is_file_system_provider_cloud_file_system_enabled;
}

bool BrowserParamsProxy::IsFileSystemProviderContentCacheEnabled() const {
  return BrowserInitParams::Get()
      ->is_file_system_provider_content_cache_enabled;
}

bool BrowserParamsProxy::IsOrcaEnabled() const {
  return BrowserInitParams::Get()->is_orca_enabled;
}

bool BrowserParamsProxy::IsCrosMallWebAppEnabled() const {
  return BrowserInitParams::Get()->is_cros_mall_web_app_enabled;
}

bool BrowserParamsProxy::IsMahiEnabled() const {
  return BrowserInitParams::Get()->is_mahi_enabled;
}

bool BrowserParamsProxy::IsContainerAppPreinstallEnabled() const {
  return BrowserInitParams::Get()->is_container_app_preinstall_enabled;
}

bool BrowserParamsProxy::IsOrcaUseL10nStringsEnabled() const {
  return BrowserInitParams::Get()->is_orca_use_l10n_strings_enabled;
}

bool BrowserParamsProxy::IsOrcaInternationalizeEnabled() const {
  return BrowserInitParams::Get()->is_orca_internationalize_enabled;
}

}  // namespace chromeos

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"

class PrefService;

namespace ash {

class AutoConnectHandler;
class CellularConnectionHandler;
class CellularESimProfileHandler;
class CellularESimInstaller;
class CellularESimUninstallHandler;
class CellularInhibitor;
class CellularMetricsLogger;
class CellularNetworkMetricsLogger;
class CellularPolicyHandler;
class ClientCertResolver;
class ConnectionInfoMetricsLogger;
class DefaultNetworkMetricsLogger;
class EnterpriseManagedMetadataStore;
class EphemeralNetworkConfigurationHandler;
class EphemeralNetworkPoliciesEnablementHandler;
class ESimPolicyLoginMetricsLogger;
class GeolocationHandler;
class HiddenNetworkHandler;
class HotspotAllowedFlagHandler;
class HotspotCapabilitiesProvider;
class HotspotConfigurationHandler;
class HotspotController;
class HotspotFeatureUsageMetrics;
class HotspotMetricsHelper;
class HotspotStateHandler;
class HotspotEnabledStateNotifier;
class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
class ManagedNetworkConfigurationHandlerImpl;
class NetworkActivationHandler;
class NetworkCertificateHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkDeviceHandler;
class NetworkDeviceHandlerImpl;
class NetworkMetadataStore;
class NetworkProfileHandler;
class NetworkStateHandler;
class NetworkSmsHandler;
class Network3gppHandler;
class ProhibitedTechnologiesHandler;
class StubCellularNetworksProvider;
class TechnologyStateController;
class TextMessageProvider;
class UIProxyConfigService;
class VpnNetworkMetricsHelper;

// Class for handling initialization and access to chromeos network handlers.
// This class should NOT be used in unit tests. Instead, construct individual
// classes independently.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkHandler {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Sets the global fake instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkHandler* Get();

  NetworkHandler(const NetworkHandler&) = delete;
  NetworkHandler& operator=(const NetworkHandler&) = delete;

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Called whenever the pref services change, e.g. on login. Initializes
  // services with PrefService dependencies (i.e. ui_proxy_config_service).
  // |logged_in_profile_prefs| is the PrefService associated with the logged
  // in user profile. |device_prefs| is the PrefService associated with the
  // device (e.g. in Chrome, g_browser_process->local_state()).
  void InitializePrefServices(PrefService* logged_in_profile_prefs,
                              PrefService* device_prefs);

  // Must be called before pref services are shut down.
  void ShutdownPrefServices();

  // Global network configuration services.
  static bool HasUiProxyConfigService();
  static UIProxyConfigService* GetUiProxyConfigService();

  // Sets whether the device is managed by policy. This is called when the
  // primary user logs in.
  void SetIsEnterpriseManaged(bool is_enterprise_managed);

  // Returns a flag indicating if the device is managed by an enterprise
  // or not.
  bool is_enterprise_managed() { return is_enterprise_managed_; }

  // Returns the task runner for posting NetworkHandler calls from other
  // threads.
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

  // Do not use these accessors within this module; all dependencies should be
  // explicit so that classes can be constructed explicitly in tests without
  // NetworkHandler.
  AutoConnectHandler* auto_connect_handler();
  CellularConnectionHandler* cellular_connection_handler();
  CellularESimInstaller* cellular_esim_installer();
  CellularESimProfileHandler* cellular_esim_profile_handler();
  CellularESimUninstallHandler* cellular_esim_uninstall_handler();
  CellularInhibitor* cellular_inhibitor();
  CellularPolicyHandler* cellular_policy_handler();
  HiddenNetworkHandler* hidden_network_handler();
  HotspotCapabilitiesProvider* hotspot_capabilities_provider();
  HotspotController* hotspot_controller();
  HotspotConfigurationHandler* hotspot_configuration_handler();
  HotspotStateHandler* hotspot_state_handler();
  HotspotEnabledStateNotifier* hotspot_enabled_state_notifier();
  NetworkStateHandler* network_state_handler();
  NetworkDeviceHandler* network_device_handler();
  NetworkProfileHandler* network_profile_handler();
  NetworkConfigurationHandler* network_configuration_handler();
  ManagedCellularPrefHandler* managed_cellular_pref_handler();
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler();
  NetworkActivationHandler* network_activation_handler();
  NetworkCertificateHandler* network_certificate_handler();
  NetworkConnectionHandler* network_connection_handler();
  NetworkMetadataStore* network_metadata_store();
  NetworkSmsHandler* network_sms_handler();
  Network3gppHandler* network_3gpp_handler();
  GeolocationHandler* geolocation_handler();
  ProhibitedTechnologiesHandler* prohibited_technologies_handler();
  TechnologyStateController* technology_state_controller();
  TextMessageProvider* text_message_provider();

 private:
  friend class ConnectionInfoMetricsLoggerTest;

  NetworkHandler(std::unique_ptr<NetworkStateHandler> handler);
  virtual ~NetworkHandler();

  void Init();

  // Called when ephemeral network policies become enabled.
  void OnEphemeralNetworkPoliciesEnabled();

  // True when the device was manged by device policy at initialization time.
  // TODO: b/302726243 - Introduce
  // InstallAttributes::WasEnterpriseManagedAtStartup to avoid this.
  const bool was_enterprise_managed_at_startup_;

  // The order of these determines the (inverse) destruction order.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandlerImpl> network_device_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<CellularESimProfileHandler> cellular_esim_profile_handler_;
  std::unique_ptr<StubCellularNetworksProvider>
      stub_cellular_networks_provider_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<CellularESimUninstallHandler>
      cellular_esim_uninstall_handler_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;
  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
  std::unique_ptr<CellularMetricsLogger> cellular_metrics_logger_;
  std::unique_ptr<ConnectionInfoMetricsLogger> connection_info_metrics_logger_;
  std::unique_ptr<DefaultNetworkMetricsLogger> default_network_metrics_logger_;
  std::unique_ptr<HiddenNetworkHandler> hidden_network_handler_;
  std::unique_ptr<EnterpriseManagedMetadataStore>
      enterprise_managed_metadata_store_;
  std::unique_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  std::unique_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_;
  std::unique_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_;
  std::unique_ptr<HotspotStateHandler> hotspot_state_handler_;
  std::unique_ptr<HotspotController> hotspot_controller_;
  std::unique_ptr<HotspotConfigurationHandler> hotspot_configuration_handler_;
  std::unique_ptr<HotspotEnabledStateNotifier> hotspot_enabled_state_notifier_;
  std::unique_ptr<HotspotMetricsHelper> hotspot_metrics_helper_;
  std::unique_ptr<ESimPolicyLoginMetricsLogger>
      esim_policy_login_metrics_logger_;
  std::unique_ptr<VpnNetworkMetricsHelper> vpn_network_metrics_helper_;
  std::unique_ptr<CellularNetworkMetricsLogger>
      cellular_network_metrics_logger_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  std::unique_ptr<AutoConnectHandler> auto_connect_handler_;
  std::unique_ptr<NetworkCertificateHandler> network_certificate_handler_;
  std::unique_ptr<NetworkActivationHandler> network_activation_handler_;
  std::unique_ptr<ProhibitedTechnologiesHandler>
      prohibited_technologies_handler_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<Network3gppHandler> network_3gpp_handler_;
  std::unique_ptr<TextMessageProvider> text_message_provider_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<NetworkMetadataStore> network_metadata_store_;
  std::unique_ptr<EphemeralNetworkPoliciesEnablementHandler>
      ephemeral_network_policies_enablement_handler_;
  std::unique_ptr<EphemeralNetworkConfigurationHandler>
      ephemeral_network_configuration_handler_;

  // True when the device is managed by policy.
  bool is_enterprise_managed_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_H_

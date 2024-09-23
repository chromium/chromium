// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/auto_connect_handler.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/cellular_policy_handler.h"
#include "chromeos/ash/components/network/client_cert_resolver.h"
#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/ephemeral_network_configuration_handler.h"
#include "chromeos/ash/components/network/ephemeral_network_policies_enablement_handler.h"
#include "chromeos/ash/components/network/fake_network_state_handler.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/hidden_network_handler.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_configuration_handler.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/default_network_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/esim_policy_login_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/metrics/vpn_network_metrics_helper.h"
#include "chromeos/ash/components/network/network_3gpp_handler.h"
#include "chromeos/ash/components/network/network_activation_handler_impl.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler_impl.h"
#include "chromeos/ash/components/network/network_device_handler_impl.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_profile_observer.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/network/stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/network/text_message_provider.h"

namespace ash {

static NetworkHandler* g_network_handler = NULL;

NetworkHandler::NetworkHandler(std::unique_ptr<NetworkStateHandler> handler)
    : was_enterprise_managed_at_startup_(
          InstallAttributes::IsInitialized() &&
          InstallAttributes::Get()->IsEnterpriseManaged()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      network_state_handler_(std::move(handler)) {
  network_device_handler_.reset(new NetworkDeviceHandlerImpl());
  cellular_inhibitor_.reset(new CellularInhibitor());
  cellular_esim_profile_handler_.reset(new CellularESimProfileHandlerImpl());
  stub_cellular_networks_provider_.reset(new StubCellularNetworksProvider());
  technology_state_controller_.reset(new TechnologyStateController());
  cellular_connection_handler_.reset(new CellularConnectionHandler());
  network_profile_handler_.reset(new NetworkProfileHandler());
  network_configuration_handler_.reset(new NetworkConfigurationHandler());
  managed_network_configuration_handler_.reset(
      new ManagedNetworkConfigurationHandlerImpl());
  network_connection_handler_.reset(new NetworkConnectionHandlerImpl());
  cellular_esim_installer_.reset(new CellularESimInstaller());
  cellular_esim_uninstall_handler_.reset(new CellularESimUninstallHandler());
  cellular_policy_handler_.reset(new CellularPolicyHandler());
  esim_policy_login_metrics_logger_.reset(new ESimPolicyLoginMetricsLogger());
  managed_cellular_pref_handler_.reset(new ManagedCellularPrefHandler());
  cellular_metrics_logger_.reset(new CellularMetricsLogger());
  connection_info_metrics_logger_.reset(new ConnectionInfoMetricsLogger());
  default_network_metrics_logger_.reset(new DefaultNetworkMetricsLogger());
  hotspot_allowed_flag_handler_.reset(new HotspotAllowedFlagHandler());
  vpn_network_metrics_helper_.reset(new VpnNetworkMetricsHelper());
  hidden_network_handler_.reset(new HiddenNetworkHandler());
  enterprise_managed_metadata_store_.reset(
      new EnterpriseManagedMetadataStore());
  hotspot_capabilities_provider_.reset(new HotspotCapabilitiesProvider());
  hotspot_feature_usage_metrics_.reset(new HotspotFeatureUsageMetrics());
  hotspot_state_handler_.reset(new HotspotStateHandler());
  hotspot_controller_.reset(new HotspotController());
  hotspot_configuration_handler_.reset(new HotspotConfigurationHandler());
  hotspot_enabled_state_notifier_.reset(new HotspotEnabledStateNotifier());
  hotspot_metrics_helper_.reset(new HotspotMetricsHelper());
  if (NetworkCertLoader::IsInitialized()) {
    client_cert_resolver_.reset(new ClientCertResolver());
    auto_connect_handler_.reset(new AutoConnectHandler());
    network_certificate_handler_.reset(new NetworkCertificateHandler());
  }
  network_activation_handler_.reset(new NetworkActivationHandlerImpl());
  prohibited_technologies_handler_.reset(new ProhibitedTechnologiesHandler());
  network_sms_handler_.reset(new NetworkSmsHandler());
  text_message_provider_.reset(new TextMessageProvider());
  geolocation_handler_.reset(new GeolocationHandler());
  network_3gpp_handler_.reset(new Network3gppHandler());

  // Only watch ephemeral network policies enablement if ephemeral network
  // policies should be enabled by the feature or if the device policy to enable
  // ephemeral network policies should be respected.
  if (features::AreEphemeralNetworkPoliciesEnabled() ||
      features::CanEphemeralNetworkPoliciesBeEnabledByPolicy()) {
    // base::Unretained is safe because
    // `ephemeral_network_policies_enablement_handler_` is a member of
    // NetworkHandler so it will be destroyed before `this`.
    ephemeral_network_policies_enablement_handler_ =
        std::make_unique<EphemeralNetworkPoliciesEnablementHandler>(
            base::BindOnce(&NetworkHandler::OnEphemeralNetworkPoliciesEnabled,
                           base::Unretained(this)));
  }
}

NetworkHandler::~NetworkHandler() {
  network_state_handler_->Shutdown();
}

void NetworkHandler::Init() {
  network_state_handler_->InitShillPropertyHandler();
  network_device_handler_->Init(network_state_handler_.get());
  cellular_inhibitor_->Init(network_state_handler_.get(),
                            network_device_handler_.get());
  cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get());
  stub_cellular_networks_provider_->Init(network_state_handler_.get(),
                                         cellular_esim_profile_handler_.get(),
                                         managed_cellular_pref_handler_.get());
  technology_state_controller_->Init(network_state_handler_.get());
  cellular_connection_handler_->Init(network_state_handler_.get(),
                                     cellular_inhibitor_.get(),
                                     cellular_esim_profile_handler_.get());
  network_profile_handler_->Init();
  network_configuration_handler_->Init(network_state_handler_.get(),
                                       network_device_handler_.get());
  managed_network_configuration_handler_->Init(
      cellular_policy_handler_.get(), managed_cellular_pref_handler_.get(),
      network_state_handler_.get(), network_profile_handler_.get(),
      network_configuration_handler_.get(), network_device_handler_.get(),
      prohibited_technologies_handler_.get(), hotspot_controller_.get());
  network_connection_handler_->Init(
      network_state_handler_.get(), network_configuration_handler_.get(),
      managed_network_configuration_handler_.get(),
      cellular_connection_handler_.get());
  cellular_esim_installer_->Init(
      cellular_connection_handler_.get(), cellular_inhibitor_.get(),
      network_connection_handler_.get(), network_profile_handler_.get(),
      network_state_handler_.get());
  cellular_esim_uninstall_handler_->Init(
      cellular_inhibitor_.get(), cellular_esim_profile_handler_.get(),
      managed_cellular_pref_handler_.get(),
      network_configuration_handler_.get(), network_connection_handler_.get(),
      network_state_handler_.get());
  cellular_policy_handler_->Init(
      cellular_esim_profile_handler_.get(), cellular_esim_installer_.get(),
      cellular_inhibitor_.get(), network_profile_handler_.get(),
      network_state_handler_.get(), managed_cellular_pref_handler_.get(),
      managed_network_configuration_handler_.get());
  hidden_network_handler_->Init(managed_network_configuration_handler_.get(),
                                network_state_handler_.get());
  hotspot_allowed_flag_handler_->Init();
  hotspot_capabilities_provider_->Init(network_state_handler_.get(),
                                       hotspot_allowed_flag_handler_.get());
  hotspot_feature_usage_metrics_->Init(enterprise_managed_metadata_store_.get(),
                                       hotspot_capabilities_provider_.get());
  hotspot_state_handler_->Init();
  hotspot_controller_->Init(hotspot_capabilities_provider_.get(),
                            hotspot_feature_usage_metrics_.get(),
                            hotspot_state_handler_.get(),
                            technology_state_controller_.get());
  hotspot_configuration_handler_->Init();
  hotspot_enabled_state_notifier_->Init(hotspot_state_handler_.get(),
                                        hotspot_controller_.get());
  hotspot_metrics_helper_->Init(
      enterprise_managed_metadata_store_.get(),
      hotspot_capabilities_provider_.get(), hotspot_state_handler_.get(),
      hotspot_controller_.get(), hotspot_configuration_handler_.get(),
      hotspot_enabled_state_notifier_.get(), network_state_handler_.get());
  managed_cellular_pref_handler_->Init(network_state_handler_.get());
  esim_policy_login_metrics_logger_->Init(
      network_state_handler_.get(),
      managed_network_configuration_handler_.get());
  cellular_metrics_logger_->Init(network_state_handler_.get(),
                                 network_connection_handler_.get(),
                                 cellular_esim_profile_handler_.get(),
                                 managed_network_configuration_handler_.get());
  connection_info_metrics_logger_->Init(network_state_handler_.get(),
                                        network_connection_handler_.get());
  default_network_metrics_logger_->Init(network_state_handler_.get());
  vpn_network_metrics_helper_->Init(network_configuration_handler_.get());
  if (client_cert_resolver_) {
    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_network_configuration_handler_.get());
  }
  if (auto_connect_handler_) {
    auto_connect_handler_->Init(client_cert_resolver_.get(),
                                network_connection_handler_.get(),
                                network_state_handler_.get(),
                                managed_network_configuration_handler_.get());
  }
  prohibited_technologies_handler_->Init(
      managed_network_configuration_handler_.get(),
      network_state_handler_.get(), technology_state_controller_.get());

    network_sms_handler_->Init(network_state_handler_.get());

    text_message_provider_->Init(network_sms_handler_.get(),
                                 managed_network_configuration_handler_.get());
  geolocation_handler_->Init();
  network_3gpp_handler_->Init();
}

// static
void NetworkHandler::Initialize() {
  CHECK(!g_network_handler);
  g_network_handler =
      new NetworkHandler(base::WrapUnique(new NetworkStateHandler()));
  g_network_handler->Init();
}

// static
void NetworkHandler::InitializeFake() {
  CHECK(!g_network_handler);
  g_network_handler =
      new NetworkHandler(std::make_unique<FakeNetworkStateHandler>());
  g_network_handler->Init();
}

// static
void NetworkHandler::Shutdown() {
  CHECK(g_network_handler);
  delete g_network_handler;
  g_network_handler = NULL;
}

// static
NetworkHandler* NetworkHandler::Get() {
  CHECK(g_network_handler)
      << "NetworkHandler::Get() called before Initialize()";
  return g_network_handler;
}

// static
bool NetworkHandler::IsInitialized() {
  return g_network_handler;
}

void NetworkHandler::InitializePrefServices(
    PrefService* logged_in_profile_prefs,
    PrefService* device_prefs) {
  cellular_esim_profile_handler_->SetDevicePrefs(device_prefs);
  managed_cellular_pref_handler_->SetDevicePrefs(device_prefs);
  ui_proxy_config_service_.reset(new UIProxyConfigService(
      logged_in_profile_prefs, device_prefs, network_state_handler_.get(),
      network_profile_handler_.get()));
  managed_network_configuration_handler_->set_ui_proxy_config_service(
      ui_proxy_config_service_.get());
  managed_network_configuration_handler_->set_user_prefs(
      logged_in_profile_prefs);
  network_metadata_store_.reset(new NetworkMetadataStore(
      network_configuration_handler_.get(), network_connection_handler_.get(),
      network_state_handler_.get(),
      managed_network_configuration_handler_.get(), logged_in_profile_prefs,
      device_prefs, is_enterprise_managed_));
  cellular_network_metrics_logger_.reset(new CellularNetworkMetricsLogger(
      network_state_handler_.get(), network_metadata_store_.get(),
      connection_info_metrics_logger_.get()));
  hidden_network_handler_->SetNetworkMetadataStore(
      network_metadata_store_.get());
    text_message_provider_->SetNetworkMetadataStore(
        network_metadata_store_.get());

  if (ephemeral_network_policies_enablement_handler_) {
    ephemeral_network_policies_enablement_handler_->SetDevicePrefs(
        device_prefs);
  }
}

void NetworkHandler::ShutdownPrefServices() {
  if (ephemeral_network_policies_enablement_handler_) {
    ephemeral_network_policies_enablement_handler_->SetDevicePrefs(nullptr);
  }
  cellular_esim_profile_handler_->SetDevicePrefs(nullptr);
  managed_cellular_pref_handler_->SetDevicePrefs(nullptr);
  ui_proxy_config_service_.reset();
  managed_network_configuration_handler_->set_user_prefs(nullptr);
  hidden_network_handler_->SetNetworkMetadataStore(nullptr);
    text_message_provider_->SetNetworkMetadataStore(nullptr);

  network_metadata_store_.reset();
}

bool NetworkHandler::HasUiProxyConfigService() {
  return IsInitialized() && Get()->ui_proxy_config_service_.get();
}

UIProxyConfigService* NetworkHandler::GetUiProxyConfigService() {
  DCHECK(HasUiProxyConfigService());
  return Get()->ui_proxy_config_service_.get();
}

NetworkStateHandler* NetworkHandler::network_state_handler() {
  return network_state_handler_.get();
}

AutoConnectHandler* NetworkHandler::auto_connect_handler() {
  return auto_connect_handler_.get();
}

CellularConnectionHandler* NetworkHandler::cellular_connection_handler() {
  return cellular_connection_handler_.get();
}

CellularESimInstaller* NetworkHandler::cellular_esim_installer() {
  return cellular_esim_installer_.get();
}

CellularESimProfileHandler* NetworkHandler::cellular_esim_profile_handler() {
  return cellular_esim_profile_handler_.get();
}

CellularESimUninstallHandler*
NetworkHandler::cellular_esim_uninstall_handler() {
  return cellular_esim_uninstall_handler_.get();
}

CellularInhibitor* NetworkHandler::cellular_inhibitor() {
  return cellular_inhibitor_.get();
}

CellularPolicyHandler* NetworkHandler::cellular_policy_handler() {
  return cellular_policy_handler_.get();
}

TechnologyStateController* NetworkHandler::technology_state_controller() {
  return technology_state_controller_.get();
}

HiddenNetworkHandler* NetworkHandler::hidden_network_handler() {
  return hidden_network_handler_.get();
}

HotspotCapabilitiesProvider* NetworkHandler::hotspot_capabilities_provider() {
  return hotspot_capabilities_provider_.get();
}

HotspotController* NetworkHandler::hotspot_controller() {
  return hotspot_controller_.get();
}

HotspotConfigurationHandler* NetworkHandler::hotspot_configuration_handler() {
  return hotspot_configuration_handler_.get();
}

HotspotStateHandler* NetworkHandler::hotspot_state_handler() {
  return hotspot_state_handler_.get();
}

HotspotEnabledStateNotifier* NetworkHandler::hotspot_enabled_state_notifier() {
  return hotspot_enabled_state_notifier_.get();
}

ManagedCellularPrefHandler* NetworkHandler::managed_cellular_pref_handler() {
  return managed_cellular_pref_handler_.get();
}

NetworkDeviceHandler* NetworkHandler::network_device_handler() {
  return network_device_handler_.get();
}

NetworkProfileHandler* NetworkHandler::network_profile_handler() {
  return network_profile_handler_.get();
}

NetworkConfigurationHandler* NetworkHandler::network_configuration_handler() {
  return network_configuration_handler_.get();
}

ManagedNetworkConfigurationHandler*
NetworkHandler::managed_network_configuration_handler() {
  return managed_network_configuration_handler_.get();
}

NetworkActivationHandler* NetworkHandler::network_activation_handler() {
  return network_activation_handler_.get();
}

NetworkCertificateHandler* NetworkHandler::network_certificate_handler() {
  return network_certificate_handler_.get();
}

NetworkConnectionHandler* NetworkHandler::network_connection_handler() {
  return network_connection_handler_.get();
}

NetworkMetadataStore* NetworkHandler::network_metadata_store() {
  return network_metadata_store_.get();
}

NetworkSmsHandler* NetworkHandler::network_sms_handler() {
  return network_sms_handler_.get();
}

TextMessageProvider* NetworkHandler::text_message_provider() {
  return text_message_provider_.get();
}

GeolocationHandler* NetworkHandler::geolocation_handler() {
  return geolocation_handler_.get();
}

Network3gppHandler* NetworkHandler::network_3gpp_handler() {
  return network_3gpp_handler_.get();
}

ProhibitedTechnologiesHandler*
NetworkHandler::prohibited_technologies_handler() {
  return prohibited_technologies_handler_.get();
}

void NetworkHandler::SetIsEnterpriseManaged(bool is_enterprise_managed) {
  is_enterprise_managed_ = is_enterprise_managed;
  if (esim_policy_login_metrics_logger_) {
    // Call SetIsEnterpriseManaged on ESimPolicyLoginMetricsLogger, this only
    // gets called when the primary user logs in.
    esim_policy_login_metrics_logger_->SetIsEnterpriseManaged(
        is_enterprise_managed);
  }
  enterprise_managed_metadata_store_->set_is_enterprise_managed(
      is_enterprise_managed);
}

void NetworkHandler::OnEphemeralNetworkPoliciesEnabled() {
  DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());
  ephemeral_network_configuration_handler_ =
      EphemeralNetworkConfigurationHandler::TryCreate(
          managed_network_configuration_handler_.get(),
          was_enterprise_managed_at_startup_);
}

}  // namespace ash

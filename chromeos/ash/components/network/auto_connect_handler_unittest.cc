// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/auto_connect_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/client_cert_resolver.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

constexpr char kESimDisconnectByPolicyHistogram[] =
    "Network.Cellular.ESim.DisconnectByPolicy.Result";
constexpr char kPSimDisconnectByPolicyHistogram[] =
    "Network.Cellular.PSim.DisconnectByPolicy.Result";

class TestAutoConnectHandlerObserver : public AutoConnectHandler::Observer {
 public:
  TestAutoConnectHandlerObserver() = default;
  virtual ~TestAutoConnectHandlerObserver() = default;

  int num_auto_connect_events() { return num_auto_connect_events_; }

  int auto_connect_reasons() { return auto_connect_reasons_; }

  // AutoConnectHandler::Observer:
  void OnAutoConnectedInitiated(int auto_connect_reasons) override {
    ++num_auto_connect_events_;
    auto_connect_reasons_ = auto_connect_reasons;
  }

 private:
  int num_auto_connect_events_ = 0;
  int auto_connect_reasons_ = 0;
};

class TestCertResolveObserver : public ClientCertResolver::Observer {
 public:
  explicit TestCertResolveObserver(ClientCertResolver* cert_resolver)
      : changed_network_properties_(false), cert_resolver_(cert_resolver) {
    cert_resolver_->AddObserver(this);
  }

  void ResolveRequestCompleted(bool changed_network_properties) override {
    cert_resolver_->RemoveObserver(this);
    changed_network_properties_ = changed_network_properties;
  }

  bool DidNetworkPropertiesChange() { return changed_network_properties_; }

 private:
  bool changed_network_properties_;
  raw_ptr<ClientCertResolver> cert_resolver_;
};

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  explicit TestNetworkConnectionHandler(
      base::RepeatingCallback<void(const std::string&)> disconnect_handler)
      : disconnect_handler_(std::move(disconnect_handler)) {}
  ~TestNetworkConnectionHandler() override = default;

  // NetworkConnectionHandler:
  void DisconnectNetwork(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override {
    disconnect_handler_.Run(service_path);
    std::move(success_callback).Run();
  }

  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        network_handler::ErrorCallback error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override {}

  void Init(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      CellularConnectionHandler* cellular_connection_handler) override {}

  void OnAutoConnectedInitiated(int auto_connect_reasons) override {
    ++num_auto_connect_events_;
    auto_connect_reasons_ = auto_connect_reasons;
  }

  int num_auto_connect_events() { return num_auto_connect_events_; }

  int auto_connect_reasons() { return auto_connect_reasons_; }

 private:
  base::RepeatingCallback<void(const std::string&)> disconnect_handler_;
  int num_auto_connect_events_ = 0;
  int auto_connect_reasons_ = 0;
};

}  // namespace

class AutoConnectHandlerTest : public testing::Test {
 public:
  AutoConnectHandlerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  AutoConnectHandlerTest(const AutoConnectHandlerTest&) = delete;
  AutoConnectHandlerTest& operator=(const AutoConnectHandlerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));

    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    NetworkCertLoader::ForceAvailableForNetworkAuthForTesting();

    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    network_config_handler_ = NetworkConfigurationHandler::InitializeForTest(
        helper_.network_state_handler(), nullptr /* network_device_handler */);

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(
        /*cellular_policy_handler=*/nullptr,
        /*managed_cellular_pref_handler=*/nullptr,
        helper_.network_state_handler(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        nullptr /* prohibited_technologies_handler */,
        /*hotspot_controller=*/nullptr);

    test_network_connection_handler_ =
        std::make_unique<TestNetworkConnectionHandler>(base::BindRepeating(
            &AutoConnectHandlerTest::SetDisconnected, base::Unretained(this)));

    client_cert_resolver_ = std::make_unique<ClientCertResolver>();
    client_cert_resolver_->Init(helper_.network_state_handler(),
                                managed_config_handler_.get());

    auto_connect_handler_.reset(new AutoConnectHandler());
    auto_connect_handler_->Init(
        client_cert_resolver_.get(), test_network_connection_handler_.get(),
        helper_.network_state_handler(), managed_config_handler_.get());

    test_observer_ = std::make_unique<TestAutoConnectHandlerObserver>();
    auto_connect_handler_->AddObserver(test_observer_.get());

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    auto_connect_handler_->RemoveObserver(test_observer_.get());
    auto_connect_handler_.reset();
    test_network_connection_handler_.reset();
    client_cert_resolver_.reset();
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_config_handler_.reset();

    LoginState::Shutdown();

    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
  }

 protected:
  void SetDisconnected(const std::string& service_path) {
    helper_.SetServiceProperty(service_path, shill::kStateProperty,
                               base::Value(shill::kStateIdle));
  }

  std::string GetServiceState(const std::string& service_path) {
    return helper_.GetServiceStringProperty(service_path,
                                            shill::kStateProperty);
  }

  void StartNetworkCertLoader() {
    NetworkCertLoader::Get()->SetUserNSSDB(test_nsscertdb_.get());
    task_environment_.RunUntilIdle();
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    task_environment_.RunUntilIdle();
  }

  scoped_refptr<net::X509Certificate> ImportTestClientCert() {
    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), "client_1_ca.pem",
            net::X509Certificate::FORMAT_AUTO);
    if (ca_cert_list.empty()) {
      LOG(ERROR) << "No CA cert loaded.";
      return nullptr;
    }
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    if (!failures.empty()) {
      LOG(ERROR) << net::ErrorToString(failures[0].net_error);
      return nullptr;
    }

    // Import a client cert signed by that CA.
    scoped_refptr<net::X509Certificate> client_cert(
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            "client_1.pem", "client_1.pk8",
                                            test_nssdb_.slot()));
    return client_cert;
  }

  void SetupUserPolicy(const std::string& network_configs_json) {
    base::Value::List network_configs;
    if (!network_configs_json.empty()) {
      auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
          network_configs_json, base::JSON_ALLOW_TRAILING_COMMAS);
      ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
      ASSERT_TRUE(parsed_json->is_list());
      network_configs = std::move(parsed_json->GetList());
    }
    managed_config_handler_->SetPolicy(
        ::onc::ONC_SOURCE_USER_POLICY, helper_.UserHash(), network_configs,
        /*global_network_config=*/base::Value::Dict());
    task_environment_.RunUntilIdle();
  }

  void SetupDevicePolicy(const std::string& network_configs_json,
                         const base::Value::Dict& global_config) {
    base::Value::List network_configs;
    if (!network_configs_json.empty()) {
      auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
          network_configs_json, base::JSON_ALLOW_TRAILING_COMMAS);
      ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
      ASSERT_TRUE(parsed_json->is_list());
      network_configs = std::move(parsed_json->GetList());
    }
    managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                       std::string(),  // no username hash
                                       network_configs, global_config);
    task_environment_.RunUntilIdle();
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  void CheckServiceAutoConnectProperty(const std::string& service_path,
                                       bool expected_auto_connect) {
    const base::Value::Dict* properties =
        helper().service_test()->GetServiceProperties(service_path);
    std::optional<bool> auto_connect =
        properties->FindBool(shill::kAutoConnectProperty);
    EXPECT_TRUE(auto_connect);
    EXPECT_EQ(*auto_connect, expected_auto_connect);
  }

  NetworkStateTestHelper& helper() { return helper_; }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};
  std::unique_ptr<AutoConnectHandler> auto_connect_handler_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
  std::unique_ptr<TestAutoConnectHandlerObserver> test_observer_;
};

namespace {

const char* kConfigureCellular1UnmanagedConnected = R"(
  { "GUID": "cellular1", "Type": "cellular", "State": "online",
    "AutoConnect": true, "Profile": "/profile/default" })";

const char* kConfigureCellular2ManagedConnectable = R"(
  { "GUID": "cellular2", "Type": "cellular", "State": "idle",
    "AutoConnect": true, "Profile": "/profile/default" })";

const char* kConfigureCellular3UnmanagedConnected = R"(
  { "GUID": "cellular3", "Type": "cellular", "State": "online",
    "AutoConnect": true, "Profile": "/profile/default",
    "Cellular.EID": "1234567890"})";

const char* kConfigWifi0UnmanagedSharedConnected = R"(
  { "GUID": "wifi0", "Type": "wifi", "State": "online",
    "Security": "wpa", "Profile": "/profile/default" })";

const char* kConfigWifi1ManagedSharedConnectable = R"(
  { "GUID": "wifi1", "Type": "wifi", "State": "idle",
    "Connectable": true, "Security": "wpa", "Profile":
  "/profile/default" })";

// HexSSID 7769666931 is "wifi1".
const char* kPolicy = R"(
  [ { "GUID": "wifi1",
      "Name": "wifi1",
      "Type": "WiFi",
      "WiFi": {
        "Security": "WPA-PSK",
        "HexSSID": "7769666931",
        "Passphrase": "passphrase"
      }
  } ])";

// HexSSID 7769666931 is "wifi1".
const char* kPolicyCertPattern = R"(
  [ { "GUID": "wifi1",
      "Name": "wifi1",
      "Type": "WiFi",
      "WiFi": {
        "Security": "WPA-EAP",
        "HexSSID": "7769666931",
        "EAP": {
          "Outer": "EAP-TLS",
          "ClientCertType": "Pattern",
          "ClientCertPattern": {
            "Issuer": {
              "CommonName": "B CA"
            }
          }
        }
      }
  } ])";

const char* kCellularPolicy = R"(
    [
      { "GUID": "cellular2",
        "Name": "cellular2",
        "Type": "Cellular",
        "Cellular": {
          "SMDPAddress": "123"
        }
      }
    ])";

}  // namespace

TEST_F(AutoConnectHandlerTest, ReconnectOnCertLoading) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);

  // User login shouldn't trigger any change until the certificates and policy
  // are loaded.
  LoginToRegularUser();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);

  SetupUserPolicy(/*network_configs_json=*/std::string());
  SetupDevicePolicy(kPolicy, global_config);
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Certificate loading should trigger connecting to the 'best' network.
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi1_service_path));
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED,
            test_observer_->auto_connect_reasons());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED,
            test_network_connection_handler_->auto_connect_reasons());
}

TEST_F(AutoConnectHandlerTest, ReconnectOnCertPatternResolved) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  helper().manager_test()->SetBestServiceToConnect(wifi0_service_path);

  SetupDevicePolicy(
      /*network_configs_json=*/std::string(),
      /*global_config=*/base::Value::Dict());
  EXPECT_EQ(0, test_observer_->num_auto_connect_events());

  LoginToRegularUser();
  SetupUserPolicy(kPolicyCertPattern);
  StartNetworkCertLoader();
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
                AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED,
            test_observer_->auto_connect_reasons());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
                AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED,
            test_network_connection_handler_->auto_connect_reasons());

  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);
  TestCertResolveObserver observer(client_cert_resolver_.get());

  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer.DidNetworkPropertiesChange());

  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi1_service_path));
  EXPECT_EQ(2, test_observer_->num_auto_connect_events());
  EXPECT_EQ(2, test_network_connection_handler_->num_auto_connect_events());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
                AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED,
            test_observer_->auto_connect_reasons());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
                AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED,
            test_network_connection_handler_->auto_connect_reasons());
}

// Ensure that resolving of certificate patterns only triggers a reconnect if at
// least one pattern was resolved.
TEST_F(AutoConnectHandlerTest, NoReconnectIfNoCertResolved) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  helper().manager_test()->SetBestServiceToConnect(wifi0_service_path);

  SetupDevicePolicy(
      /*network_configs_json=*/std::string(),
      /*global_config=*/base::Value::Dict());
  LoginToRegularUser();
  StartNetworkCertLoader();
  SetupUserPolicy(kPolicy);

  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);
  TestCertResolveObserver observer(client_cert_resolver_.get());
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer.DidNetworkPropertiesChange());

  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED,
            test_observer_->auto_connect_reasons());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN |
                AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED,
            test_network_connection_handler_->auto_connect_reasons());
}

TEST_F(AutoConnectHandlerTest, DisconnectOnPolicyLoading) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  // Because no best service is set, the fake implementation of
  // ConnectToBestServices will be a no-op.
  SetupDevicePolicy(kPolicy, global_config);

  // Should not trigger any change until user policy is loaded
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_EQ(0, test_observer_->num_auto_connect_events());
}

TEST_F(AutoConnectHandlerTest, AutoConnectOnDevicePolicyApplied) {
  // Initial state: wifi0 is online, wifi1 is idle.
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  // When shill's ConnectToBestServices is called, wifi1 should be come online.
  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);

  // Starting NetworkCertLoader doesn't change anything yet if policy is not
  // applied.
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Apply device policy which should trigger ConnectToBestServices.
  SetupDevicePolicy(kPolicy,
                    /*global_config=*/base::Value::Dict());

  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi1_service_path));
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
}

TEST_F(AutoConnectHandlerTest, AutoConnectOnUserPolicyApplied) {
  // Initial state: wifi0 is online, wifi1 is idle.
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  // Device policy has already been applied.
  SetupDevicePolicy(std::string(),
                    /*global_config=*/base::Value::Dict());

  // When shill's ConnectToBestServices is called, wifi1 should be come online.
  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);

  // Starting NetworkCertLoader and log in as a user. Nothing happens yet
  // because user policy is not applied yet.
  StartNetworkCertLoader();
  LoginToRegularUser();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Applying user policy should trigger connection to "best" service.
  SetupUserPolicy(kPolicy);
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi1_service_path));
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
}

TEST_F(AutoConnectHandlerTest,
       DisconnectOnPolicyLoadingAllowOnlyPolicyWiFiToConnect) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect, true);

  // Applying the policy which restricts connections should disconnect from the
  // shared, unmanaged network.
  // Because no best service is set, the fake implementation of
  // ConnectToBestServices will be a no-op.
  SetupDevicePolicy(kPolicy, global_config);

  // Should not trigger any change until user policy is loaded
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_EQ(0, test_observer_->num_auto_connect_events());
}

// After login a reconnect is triggered even if there is no managed network.
TEST_F(AutoConnectHandlerTest, ReconnectAfterLogin) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Applying an empty device policy will not trigger anything yet, until also
  // the user policy is applied.
  SetupDevicePolicy(std::string(),
                    /*global_config=*/base::Value::Dict());
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // Applying also an empty user policy should trigger connecting to the 'best'
  // network.
  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi1_service_path));
  EXPECT_EQ(1, test_observer_->num_auto_connect_events());
  EXPECT_EQ(1, test_network_connection_handler_->num_auto_connect_events());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN,
            test_observer_->auto_connect_reasons());
  EXPECT_EQ(AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN,
            test_network_connection_handler_->auto_connect_reasons());
}

TEST_F(AutoConnectHandlerTest, ManualConnectAbortsReconnectAfterLogin) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());
  helper().manager_test()->SetBestServiceToConnect(wifi1_service_path);

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartNetworkCertLoader();
  SetupDevicePolicy(std::string(),
                    /*global_config=*/base::Value::Dict());

  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));

  // A manual connect request should prevent a reconnect after login.
  auto_connect_handler_->ConnectToNetworkRequested(
      std::string() /* service_path */);

  // Applying the user policy after login would usually trigger connecting to
  // the 'best' network. But the manual connect prevents this.
  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_EQ(0, test_observer_->num_auto_connect_events());
}

TEST_F(AutoConnectHandlerTest,
       DisableCellularAutoConnectOnAllowOnlyPolicyNetworksAutoconnect) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histogram_tester;
  std::string cellular1_service_path =
      ConfigureService(kConfigureCellular1UnmanagedConnected);
  ASSERT_FALSE(cellular1_service_path.empty());
  std::string cellular2_service_path =
      ConfigureService(kConfigureCellular2ManagedConnectable);
  ASSERT_FALSE(cellular2_service_path.empty());
  std::string cellular3_service_path =
      ConfigureService(kConfigureCellular3UnmanagedConnected);
  ASSERT_FALSE(cellular3_service_path.empty());

  EXPECT_EQ(shill::kStateOnline, GetServiceState(cellular1_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(cellular2_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(cellular3_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular2_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular3_service_path));
  CheckServiceAutoConnectProperty(cellular1_service_path,
                                  /*expected_auto_connect=*/true);
  CheckServiceAutoConnectProperty(cellular3_service_path,
                                  /*expected_auto_connect=*/true);
  histogram_tester.ExpectTotalCount(kESimDisconnectByPolicyHistogram, 0);
  histogram_tester.ExpectTotalCount(kPSimDisconnectByPolicyHistogram, 0);

  // Apply 'AllowOnlyPolicyNetworksToAutoconnect' policy as a device
  // policy and provide a network configuration for cellular2 to make it
  // managed.
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);
  SetupDevicePolicy(kCellularPolicy, global_config);
  // cellular1 and cellular3's service state should be set to idle and
  // autoconnect property should be set false.
  EXPECT_EQ(shill::kStateIdle, GetServiceState(cellular1_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(cellular2_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(cellular3_service_path));
  CheckServiceAutoConnectProperty(cellular1_service_path,
                                  /*expected_auto_connect=*/false);
  CheckServiceAutoConnectProperty(cellular3_service_path,
                                  /*expected_auto_connect=*/false);

  histogram_tester.ExpectTotalCount(kESimDisconnectByPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(kESimDisconnectByPolicyHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kPSimDisconnectByPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(kPSimDisconnectByPolicyHistogram, true, 1);
}

TEST_F(AutoConnectHandlerTest,
       DisconnectCellularOnPolicyLoadingAllowOnlyPolicyCellularNetworks) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histogram_tester;
  std::string cellular1_service_path =
      ConfigureService(kConfigureCellular1UnmanagedConnected);
  ASSERT_FALSE(cellular1_service_path.empty());
  std::string cellular2_service_path =
      ConfigureService(kConfigureCellular2ManagedConnectable);
  ASSERT_FALSE(cellular2_service_path.empty());
  std::string cellular3_service_path =
      ConfigureService(kConfigureCellular3UnmanagedConnected);
  ASSERT_FALSE(cellular3_service_path.empty());

  EXPECT_EQ(shill::kStateOnline, GetServiceState(cellular1_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(cellular2_service_path));
  EXPECT_EQ(shill::kStateOnline, GetServiceState(cellular3_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular3_service_path));
  histogram_tester.ExpectTotalCount(kPSimDisconnectByPolicyHistogram, 0);
  histogram_tester.ExpectTotalCount(kESimDisconnectByPolicyHistogram, 0);

  // Apply 'AllowOnlyPolicyCellularNetworks' policy as a device policy and
  // provide a network configuration for cellular2 to make it managed.
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks, true);
  SetupDevicePolicy(kCellularPolicy, global_config);

  // Cellular1 and Cellular3's service configuration should be not removed
  // because they are cellular networks and auto connect property should set
  // to false since they are unmanaged networks.
  EXPECT_TRUE(helper().profile_test()->HasService(cellular1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(cellular3_service_path));
  CheckServiceAutoConnectProperty(cellular1_service_path,
                                  /*expected_auto_connect=*/false);
  CheckServiceAutoConnectProperty(cellular3_service_path,
                                  /*expected_auto_connect=*/false);

  histogram_tester.ExpectTotalCount(kESimDisconnectByPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(kESimDisconnectByPolicyHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kPSimDisconnectByPolicyHistogram, 1);
  histogram_tester.ExpectBucketCount(kPSimDisconnectByPolicyHistogram, true, 1);
}

TEST_F(AutoConnectHandlerTest, DisconnectFromBlockedNetwork) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());

  LoginToRegularUser();
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(wifi0_service_path));

  // Apply a device policy, which blocks wifi0. No disconnects should occur
  // since we wait for both device & user policy before possibly disconnecting.
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kBlockedHexSSIDs,
      base::Value::List().Append("7769666930")  // hex(wifi0) = 7769666930
  );
  SetupDevicePolicy(std::string(), global_config);
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(wifi0_service_path));

  // Apply an empty user policy (no allow list for wifi0). Connection to wifi0
  // should be disconnected due to being blocked.
  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_FALSE(helper().profile_test()->HasService(wifi0_service_path));

  EXPECT_EQ(0, test_observer_->num_auto_connect_events());
}

TEST_F(AutoConnectHandlerTest, AllowOnlyPolicyWiFiToConnectIfAvailable) {
  std::string wifi0_service_path =
      ConfigureService(kConfigWifi0UnmanagedSharedConnected);
  ASSERT_FALSE(wifi0_service_path.empty());
  std::string wifi1_service_path =
      ConfigureService(kConfigWifi1ManagedSharedConnectable);
  ASSERT_FALSE(wifi1_service_path.empty());

  LoginToRegularUser();
  StartNetworkCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(wifi0_service_path));

  // Apply 'AllowOnlyPolicyWiFiToConnectIfAvailable' policy as a device
  // policy and provide a network configuration for wifi1 to make it managed.
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnectIfAvailable,
      true);
  SetupDevicePolicy(kPolicy, global_config);
  EXPECT_EQ(shill::kStateOnline, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(wifi0_service_path));

  // Apply an empty user policy (no allow list for wifi0). Connection to wifi0
  // should be disconnected due to being unmanaged and managed network wifi1
  // being available. wifi0 configuration should not be removed.
  SetupUserPolicy(/*network_configs_json=*/std::string());
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi0_service_path));
  EXPECT_EQ(shill::kStateIdle, GetServiceState(wifi1_service_path));
  EXPECT_TRUE(helper().profile_test()->HasService(wifi0_service_path));

  EXPECT_EQ(0, test_observer_->num_auto_connect_events());
}

}  // namespace ash

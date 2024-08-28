// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connection_handler_impl.h"

#include <map>
#include <memory>
#include <set>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/auto_connect_handler.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/client_cert_resolver.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kSuccessResult[] = "success";

const char kTetherGuid[] = "tether-guid";

const char kTestCellularGuid[] = "cellular_guid";
const char kTestCellularDevicePath[] = "cellular_path";
const char kTestCellularDeviceName[] = "cellular_name";
const char kTestCellularServicePath[] = "cellular_service_path";

const char kTestCellularName[] = "cellular_name";
const char kTestIccid[] = "1234567890123456789";
const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/1";
const char kTestEid[] = "123456789012345678901234567890123";

const char kTestCellularServicePath2[] = "cellular_service_path_2";
const char kTestIccid2[] = "9876543210987654321";
constexpr base::TimeDelta kCellularAutoConnectTimeout = base::Seconds(120);

class TestNetworkConnectionObserver : public NetworkConnectionObserver {
 public:
  TestNetworkConnectionObserver() = default;

  TestNetworkConnectionObserver(const TestNetworkConnectionObserver&) = delete;
  TestNetworkConnectionObserver& operator=(
      const TestNetworkConnectionObserver&) = delete;

  ~TestNetworkConnectionObserver() override = default;

  // NetworkConnectionObserver
  void ConnectToNetworkRequested(const std::string& service_path) override {
    requests_.insert(service_path);
  }

  void ConnectSucceeded(const std::string& service_path) override {
    results_[service_path] = kSuccessResult;
  }

  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override {
    results_[service_path] = error_name;
  }

  void DisconnectRequested(const std::string& service_path) override {
    requests_.insert(service_path);
    disconnect_requests_.insert(service_path);
  }

  bool GetRequested(const std::string& service_path) {
    return requests_.count(service_path) != 0;
  }

  std::string GetResult(const std::string& service_path) {
    auto iter = results_.find(service_path);
    if (iter == results_.end())
      return "";
    return iter->second;
  }

  const std::set<std::string>& disconnect_requests() {
    return disconnect_requests_;
  }

 private:
  std::set<std::string> disconnect_requests_;
  std::set<std::string> requests_;
  std::map<std::string, std::string> results_;
};

class FakeTetherDelegate : public NetworkConnectionHandler::TetherDelegate {
 public:
  FakeTetherDelegate()
      : last_delegate_function_type_(DelegateFunctionType::NONE) {}
  ~FakeTetherDelegate() override = default;

  enum class DelegateFunctionType { NONE, CONNECT, DISCONNECT };

  DelegateFunctionType last_delegate_function_type() {
    return last_delegate_function_type_;
  }

  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        StringErrorCallback error_callback) override {
    last_delegate_function_type_ = DelegateFunctionType::CONNECT;
    last_service_path_ = service_path;
    last_success_callback_ = std::move(success_callback);
    last_error_callback_ = std::move(error_callback);
  }

  void DisconnectFromNetwork(const std::string& service_path,
                             base::OnceClosure success_callback,
                             StringErrorCallback error_callback) override {
    last_delegate_function_type_ = DelegateFunctionType::DISCONNECT;
    last_service_path_ = service_path;
    last_success_callback_ = std::move(success_callback);
    last_error_callback_ = std::move(error_callback);
  }

  std::string& last_service_path() { return last_service_path_; }

  base::OnceClosure& last_success_callback() { return last_success_callback_; }

  StringErrorCallback& last_error_callback() { return last_error_callback_; }

 private:
  std::string last_service_path_;
  base::OnceClosure last_success_callback_;
  StringErrorCallback last_error_callback_;
  DelegateFunctionType last_delegate_function_type_;
};

}  // namespace

class NetworkConnectionHandlerImplTest : public testing::Test {
 public:
  NetworkConnectionHandlerImplTest() = default;

  NetworkConnectionHandlerImplTest(const NetworkConnectionHandlerImplTest&) =
      delete;
  NetworkConnectionHandlerImplTest& operator=(
      const NetworkConnectionHandlerImplTest&) = delete;

  ~NetworkConnectionHandlerImplTest() override = default;

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
  }

  void Init(bool use_cellular_connection_handler = true) {
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
        nullptr /* prohibited_tecnologies_handler */,
        /*hotspot_controller=*/nullptr);

    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(helper_.network_state_handler(),
                              helper_.network_device_handler());

    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(helper_.network_state_handler(),
                                         cellular_inhibitor_.get());

    stub_cellular_networks_provider_ =
        std::make_unique<StubCellularNetworksProvider>();
    stub_cellular_networks_provider_->Init(
        helper_.network_state_handler(), cellular_esim_profile_handler_.get(),
        /*managed_cellular_pref_handler=*/nullptr);

    cellular_connection_handler_.reset(new CellularConnectionHandler());
    cellular_connection_handler_->Init(helper_.network_state_handler(),
                                       cellular_inhibitor_.get(),
                                       cellular_esim_profile_handler_.get());

    network_connection_handler_ =
        std::make_unique<NetworkConnectionHandlerImpl>();
    network_connection_handler_->Init(
        helper_.network_state_handler(), network_config_handler_.get(),
        managed_config_handler_.get(),
        use_cellular_connection_handler ? cellular_connection_handler_.get()
                                        : nullptr);
    network_connection_observer_.reset(new TestNetworkConnectionObserver);
    network_connection_handler_->AddObserver(
        network_connection_observer_.get());

    client_cert_resolver_ = std::make_unique<ClientCertResolver>();
    client_cert_resolver_->Init(helper_.network_state_handler(),
                                managed_config_handler_.get());

    auto_connect_handler_ = std::make_unique<AutoConnectHandler>();
    auto_connect_handler_->Init(
        client_cert_resolver_.get(), network_connection_handler_.get(),
        helper_.network_state_handler(), managed_config_handler_.get());

    task_environment_.RunUntilIdle();

    fake_tether_delegate_ = std::make_unique<FakeTetherDelegate>();
  }

  void TearDown() override {
    helper_.hermes_euicc_test()->SetInteractiveDelay(base::Seconds(0));
    helper_.manager_test()->SetInteractiveDelay(base::Seconds(0));
    auto_connect_handler_.reset();
    client_cert_resolver_.reset();
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_connection_handler_->RemoveObserver(
        network_connection_observer_.get());
    network_connection_observer_.reset();
    network_connection_handler_.reset();
    network_config_handler_.reset();

    LoginState::Shutdown();

    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
  }

 protected:
  std::string ServicePathFromGuid(const std::string& guid) {
    std::string service_path =
        helper_.service_test()->FindServiceMatchingGUID(guid);
    EXPECT_FALSE(service_path.empty());
    return service_path;
  }

  void Connect(const std::string& service_path) {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);
    network_connection_handler_->ConnectToNetwork(
        service_path,
        base::BindOnce(&NetworkConnectionHandlerImplTest::SuccessCallback,
                       base::Unretained(this)),
        base::BindOnce(&NetworkConnectionHandlerImplTest::ErrorCallback,
                       base::Unretained(this)),
        true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
    task_environment_.RunUntilIdle();
  }

  void Disconnect(const std::string& service_path) {
    network_connection_handler_->DisconnectNetwork(
        service_path,
        base::BindOnce(&NetworkConnectionHandlerImplTest::SuccessCallback,
                       base::Unretained(this)),
        base::BindOnce(&NetworkConnectionHandlerImplTest::ErrorCallback,
                       base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  void SuccessCallback() { result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) { result_ = error_name; }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  void StartNetworkCertLoader() {
    NetworkCertLoader::Get()->SetUserNSSDB(test_nsscertdb_.get());
    task_environment_.RunUntilIdle();
  }

  void LoginToUser(LoginState::LoggedInUserType user_type) {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        user_type);
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

  // For a user-scoped network policy (previously set through `SetupUserPolicy`)
  // that configures a client certificate pattern, sets the resolved client
  // certificate. Outside of unit tests, `ClientCertResolver` would do this.
  void SetResolvedClientCertForUserPolicyNetwork(
      const std::string& network_policy_guid,
      client_cert::ResolvedCert resolved_cert) {
    managed_config_handler_->SetResolvedClientCertificate(
        helper_.UserHash(), network_policy_guid, std::move(resolved_cert));
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  std::string ConfigureVpnServiceWithProviderType(
      const std::string& vpn_provider_type) {
    const std::string kVpnGuid = "vpn_guid";
    const std::string kShillJsonStringTemplate =
        R"({"GUID": "$1", "Type": "vpn", "State": "idle",
            "Provider": {"Type": "$2", "Host": "host"}})";

    const std::string shill_json_string = base::ReplaceStringPlaceholders(
        kShillJsonStringTemplate, {kVpnGuid, vpn_provider_type},
        /*offsets=*/nullptr);
    return ConfigureService(shill_json_string);
  }

  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key) {
    return helper_.GetServiceStringProperty(service_path, key);
  }

  void QueueEuiccErrorStatus() {
    helper_.hermes_euicc_test()->QueueHermesErrorStatus(
        HermesResponseStatus::kErrorUnknown);
  }

  void SetCellularServiceConnectable(
      const std::string& service_path = kTestCellularServicePath) {
    helper_.service_test()->SetServiceProperty(
        service_path, shill::kConnectableProperty, base::Value(true));
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularServiceState(const std::string& state) {
    helper_.service_test()->SetServiceProperty(
        kTestCellularServicePath, shill::kStateProperty, base::Value(state));
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularServiceOutOfCredits() {
    helper_.service_test()->SetServiceProperty(kTestCellularServicePath,
                                               shill::kOutOfCreditsProperty,
                                               base::Value(true));
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularSimLocked() {
    // Simulate a locked SIM.
    auto sim_lock_status = base::Value::Dict().Set(shill::kSIMLockTypeProperty,
                                                   shill::kSIMLockPin);
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kSIMLockStatusProperty,
        base::Value(std::move(sim_lock_status)), /*notify_changed=*/true);

    // Set the cellular service to be the active profile.
    auto sim_slot_infos = base::Value::List().Append(
        base::Value::Dict()
            .Set(shill::kSIMSlotInfoICCID, kTestIccid)
            .Set(shill::kSIMSlotInfoPrimary, true));
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)), /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void SetCellularSimCarrierLocked() {
    // Simulate a locked SIM.
    base::Value::Dict sim_lock_status;
    sim_lock_status.Set(shill::kSIMLockTypeProperty, shill::kSIMLockNetworkPin);
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kSIMLockStatusProperty,
        base::Value(std::move(sim_lock_status)), /*notify_changed=*/true);

    // Set the cellular service to be the active profile.
    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoICCID, kTestIccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.Append(std::move(slot_info_item));
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)), /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void AddNonConnectablePSimService() {
    AddCellularDevice();
    AddCellularService(/*has_eid=*/false);
  }

  void AddNonConnectableESimService() {
    AddCellularDevice();
    AddCellularService(/*has_eid=*/true);
  }

  void AddCellularServiceWithESimProfile(bool is_stub = false) {
    AddCellularDevice();

    // Add EUICC which will hold the profile.
    helper_.hermes_manager_test()->AddEuicc(dbus::ObjectPath(kTestEuiccPath),
                                            kTestEid, /*is_active=*/true,
                                            /*physical_slot=*/0);

    HermesEuiccClient::TestInterface::AddCarrierProfileBehavior behavior =
        is_stub ? HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
                      kAddProfileWithoutService
                : HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
                      kAddProfileWithService;

    helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(kTestCellularServicePath),
        dbus::ObjectPath(kTestEuiccPath), kTestIccid, kTestCellularName,
        "nickname", "service_provider", "activation_code",
        kTestCellularServicePath, hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational, behavior);
    base::RunLoop().RunUntilIdle();
  }

  void AddCellularService(
      bool has_eid,
      const std::string& service_path = kTestCellularServicePath,
      const std::string& iccid = kTestIccid) {
    // Add idle, non-connectable network.
    helper_.service_test()->AddService(service_path, kTestCellularGuid,
                                       kTestCellularName, shill::kTypeCellular,
                                       shill::kStateIdle, /*visible=*/true);

    if (has_eid) {
      helper_.service_test()->SetServiceProperty(
          service_path, shill::kEidProperty, base::Value(kTestEid));
    }

    helper_.service_test()->SetServiceProperty(
        service_path, shill::kIccidProperty, base::Value(iccid));
    base::RunLoop().RunUntilIdle();
  }

  // Used when testing a code that accesses NetworkHandler::Get() directly (e.g.
  // when checking if VPN is disabled by policy when attempting to connect to a
  // VPN network). NetworkStateTestHelper can not be used here. That's because
  // NetworkStateTestHelper initializes a NetworkStateHandler for testing, but
  // NetworkHandler::Initialize() constructs its own NetworkStateHandler
  // instance and NetworkHandler::Get() uses it.
  // Note: Tests using this method must call NetworkHandler::Shutdown() before
  // returning.
  void ProhibitVpnForNetworkHandler() {
    NetworkHandler::Initialize();
    NetworkHandler::Get()
        ->prohibited_technologies_handler()
        ->AddGloballyProhibitedTechnology(shill::kTypeVPN);
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

  void SetShillConnectError(const std::string& error_name) {
    helper_.service_test()->SetErrorForNextConnectionAttempt(error_name);
  }

  void TriggerPolicyAutoConnectOverrideRequest(
      const std::string& service_path_to_override) {
    auto_connect_handler_->NotifyAutoConnectInitiatedForTest(
        AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
    // Simulate the policy autoconnection override the manual connection
    // request.
    helper_.service_test()->SetServiceProperty(service_path_to_override,
                                               shill::kStateProperty,
                                               base::Value(shill::kStateIdle));
    base::RunLoop().RunUntilIdle();
  }

  NetworkStateHandler* network_state_handler() {
    return helper_.network_state_handler();
  }
  TestNetworkConnectionObserver* network_connection_observer() {
    return network_connection_observer_.get();
  }
  NetworkConnectionHandler* network_connection_handler() {
    return network_connection_handler_.get();
  }
  FakeTetherDelegate* fake_tether_delegate() {
    return fake_tether_delegate_.get();
  }
  std::string UserProfilePath() {
    return helper_.ProfilePathUser();
  }

 private:
  void AddCellularDevice() {
    helper_.device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular, kTestCellularDeviceName);
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<TestNetworkConnectionObserver> network_connection_observer_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<StubCellularNetworksProvider>
      stub_cellular_networks_provider_;
  std::unique_ptr<AutoConnectHandler> auto_connect_handler_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
  std::string result_;
  std::unique_ptr<FakeTetherDelegate> fake_tether_delegate_;
};

namespace {

const char* kNoNetwork = "no-network";

const char* kConfigWifi0Connectable =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
const char* kConfigWifi1Connected =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"online\" }";
const char* kConfigWifi2Connecting =
    "{ \"GUID\": \"wifi2\", \"Type\": \"wifi\", \"State\": \"association\" }";
const char* kConfigWifi3RequiresPassphrase =
    "{ \"GUID\": \"wifi3\", \"Type\": \"wifi\", "
    "  \"PassphraseRequired\": true }";

const char* kPolicyWifi0 =
    "[{ \"GUID\": \"wifi0\",  \"IPAddressConfigType\": \"DHCP\", "
    "   \"Type\": \"WiFi\", \"Name\": \"My WiFi Network\","
    "   \"WiFi\": { \"SSID\": \"wifi0\"}}]";

}  // namespace

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectSuccess) {
  Init();

  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(
      shill::kStateOnline,
      GetServiceStringProperty(wifi0_service_path, shill::kStateProperty));
  // Observer expectations
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi0_service_path));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(wifi0_service_path));
  EXPECT_EQ("wifi0",
            GetServiceStringProperty(wifi0_service_path, shill::kGuidProperty));
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectSuccess_GuestUser) {
  Init();

  ProhibitVpnForNetworkHandler();

  LoginToUser(LoginState::LOGGED_IN_USER_GUEST);
  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  // Observer expectations
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi0_service_path));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(wifi0_service_path));
  EXPECT_EQ(
      UserProfilePath(),
      GetServiceStringProperty(wifi0_service_path, shill::kProfileProperty));

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectBlockedByManagedOnly) {
  Init();

  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect, true);
  SetupDevicePolicy("[]", global_config);
  SetupUserPolicy("[]");
  LoginToUser(LoginState::LOGGED_IN_USER_REGULAR);
  Connect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  SetupDevicePolicy(kPolicyWifi0, global_config);
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectBlockedBySSID) {
  Init();

  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());

  // Set a device policy which blocks wifi0.
  auto blocked =
      base::Value::List().Append("7769666930");  // hex(wifi0) = 7769666930
  auto global_config = base::Value::Dict().Set(
      ::onc::global_network_config::kBlockedHexSSIDs, std::move(blocked));
  SetupDevicePolicy("[]", global_config);
  SetupUserPolicy("[]");

  LoginToUser(LoginState::LOGGED_IN_USER_REGULAR);

  Connect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  // Set a user policy, which configures wifi0 (==allowed).
  SetupUserPolicy(kPolicyWifi0);
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

// Handles basic failure cases.
TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectFailure) {
  Init();

  Connect(kNoNetwork);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kNoNetwork));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            network_connection_observer()->GetResult(kNoNetwork));

  std::string wifi1_service_path = ConfigureService(kConfigWifi1Connected);
  ASSERT_FALSE(wifi1_service_path.empty());
  Connect(wifi1_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi1_service_path));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected,
            network_connection_observer()->GetResult(wifi1_service_path));

  std::string wifi2_service_path = ConfigureService(kConfigWifi2Connecting);
  ASSERT_FALSE(wifi2_service_path.empty());
  Connect(wifi2_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnecting, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi2_service_path));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnecting,
            network_connection_observer()->GetResult(wifi2_service_path));

  std::string wifi3_service_path =
      ConfigureService(kConfigWifi3RequiresPassphrase);
  Connect(wifi3_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorPassphraseRequired,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi3_service_path));
  EXPECT_EQ(NetworkConnectionHandler::kErrorPassphraseRequired,
            network_connection_observer()->GetResult(wifi3_service_path));
}

TEST_F(NetworkConnectionHandlerImplTest,
       IgnoreConnectInProgressError_Succeeds) {
  Init();

  AddCellularServiceWithESimProfile();
  // Verify the result is not returned and observers are not called if shill
  // returns InProgress error.
  SetShillConnectError(shill::kErrorResultInProgress);
  Connect(kTestCellularServicePath);
  EXPECT_TRUE(GetResultAndReset().empty());
  EXPECT_TRUE(network_connection_observer()
                  ->GetResult(kTestCellularServicePath)
                  .empty());

  // Verify that connect request returns when service state changes to
  // connected.
  SetCellularServiceState(shill::kStateOnline);
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTestCellularServicePath));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, IgnoreConnectInProgressError_Fails) {
  Init();

  AddNonConnectablePSimService();
  SetCellularServiceConnectable();
  SetShillConnectError(shill::kErrorResultInProgress);
  Connect(kTestCellularServicePath);
  EXPECT_TRUE(GetResultAndReset().empty());
  EXPECT_TRUE(network_connection_observer()
                  ->GetResult(kTestCellularServicePath)
                  .empty());

  // Set cellular service to connecting state.
  SetCellularServiceState(shill::kStateAssociation);

  // Verify the connect request fails with error when returned and observers are
  // not called if shill returns InProgress error.
  SetCellularServiceState(shill::kStateIdle);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
            network_connection_observer()->GetResult(kTestCellularServicePath));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
}

namespace {

constexpr char kPolicyWithCertPatternTemplate[] =
    "[ { \"GUID\": \"wifi4\","
    "    \"Name\": \"wifi4\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-EAP\","
    "      \"SSID\": \"wifi_ssid\","
    "      \"EAP\": {"
    "        \"Outer\": \"EAP-TLS\","
    "        \"ClientCertType\": \"Pattern\","
    "        \"ClientCertPattern\": {"
    "          \"Subject\": {"
    "            \"CommonName\" : \"%s\""
    "          }"
    "        }"
    "      }"
    "    }"
    "} ]";

constexpr char kPolicyWithCertPatternGuid[] = "wifi4";

}  // namespace

// Handle certificates.
TEST_F(NetworkConnectionHandlerImplTest, ConnectCertificateMissing) {
  Init();

  StartNetworkCertLoader();
  SetupUserPolicy(
      base::StringPrintf(kPolicyWithCertPatternTemplate, "unknown"));

  // Simulate that ClientCertResolver finished its run and marked that no
  // certificate matched the pattern.
  SetResolvedClientCertForUserPolicyNetwork(
      kPolicyWithCertPatternGuid, client_cert::ResolvedCert::NothingMatched());

  Connect(ServicePathFromGuid(kPolicyWithCertPatternGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectWithCertificateSuccess) {
  Init();

  StartNetworkCertLoader();
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupUserPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                     cert->subject().common_name.c_str()));

  Connect(ServicePathFromGuid(kPolicyWithCertPatternGuid));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectWithCertificateRequestedWhenCertsCanNotBeAvailable) {
  Init();

  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupUserPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                     cert->subject().common_name.c_str()));

  Connect(ServicePathFromGuid(kPolicyWithCertPatternGuid));

  // Connect request came when no client certificates can exist because
  // NetworkCertLoader doesn't have a NSSCertDatabase configured and also has
  // not notified that a NSSCertDatabase is being initialized.
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectWithCertificateRequestedBeforeCertsAreLoaded) {
  Init();

  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupUserPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                     cert->subject().common_name.c_str()));

  // Mark that a user slot NSSCertDatabase is being initialized so that
  // NetworkConnectionHandler attempts to wait for certificates to be loaded.
  NetworkCertLoader::Get()->MarkUserNSSDBWillBeInitialized();

  Connect(ServicePathFromGuid(kPolicyWithCertPatternGuid));

  // Connect request came before the cert loader loaded certificates, so the
  // connect request should have been throttled until the certificates are
  // loaded.
  EXPECT_EQ("", GetResultAndReset());

  StartNetworkCertLoader();

  // |StartNetworkCertLoader| should have triggered certificate loading.
  // When the certificates got loaded, the connection request should have
  // proceeded and eventually succeeded.
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectWithCertificateRequestedBeforeCertsAreLoaded_NeverLoaded) {
  const base::TimeDelta kMaxCertLoadTimeSeconds = base::Seconds(15);

  Init();

  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupUserPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                     cert->subject().common_name.c_str()));

  // Mark that a user slot NSSCertDatabase is being initialized so that
  // NetworkConnectionHandler attempts to wait for certificates to be loaded.
  NetworkCertLoader::Get()->MarkUserNSSDBWillBeInitialized();

  Connect(ServicePathFromGuid(kPolicyWithCertPatternGuid));

  // Connect request came before the cert loader loaded certificates, so the
  // connect request should have been throttled until the certificates are
  // loaded.
  EXPECT_EQ("", GetResultAndReset());

  AdvanceClock(kMaxCertLoadTimeSeconds);

  // The result should indicate a certificate load timeout.
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertLoadTimeout,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerDisconnectSuccess) {
  Init();

  std::string wifi1_service_path = ConfigureService(kConfigWifi1Connected);
  ASSERT_FALSE(wifi1_service_path.empty());
  Disconnect(wifi1_service_path);
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi1_service_path));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerDisconnectFailure) {
  Init();

  Connect(kNoNetwork);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());

  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  Disconnect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectToTetherNetwork_Success) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);
  network_connection_handler()->SetTetherDelegate(fake_tether_delegate());

  // For tether networks, guid == service_path.
  Connect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::CONNECT,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(kTetherGuid, fake_tether_delegate()->last_service_path());
  std::move(fake_tether_delegate()->last_success_callback()).Run();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectToTetherNetwork_Failure) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);
  network_connection_handler()->SetTetherDelegate(fake_tether_delegate());

  // For tether networks, guid == service_path.
  Connect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::CONNECT,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(kTetherGuid, fake_tether_delegate()->last_service_path());
  std::move(fake_tether_delegate()->last_error_callback())
      .Run(NetworkConnectionHandler::kErrorConnectFailed);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToIkev2VpnNetworkWhenProhibited_Failure) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderIKEv2);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToL2tpIpsecVpnNetworkWhenProhibited_Failure) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderL2tpIpsec);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToOpenVpnNetworkWhenProhibited_Failure) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderOpenVpn);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToWireGuardNetworkWhenProhibited_Failure) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderWireGuard);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToThirdPartyVpnNetworkWhenProhibited_Success) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderThirdPartyVpn);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToArcVpnNetworkWhenProhibited_Success) {
  Init();

  ProhibitVpnForNetworkHandler();

  const std::string vpn_service_path =
      ConfigureVpnServiceWithProviderType(shill::kProviderArcVpn);
  ASSERT_FALSE(vpn_service_path.empty());

  Connect(/*service_path=*/vpn_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  NetworkHandler::Shutdown();
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToTetherNetwork_NoTetherDelegate) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);

  // Do not set a tether delegate.

  // For tether networks, guid == service_path.
  Connect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::NONE,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest, DisconnectFromTetherNetwork_Success) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);
  network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);
  network_connection_handler()->SetTetherDelegate(fake_tether_delegate());

  // For tether networks, guid == service_path.
  Disconnect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::DISCONNECT,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(kTetherGuid, fake_tether_delegate()->last_service_path());
  std::move(fake_tether_delegate()->last_success_callback()).Run();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest, DisconnectFromTetherNetwork_Failure) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);
  network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);
  network_connection_handler()->SetTetherDelegate(fake_tether_delegate());

  // For tether networks, guid == service_path.
  Disconnect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::DISCONNECT,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(kTetherGuid, fake_tether_delegate()->last_service_path());
  std::move(fake_tether_delegate()->last_error_callback())
      .Run(NetworkConnectionHandler::kErrorConnectFailed);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest,
       DisconnectFromTetherNetwork_NoTetherDelegate) {
  Init();

  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TECHNOLOGY_ENABLED);
  network_state_handler()->AddTetherNetworkState(
      kTetherGuid, "TetherNetwork", "Carrier", 100 /* battery_percentage */,
      100 /* signal_strength */, true /* has_connected_to_host */);
  network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);

  // Do not set a tether delegate.

  // For tether networks, guid == service_path.
  Disconnect(kTetherGuid /* service_path */);

  EXPECT_EQ(FakeTetherDelegate::DelegateFunctionType::NONE,
            fake_tether_delegate()->last_delegate_function_type());
  EXPECT_EQ(NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorTetherAttemptWithNoDelegate,
            network_connection_observer()->GetResult(kTetherGuid));
}

// Regression test for b/186381398.
TEST_F(NetworkConnectionHandlerImplTest,
       PSimProfile_NoCellularConnectionHandler) {
  Init(/*use_cellular_connection_handler=*/false);
  AddNonConnectablePSimService();
  SetCellularServiceConnectable();
  Connect(kTestCellularServicePath);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, PSimProfile_NotConnectable) {
  Init();
  AddNonConnectablePSimService();

  Connect(kTestCellularServicePath);
  SetCellularServiceConnectable();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, PSimProfile_OutOfCredits) {
  Init();
  AddNonConnectablePSimService();

  SetCellularServiceOutOfCredits();
  Connect(kTestCellularServicePath);
  EXPECT_EQ(NetworkConnectionHandler::kErrorCellularOutOfCredits,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, SimLocked) {
  Init();
  AddNonConnectablePSimService();
  SetCellularSimLocked();
  SetCellularServiceConnectable();

  Connect(kTestCellularServicePath);
  EXPECT_EQ(NetworkConnectionHandler::kErrorSimPinPukLocked,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, SimCarrierLocked) {
  Init();
  AddNonConnectablePSimService();
  SetCellularSimCarrierLocked();
  SetCellularServiceConnectable();

  Connect(kTestCellularServicePath);
  EXPECT_EQ(NetworkConnectionHandler::kErrorSimCarrierLocked,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ESimProfile_AlreadyConnectable) {
  Init();
  AddCellularServiceWithESimProfile();

  // Set the service to be connectable before trying to connect. This does not
  // invoke the CellularConnectionHandler flow since the profile is already
  // enabled.
  SetCellularServiceConnectable();
  Connect(kTestCellularServicePath);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       ESimProfile_EnableProfileAndWaitForAutoconnect) {
  Init();
  AddCellularServiceWithESimProfile();

  // Do not set the service to be connectable before trying to connect. When a
  // connection is initiated, we attempt to enable the profile via Hermes.
  Connect(kTestCellularServicePath);
  SetCellularServiceConnectable();
  // Set cellular service to connected state.
  SetCellularServiceState(shill::kStateOnline);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       ESimProfile_EnableProfileAndAutoconnectTimeout) {
  Init();
  AddCellularServiceWithESimProfile();

  // Do not set the service to be connectable before trying to connect. When a
  // connection is initiated, we attempt to enable the profile via Hermes.
  Connect(kTestCellularServicePath);
  SetCellularServiceConnectable();
  EXPECT_TRUE(GetResultAndReset().empty());

  AdvanceClock(kCellularAutoConnectTimeout);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ESimProfile_StubToShillBacked) {
  Init();
  AddCellularServiceWithESimProfile(/*is_stub=*/true);

  // Connect to a stub path. Internally, this should wait until a connectable
  // Shill-backed service is created.
  Connect(cellular_utils::GenerateStubCellularServicePath(kTestIccid));

  // Now, create a non-stub service and make it connectable.
  AddNonConnectableESimService();
  SetCellularServiceConnectable();
  // Set cellular service to connected state.
  SetCellularServiceState(shill::kStateOnline);

  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // A connection was requested to the stub service path, not the actual one.
  EXPECT_TRUE(network_connection_observer()->GetRequested(
      cellular_utils::GenerateStubCellularServicePath(kTestIccid)));
  EXPECT_FALSE(
      network_connection_observer()->GetRequested(kTestCellularServicePath));

  // However, the connection success was part of the actual service path, not
  // the stub one.
  EXPECT_EQ(std::string(),
            network_connection_observer()->GetResult(
                cellular_utils::GenerateStubCellularServicePath(kTestIccid)));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTestCellularServicePath));
}

TEST_F(NetworkConnectionHandlerImplTest, ESimProfile_EnableProfile_Fails) {
  Init();
  AddCellularServiceWithESimProfile();

  // Queue an error which should cause enabling the profile to fail.
  QueueEuiccErrorStatus();

  // Do not set the service to be connectable before trying to connect. When a
  // connection is initiated, we attempt to enable the profile via Hermes.
  Connect(kTestCellularServicePath);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkConnectionHandler::kErrorESimProfileIssue,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, MultipleCellularConnect) {
  Init();
  AddCellularServiceWithESimProfile();
  AddCellularService(/*has_eid=*/false, kTestCellularServicePath2, kTestIccid2);

  // Delay hermes operation so that first connect will be waiting in
  // CellularConnectionHandler.
  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(10));
  Connect(kTestCellularServicePath);
  Connect(kTestCellularServicePath2);

  // Verify that second connect request fails with device busy.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkConnectionHandler::kErrorCellularDeviceBusy,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, CellularConnect) {
  Init();
  AddCellularServiceWithESimProfile();
  LoginToUser(LoginState::LOGGED_IN_USER_REGULAR);
  Connect(kTestCellularServicePath);
  SetCellularServiceConnectable();
  EXPECT_TRUE(GetResultAndReset().empty());
  AdvanceClock(kCellularAutoConnectTimeout);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty(kTestCellularServicePath,
                                     shill::kStateProperty));
  // Expect the service to be added to the shared profile even when logged in.
  EXPECT_EQ(ShillProfileClient::Get()->GetSharedProfilePath(),
            GetServiceStringProperty(kTestCellularServicePath,
                                     shill::kProfileProperty));
}

TEST_F(NetworkConnectionHandlerImplTest, CellularConnectTimeout) {
  const base::TimeDelta kCellularConnectTimeout = base::Seconds(150);
  Init();
  AddNonConnectablePSimService();
  SetCellularServiceConnectable(kTestCellularServicePath);

  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(200));
  Connect(kTestCellularServicePath);
  AdvanceClock(kCellularConnectTimeout);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectTimeout,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       CellularConnectTimeout_StubToShillBacked) {
  const base::TimeDelta kCellularConnectTimeout = base::Seconds(150);
  Init();
  AddCellularServiceWithESimProfile(/*is_stub=*/true);

  // Connect to a stub path. Internally, this should wait until a connectable
  // Shill-backed service is created.
  Connect(cellular_utils::GenerateStubCellularServicePath(kTestIccid));

  // Now, Create a shill backed service for the same network.
  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(200));
  AddNonConnectableESimService();
  SetCellularServiceConnectable();

  // Verify that connection timesout properly even when network path
  // transitioned.
  AdvanceClock(kCellularConnectTimeout);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectTimeout,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, PolicyConnectOverride) {
  const base::TimeDelta kConectDelay = base::Seconds(1);
  Init();
  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);

  ShillManagerClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kConectDelay);
  Connect(wifi0_service_path);
  TriggerPolicyAutoConnectOverrideRequest(wifi0_service_path);

  AdvanceClock(kConectDelay);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectCanceled,
            GetResultAndReset());
}

}  // namespace ash

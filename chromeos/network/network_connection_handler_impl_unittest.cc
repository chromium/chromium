// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler_impl.h"

#include <map>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/onc/onc_utils.h"
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

namespace chromeos {

namespace {

const char kSuccessResult[] = "success";

const char kTetherGuid[] = "tether-guid";

class TestNetworkConnectionObserver : public NetworkConnectionObserver {
 public:
  TestNetworkConnectionObserver() = default;
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

 private:
  std::set<std::string> requests_;
  std::map<std::string, std::string> results_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkConnectionObserver);
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

  void ConnectToNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override {
    last_delegate_function_type_ = DelegateFunctionType::CONNECT;
    last_service_path_ = service_path;
    last_success_callback_ = success_callback;
    last_error_callback_ = error_callback;
  }

  void DisconnectFromNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override {
    last_delegate_function_type_ = DelegateFunctionType::DISCONNECT;
    last_service_path_ = service_path;
    last_success_callback_ = success_callback;
    last_error_callback_ = error_callback;
  }

  std::string& last_service_path() { return last_service_path_; }

  base::Closure& last_success_callback() { return last_success_callback_; }

  network_handler::StringResultCallback& last_error_callback() {
    return last_error_callback_;
  }

 private:
  std::string last_service_path_;
  base::Closure last_success_callback_;
  network_handler::StringResultCallback last_error_callback_;
  DelegateFunctionType last_delegate_function_type_;
};

}  // namespace

class NetworkConnectionHandlerImplTest : public testing::Test {
 public:
  NetworkConnectionHandlerImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  ~NetworkConnectionHandlerImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));

    NetworkCertLoader::Initialize();
    NetworkCertLoader::ForceHardwareBackedForTesting();

    LoginState::Initialize();

    network_config_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            helper_.network_state_handler(),
            nullptr /* network_device_handler */));

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(
        helper_.network_state_handler(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        nullptr /* prohibited_tecnologies_handler */);

    network_connection_handler_.reset(new NetworkConnectionHandlerImpl());
    network_connection_handler_->Init(helper_.network_state_handler(),
                                      network_config_handler_.get(),
                                      managed_config_handler_.get());
    network_connection_observer_.reset(new TestNetworkConnectionObserver);
    network_connection_handler_->AddObserver(
        network_connection_observer_.get());

    task_environment_.RunUntilIdle();

    fake_tether_delegate_.reset(new FakeTetherDelegate());
  }

  void TearDown() override {
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_connection_handler_->RemoveObserver(
        network_connection_observer_.get());
    network_connection_observer_.reset();
    network_connection_handler_.reset();
    network_config_handler_.reset();

    LoginState::Shutdown();

    NetworkCertLoader::Shutdown();
  }

 protected:
  std::string ServicePathFromGuid(const std::string& guid) {
    std::string service_path =
        helper_.service_test()->FindServiceMatchingGUID(guid);
    EXPECT_FALSE(service_path.empty());
    return service_path;
  }

  void Connect(const std::string& service_path) {
    network_connection_handler_->ConnectToNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerImplTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerImplTest::ErrorCallback,
                   base::Unretained(this)),
        true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
    task_environment_.RunUntilIdle();
  }

  void Disconnect(const std::string& service_path) {
    network_connection_handler_->DisconnectNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerImplTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerImplTest::ErrorCallback,
                   base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  void SuccessCallback() { result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name,
                     std::unique_ptr<base::DictionaryValue> error_data) {
    result_ = error_name;
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
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

  void SetupPolicy(const std::string& network_configs_json,
                   const base::DictionaryValue& global_config,
                   bool user_policy) {
    std::string error;
    std::unique_ptr<base::Value> network_configs_value =
        base::JSONReader::ReadAndReturnErrorDeprecated(
            network_configs_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr,
            &error);
    ASSERT_TRUE(network_configs_value) << error;

    base::ListValue* network_configs = nullptr;
    ASSERT_TRUE(network_configs_value->GetAsList(&network_configs));

    if (user_policy) {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
                                         helper_.UserHash(), *network_configs,
                                         global_config);
    } else {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         std::string(),  // no username hash
                                         *network_configs, global_config);
    }
    task_environment_.RunUntilIdle();
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key) {
    return helper_.GetServiceStringProperty(service_path, key);
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

 private:
  base::test::TaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<TestNetworkConnectionObserver> network_connection_observer_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
  std::string result_;
  std::unique_ptr<FakeTetherDelegate> fake_tether_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerImplTest);
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
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectBlockedByManagedOnly) {
  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
      base::Value(true));
  SetupPolicy("[]", global_config, false /* load as device policy */);
  SetupPolicy("[]", base::DictionaryValue(), true /* load as user policy */);
  LoginToRegularUser();
  Connect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  SetupPolicy(kPolicyWifi0, global_config, false /* load as device policy */);
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectBlockedByBlacklist) {
  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());

  // Set a device policy which blocks wifi0.
  base::Value::ListStorage blacklist;
  blacklist.push_back(base::Value("7769666930"));  // hex(wifi0) = 7769666930
  base::DictionaryValue global_config;
  global_config.SetKey(::onc::global_network_config::kBlacklistedHexSSIDs,
                       base::Value(blacklist));
  SetupPolicy("[]", global_config, false /* load as device policy */);
  SetupPolicy("[]", base::DictionaryValue(), true /* load as user policy */);

  LoginToRegularUser();

  Connect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorBlockedByPolicy,
            GetResultAndReset());

  // Set a user policy, which configures wifi0 (==whitelisted).
  SetupPolicy(kPolicyWifi0, base::DictionaryValue(),
              true /* load as user policy */);
  Connect(wifi0_service_path);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

// Handles basic failure cases.
TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerConnectFailure) {
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

namespace {

const char* kPolicyWithCertPatternTemplate =
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

}  // namespace

// Handle certificates.
TEST_F(NetworkConnectionHandlerImplTest, ConnectCertificateMissing) {
  StartNetworkCertLoader();
  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate, "unknown"),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect(ServicePathFromGuid("wifi4"));
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectWithCertificateSuccess) {
  StartNetworkCertLoader();
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect(ServicePathFromGuid("wifi4"));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

// Disabled, see http://crbug.com/396729.
TEST_F(NetworkConnectionHandlerImplTest,
       DISABLED_ConnectWithCertificateRequestedBeforeCertsAreLoaded) {
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect(ServicePathFromGuid("wifi4"));

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
       NetworkConnectionHandlerDisconnectSuccess) {
  std::string wifi1_service_path = ConfigureService(kConfigWifi1Connected);
  ASSERT_FALSE(wifi1_service_path.empty());
  Disconnect(wifi1_service_path);
  EXPECT_TRUE(network_connection_observer()->GetRequested(wifi1_service_path));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest,
       NetworkConnectionHandlerDisconnectFailure) {
  Connect(kNoNetwork);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());

  std::string wifi0_service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_FALSE(wifi0_service_path.empty());
  Disconnect(wifi0_service_path);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectToTetherNetwork_Success) {
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
  fake_tether_delegate()->last_success_callback().Run();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest, ConnectToTetherNetwork_Failure) {
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
  fake_tether_delegate()->last_error_callback().Run(
      NetworkConnectionHandler::kErrorConnectFailed);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest,
       ConnectToTetherNetwork_NoTetherDelegate) {
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
  fake_tether_delegate()->last_success_callback().Run();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(kSuccessResult,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest, DisconnectFromTetherNetwork_Failure) {
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
  fake_tether_delegate()->last_error_callback().Run(
      NetworkConnectionHandler::kErrorConnectFailed);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer()->GetRequested(kTetherGuid));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnectFailed,
            network_connection_observer()->GetResult(kTetherGuid));
}

TEST_F(NetworkConnectionHandlerImplTest,
       DisconnectFromTetherNetwork_NoTetherDelegate) {
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

}  // namespace chromeos

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/network/client_cert_resolver.h"

#include <cert.h>
#include <pk11pub.h>

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

using ::testing::IsEmpty;
using ::testing::Not;

namespace {

constexpr char kWifiStub[] = "wifi_stub";
constexpr char kWifiSSID[] = "wifi_ssid";
constexpr char kUserProfilePath[] = "user_profile";
constexpr char kUserHash[] = "user_hash";

void OnImportCompleted(base::OnceClosure loop_quit_closure, bool success) {
  EXPECT_TRUE(success);
  std::move(loop_quit_closure).Run();
}

void OnListCertsDone(base::OnceClosure loop_quit_closure,
                     net::ScopedCERTCertificateList* out_cert_list,
                     net::ScopedCERTCertificateList cert_list) {
  out_cert_list->swap(cert_list);
  std::move(loop_quit_closure).Run();
}

// Returns a |OncParsedCertificates| which contains exactly one client
// certificate with the contents of |client_cert_pkcs12_file| and the GUID
// |guid|. Returns nullptr if the file could not be read.
std::unique_ptr<chromeos::onc::OncParsedCertificates>
OncParsedCertificatesForPkcs12File(
    const base::FilePath& client_cert_pkcs12_file,
    std::string_view guid) {
  std::string pkcs12_raw;
  if (!base::ReadFileToString(client_cert_pkcs12_file, &pkcs12_raw))
    return nullptr;

  std::string pkcs12_base64_encoded = base::Base64Encode(pkcs12_raw);

  auto onc_certificates =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("GUID", guid)
                                     .Set("Type", "Client")
                                     .Set("PKCS12", pkcs12_base64_encoded));
  return std::make_unique<chromeos::onc::OncParsedCertificates>(
      onc_certificates);
}

std::string GetStringFromDict(const base::Value::Dict& dict, const char* key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

}  // namespace

class ClientCertResolverTest : public testing::Test,
                               public ClientCertResolver::Observer {
 public:
  ClientCertResolverTest() = default;

  ClientCertResolverTest(const ClientCertResolverTest&) = delete;
  ClientCertResolverTest& operator=(const ClientCertResolverTest&) = delete;

  ~ClientCertResolverTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());
    ASSERT_TRUE(test_system_nssdb_.is_open());
    // Use the same slot as public and private slot for the user's
    // NSSCertDatabse for testing.
    test_nsscertdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
    // Create a NSSCertDatabase for the system slot. While NetworkCertLoader
    // does not care about the public slot in this database, NSSCertDatabase
    // requires a public slot. Pass the system slot there for testing.
    test_system_nsscertdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_system_nssdb_.slot())),
        crypto::ScopedPK11Slot() /* private_slot */);
    test_system_nsscertdb_->SetSystemSlot(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_system_nssdb_.slot())));

    shill_clients::InitializeFakes();
    service_test_ = ShillServiceClient::Get()->GetTestInterface();
    profile_test_ = ShillProfileClient::Get()->GetTestInterface();
    profile_test_->AddProfile(kUserProfilePath, kUserHash);
    task_environment_.RunUntilIdle();
    service_test_->ClearServices();
    task_environment_.RunUntilIdle();

    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    network_cert_loader_ = NetworkCertLoader::Get();
    NetworkCertLoader::ForceAvailableForNetworkAuthForTesting();
  }

  void TearDown() override {
    if (client_cert_resolver_)
      client_cert_resolver_->RemoveObserver(this);
    client_cert_resolver_.reset();
    test_clock_.reset();
    if (network_state_handler_)
      network_state_handler_->Shutdown();
    managed_config_handler_.reset();
    network_config_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    shill_clients::Shutdown();
  }

 protected:
  void StartNetworkCertLoader() {
    network_cert_loader_->SetUserNSSDB(test_nsscertdb_.get());
    network_cert_loader_->SetSystemNssDbForTesting(
        test_system_nsscertdb_.get());
    if (test_client_cert_.get()) {
      int slot_id = 0;
      const std::string pkcs11_id =
          NetworkCertLoader::GetPkcs11IdAndSlotForCert(test_client_cert_.get(),
                                                       &slot_id);
      test_cert_id_ = base::StringPrintf("%i:%s", slot_id, pkcs11_id.c_str());
    }
  }

  // Imports a client certificate. After a subsequent StartNetworkCertLoader()
  // invocation, the PKCS#11 ID of the imported certificate will be stored in
  // |test_cert_id_|.  If |import_issuer| is true, also imports the CA cert
  // (stored as PEM in test_ca_cert_pem_) that issued the client certificate.
  void SetupTestCerts(const std::string& prefix, bool import_issuer) {
    // Load a CA cert.
    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), prefix + "_ca.pem",
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(!ca_cert_list.empty());
    net::x509_util::GetPEMEncoded(ca_cert_list[0].get(), &test_ca_cert_pem_);
    ASSERT_TRUE(!test_ca_cert_pem_.empty());

    if (import_issuer) {
      net::NSSCertDatabase::ImportCertFailureList failures;
      EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
          ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
      ASSERT_TRUE(failures.empty())
          << net::ErrorToString(failures[0].net_error);
    }

    // Import a client cert signed by that CA.
    net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                        prefix + ".pem", prefix + ".pk8",
                                        test_nssdb_.slot(), &test_client_cert_);
    ASSERT_TRUE(test_client_cert_.get());
  }

  // Imports a client certificate with a subject CommonName encoded as
  // PrintableString, but containing invalid characters. It is imported into the
  // user slot. After a subsequent StartNetworkCertLoader() invocation, the
  // PKCS#11 ID of the imported certificate will be stored in |test_cert_id_|.
  void SetupTestCertWithBadPrintableString() {
    base::FilePath certs_dir = net::GetTestNetDataDirectory().AppendASCII(
        "parse_certificate_unittest");
    ASSERT_TRUE(net::ImportSensitiveKeyFromFile(
        certs_dir, "v3_certificate_template.pk8", test_nssdb_.slot()));

    std::string file_data;
    ASSERT_TRUE(base::ReadFileToString(
        certs_dir.AppendASCII(
            "subject_printable_string_containing_utf8_client_cert.pem"),
        &file_data));

    bssl::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
    ASSERT_TRUE(pem_tokenizer.GetNext());
    std::string cert_der(pem_tokenizer.data());
    ASSERT_FALSE(pem_tokenizer.GetNext());

    test_client_cert_ = net::x509_util::CreateCERTCertificateFromBytes(
        base::as_bytes(base::make_span(cert_der)));
    ASSERT_TRUE(test_client_cert_);

    ASSERT_TRUE(net::ImportClientCertToSlot(test_client_cert_.get(),
                                            test_nssdb_.slot()));
  }

  void SetupTestCertInSystemToken(const std::string& prefix) {
    net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), prefix + ".pem", prefix + ".pk8",
        test_system_nssdb_.slot(), &test_client_cert_);
    ASSERT_TRUE(test_client_cert_.get());
  }

  void SetupNetworkHandlers() {
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_profile_handler_.reset(new NetworkProfileHandler());
    network_config_handler_.reset(new NetworkConfigurationHandler());
    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    client_cert_resolver_ = std::make_unique<ClientCertResolver>();

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(base::Time::Now());
    client_cert_resolver_->SetClockForTesting(test_clock_.get());

    network_profile_handler_->Init();
    network_config_handler_->Init(network_state_handler_.get(),
                                  nullptr /* network_device_handler */);
    managed_config_handler_->Init(
        /*cellular_policy_handler=*/nullptr,
        /*managed_cellular_pref_handler=*/nullptr, network_state_handler_.get(),
        network_profile_handler_.get(), network_config_handler_.get(),
        nullptr /* network_device_handler */,
        nullptr /* prohibited_technologies_handler */,
        /*hotspot_controller=*/nullptr);
    // Run all notifications before starting the cert loader to reduce run time.
    task_environment_.RunUntilIdle();

    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_config_handler_.get());
    client_cert_resolver_->AddObserver(this);
  }

  void SetupWifi() {
    service_test_->SetServiceProperties(kWifiStub, kWifiStub, kWifiSSID,
                                        shill::kTypeWifi, shill::kStateOnline,
                                        true /* visible */);
    // Set an arbitrary cert id, so that we can check afterwards whether we
    // cleared the property or not.
    service_test_->SetServiceProperty(kWifiStub, shill::kEapCertIdProperty,
                                      base::Value("invalid id"));
    profile_test_->AddService(kUserProfilePath, kWifiStub);

    ShillManagerClient::Get()->GetTestInterface()->AddManagerService(kWifiStub,
                                                                     true);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // with a certain Issuer CN. It will match the test client cert imported by
  // SetupTestCerts.
  void SetupPolicyMatchingIssuerCN(::onc::ONCSource onc_source) {
    const char* test_policy = R"(
        [ { "GUID": "wifi_stub",
            "Name": "wifi_stub",
            "Type": "WiFi",
            "WiFi": {
              "Security": "WPA-EAP",
              "SSID": "wifi_ssid",
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
    ASSERT_NO_FATAL_FAILURE(SetManagedNetworkPolicy(onc_source, test_policy));
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // with a Subject Organization set to "Blar". It will match the test client
  // cert imported by SetupTestCertWithBadPrintableString.
  void SetupPolicyMatchingSubjectOrgForBadPrintableStringCert(
      ::onc::ONCSource onc_source) {
    const char* test_policy = R"(
        [ { "GUID": "wifi_stub",
            "Name": "wifi_stub",
            "Type": "WiFi",
            "WiFi": {
              "Security": "WPA-EAP",
              "SSID": "wifi_ssid",
              "EAP": {
                "Outer": "EAP-TLS",
                "ClientCertType": "Pattern",
                "ClientCertPattern": {
                  "Subject": {
                    "Organization": "Blar"
                  }
                }
              }
            }
        } ])";
    ASSERT_NO_FATAL_FAILURE(SetManagedNetworkPolicy(onc_source, test_policy));
  }

  void SetupCertificateConfigMatchingIssuerCN(
      ::onc::ONCSource onc_source,
      client_cert::ClientCertConfig* client_cert_config) {
    const char* test_onc_pattern = R"(
        {
          "Issuer": {
            "CommonName": "B CA"
          }
        })";
    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
        test_onc_pattern, base::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

    client_cert_config->onc_source = onc_source;
    client_cert_config->client_cert_type = ::onc::client_cert::kPattern;
    client_cert_config->pattern.ReadFromONCDictionary(parsed_json->GetDict());
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // that is signed by the test CA cert (stored in |test_ca_cert_pem_|). In
  // particular it will match the test client cert.
  void SetupPolicyMatchingIssuerPEM(::onc::ONCSource onc_source,
                                    const std::string& identity) {
    static constexpr char kTestPolicyTemplate[] = R"(
        [ { "GUID": "wifi_stub",
            "Name": "wifi_stub",
            "Type": "WiFi",
            "WiFi": {
              "Security": "WPA-EAP",
              "SSID": "wifi_ssid",
              "EAP": {
                "Identity": "%s",
                "Outer": "EAP-TLS",
                "ClientCertType": "Pattern",
                "ClientCertPattern": {
                  "IssuerCAPEMs": [ "%s" ]
                }
              }
            }
        } ])";
    std::string policy_json = base::StringPrintf(
        kTestPolicyTemplate, identity.c_str(), test_ca_cert_pem_.c_str());
    ASSERT_NO_FATAL_FAILURE(SetManagedNetworkPolicy(onc_source, policy_json));
  }

  void SetManagedNetworkPolicy(::onc::ONCSource onc_source,
                               std::string_view policy_json) {
    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
        policy_json,
        base::JSON_ALLOW_TRAILING_COMMAS | base::JSON_ALLOW_CONTROL_CHARS);
    ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
    ASSERT_TRUE(parsed_json->is_list());

    std::string user_hash =
        onc_source == ::onc::ONC_SOURCE_USER_POLICY ? kUserHash : "";
    managed_config_handler_->SetPolicy(
        onc_source, user_hash, parsed_json->GetList(),
        /*global_network_config=*/base::Value::Dict());
  }

  void SetWifiState(const std::string& state) {
    ASSERT_TRUE(service_test_->SetServiceProperty(
        kWifiStub, shill::kStateProperty, base::Value(state)));
  }

  void GetServiceProperty(const std::string& prop_name,
                          std::string* prop_value) {
    prop_value->clear();
    const base::Value::Dict* properties =
        service_test_->GetServiceProperties(kWifiStub);
    if (!properties)
      return;
    const std::string* value = properties->FindString(prop_name);
    if (value)
      *prop_value = *value;
  }

  // Returns a list of all certificates that are stored on |test_nsscertdb_|'s
  // private slot.
  net::ScopedCERTCertificateList ListCertsOnPrivateSlot() {
    net::ScopedCERTCertificateList certs;
    base::RunLoop run_loop;
    test_nsscertdb_->ListCertsInSlot(
        base::BindOnce(&OnListCertsDone, run_loop.QuitClosure(), &certs),
        test_nsscertdb_->GetPrivateSlot().get());
    run_loop.Run();
    return certs;
  }

  void ResolveTestHelper(const char* test_policy_network, bool expect_failure) {
    SetupWifi();
    task_environment_.RunUntilIdle();
    StartNetworkCertLoader();
    task_environment_.RunUntilIdle();
    SetupNetworkHandlers();
    task_environment_.RunUntilIdle();

    // Make sure that expiring client certs don't cause issues.
    test_clock_->SetNow(base::Time::Min());

    // Apply the network policy.
    network_properties_changed_count_ = 0;
    ASSERT_NO_FATAL_FAILURE(SetManagedNetworkPolicy(
        ::onc::ONC_SOURCE_USER_POLICY, test_policy_network));
    task_environment_.RunUntilIdle();

    // The referenced client cert does not exist yet, so expect that it has not
    // been resolved.
    std::string pkcs11_id;
    GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
    EXPECT_TRUE(pkcs11_id.empty());
    EXPECT_EQ(1, network_properties_changed_count_);

    // Now import a client certificate which has the GUID required using the
    // |CertificateImporterImpl|.
    auto onc_parsed_certificates = OncParsedCertificatesForPkcs12File(
        net::GetTestCertsDirectory().AppendASCII("client-empty-password.p12"),
        "{some-unique-guid}");
    ASSERT_TRUE(onc_parsed_certificates);

    onc::CertificateImporterImpl importer(
        task_environment_.GetMainThreadTaskRunner(), test_nsscertdb_.get());
    base::RunLoop import_loop;
    importer.ImportClientCertificates(
        onc_parsed_certificates->client_certificates(),
        base::BindOnce(&OnImportCompleted, import_loop.QuitClosure()));
    import_loop.Run();
    task_environment_.RunUntilIdle();

    // Find the imported cert and get its id.
    net::ScopedCERTCertificateList private_slot_certs =
        ListCertsOnPrivateSlot();
    ASSERT_EQ(1u, private_slot_certs.size());
    int slot_id = 0;
    const std::string imported_cert_pkcs11_id =
        NetworkCertLoader::GetPkcs11IdAndSlotForCert(
            private_slot_certs[0].get(), &slot_id);
    std::string imported_cert_formatted_pkcs11_id =
        base::StringPrintf("%i:%s", slot_id, imported_cert_pkcs11_id.c_str());

    // Verify that the resolver positively matched the pattern in the policy
    // with the test client cert and configured the network.
    GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
    if (!expect_failure) {
      EXPECT_EQ(imported_cert_formatted_pkcs11_id, pkcs11_id);
      EXPECT_EQ(2, network_properties_changed_count_);
    } else {
      EXPECT_NE(imported_cert_formatted_pkcs11_id, pkcs11_id);
      EXPECT_EQ(1, network_properties_changed_count_);
    }
  }

  base::test::TaskEnvironment task_environment_;
  int network_properties_changed_count_ = 0;
  std::string test_cert_id_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  raw_ptr<NetworkCertLoader, DanglingUntriaged> network_cert_loader_ = nullptr;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_system_nsscertdb_;

 private:
  // ClientCertResolver::Observer:
  void ResolveRequestCompleted(bool network_properties_changed) override {
    if (network_properties_changed)
      ++network_properties_changed_count_;
  }

 protected:
  raw_ptr<ShillServiceClient::TestInterface, DanglingUntriaged> service_test_ =
      nullptr;
  raw_ptr<ShillProfileClient::TestInterface, DanglingUntriaged> profile_test_ =
      nullptr;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  net::ScopedCERTCertificate test_client_cert_;
  std::string test_ca_cert_pem_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB test_system_nssdb_;
};

TEST_F(ClientCertResolverTest, NoMatchingCertificates) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  StartNetworkCertLoader();
  SetupWifi();
  task_environment_.RunUntilIdle();
  network_properties_changed_count_ = 0;
  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
}

TEST_F(ClientCertResolverTest, MatchIssuerCNWithoutIssuerInstalled) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerCN(::onc::ONC_SOURCE_USER_POLICY));
  task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

// Test that matching works on a certificate with invalid characters in a
// PrintableString field. See crbug.com/788655.
TEST_F(ClientCertResolverTest, MatchSubjectOrgOnBadPrintableStringCert) {
  ASSERT_NO_FATAL_FAILURE(SetupTestCertWithBadPrintableString());

  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingSubjectOrgForBadPrintableStringCert(
          ::onc::ONC_SOURCE_USER_POLICY));
  task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveOnCertificatesLoaded) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveAfterPolicyApplication) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();
  StartNetworkCertLoader();
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Policy application will trigger the ClientCertResolver.
  network_properties_changed_count_ = 0;
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ExpiringCertificate) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  SetWifiState(shill::kStateOnline);
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);

  // Verify that, after the certificate expired and the network disconnection
  // happens, no client certificate was configured and the ClientCertResolver
  // notified its observers with |network_properties_changed| = true.
  network_properties_changed_count_ = 0;
  test_clock_->SetNow(base::Time::Max());
  SetWifiState(shill::kStateIdle);
  task_environment_.RunUntilIdle();
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

// Test for crbug.com/765489. When the ClientCertResolver re-resolves a network
// as response to a NetworkConnectionStateChanged notification, and the
// resulting cert is the same as the last resolved cert, it should not call
// ResolveRequestCompleted with |network_properties_changed| = true.
TEST_F(ClientCertResolverTest, SameCertAfterNetworkConnectionStateChanged) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  SetWifiState(shill::kStateOnline);
  task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);

  // Verify that after the network disconnection happens, the configured
  // certificate doesn't change and ClientCertResolver does not notify its
  // observers with |network_properties_changed| = true.
  network_properties_changed_count_ = 0;
  SetWifiState(shill::kStateIdle);
  task_environment_.RunUntilIdle();
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(0, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, UserPolicyUsesSystemToken) {
  SetupTestCertInSystemToken("client_1");
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerCN(::onc::ONC_SOURCE_USER_POLICY));
  task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, network_cert_loader_->client_certs().size());
  EXPECT_TRUE(network_cert_loader_->client_certs()[0].is_device_wide());

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, UserPolicyUsesSystemTokenSync) {
  SetupTestCertInSystemToken("client_1");
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(::onc::ONC_SOURCE_USER_POLICY,
                                         &client_cert_config);

  base::Value::Dict shill_properties;
  ClientCertResolver::ResolveClientCertificateSync(
      client_cert::ConfigType::kEap, client_cert_config, &shill_properties);
  std::string pkcs11_id =
      GetStringFromDict(shill_properties, shill::kEapCertIdProperty);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyUsesSystemToken) {
  SetupTestCertInSystemToken("client_1");
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerCN(::onc::ONC_SOURCE_USER_POLICY));
  task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, network_cert_loader_->client_certs().size());
  EXPECT_TRUE(network_cert_loader_->client_certs()[0].is_device_wide());

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyUsesSystemTokenSync) {
  SetupTestCertInSystemToken("client_1");
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         &client_cert_config);

  base::Value::Dict shill_properties;
  ClientCertResolver::ResolveClientCertificateSync(
      client_cert::ConfigType::kEap, client_cert_config, &shill_properties);
  std::string pkcs11_id =
      GetStringFromDict(shill_properties, shill::kEapCertIdProperty);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyDoesNotUseUserToken) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerCN(::onc::ONC_SOURCE_DEVICE_POLICY));
  task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, network_cert_loader_->client_certs().size());
  EXPECT_FALSE(network_cert_loader_->client_certs()[0].is_device_wide());

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
}

TEST_F(ClientCertResolverTest, DevicePolicyDoesNotUseUserTokenSync) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         &client_cert_config);

  base::Value::Dict shill_properties;
  ClientCertResolver::ResolveClientCertificateSync(
      client_cert::ConfigType::kEap, client_cert_config, &shill_properties);
  std::string pkcs11_id =
      GetStringFromDict(shill_properties, shill::kEapCertIdProperty);
  EXPECT_EQ(std::string(), pkcs11_id);
}

TEST_F(ClientCertResolverTest, PopulateIdentityFromCert) {
  SetupTestCerts("client_3", true /* import issuer */);
  SetupWifi();
  task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(SetupPolicyMatchingIssuerPEM(
      ::onc::ONC_SOURCE_USER_POLICY, "${CERT_SAN_EMAIL}"));
  task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  task_environment_.RunUntilIdle();

  // Verify that the resolver read the subjectAltName email field from the
  // cert, and wrote it into the shill service entry.
  std::string identity;
  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("santest@example.com", identity);
  EXPECT_EQ(1, network_properties_changed_count_);

  // Verify that after changing the ONC policy to request a variant of the
  // Microsoft Universal Principal Name field instead, the correct value is
  // substituted into the shill service entry.
  ASSERT_NO_FATAL_FAILURE(SetupPolicyMatchingIssuerPEM(
      ::onc::ONC_SOURCE_USER_POLICY, "upn-${CERT_SAN_UPN}-suffix"));
  task_environment_.RunUntilIdle();

  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("upn-santest@ad.corp.example.com-suffix", identity);

  // Verify that after changing the ONC policy to request the subject CommonName
  // field, the correct value is substituted into the shill service entry.
  ASSERT_NO_FATAL_FAILURE(SetupPolicyMatchingIssuerPEM(
      ::onc::ONC_SOURCE_USER_POLICY,
      "subject-cn-${CERT_SUBJECT_COMMON_NAME}-suffix"));
  task_environment_.RunUntilIdle();

  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("subject-cn-Client Cert F-suffix", identity);
}

// Test for crbug.com/781276. A notification which results in no networks to be
// resolved should not alter the state of IsAnyResolveTaskRunning().
TEST_F(ClientCertResolverTest, TestResolveTaskQueued) {
  // Set up ClientCertResolver and let it run initially
  SetupTestCerts("client_1", true /* import issuer */);
  StartNetworkCertLoader();
  SetupWifi();
  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingIssuerPEM(::onc::ONC_SOURCE_USER_POLICY, ""));
  task_environment_.RunUntilIdle();

  // Pretend that policy was applied, this shall queue a resolving task.
  static_cast<NetworkPolicyObserver*>(client_cert_resolver_.get())
      ->PolicyAppliedToNetwork(kWifiStub);
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Pretend that the network list has changed. One resolving task should still
  // be queued.
  static_cast<NetworkStateHandlerObserver*>(client_cert_resolver_.get())
      ->NetworkListChanged();
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Pretend that certificates have changed. One resolving task should still be
  // queued.
  static_cast<NetworkCertLoader::Observer*>(client_cert_resolver_.get())
      ->OnCertificatesLoaded();
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

// Tests that a ClientCertRef reference is resolved by |ClientCertResolver|.
// Uses the |CertificateImporterImpl| to import the client certificate from ONC
// policy, ensuring that setting the cert's key's nickname (in the import step)
// and evaluating it (in the matching step) work well together.
TEST_F(ClientCertResolverTest, ResolveClientCertRef) {
  const char* test_policy_network =
      R"([ { "GUID": "wifi_stub",
               "Name": "wifi_stub",
               "Type": "WiFi",
               "WiFi": {
                 "Security": "WPA-EAP",
                 "SSID": "wifi_ssid",
                 "EAP": {
                   "Identity": "TestIdentity",
                   "Outer": "EAP-TLS",
                   "ClientCertType": "Ref",
                   "ClientCertRef": "{some-unique-guid}"
                 }
               }
           } ])";

  ResolveTestHelper(test_policy_network, false);
}

// Tests that a ClientCertProvisioningProfileId is resolved by
// |ClientCertResolver|.
// Same test as above except that we search for a different key. Note that
// this is using the Ref type instead of the ProvisioningProfileId type because
// that syntax was chosen by the Android team for this type of match. We also
// support a dedicated syntax which is tested below.
TEST_F(ClientCertResolverTest, ResolveByCertProfileIdInClientCertRef) {
  const char* test_policy_network =
      R"([ { "GUID": "wifi_stub",
               "Name": "wifi_stub",
               "Type": "WiFi",
               "WiFi": {
                 "Security": "WPA-EAP",
                 "SSID": "wifi_ssid",
                 "EAP": {
                   "Identity": "TestIdentity",
                   "Outer": "EAP-TLS",
                   "ClientCertType": "Ref",
                   "ClientCertRef": "{some-provisioning-id}"
                 }
               }
           } ])";

  // Override the getter for the provisioning id. See
  // ClientCertResolver::SetProvisioningIdForCertGetterForTesting for details.
  // We know that we only import one cert, so we do not need to check more,
  // here.
  auto runner = ClientCertResolver::SetProvisioningIdForCertGetterForTesting(
      base::BindRepeating([](CERTCertificate* cert) -> std::string {
        return "{some-provisioning-id}";
      }));

  ResolveTestHelper(test_policy_network, false);
}

// Tests that a ClientCertProvisioningProfileId is resolved by
// |ClientCertResolver|.
// Same test as above except that we use the dedicated syntax for a
// ClientCertProvisioningProfileId.
TEST_F(ClientCertResolverTest, ResolveByCertProfileId) {
  const char* test_policy_network =
      R"([ { "GUID": "wifi_stub",
               "Name": "wifi_stub",
               "Type": "WiFi",
               "WiFi": {
                 "Security": "WPA-EAP",
                 "SSID": "wifi_ssid",
                 "EAP": {
                   "Identity": "TestIdentity",
                   "Outer": "EAP-TLS",
                   "ClientCertType": "ProvisioningProfileId",
                   "ClientCertProvisioningProfileId": "{some-provisioning-id}"
                 }
               }
           } ])";

  // Override the getter for the provisioning id. See
  // ClientCertResolver::SetProvisioningIdForCertGetterForTesting for details.
  // We know that we only import one cert, so we do not need to check more,
  // here.
  auto runner = ClientCertResolver::SetProvisioningIdForCertGetterForTesting(
      base::BindRepeating([](CERTCertificate* cert) -> std::string {
        return "{some-provisioning-id}";
      }));

  ResolveTestHelper(test_policy_network, false);
}

// Tests that a ClientCertProvisioningProfileId is not resolved by
// |ClientCertResolver| if it has the wrong profile id.
TEST_F(ClientCertResolverTest, ResolveByCertProfileIdFailure) {
  const char* test_policy_network =
      R"([ { "GUID": "wifi_stub",
               "Name": "wifi_stub",
               "Type": "WiFi",
               "WiFi": {
                 "Security": "WPA-EAP",
                 "SSID": "wifi_ssid",
                 "EAP": {
                   "Identity": "TestIdentity",
                   "Outer": "EAP-TLS",
                   "ClientCertType": "ProvisioningProfileId",
                   "ClientCertProvisioningProfileId": "{wrong-provisioning-id}"
                 }
               }
           } ])";

  // Override the getter for the provisioning id. See
  // ClientCertResolver::SetProvisioningIdForCertGetterForTesting
  // for details. We know that we only import one cert, so we do not need to
  // check more, here.
  auto runner = ClientCertResolver::SetProvisioningIdForCertGetterForTesting(
      base::BindRepeating([](CERTCertificate* cert) -> std::string {
        return "{some-provisioning-id}";
      }));

  ResolveTestHelper(test_policy_network, true);
}

}  // namespace ash

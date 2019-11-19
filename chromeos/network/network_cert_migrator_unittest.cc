// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>
#include <pk11pub.h>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_state_handler.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kWifiStub = "wifi_stub";
const char* kEthernetEapStub = "ethernet_eap_stub";
const char* kVPNStub = "vpn_stub";
const char* kUserShillProfile = "/profile/profile1";

}  // namespace

class NetworkCertMigratorTest : public testing::Test {
 public:
  NetworkCertMigratorTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        service_test_(nullptr) {}
  ~NetworkCertMigratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(test_system_nssdb_.is_open());
    ASSERT_TRUE(test_user_nssdb_.is_open());
    // Use the same slot as public and private slot for the user's
    // NSSCertDatabse for testing.
    test_user_nsscertdb_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_user_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_user_nssdb_.slot())));
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
    ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        kUserShillProfile, "" /* userhash */);
    task_environment_.RunUntilIdle();
    service_test_->ClearServices();
    task_environment_.RunUntilIdle();

    NetworkCertLoader::Initialize();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_cert_migrator_.reset();
    network_state_handler_.reset();
    NetworkCertLoader::Shutdown();
    shill_clients::Shutdown();
  }

 protected:
  enum class ShillProfile { SHARED, USER };

  void SetupTestClientCert(crypto::ScopedTestNSSDB* nssdb) {
    net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                        "client_1.pem", "client_1.pk8",
                                        nssdb->slot(), &test_client_cert_);
    ASSERT_TRUE(test_client_cert_.get());

    int slot_id = -1;
    test_client_cert_pkcs11_id_ = NetworkCertLoader::GetPkcs11IdAndSlotForCert(
        test_client_cert_.get(), &slot_id);
    ASSERT_FALSE(test_client_cert_pkcs11_id_.empty());
    ASSERT_NE(-1, slot_id);
    test_client_cert_slot_id_ = base::NumberToString(slot_id);
  }

  void SetupNetworkHandlers() {
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_cert_migrator_.reset(new NetworkCertMigrator);
    network_cert_migrator_->Init(network_state_handler_.get());
  }

  void AddService(ShillProfile shill_profile,
                  const std::string& network_id,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(network_id /* service_path */,
                              network_id /* guid */,
                              network_id /* name */,
                              type,
                              state,
                              true /* add_to_visible */);

    // Ensure that the service appears as 'configured', i.e. is associated to a
    // Shill profile.
    std::string shill_profile_path =
        shill_profile == ShillProfile::SHARED
            ? ShillProfileClient::GetSharedProfilePath()
            : std::string(kUserShillProfile);
    service_test_->SetServiceProperty(network_id, shill::kProfileProperty,
                                      base::Value(shill_profile_path));
  }

  void SetupNetworkWithEapCertId(ShillProfile shill_profile,
                                 bool wifi,
                                 const std::string& cert_id) {
    std::string type = wifi ? shill::kTypeWifi: shill::kTypeEthernetEap;
    std::string name = wifi ? kWifiStub : kEthernetEapStub;
    AddService(shill_profile, name, type, shill::kStateOnline);
    service_test_->SetServiceProperty(name, shill::kEapCertIdProperty,
                                      base::Value(cert_id));
    service_test_->SetServiceProperty(name, shill::kEapKeyIdProperty,
                                      base::Value(cert_id));

    if (wifi) {
      service_test_->SetServiceProperty(name, shill::kSecurityClassProperty,
                                        base::Value(shill::kSecurity8021x));
    }
  }

  void GetEapCertId(bool wifi, std::string* cert_id) {
    cert_id->clear();

    std::string name = wifi ? kWifiStub : kEthernetEapStub;
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(name);
    properties->GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                              cert_id);
  }

  void SetupVpnWithCertId(ShillProfile shill_profile,
                          bool open_vpn,
                          const std::string& slot_id,
                          const std::string& pkcs11_id) {
    AddService(shill_profile, kVPNStub, shill::kTypeVPN, shill::kStateIdle);
    base::DictionaryValue provider;
    if (open_vpn) {
      provider.SetKey(shill::kTypeProperty,
                      base::Value(shill::kProviderOpenVpn));
      provider.SetKey(shill::kOpenVPNClientCertIdProperty,
                      base::Value(pkcs11_id));
    } else {
      provider.SetKey(shill::kTypeProperty,
                      base::Value(shill::kProviderL2tpIpsec));
      provider.SetKey(shill::kL2tpIpsecClientCertSlotProperty,
                      base::Value(slot_id));
      provider.SetKey(shill::kL2tpIpsecClientCertIdProperty,
                      base::Value(pkcs11_id));
    }
    service_test_->SetServiceProperty(
        kVPNStub, shill::kProviderProperty, provider);
  }

  void GetVpnCertId(bool open_vpn,
                    std::string* slot_id,
                    std::string* pkcs11_id) {
    slot_id->clear();
    pkcs11_id->clear();

    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kVPNStub);
    ASSERT_TRUE(properties);
    const base::DictionaryValue* provider = nullptr;
    properties->GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                                  &provider);
    if (!provider)
      return;
    if (open_vpn) {
      provider->GetStringWithoutPathExpansion(
          shill::kOpenVPNClientCertIdProperty, pkcs11_id);
    } else {
      provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, slot_id);
      provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, pkcs11_id);
    }
  }

  base::test::TaskEnvironment task_environment_;
  ShillServiceClient::TestInterface* service_test_;
  net::ScopedCERTCertificate test_client_cert_;
  std::string test_client_cert_pkcs11_id_;
  std::string test_client_cert_slot_id_;
  crypto::ScopedTestNSSDB test_system_nssdb_;
  crypto::ScopedTestNSSDB test_user_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_system_nsscertdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_user_nsscertdb_;

 private:
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkCertMigrator> network_cert_migrator_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertMigratorTest);
};

// Test that migration of user profile networks is deferred until the user's NSS
// Database has been loaded.
// See crbug.com/774745
TEST_F(NetworkCertMigratorTest, DeferUserNetworkMigrationToUserCertDbLoad) {
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */, "123:12345");
  // Load the system NSSDB only first
  NetworkCertLoader::Get()->SetSystemNSSDB(test_system_nsscertdb_.get());

  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Migration should not have been performed on the user profile network,
  // because the user NSSDB has not been loaded yet.
  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id = "123:12345";
  EXPECT_EQ(expected_cert_id, cert_id);

  // Load the user NSSDB now
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());
  task_environment_.RunUntilIdle();

  // Since the PKCS11 ID is unknown, the certificate configuration of the shared
  // profile network will be cleared.
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(std::string(), cert_id);
}

// Test that migration of shared profile networks is done on first NSS database
// load.
TEST_F(NetworkCertMigratorTest, RunSharedNetworkMigrationOnFirstCertDbLoad) {
  SetupNetworkWithEapCertId(ShillProfile::SHARED, true /* wifi */, "123:12345");
  // Load the system NSSDB only first
  NetworkCertLoader::Get()->SetSystemNSSDB(test_system_nsscertdb_.get());

  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Since the PKCS11 ID is unknown, the certificate configuration of the shared
  // profile network will be cleared.
  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(std::string(), cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateOnInitialization) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  // Add a network for migration before the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */,
                            "123:" + test_client_cert_pkcs11_id_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateEapCertIdNoMatchingCert) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */,
                            "unknown pkcs11 id");

  task_environment_.RunUntilIdle();
  // Since the PKCS11 ID is unknown, the certificate configuration will be
  // cleared.
  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(std::string(), cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateEapCertIdNoSlotId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */,
                            test_client_cert_pkcs11_id_);

  task_environment_.RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateWifiEapCertIdWrongSlotId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */,
                            "123:" + test_client_cert_pkcs11_id_);

  task_environment_.RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, DoNotChangeEapCertIdWithCorrectSlotId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER, true /* wifi */,
                            expected_cert_id);

  task_environment_.RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, IgnoreOpenVPNCertId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  const char kPkcs11Id[] = "any slot id";

  // Add a new network for migration after the handlers are initialized.
  SetupVpnWithCertId(ShillProfile::USER,

                     true /* OpenVPN */, std::string() /* no slot id */,
                     kPkcs11Id);

  task_environment_.RunUntilIdle();

  std::string pkcs11_id;
  std::string unused_slot_id;
  GetVpnCertId(true /* OpenVPN */, &unused_slot_id, &pkcs11_id);
  EXPECT_EQ(kPkcs11Id, pkcs11_id);
}

TEST_F(NetworkCertMigratorTest, MigrateEthernetEapCertIdWrongSlotId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(ShillProfile::USER,

                            false /* ethernet */,
                            "123:" + test_client_cert_pkcs11_id_);

  task_environment_.RunUntilIdle();

  std::string cert_id;
  GetEapCertId(false /* ethernet */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateIpsecCertIdWrongSlotId) {
  NetworkCertLoader::Get()->SetUserNSSDB(test_user_nsscertdb_.get());

  SetupTestClientCert(&test_user_nssdb_);
  SetupNetworkHandlers();
  task_environment_.RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupVpnWithCertId(ShillProfile::USER, false /* IPsec */, "123",
                     test_client_cert_pkcs11_id_);

  task_environment_.RunUntilIdle();

  std::string pkcs11_id;
  std::string slot_id;
  GetVpnCertId(false /* IPsec */, &slot_id, &pkcs11_id);
  EXPECT_EQ(test_client_cert_pkcs11_id_, pkcs11_id);
  EXPECT_EQ(test_client_cert_slot_id_, slot_id);
}

}  // namespace chromeos

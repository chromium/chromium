// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/policy_certificate_provider.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FakePolicyCertificateProvider : public PolicyCertificateProvider {
 public:
  void AddPolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemovePolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  net::CertificateList GetAllServerAndAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  net::CertificateList GetAllAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // NetworkCertLoader only retrieves profile-wide certificates.
    EXPECT_EQ(chromeos::onc::CertificateScope::Default(), scope);

    return authority_certificates_;
  }

  net::CertificateList GetWebTrustedCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  net::CertificateList GetCertificatesWithoutWebTrust(
      const chromeos::onc::CertificateScope& scope) const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  const std::set<std::string>& GetExtensionIdsWithPolicyCertificates()
      const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return kNoExtensions;
  }

  void SetAuthorityCertificates(
      const net::CertificateList& authority_certificates) {
    authority_certificates_ = authority_certificates;
  }

  void NotifyObservers() {
    for (auto& observer : observer_list_)
      observer.OnPolicyProvidedCertsChanged();
  }

 private:
  base::ObserverList<PolicyCertificateProvider::Observer,
                     true /* check_empty */>::Unchecked observer_list_;
  net::CertificateList authority_certificates_;
  const std::set<std::string> kNoExtensions = {};
};

bool IsCertInCertificateList(
    CERTCertificate* cert,
    bool device_wide,
    const std::vector<NetworkCertLoader::NetworkCert>& network_cert_list) {
  for (const auto& network_cert : network_cert_list) {
    if (device_wide == network_cert.is_device_wide() &&
        net::x509_util::IsSameCertificate(network_cert.cert(), cert)) {
      return true;
    }
  }
  return false;
}

bool IsCertInCertificateList(
    const net::X509Certificate* cert,
    bool device_wide,
    const std::vector<NetworkCertLoader::NetworkCert>& network_cert_list) {
  for (const auto& network_cert : network_cert_list) {
    if (device_wide == network_cert.is_device_wide() &&
        net::x509_util::IsSameCertificate(network_cert.cert(), cert)) {
      return true;
    }
  }
  return false;
}

size_t CountCertOccurencesInCertificateList(
    CERTCertificate* cert,
    const std::vector<NetworkCertLoader::NetworkCert>& network_cert_list) {
  size_t count = 0;
  for (const auto& network_cert : network_cert_list) {
    if (net::x509_util::IsSameCertificate(network_cert.cert(), cert))
      ++count;
  }
  return count;
}

class TestNSSCertDatabase : public net::NSSCertDatabaseChromeOS {
 public:
  TestNSSCertDatabase(crypto::ScopedPK11Slot public_slot,
                      crypto::ScopedPK11Slot private_slot)
      : NSSCertDatabaseChromeOS(std::move(public_slot),
                                std::move(private_slot)) {}
  ~TestNSSCertDatabase() override = default;

  // Make this method visible in the public interface.
  void NotifyObserversCertDBChanged() {
    NSSCertDatabaseChromeOS::NotifyObserversCertDBChanged();
  }
};

// Describes a client certificate along with a key, stored in
// net::GetTestCertsDirectory().
struct TestClientCertWithKey {
  const char* cert_pem_filename;
  const char* key_pk8_filename;
};

const TestClientCertWithKey TEST_CLIENT_CERT_1 = {"client_1.pem",
                                                  "client_1.pk8"};
const TestClientCertWithKey TEST_CLIENT_CERT_2 = {"client_2.pem",
                                                  "client_2.pk8"};

class NetworkCertLoaderTest : public testing::Test,
                              public NetworkCertLoader::Observer {
 public:
  NetworkCertLoaderTest()
      : cert_loader_(nullptr), certificates_loaded_events_count_(0U) {}

  ~NetworkCertLoaderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(primary_public_slot_db_.is_open());
    ASSERT_TRUE(primary_private_slot_db_.is_open());

    NetworkCertLoader::Initialize();
    cert_loader_ = NetworkCertLoader::Get();
    cert_loader_->AddObserver(this);
  }

  void TearDown() override {
    cert_loader_->RemoveObserver(this);
    NetworkCertLoader::Shutdown();
  }

 protected:
  // Initializes |primary_certdb_| as a NSSCertDatabse based on
  // |primary_public_slot_db| and |primary_private_slot_db| and starts the
  // NetworkCertLoader with it.
  void StartCertLoaderWithPrimaryDB() {
    CreateCertDatabase(&primary_public_slot_db_, &primary_private_slot_db_,
                       &primary_certdb_);
    cert_loader_->SetUserNSSDB(primary_certdb_.get());

    task_environment_.RunUntilIdle();
    GetAndResetCertificatesLoadedEventsCount();
  }

  // NetworkCertLoader::Observer:
  // The test keeps count of times the observer method was called.
  void OnCertificatesLoaded() override {
    EXPECT_TRUE(certificates_loaded_events_count_ == 0);
    certificates_loaded_events_count_++;
  }

  // Returns the number of |OnCertificatesLoaded| calls observed since the
  // last call to this method equals |value|.
  size_t GetAndResetCertificatesLoadedEventsCount() {
    size_t result = certificates_loaded_events_count_;
    certificates_loaded_events_count_ = 0;
    return result;
  }

  // Creates a TestNSSCertDatabase in *|certdb|, using the (mandatory)
  // |public_slot_db| for the public slot and the (optional) |private_slot_db|
  // as the private slot.
  void CreateCertDatabase(crypto::ScopedTestNSSDB* public_slot_db,
                          crypto::ScopedTestNSSDB* private_slot_db,
                          std::unique_ptr<TestNSSCertDatabase>* certdb) {
    ASSERT_TRUE(public_slot_db && public_slot_db->is_open());

    crypto::ScopedPK11Slot scoped_public_slot(
        PK11_ReferenceSlot(public_slot_db->slot()));
    crypto::ScopedPK11Slot scoped_private_slot;
    if (private_slot_db) {
      ASSERT_TRUE(private_slot_db->is_open());
      scoped_private_slot =
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(private_slot_db->slot()));
    }
    *certdb = std::make_unique<TestNSSCertDatabase>(
        std::move(scoped_public_slot), std::move(scoped_private_slot));
  }

  void ImportCACert(const std::string& cert_file,
                    net::NSSCertDatabase* database,
                    net::ScopedCERTCertificateList* imported_certs) {
    ASSERT_TRUE(database);
    ASSERT_TRUE(imported_certs);

    *imported_certs = net::CreateCERTCertificateListFromFile(
        net::GetTestCertsDirectory(), cert_file,
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1U, imported_certs->size());

    net::NSSCertDatabase::ImportCertFailureList failed;
    ASSERT_TRUE(database->ImportCACerts(
        *imported_certs, net::NSSCertDatabase::TRUST_DEFAULT, &failed));
    ASSERT_TRUE(failed.empty());
  }

  // Import a client cert described by |test_cert| and key into a PKCS11 slot.
  // Then notify |database_to_notify| (which is presumably using that slot) that
  // new certificates are available.
  net::ScopedCERTCertificate ImportClientCertAndKey(
      TestNSSCertDatabase* database_to_notify,
      PK11SlotInfo* slot_to_use,
      const TestClientCertWithKey& test_cert) {
    // Import a client cert signed by that CA.
    net::ScopedCERTCertificate client_cert;
    net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), test_cert.cert_pem_filename,
        test_cert.key_pk8_filename, slot_to_use, &client_cert);
    database_to_notify->NotifyObserversCertDBChanged();
    return client_cert;
  }

  // Import |TEST_CLIENT_CERT_1| into a PKCS11 slot. Then notify
  // |database_to_notify| (which is presumably using that slot) that new
  // certificates are avialable.
  net::ScopedCERTCertificate ImportClientCertAndKey(
      TestNSSCertDatabase* database_to_notify,
      PK11SlotInfo* slot_to_use) {
    return ImportClientCertAndKey(database_to_notify, slot_to_use,
                                  TEST_CLIENT_CERT_1);
  }

  // Import a client cert into |database|'s private slot.
  net::ScopedCERTCertificate ImportClientCertAndKey(
      TestNSSCertDatabase* database) {
    return ImportClientCertAndKey(database, database->GetPrivateSlot().get());
  }

  // Adds the PKCS11 slot from |system_db_| to |certdb| as system slot.
  void AddSystemToken(TestNSSCertDatabase* certdb) {
    ASSERT_TRUE(system_db_.is_open());
    certdb->SetSystemSlot(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_db_.slot())));
  }

  base::test::TaskEnvironment task_environment_;

  NetworkCertLoader* cert_loader_;

  // The NSSCertDatabse and underlying slots for the primary user (because
  // NetworkCertLoader uses device-wide certs and primary user's certs).
  crypto::ScopedTestNSSDB primary_private_slot_db_;
  crypto::ScopedTestNSSDB primary_public_slot_db_;
  std::unique_ptr<TestNSSCertDatabase> primary_certdb_;

  // Additional NSS DB simulating the system token.
  crypto::ScopedTestNSSDB system_db_;

  // A NSSCertDatabase which only uses the system token (simulated by
  // system_db_).
  std::unique_ptr<TestNSSCertDatabase> system_certdb_;

 private:
  size_t certificates_loaded_events_count_;
};

}  // namespace

TEST_F(NetworkCertLoaderTest, BasicOnlyUserDB) {
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  CreateCertDatabase(&primary_public_slot_db_, &primary_private_slot_db_,
                     &primary_certdb_);
  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);
  task_environment_.RunUntilIdle();

  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->authority_certs().empty());
  EXPECT_TRUE(cert_loader_->client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Default CA cert roots should get loaded but not be available in
  // NetworkCertLoader.
  EXPECT_EQ(1U, cert_loader_->authority_certs().size());
  EXPECT_TRUE(cert_loader_->client_certs().empty());
}

TEST_F(NetworkCertLoaderTest, BasicOnlySystemDB) {
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  CreateCertDatabase(&system_db_, nullptr /* private_slot_db */,
                     &system_certdb_);
  AddSystemToken(system_certdb_.get());
  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", system_certdb_.get(), &certs);
  task_environment_.RunUntilIdle();

  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->authority_certs().empty());
  EXPECT_TRUE(cert_loader_->client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Default CA cert roots should get loaded, but not be exposed through
  // NetworkCertLoader.
  EXPECT_EQ(1U, cert_loader_->authority_certs().size());
  EXPECT_TRUE(cert_loader_->client_certs().empty());
}

// Tests the NetworkCertLoader with a system DB and then with an additional user
// DB which does not have access to the system token.
TEST_F(NetworkCertLoaderTest, SystemAndUnaffiliatedUserDB) {
  CreateCertDatabase(&system_db_, nullptr /* private_slot_db */,
                     &system_certdb_);
  AddSystemToken(system_certdb_.get());
  net::ScopedCERTCertificate system_token_cert(ImportClientCertAndKey(
      system_certdb_.get(), system_db_.slot(), TEST_CLIENT_CERT_1));
  ASSERT_TRUE(system_token_cert);

  CreateCertDatabase(&primary_public_slot_db_, &primary_private_slot_db_,
                     &primary_certdb_);
  net::ScopedCERTCertificate user_token_cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_private_slot_db_.slot(),
      TEST_CLIENT_CERT_2));
  ASSERT_TRUE(user_token_cert);

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_EQ(1U, cert_loader_->client_certs().size());

  EXPECT_TRUE(IsCertInCertificateList(system_token_cert.get(),
                                      true /* device_wide */,
                                      cert_loader_->client_certs()));

  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_EQ(2U, cert_loader_->client_certs().size());

  EXPECT_FALSE(IsCertInCertificateList(user_token_cert.get(),
                                       true /* device_wide */,
                                       cert_loader_->client_certs()));
  EXPECT_TRUE(IsCertInCertificateList(user_token_cert.get(),
                                      false /* device_wide */,
                                      cert_loader_->client_certs()));
}

// Tests the NetworkCertLoader with a system DB and then with an additional user
// DB which has access to the system token.
TEST_F(NetworkCertLoaderTest, SystemAndAffiliatedUserDB) {
  CreateCertDatabase(&system_db_, nullptr /* private_slot_db */,
                     &system_certdb_);
  AddSystemToken(system_certdb_.get());
  net::ScopedCERTCertificate system_token_cert(ImportClientCertAndKey(
      system_certdb_.get(), system_db_.slot(), TEST_CLIENT_CERT_1));
  ASSERT_TRUE(system_token_cert);

  CreateCertDatabase(&primary_public_slot_db_, &primary_private_slot_db_,
                     &primary_certdb_);
  net::ScopedCERTCertificate user_token_cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_private_slot_db_.slot(),
      TEST_CLIENT_CERT_2));
  ASSERT_TRUE(user_token_cert);

  AddSystemToken(primary_certdb_.get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_EQ(1U, cert_loader_->client_certs().size());

  EXPECT_TRUE(IsCertInCertificateList(system_token_cert.get(),
                                      true /* device_wide */,
                                      cert_loader_->client_certs()));

  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_EQ(2U, cert_loader_->client_certs().size());

  EXPECT_FALSE(IsCertInCertificateList(user_token_cert.get(),
                                       true /* device_wide */,
                                       cert_loader_->client_certs()));
  EXPECT_TRUE(IsCertInCertificateList(user_token_cert.get(),
                                      false /* device_wide */,
                                      cert_loader_->client_certs()));
  EXPECT_EQ(1U, CountCertOccurencesInCertificateList(
                    user_token_cert.get(), cert_loader_->client_certs()));
}

// Tests that NetworkCertLoader does not list certs twice if they appear on
// multiple slots.
TEST_F(NetworkCertLoaderTest, DeduplicatesCerts) {
  // Use the same slot as public and private slot.
  crypto::ScopedTestNSSDB* single_slot_db = &primary_public_slot_db_;
  CreateCertDatabase(single_slot_db /* public_slot_db */,
                     single_slot_db /* private_slot_db */, &primary_certdb_);
  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);
  task_environment_.RunUntilIdle();

  cert_loader_->SetUserNSSDB(primary_certdb_.get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Default CA cert roots should get loaded but not be available in
  // NetworkCertLoader.
  EXPECT_EQ(1U, cert_loader_->authority_certs().size());
  EXPECT_TRUE(cert_loader_->client_certs().empty());
}

TEST_F(NetworkCertLoaderTest, UpdateCertListOnNewCert) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  // Certs are loaded asynchronously, so the new cert should not yet be in the
  // cert list.
  EXPECT_FALSE(IsCertInCertificateList(certs[0].get(), false /* device_wide */,
                                       cert_loader_->client_certs()));

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // The certificate list should be updated now, as the message loop's been run.
  EXPECT_TRUE(IsCertInCertificateList(certs[0].get(), false /* device_wide */,
                                      cert_loader_->authority_certs()));

  EXPECT_FALSE(cert_loader_->IsCertificateHardwareBacked(certs[0].get()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateOnSecondaryDbChanges) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db /* public_slot_db */,
                     &secondary_db /* private_slot_db */, &secondary_certdb);

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", secondary_certdb.get(), &certs);

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(certs[0].get(), false /* device_wide */,
                                       cert_loader_->client_certs()));
  EXPECT_FALSE(IsCertInCertificateList(certs[0].get(), true /* device_wide */,
                                       cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, ClientLoaderUpdateOnNewClientCert) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(primary_certdb_.get()));
  ASSERT_TRUE(cert);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(IsCertInCertificateList(cert.get(), false /* device_wide */,
                                      cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, ClientLoaderUpdateOnNewClientCertInSystemToken) {
  CreateCertDatabase(&system_db_ /* public_slot_db */,
                     nullptr /* private_slot_db */, &system_certdb_);
  AddSystemToken(system_certdb_.get());
  task_environment_.RunUntilIdle();

  cert_loader_->SetSystemNSSDB(system_certdb_.get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->client_certs().empty());
  net::ScopedCERTCertificate cert(ImportClientCertAndKey(
      system_certdb_.get(), system_certdb_->GetSystemSlot().get()));
  ASSERT_TRUE(cert);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_EQ(1U, cert_loader_->client_certs().size());
  EXPECT_TRUE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                      cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateOnNewClientCertInSecondaryDb) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db /* public_slot_db */,
                     &secondary_db /* private_slot_db */, &secondary_certdb);

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(secondary_certdb.get()));
  ASSERT_TRUE(cert);

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(cert.get(), false /* device_wide */,
                                       cert_loader_->client_certs()));
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                       cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdatedOnCertRemoval) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(primary_certdb_.get()));
  ASSERT_TRUE(cert);

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(cert.get(), false /* device_wide */,
                                      cert_loader_->client_certs()));

  primary_certdb_->DeleteCertAndKey(cert.get());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  ASSERT_FALSE(IsCertInCertificateList(cert.get(), false /* device_wide */,
                                       cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdatedOnCACertTrustChange) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(certs[0].get(), false /* device_wide */,
                                      cert_loader_->authority_certs()));

  // The value that should have been set by |ImportCACert|.
  ASSERT_EQ(net::NSSCertDatabase::TRUST_DEFAULT,
            primary_certdb_->GetCertTrust(certs[0].get(), net::CA_CERT));
  ASSERT_TRUE(primary_certdb_->SetCertTrust(certs[0].get(), net::CA_CERT,
                                            net::NSSCertDatabase::TRUSTED_SSL));

  // Cert trust change should trigger certificate reload in cert_loader_.
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
}

TEST_F(NetworkCertLoaderTest, UpdateSinglePolicyCertificateProvider) {
  // Load a CA cert for testing.
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(cert.get());
  StartCertLoaderWithPrimaryDB();

  FakePolicyCertificateProvider device_policy_certs_provider;

  // Setting the cert provider triggers an update.
  cert_loader_->SetDevicePolicyCertificateProvider(
      &device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // When policy changes, an update is triggered too.
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                       cert_loader_->authority_certs()));
  device_policy_certs_provider.SetAuthorityCertificates({cert});
  device_policy_certs_provider.NotifyObservers();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                      cert_loader_->authority_certs()));

  // Removing the cert provider triggers an update.
  cert_loader_->SetDevicePolicyCertificateProvider(nullptr);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                       cert_loader_->authority_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdateOnTwoPolicyCertificateProviders) {
  // Load a CA cert for device and user policy.
  scoped_refptr<net::X509Certificate> device_policy_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "websocket_cacert.pem");
  ASSERT_TRUE(device_policy_cert.get());

  scoped_refptr<net::X509Certificate> user_policy_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(user_policy_cert.get());

  StartCertLoaderWithPrimaryDB();

  FakePolicyCertificateProvider device_policy_certs_provider;
  device_policy_certs_provider.SetAuthorityCertificates({device_policy_cert});
  FakePolicyCertificateProvider user_policy_certs_provider;
  user_policy_certs_provider.SetAuthorityCertificates({user_policy_cert});

  // Adding a device policy certificate provider triggers an update. In this
  // test case, the device policy certs provider already contains a cert.
  cert_loader_->SetDevicePolicyCertificateProvider(
      &device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(device_policy_cert.get(),
                                      true /* device_wide */,
                                      cert_loader_->authority_certs()));
  EXPECT_FALSE(IsCertInCertificateList(user_policy_cert.get(),
                                       false /* device_wide */,
                                       cert_loader_->authority_certs()));

  // Adding a user policy certificate provider triggers an update. In this
  // test case, the user policy certs provider already contains a cert.
  cert_loader_->SetUserPolicyCertificateProvider(&user_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(device_policy_cert.get(),
                                      true /* device_wide */,
                                      cert_loader_->authority_certs()));
  EXPECT_TRUE(IsCertInCertificateList(user_policy_cert.get(),
                                      false /* device_wide */,
                                      cert_loader_->authority_certs()));

  cert_loader_->SetDevicePolicyCertificateProvider(nullptr);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  cert_loader_->SetUserPolicyCertificateProvider(nullptr);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
}

TEST_F(NetworkCertLoaderTest,
       NoUpdateDueToPolicyCertificateProviderBeforeCertDbLoaded) {
  // Load a CA cert for testing.
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(cert.get());

  FakePolicyCertificateProvider device_policy_certs_provider;

  // Setting the cert provider does not trigger an update yet, because the
  // NetworkCertLoader has not been set to use a system or user NSS Database.
  cert_loader_->SetDevicePolicyCertificateProvider(
      &device_policy_certs_provider);
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());

  // Same when the policy changes.
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                       cert_loader_->authority_certs()));
  device_policy_certs_provider.SetAuthorityCertificates({cert});
  device_policy_certs_provider.NotifyObservers();
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());

  // After starting the NetworkCertLoader, the policy-provided cert is there.
  StartCertLoaderWithPrimaryDB();
  EXPECT_TRUE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                      cert_loader_->authority_certs()));

  // Removing the cert provider triggers an update.
  cert_loader_->SetDevicePolicyCertificateProvider(nullptr);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), true /* device_wide */,
                                       cert_loader_->authority_certs()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateWhenShuttingDown) {
  StartCertLoaderWithPrimaryDB();

  FakePolicyCertificateProvider device_policy_certs_provider;

  // Setting the cert provider triggers an update.
  cert_loader_->SetDevicePolicyCertificateProvider(
      &device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // Removing the cert provider does not trigger an update if the shutdown
  // procedure has started.
  cert_loader_->set_is_shutting_down();
  cert_loader_->SetDevicePolicyCertificateProvider(nullptr);
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
}

}  // namespace chromeos

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
#include "base/test/scoped_task_environment.h"
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

  net::CertificateList GetAllServerAndAuthorityCertificates() const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  net::CertificateList GetAllAuthorityCertificates() const override {
    return authority_certificates_;
  }

  net::CertificateList GetWebTrustedCertificates() const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  net::CertificateList GetCertificatesWithoutWebTrust() const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
    return net::CertificateList();
  }

  void SetAuthorityCertificates(
      const net::CertificateList& authority_certificates) {
    authority_certificates_ = authority_certificates;
  }

  void NotifyObservers() {
    // NetworkCertLoader does not use these parameters - it calls
    // |GetAllAuthorityCertificates| explicitly.
    net::CertificateList all_server_and_authority_certs;
    net::CertificateList trust_anchors;
    for (auto& observer : observer_list_) {
      observer.OnPolicyProvidedCertsChanged(all_server_and_authority_certs,
                                            trust_anchors);
    }
  }

 private:
  base::ObserverList<PolicyCertificateProvider::Observer,
                     true /* check_empty */>::Unchecked observer_list_;
  net::CertificateList authority_certificates_;
};

bool IsCertInCertificateList(CERTCertificate* cert,
                             const net::ScopedCERTCertificateList& cert_list) {
  for (const auto& cert_list_element : cert_list) {
    if (net::x509_util::IsSameCertificate(cert_list_element.get(), cert))
      return true;
  }
  return false;
}

bool IsCertInCertificateList(const net::X509Certificate* cert,
                             const net::ScopedCERTCertificateList& cert_list) {
  for (const auto& cert_list_element : cert_list) {
    if (net::x509_util::IsSameCertificate(cert_list_element.get(), cert))
      return true;
  }
  return false;
}

size_t CountCertOccurencesInCertificateList(
    CERTCertificate* cert,
    const net::ScopedCERTCertificateList& cert_list) {
  size_t count = 0;
  for (net::ScopedCERTCertificateList::const_iterator it = cert_list.begin();
       it != cert_list.end(); ++it) {
    if (net::x509_util::IsSameCertificate(it->get(), cert))
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
    ASSERT_TRUE(primary_db_.is_open());

    NetworkCertLoader::Initialize();
    cert_loader_ = NetworkCertLoader::Get();
    cert_loader_->AddObserver(this);
  }

  void TearDown() override {
    cert_loader_->RemoveObserver(this);
    NetworkCertLoader::Shutdown();
  }

 protected:
  void StartCertLoaderWithPrimaryDB() {
    CreateCertDatabase(&primary_db_, &primary_certdb_);
    cert_loader_->SetUserNSSDB(primary_certdb_.get());

    scoped_task_environment_.RunUntilIdle();
    GetAndResetCertificatesLoadedEventsCount();
  }

  // Starts the cert loader with a primary cert database which has access to the
  // system token.
  void StartCertLoaderWithPrimaryDBAndSystemToken() {
    CreateCertDatabase(&primary_db_, &primary_certdb_);
    AddSystemToken(primary_certdb_.get());
    cert_loader_->SetUserNSSDB(primary_certdb_.get());

    scoped_task_environment_.RunUntilIdle();
    GetAndResetCertificatesLoadedEventsCount();
  }

  // NetworkCertLoader::Observer:
  // The test keeps count of times the observer method was called.
  void OnCertificatesLoaded(
      const net::ScopedCERTCertificateList& cert_list) override {
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

  void CreateCertDatabase(crypto::ScopedTestNSSDB* db,
                          std::unique_ptr<TestNSSCertDatabase>* certdb) {
    ASSERT_TRUE(db->is_open());

    certdb->reset(new TestNSSCertDatabase(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(db->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(db->slot()))));
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

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  NetworkCertLoader* cert_loader_;

  // The user is primary as the one whose certificates NetworkCertLoader
  // handles, it has nothing to do with crypto::InitializeNSSForChromeOSUser
  // is_primary_user parameter (which is irrelevant for these tests).
  crypto::ScopedTestNSSDB primary_db_;
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

  CreateCertDatabase(&primary_db_, &primary_certdb_);
  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->all_certs().empty());
  EXPECT_TRUE(cert_loader_->system_token_client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Default CA cert roots should get loaded.
  EXPECT_FALSE(cert_loader_->all_certs().empty());
  EXPECT_TRUE(cert_loader_->system_token_client_certs().empty());
}

TEST_F(NetworkCertLoaderTest, BasicOnlySystemDB) {
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  CreateCertDatabase(&system_db_, &system_certdb_);
  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->all_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Default CA cert roots should get loaded.
  EXPECT_FALSE(cert_loader_->all_certs().empty());
}

// Tests the NetworkCertLoader with a system DB and then with an additional user
// DB which does not have access to the system token.
TEST_F(NetworkCertLoaderTest, SystemAndUnaffiliatedUserDB) {
  CreateCertDatabase(&system_db_, &system_certdb_);
  net::ScopedCERTCertificate system_token_cert(ImportClientCertAndKey(
      system_certdb_.get(), system_db_.slot(), TEST_CLIENT_CERT_1));
  ASSERT_TRUE(system_token_cert);

  CreateCertDatabase(&primary_db_, &primary_certdb_);
  net::ScopedCERTCertificate user_token_cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_db_.slot(), TEST_CLIENT_CERT_2));
  ASSERT_TRUE(user_token_cert);

  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->all_certs().empty());
  EXPECT_TRUE(cert_loader_->system_token_client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  EXPECT_TRUE(IsCertInCertificateList(
      system_token_cert.get(), cert_loader_->system_token_client_certs()));
  EXPECT_TRUE(IsCertInCertificateList(system_token_cert.get(),
                                      cert_loader_->all_certs()));

  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->all_certs().empty());
  EXPECT_FALSE(cert_loader_->system_token_client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  EXPECT_FALSE(IsCertInCertificateList(
      user_token_cert.get(), cert_loader_->system_token_client_certs()));
  EXPECT_TRUE(IsCertInCertificateList(user_token_cert.get(),
                                      cert_loader_->all_certs()));
}

// Tests the NetworkCertLoader with a system DB and then with an additional user
// DB which has access to the system token.
TEST_F(NetworkCertLoaderTest, SystemAndAffiliatedUserDB) {
  CreateCertDatabase(&system_db_, &system_certdb_);
  net::ScopedCERTCertificate system_token_cert(ImportClientCertAndKey(
      system_certdb_.get(), system_db_.slot(), TEST_CLIENT_CERT_1));
  ASSERT_TRUE(system_token_cert);

  CreateCertDatabase(&primary_db_, &primary_certdb_);
  net::ScopedCERTCertificate user_token_cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_db_.slot(), TEST_CLIENT_CERT_2));
  ASSERT_TRUE(user_token_cert);

  AddSystemToken(primary_certdb_.get());
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->SetSystemNSSDB(system_certdb_.get());

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->all_certs().empty());
  EXPECT_TRUE(cert_loader_->system_token_client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  EXPECT_TRUE(IsCertInCertificateList(
      system_token_cert.get(), cert_loader_->system_token_client_certs()));
  EXPECT_TRUE(IsCertInCertificateList(system_token_cert.get(),
                                      cert_loader_->all_certs()));

  cert_loader_->SetUserNSSDB(primary_certdb_.get());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->all_certs().empty());
  EXPECT_FALSE(cert_loader_->system_token_client_certs().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  EXPECT_FALSE(IsCertInCertificateList(
      user_token_cert.get(), cert_loader_->system_token_client_certs()));
  EXPECT_EQ(1U, CountCertOccurencesInCertificateList(
                    user_token_cert.get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdateCertListOnNewCert) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  // Certs are loaded asynchronously, so the new cert should not yet be in the
  // cert list.
  EXPECT_FALSE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->all_certs()));

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // The certificate list should be updated now, as the message loop's been run.
  EXPECT_TRUE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->all_certs()));

  EXPECT_FALSE(cert_loader_->IsCertificateHardwareBacked(certs[0].get()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateOnSecondaryDbChanges) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db, &secondary_certdb);

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", secondary_certdb.get(), &certs);

  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, ClientLoaderUpdateOnNewClientCert) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(primary_certdb_.get()));
  ASSERT_TRUE(cert);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, ClientLoaderUpdateOnNewClientCertInSystemToken) {
  StartCertLoaderWithPrimaryDBAndSystemToken();

  EXPECT_TRUE(cert_loader_->system_token_client_certs().empty());
  net::ScopedCERTCertificate cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_certdb_->GetSystemSlot().get()));
  ASSERT_TRUE(cert);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
  EXPECT_EQ(1U, cert_loader_->system_token_client_certs().size());
  EXPECT_TRUE(IsCertInCertificateList(
      cert.get(), cert_loader_->system_token_client_certs()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateOnNewClientCertInSecondaryDb) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db, &secondary_certdb);

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(secondary_certdb.get()));
  ASSERT_TRUE(cert);

  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdatedOnCertRemoval) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(primary_certdb_.get()));
  ASSERT_TRUE(cert);

  scoped_task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));

  primary_certdb_->DeleteCertAndKey(cert.get());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  ASSERT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdatedOnCACertTrustChange) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->all_certs()));

  // The value that should have been set by |ImportCACert|.
  ASSERT_EQ(net::NSSCertDatabase::TRUST_DEFAULT,
            primary_certdb_->GetCertTrust(certs[0].get(), net::CA_CERT));
  ASSERT_TRUE(primary_certdb_->SetCertTrust(certs[0].get(), net::CA_CERT,
                                            net::NSSCertDatabase::TRUSTED_SSL));

  // Cert trust change should trigger certificate reload in cert_loader_.
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  scoped_task_environment_.RunUntilIdle();
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
  cert_loader_->AddPolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // When policy changes, an update is triggered too.
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
  device_policy_certs_provider.SetAuthorityCertificates({cert});
  device_policy_certs_provider.NotifyObservers();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));

  // Removing the cert provider triggers an update.
  cert_loader_->RemovePolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
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

  // Adding a first policy certificate provider triggers an update. In this test
  // case, the device policy certs provider already contains a cert.
  cert_loader_->AddPolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(device_policy_cert.get(),
                                      cert_loader_->all_certs()));
  EXPECT_FALSE(IsCertInCertificateList(user_policy_cert.get(),
                                       cert_loader_->all_certs()));

  // Adding a second policy certificate provider triggers an update. In this
  // test case, the user policy certs provider already contains a cert.
  cert_loader_->AddPolicyCertificateProvider(&user_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_TRUE(IsCertInCertificateList(device_policy_cert.get(),
                                      cert_loader_->all_certs()));
  EXPECT_TRUE(IsCertInCertificateList(user_policy_cert.get(),
                                      cert_loader_->all_certs()));

  cert_loader_->RemovePolicyCertificateProvider(&user_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  cert_loader_->RemovePolicyCertificateProvider(&device_policy_certs_provider);
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
  cert_loader_->AddPolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());

  // Same when the policy changes.
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
  device_policy_certs_provider.SetAuthorityCertificates({cert});
  device_policy_certs_provider.NotifyObservers();
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());

  // After starting the NetworkCertLoader, the policy-provided cert is there.
  StartCertLoaderWithPrimaryDB();
  EXPECT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));

  // Removing the cert provider triggers an update.
  cert_loader_->RemovePolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->all_certs()));
}

TEST_F(NetworkCertLoaderTest, NoUpdateWhenShuttingDown) {
  StartCertLoaderWithPrimaryDB();

  FakePolicyCertificateProvider device_policy_certs_provider;

  // Setting the cert provider triggers an update.
  cert_loader_->AddPolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // Removing the cert provider does not trigger an update if the shutdown
  // procedure has started.
  cert_loader_->set_is_shutting_down();
  cert_loader_->RemovePolicyCertificateProvider(&device_policy_certs_provider);
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
}

}  // namespace chromeos

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_cert_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/server_certificate_database/server_certificate_database_service.h"
#include "components/server_certificate_database/server_certificate_database_test_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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
  }

  net::CertificateList GetCertificatesWithoutWebTrust(
      const chromeos::onc::CertificateScope& scope) const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
  }

  const std::set<std::string>& GetExtensionIdsWithPolicyCertificates()
      const override {
    // NetworkCertLoader does not call this.
    NOTREACHED();
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

size_t CountCertOccurencesInCertificateList(
    const net::X509Certificate* cert,
    const std::vector<NetworkCertLoader::NetworkCert>& network_cert_list) {
  size_t count = 0;
  for (const auto& network_cert : network_cert_list) {
    if (net::x509_util::IsSameCertificate(network_cert.cert(), cert)) {
      ++count;
    }
  }
  return count;
}

void CertDbMigrationNssSlotGetter(
    crypto::ScopedPK11Slot slot,
    base::OnceCallback<void(crypto::ScopedPK11Slot)> callback) {
  std::move(callback).Run(std::move(slot));
}

class TestNSSCertDatabase : public net::NSSCertDatabaseChromeOS {
 public:
  TestNSSCertDatabase(crypto::ScopedPK11Slot public_slot,
                      crypto::ScopedPK11Slot private_slot)
      : NSSCertDatabaseChromeOS(std::move(public_slot),
                                std::move(private_slot)) {}
  ~TestNSSCertDatabase() override = default;

  // Make this method visible in the public interface.
  void NotifyObserversClientCertStoreChanged() {
    NSSCertDatabaseChromeOS::NotifyObserversClientCertStoreChanged();
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
    ASSERT_TRUE(test_slot_for_migration_.is_open());
    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
    net::ServerCertificateDatabaseService::RegisterProfilePrefs(
        pref_service_.registry());

    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    cert_loader_ = NetworkCertLoader::Get();
    cert_loader_->AddObserver(this);
  }

  void TearDown() override {
    cert_loader_->RemoveObserver(this);
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
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
    if (wait_for_update_callback_) {
      std::move(wait_for_update_callback_).Run();
    } else {
      certificates_loaded_events_count_++;
    }
  }

  // Create a TestFuture to wait on the `OnCertificatesLoaded` observer to run.
  // The `certificates_loaded_events_count_` count will *not* be updated
  // when the future is completed. (The future being completed already
  // indicates the condition has been met, and if the count was incremented the
  // test would have to redundantly call
  // GetAndResetCertificatesLoadedEventsCount to reset the count.)
  // TODO(mattm): update the rest of the tests to use CreateUpdateWaiter.
  std::unique_ptr<base::test::TestFuture<void>> CreateUpdateWaiter() {
    if (wait_for_update_callback_) {
      ADD_FAILURE() << "CreateUpdateWaiter called while already waiting";
    }
    std::unique_ptr<base::test::TestFuture<void>> future =
        std::make_unique<base::test::TestFuture<void>>();
    wait_for_update_callback_ = future->GetCallback();
    return future;
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

  bool ImportCertDBCert(net::ServerCertificateDatabaseService& server_cert_db,
                        base::span<const uint8_t> der_cert,
                        chrome_browser_server_certificate_database::
                            CertificateTrust::CertificateTrustType trust_type) {
    base::test::TestFuture<bool> future;
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
    cert_infos.push_back(
        net::MakeCertInfo(base::as_string_view(der_cert), trust_type));
    server_cert_db.AddOrUpdateUserCertificates(std::move(cert_infos),
                                               future.GetCallback());
    return future.Take();
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
    database_to_notify->NotifyObserversClientCertStoreChanged();
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

  std::unique_ptr<net::ServerCertificateDatabaseService>
  CreateServerCertDbService() {
    return std::make_unique<net::ServerCertificateDatabaseService>(
        temp_profile_dir_.GetPath(), &pref_service_,
        base::BindOnce(&CertDbMigrationNssSlotGetter,
                       crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                           test_slot_for_migration_.slot()))));
  }

  base::test::TaskEnvironment task_environment_;

  raw_ptr<NetworkCertLoader, DanglingUntriaged> cert_loader_;

  // If non-null, will be called when an update notification is received from
  // `cert_loader_`.
  base::OnceClosure wait_for_update_callback_;

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

  // The NSS DB that will be passed to ServerCertificateDatabaseService for
  // migration. This is a separate slot from the `primary_public_slot_db_` to
  // simplify test setup. (For example, being able to test having different
  // certs in the `primary_public_slot_db_` and in the
  // ServerCertificateDatabase, without having the certs from the NSS slot
  // automatically getting migrated into the ServerCertificateDatabase.)
  crypto::ScopedTestNSSDB test_slot_for_migration_;

  base::ScopedTempDir temp_profile_dir_;
  base::ScopedTempDir temp_server_cert_db_dir_;
  TestingPrefServiceSimple pref_service_;

 private:
  size_t certificates_loaded_events_count_;
};

}  // namespace

TEST_F(NetworkCertLoaderTest, BasicOnlySystemDB) {
  EXPECT_FALSE(cert_loader_->can_have_client_certificates());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->MarkSystemNSSDBWillBeInitialized();
  EXPECT_TRUE(cert_loader_->can_have_client_certificates());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  CreateCertDatabase(&system_db_, nullptr /* private_slot_db */,
                     &system_certdb_);
  AddSystemToken(system_certdb_.get());
  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", system_certdb_.get(), &certs);
  task_environment_.RunUntilIdle();

  cert_loader_->SetSystemNssDbForTesting(system_certdb_.get());

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

  cert_loader_->SetSystemNssDbForTesting(system_certdb_.get());

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

  cert_loader_->SetSystemNssDbForTesting(system_certdb_.get());

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
// multiple sources.
TEST_F(NetworkCertLoaderTest, DeduplicatesCerts) {
  scoped_refptr<net::X509Certificate> user_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(user_cert.get());

  scoped_refptr<net::X509Certificate> db_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "intermediate_ca_cert.pem");
  ASSERT_TRUE(db_cert.get());

  scoped_refptr<net::X509Certificate> policy_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "multi-root-E-by-E.pem");
  ASSERT_TRUE(policy_cert.get());

  std::unique_ptr<net::ServerCertificateDatabaseService> server_cert_db =
      CreateServerCertDbService();

  EXPECT_TRUE(
      ImportCertDBCert(*server_cert_db, user_cert->cert_span(),
                       chrome_browser_server_certificate_database::
                           CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED));
  EXPECT_TRUE(
      ImportCertDBCert(*server_cert_db, db_cert->cert_span(),
                       chrome_browser_server_certificate_database::
                           CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED));

  StartCertLoaderWithPrimaryDB();
  cert_loader_->MarkUserServerCertDatabaseWillBeInitialized();

  auto update_waiter = CreateUpdateWaiter();
  cert_loader_->SetUserServerCertDatabaseService(server_cert_db.get());
  base::ScopedClosureRunner db_cleanup_runner(
      base::BindOnce(&NetworkCertLoader::SetUserServerCertDatabaseService,
                     base::Unretained(cert_loader_), nullptr));
  ASSERT_TRUE(update_waiter->Wait());

  update_waiter = CreateUpdateWaiter();
  FakePolicyCertificateProvider user_policy_certs_provider;
  user_policy_certs_provider.SetAuthorityCertificates({user_cert, policy_cert});
  cert_loader_->SetUserPolicyCertificateProvider(&user_policy_certs_provider);
  base::ScopedClosureRunner policy_cleanup_runner(
      base::BindOnce(&NetworkCertLoader::SetUserPolicyCertificateProvider,
                     base::Unretained(cert_loader_), nullptr));
  ASSERT_TRUE(update_waiter->Wait());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // The same cert is provided by both policy and ServerCertDatabase, but
  // should only appear in the authority_certs() list once.
  EXPECT_EQ(1U, CountCertOccurencesInCertificateList(
                    user_cert.get(), cert_loader_->authority_certs()));
  // Also check the unique certs that were added to the DB and the policy,
  // which ensures that the test setup is correct and both sources were
  // actually loaded.
  EXPECT_TRUE(IsCertInCertificateList(db_cert.get(), false /* device_wide */,
                                      cert_loader_->authority_certs()));
  EXPECT_TRUE(IsCertInCertificateList(policy_cert.get(),
                                      false /* device_wide */,
                                      cert_loader_->authority_certs()));
  EXPECT_EQ(3U, cert_loader_->authority_certs().size());
  EXPECT_TRUE(cert_loader_->client_certs().empty());
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

// In tests, the client certs are provided by NSS softoken, so they are not
// hardware-backed or available for network authentication.
TEST_F(NetworkCertLoaderTest,
       ClientCertNotHwBackedOrAvailableForNetworkAuthInTests) {
  StartCertLoaderWithPrimaryDB();

  net::ScopedCERTCertificate cert(
      ImportClientCertAndKey(primary_certdb_.get()));
  ASSERT_TRUE(cert);
  task_environment_.RunUntilIdle();

  const std::vector<NetworkCertLoader::NetworkCert>& client_certs =
      cert_loader_->client_certs();
  ASSERT_EQ(1U, client_certs.size());

  EXPECT_FALSE(client_certs[0].IsHardwareBacked());
  EXPECT_FALSE(client_certs[0].is_available_for_network_auth());
}

TEST_F(NetworkCertLoaderTest, ClientLoaderUpdateOnNewClientCertInSystemToken) {
  CreateCertDatabase(&system_db_ /* public_slot_db */,
                     nullptr /* private_slot_db */, &system_certdb_);
  AddSystemToken(system_certdb_.get());
  task_environment_.RunUntilIdle();

  cert_loader_->SetSystemNssDbForTesting(system_certdb_.get());
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

// Tests that device-wide certificates have higher priority when combining two
// policy provided certificates such that one of them is device-wide and the
// other is not.
TEST_F(NetworkCertLoaderTest, PreferDeviceWideCertsWhenCombining) {
  // Load same CA cert for device and user policy.
  scoped_refptr<net::X509Certificate> certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");

  ASSERT_TRUE(certificate.get());

  StartCertLoaderWithPrimaryDB();

  FakePolicyCertificateProvider device_policy_certs_provider;
  device_policy_certs_provider.SetAuthorityCertificates({certificate});

  FakePolicyCertificateProvider user_policy_certs_provider;
  user_policy_certs_provider.SetAuthorityCertificates({certificate});

  cert_loader_->SetDevicePolicyCertificateProvider(
      &device_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  cert_loader_->SetUserPolicyCertificateProvider(&user_policy_certs_provider);
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_FALSE(IsCertInCertificateList(certificate.get(),
                                       false /* device_wide */,
                                       cert_loader_->authority_certs()));
  EXPECT_TRUE(IsCertInCertificateList(certificate.get(), true /* device_wide */,
                                      cert_loader_->authority_certs()));

  cert_loader_->SetUserPolicyCertificateProvider(nullptr);
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  cert_loader_->SetDevicePolicyCertificateProvider(nullptr);
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
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

TEST_F(NetworkCertLoaderTest, BasicWithServerCertDB) {
  EXPECT_FALSE(cert_loader_->can_have_client_certificates());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  cert_loader_->MarkUserNSSDBWillBeInitialized();
  cert_loader_->MarkUserServerCertDatabaseWillBeInitialized();
  EXPECT_TRUE(cert_loader_->can_have_client_certificates());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());

  // Create and populate test NSS DB with both an authority cert and a client
  // cert.
  CreateCertDatabase(&primary_public_slot_db_, &primary_private_slot_db_,
                     &primary_certdb_);

  net::ScopedCERTCertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  net::ScopedCERTCertificate client_cert(ImportClientCertAndKey(
      primary_certdb_.get(), primary_private_slot_db_.slot(),
      TEST_CLIENT_CERT_2));
  ASSERT_TRUE(client_cert);

  // Create and populate test ServerCertificateDatabaseService with a different
  // authority cert.
  std::unique_ptr<net::ServerCertificateDatabaseService> server_cert_db =
      CreateServerCertDbService();
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  EXPECT_TRUE(
      ImportCertDBCert(*server_cert_db, base::as_byte_span(root->GetDER()),
                       chrome_browser_server_certificate_database::
                           CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED));

  // Start loading both databases.
  auto update_waiter = CreateUpdateWaiter();
  cert_loader_->SetUserNSSDB(primary_certdb_.get());
  cert_loader_->SetUserServerCertDatabaseService(server_cert_db.get());
  base::ScopedClosureRunner cleanup_runner(
      base::BindOnce(&NetworkCertLoader::SetUserServerCertDatabaseService,
                     base::Unretained(cert_loader_), nullptr));

  EXPECT_FALSE(cert_loader_->initial_load_finished());
  EXPECT_FALSE(cert_loader_->user_cert_database_load_finished());
  EXPECT_TRUE(cert_loader_->initial_load_of_any_database_running());
  EXPECT_TRUE(cert_loader_->authority_certs().empty());
  EXPECT_TRUE(cert_loader_->client_certs().empty());

  ASSERT_TRUE(update_waiter->Wait());

  EXPECT_TRUE(cert_loader_->initial_load_finished());
  EXPECT_TRUE(cert_loader_->user_cert_database_load_finished());
  EXPECT_FALSE(cert_loader_->initial_load_of_any_database_running());

  // Only the authority certificate from the ServerCertificateDatabaseService
  // should be in authority_certs, the NSS authority certs should not be used
  // when ServerCertificateDatabaseService is active.
  ASSERT_EQ(1U, cert_loader_->authority_certs().size());
  EXPECT_TRUE(net::x509_util::IsSameCertificate(
      cert_loader_->authority_certs()[0].cert(), root->GetCertBuffer()));

  // The client cert from NSS should still be imported.
  EXPECT_TRUE(IsCertInCertificateList(client_cert.get(),
                                      false /* device_wide */,
                                      cert_loader_->client_certs()));
}

TEST_F(NetworkCertLoaderTest, UpdateCertListOnServerCertDBChanges) {
  StartCertLoaderWithPrimaryDB();
  std::unique_ptr<net::ServerCertificateDatabaseService> server_cert_db =
      CreateServerCertDbService();
  base::ScopedClosureRunner cleanup_runner;
  {
    auto update_waiter = CreateUpdateWaiter();
    cert_loader_->SetUserServerCertDatabaseService(server_cert_db.get());
    cleanup_runner = base::ScopedClosureRunner(
        base::BindOnce(&NetworkCertLoader::SetUserServerCertDatabaseService,
                       base::Unretained(cert_loader_), nullptr));
    ASSERT_TRUE(update_waiter->Wait());
    // Initial loading should complete with no certs found.
    EXPECT_TRUE(cert_loader_->authority_certs().empty());
  }

  // Test adding a cert to ServerCertificateDatabase.
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  {
    auto update_waiter = CreateUpdateWaiter();
    EXPECT_TRUE(
        ImportCertDBCert(*server_cert_db, base::as_byte_span(root->GetDER()),
                         chrome_browser_server_certificate_database::
                             CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED));

    // Adding the new cert to the DB should have triggered an update of the
    // NetworkCertLoader and the new cert should be available.
    ASSERT_TRUE(update_waiter->Wait());
    ASSERT_EQ(1U, cert_loader_->authority_certs().size());
    EXPECT_TRUE(net::x509_util::IsSameCertificate(
        cert_loader_->authority_certs()[0].cert(), root->GetCertBuffer()));
  }

  // Test removing a cert from ServerCertificateDatabase.
  {
    std::string root_hash = net::ServerCertificateDatabase::CertInformation(
                                base::as_byte_span(root->GetDER()))
                                .sha256hash_hex;
    auto update_waiter = CreateUpdateWaiter();
    base::test::TestFuture<bool> future;
    server_cert_db->DeleteCertificate(root_hash, future.GetCallback());
    EXPECT_TRUE(future.Take());

    // Deleting the cert from the DB should have triggered an update of the
    // NetworkCertLoader and the cert should no longer be returned.
    ASSERT_TRUE(update_waiter->Wait());
    EXPECT_EQ(0U, cert_loader_->authority_certs().size());
  }
}

}  // namespace ash

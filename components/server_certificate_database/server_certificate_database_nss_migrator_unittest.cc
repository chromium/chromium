// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_nss_migrator.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/testing_pref_service.h"
#include "components/server_certificate_database/server_certificate_database.h"
#include "components/server_certificate_database/server_certificate_database.pb.h"
#include "components/server_certificate_database/server_certificate_database_service.h"
#include "components/server_certificate_database/server_certificate_database_test_util.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

void NssSlotGetter(crypto::ScopedPK11Slot slot,
                   base::OnceCallback<void(crypto::ScopedPK11Slot)> callback) {
  std::move(callback).Run(std::move(slot));
}

}  // namespace

class ServerCertificateDatabaseNSSMigratorTest : public testing::Test {
 public:
  void SetUp() override {
    ServerCertificateDatabaseService::RegisterProfilePrefs(
        pref_service_.registry());
    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
    test_nss_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();
    ASSERT_TRUE(test_nss_slot_->is_open());
    nss_cert_database_ = std::make_unique<NSSCertDatabase>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nss_slot_->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nss_slot_->slot())));
  }

  std::unique_ptr<net::ServerCertificateDatabaseService> CreateService() {
    return std::make_unique<ServerCertificateDatabaseService>(
        temp_profile_dir_.GetPath(), &pref_service_, CreateSlotGetter());
  }

  ServerCertificateDatabaseNSSMigrator::NssSlotGetter CreateSlotGetter() {
    return base::BindOnce(
        &NssSlotGetter,
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nss_slot_->slot())));
  }

  PrefService* pref_service() { return &pref_service_; }
  NSSCertDatabase* nss_cert_database() { return nss_cert_database_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_profile_dir_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<crypto::ScopedTestNSSDB> test_nss_slot_;
  std::unique_ptr<net::NSSCertDatabase> nss_cert_database_;
};

TEST_F(ServerCertificateDatabaseNSSMigratorTest, MigrateEmptyNssDb) {
  std::unique_ptr<net::ServerCertificateDatabaseService> cert_db_service =
      CreateService();
  ServerCertificateDatabaseNSSMigrator migrator(cert_db_service.get(),
                                                CreateSlotGetter());

  base::test::TestFuture<ServerCertificateDatabaseNSSMigrator::MigrationResult>
      migrate_certs_waiter;
  migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
  ServerCertificateDatabaseNSSMigrator::MigrationResult result =
      migrate_certs_waiter.Take();
  EXPECT_EQ(0, result.cert_count);
  EXPECT_EQ(0, result.error_count);

  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      get_certs_waiter;
  cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
      get_certs_waiter.Take();

  EXPECT_TRUE(cert_infos.empty());
}

TEST_F(ServerCertificateDatabaseNSSMigratorTest, MigrateCerts) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Import test certificates into NSS user database.
  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_cert_database()->ImportServerCert(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          leaf->GetX509Certificate().get()),
      NSSCertDatabase::DISTRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  EXPECT_TRUE(nss_cert_database()->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get()),
      NSSCertDatabase::TRUST_DEFAULT, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  EXPECT_TRUE(nss_cert_database()->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  // Import a client cert to NSS also. It shouldn't be migrated.
  EXPECT_TRUE(net::ImportClientCertAndKeyFromFile(
      net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
      nss_cert_database()->GetPublicSlot().get()));

  std::unique_ptr<net::ServerCertificateDatabaseService> cert_db_service =
      CreateService();
  // Do the migration from NSS to ServerCertificateDatabase.
  {
    ServerCertificateDatabaseNSSMigrator migrator(cert_db_service.get(),
                                                  CreateSlotGetter());
    base::test::TestFuture<
        ServerCertificateDatabaseNSSMigrator::MigrationResult>
        migrate_certs_waiter;
    migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
    ServerCertificateDatabaseNSSMigrator::MigrationResult result =
        migrate_certs_waiter.Take();
    EXPECT_EQ(3, result.cert_count);
    EXPECT_EQ(0, result.error_count);
  }

  // Test that the certs in the ServerCertificateDatabase match the expected
  // certs and trust settings, and that the client cert was not migrated.
  const ServerCertificateDatabase::CertInformation expected_leaf_info =
      MakeCertInfo(leaf->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
  const ServerCertificateDatabase::CertInformation expected_intermediate_info =
      MakeCertInfo(intermediate->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED);
  const ServerCertificateDatabase::CertInformation expected_root_info =
      MakeCertInfo(root->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_waiter;
    cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        get_certs_waiter.Take();
    EXPECT_THAT(cert_infos,
                UnorderedElementsAre(
                    CertInfoEquals(std::ref(expected_leaf_info)),
                    CertInfoEquals(std::ref(expected_intermediate_info)),
                    CertInfoEquals(std::ref(expected_root_info))));
  }

  // Run the migration again. This simulates what would happen if the migration
  // completed but the browser was closed before the pref could be updated to
  // record that migration had been done. The migration should run again,
  // replacing the entries in the ServerCertificateDatabase with exactly the
  // same value, so the end result should not change.
  {
    ServerCertificateDatabaseNSSMigrator migrator(cert_db_service.get(),
                                                  CreateSlotGetter());
    base::test::TestFuture<
        ServerCertificateDatabaseNSSMigrator::MigrationResult>
        migrate_certs_waiter;
    migrator.MigrateCerts(migrate_certs_waiter.GetCallback());
    ServerCertificateDatabaseNSSMigrator::MigrationResult result =
        migrate_certs_waiter.Take();
    EXPECT_EQ(3, result.cert_count);
    EXPECT_EQ(0, result.error_count);
  }

  // Test that the ServerCertificateDatabase still contains the expected certs
  // and nothing else.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        get_certs_waiter;
    cert_db_service->GetAllCertificates(get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        get_certs_waiter.Take();
    EXPECT_THAT(cert_infos,
                UnorderedElementsAre(
                    CertInfoEquals(std::ref(expected_leaf_info)),
                    CertInfoEquals(std::ref(expected_intermediate_info)),
                    CertInfoEquals(std::ref(expected_root_info))));
  }
}

}  // namespace net

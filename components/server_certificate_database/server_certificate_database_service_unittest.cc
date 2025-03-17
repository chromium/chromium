// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_service.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/server_certificate_database/server_certificate_database_test_util.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#endif

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

namespace net {
namespace {

#if BUILDFLAG(IS_CHROMEOS)
void NssSlotGetter(crypto::ScopedPK11Slot slot,
                   base::OnceCallback<void(crypto::ScopedPK11Slot)> callback) {
  std::move(callback).Run(std::move(slot));
}
#endif

}  // namespace

class ServerCertificateDatabaseServiceTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_CHROMEOS)
    ServerCertificateDatabaseService::RegisterProfilePrefs(
        pref_service_.registry());
    test_nss_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();
    ASSERT_TRUE(test_nss_slot_->is_open());
    nss_cert_database_ = std::make_unique<NSSCertDatabase>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nss_slot_->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nss_slot_->slot())));
#endif
  }

  std::unique_ptr<net::ServerCertificateDatabaseService> CreateService() {
#if BUILDFLAG(IS_CHROMEOS)
    return std::make_unique<ServerCertificateDatabaseService>(
        temp_profile_dir_.GetPath(), &pref_service_,
        base::BindOnce(&NssSlotGetter,
                       crypto::ScopedPK11Slot(
                           PK11_ReferenceSlot(test_nss_slot_->slot()))));
#else
    return std::make_unique<ServerCertificateDatabaseService>(
        temp_profile_dir_.GetPath());
#endif
  }

#if BUILDFLAG(IS_CHROMEOS)
  PrefService* pref_service() { return &pref_service_; }
  NSSCertDatabase* nss_cert_database() { return nss_cert_database_.get(); }
#endif

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_profile_dir_;
#if BUILDFLAG(IS_CHROMEOS)
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<crypto::ScopedTestNSSDB> test_nss_slot_;
  std::unique_ptr<net::NSSCertDatabase> nss_cert_database_;
#endif
};

TEST_F(ServerCertificateDatabaseServiceTest, TestNotifications) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  std::unique_ptr<net::ServerCertificateDatabaseService> cert_db_service =
      CreateService();

  base::test::TestFuture<void> update_waiter;

  auto scoped_observer_subscription =
      cert_db_service->AddObserver(update_waiter.GetRepeatingCallback());

  // Insert a new cert.
  {
    base::test::TestFuture<bool> insert_waiter;
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
    cert_infos.push_back(MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED));
    cert_db_service->AddOrUpdateUserCertificates(std::move(cert_infos),
                                                 insert_waiter.GetCallback());
    // Insert should be successful.
    EXPECT_TRUE(insert_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Update metadata for existing cert.
  {
    base::test::TestFuture<bool> insert_waiter;
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
    cert_infos.push_back(MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED));
    cert_db_service->AddOrUpdateUserCertificates(std::move(cert_infos),
                                                 insert_waiter.GetCallback());
    // Update should be successful.
    EXPECT_TRUE(insert_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Delete a cert.
  {
    base::test::TestFuture<bool> delete_waiter;
    auto cert_info = MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    cert_db_service->DeleteCertificate(cert_info.sha256hash_hex,
                                       delete_waiter.GetCallback());
    // Delete should be successful.
    EXPECT_TRUE(delete_waiter.Take());
  }
  // Observer notification should have been delivered.
  EXPECT_TRUE(update_waiter.WaitAndClear());

  // Try to delete a cert that doesn't exist.
  {
    base::test::TestFuture<bool> delete_waiter;
    auto cert_info = MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    cert_db_service->DeleteCertificate(cert_info.sha256hash_hex,
                                       delete_waiter.GetCallback());
    // Delete should fail since the cert doesn't exist in the database.
    EXPECT_FALSE(delete_waiter.Take());
  }
  // Observer notification should not be delivered since nothing was actually
  // changed.
  EXPECT_FALSE(update_waiter.IsReady());
}

#if BUILDFLAG(IS_CHROMEOS)
using ServerCertificateDatabaseServiceNSSMigratorTest =
    ServerCertificateDatabaseServiceTest;

TEST_F(ServerCertificateDatabaseServiceNSSMigratorTest, TestMigration) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Import test certificate into NSS user database.
  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_cert_database()->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  std::unique_ptr<net::ServerCertificateDatabaseService> cert_db_service =
      CreateService();

  // Verify that server cert database starts empty and migration pref default
  // is false.
  {
    base::test::TestFuture<uint32_t> get_certs_count_waiter;
    cert_db_service->GetCertificatesCount(get_certs_count_waiter.GetCallback());
    EXPECT_EQ(0U, get_certs_count_waiter.Get());
    EXPECT_EQ(
        pref_service()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
        static_cast<int>(ServerCertificateDatabaseService::
                             NSSMigrationResultPref::kNotMigrated));
  }

  // Call GetAllCertificates to begin the migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificates(
        migrate_and_get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        migrate_and_get_certs_waiter.Take();

    // Test that the the result includes the migrated cert.
    ServerCertificateDatabase::CertInformation expected_nss_root_info =
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);
    EXPECT_THAT(
        cert_infos,
        UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));
    // Migration pref should be true now.
    EXPECT_EQ(
        pref_service()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
        static_cast<int>(ServerCertificateDatabaseService::
                             NSSMigrationResultPref::kMigratedSuccessfully));
  }

  // Change the settings of the cert that was imported.
  {
    base::test::TestFuture<bool> update_cert_waiter;
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
    cert_infos.push_back(MakeCertInfo(
        root->GetDER(), CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED));
    cert_db_service->AddOrUpdateUserCertificates(
        std::move(cert_infos), update_cert_waiter.GetCallback());
    EXPECT_TRUE(update_cert_waiter.Take());
  }

  // Call GetAllCertificates again. Since the migration already completed, this
  // should just get the current contents of the database without re-doing the
  // migration.
  {
    base::test::TestFuture<
        std::vector<net::ServerCertificateDatabase::CertInformation>>
        migrate_and_get_certs_waiter;
    cert_db_service->GetAllCertificates(
        migrate_and_get_certs_waiter.GetCallback());
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos =
        migrate_and_get_certs_waiter.Take();

    // Test that the the result still includes the modified cert data and hasn't
    // been overwritten by the NSS settings (which is what would happen if the
    // migration was repeated).
    ServerCertificateDatabase::CertInformation expected_modified_root_info =
        MakeCertInfo(root->GetDER(),
                     CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED);
    EXPECT_THAT(cert_infos, UnorderedElementsAre(CertInfoEquals(
                                std::ref(expected_modified_root_info))));
  }
}

TEST_F(ServerCertificateDatabaseServiceNSSMigratorTest, SimultaneousCalls) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Import test certificate into NSS user database.
  NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(nss_cert_database()->ImportCACerts(
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get()),
      NSSCertDatabase::TRUSTED_SSL, &not_imported));
  EXPECT_TRUE(not_imported.empty());

  std::unique_ptr<net::ServerCertificateDatabaseService> cert_db_service =
      CreateService();

  // Call GetAllCertificates multiple times.
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter1;
  base::test::TestFuture<
      std::vector<net::ServerCertificateDatabase::CertInformation>>
      waiter2;
  cert_db_service->GetAllCertificates(waiter1.GetCallback());
  cert_db_service->GetAllCertificates(waiter2.GetCallback());
  // Migration pref should be false still.
  EXPECT_EQ(pref_service()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
            static_cast<int>(ServerCertificateDatabaseService::
                                 NSSMigrationResultPref::kNotMigrated));

  // Both callbacks should get run and both should have the migrated cert.
  ServerCertificateDatabase::CertInformation expected_nss_root_info =
      MakeCertInfo(root->GetDER(),
                   CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED);

  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos1 =
      waiter1.Take();
  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos2 =
      waiter2.Take();

  EXPECT_THAT(
      cert_infos1,
      UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));
  EXPECT_THAT(
      cert_infos2,
      UnorderedElementsAre(CertInfoEquals(std::ref(expected_nss_root_info))));

  // Migration pref should be true now.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kNSSCertsMigratedToServerCertDb),
      static_cast<int>(ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kMigratedSuccessfully));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace net

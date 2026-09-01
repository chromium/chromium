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

using chrome_browser_server_certificate_database::CertificateTrust;
using ::testing::UnorderedElementsAre;

namespace net {

class ServerCertificateDatabaseServiceTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  }

  std::unique_ptr<net::ServerCertificateDatabaseService> CreateService() {
    return std::make_unique<ServerCertificateDatabaseService>(
        temp_profile_dir_.GetPath());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_profile_dir_;
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

}  // namespace net

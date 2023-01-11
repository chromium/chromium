// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/system_token_cert_db_storage.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage_test_util.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class SystemTokenCertDbStorageTest : public testing::Test {
 public:
  SystemTokenCertDbStorageTest()
      : test_nssdb_(std::make_unique<crypto::ScopedTestNSSDB>()) {
    SystemTokenCertDbStorage::Initialize();
  }

  SystemTokenCertDbStorageTest(const SystemTokenCertDbStorageTest& other) =
      delete;
  SystemTokenCertDbStorageTest& operator=(
      const SystemTokenCertDbStorageTest& other) = delete;

  ~SystemTokenCertDbStorageTest() override {
    SystemTokenCertDbStorage::Shutdown();
  }

  void SetUp() override { ASSERT_TRUE(SystemTokenCertDbStorage::Get()); }

 protected:
  void SetSystemSlotInStorage() {
    test_cert_database_ = std::make_unique<net::NSSCertDatabase>(
        /*public_slot=*/crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_->slot())),
        /*private_slot=*/crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_->slot())));
    SystemTokenCertDbStorage::Get()->SetDatabase(test_cert_database_.get());
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<crypto::ScopedTestNSSDB> test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> test_cert_database_;
};

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbStorage if it was available in less than
// 5 minutes after being requested.
TEST_F(SystemTokenCertDbStorageTest, GetDatabaseSuccess) {
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDbStorage is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::Minutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);

  SetSystemSlotInStorage();

  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbInitializer if it was available in less
// than 5 minutes after being requested even if the slot was available after
// more than 5 minutes from the initialization of
// SystemTokenCertDbInitializer.
TEST_F(SystemTokenCertDbStorageTest, GetDatabaseLateRequestSuccess) {
  // Simulate waiting for 6 minutes after the initialization of the
  // SystemTokenCertDbStorage.
  const auto kSixMinuteDelay = base::Minutes(6);
  EXPECT_GT(kSixMinuteDelay,
            SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay);
  task_environment()->FastForwardBy(kSixMinuteDelay);

  SetSystemSlotInStorage();

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database retrieval will fail if the
// system token initialization doesn't succeed within 5 minutes from the first
// database request.
TEST_F(SystemTokenCertDbStorageTest, GetDatabaseTimeout) {
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  const auto kDelay1 = base::Minutes(2);
  EXPECT_LT(kDelay1, SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay);

  const auto kDelay2 =
      SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay - kDelay1;

  task_environment()->FastForwardBy(kDelay1);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  task_environment()->FastForwardBy(kDelay2);
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that if one of the system token certificate database requests timed
// out, following requests will fail as well.
TEST_F(SystemTokenCertDbStorageTest, GetDatabaseTimeoutMultipleRequests) {
  GetSystemTokenCertDbCallbackWrapper
      get_system_token_cert_db_callback_wrapper_1;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper_1.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper_1.IsCallbackCalled());

  task_environment()->FastForwardBy(
      SystemTokenCertDbStorage::kMaxCertDbRetrievalDelay);
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper_1.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper_1.IsDbRetrievalSucceeded());

  GetSystemTokenCertDbCallbackWrapper
      get_system_token_cert_db_callback_wrapper_2;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper_2.GetCallback());
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper_2.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper_2.IsDbRetrievalSucceeded());
}

// Tests that calling ResetDatabase notifies the observers.
TEST_F(SystemTokenCertDbStorageTest, ResetDatabaseNotifiesObservers) {
  // Successfully request the database.
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  SetSystemSlotInStorage();
  get_system_token_cert_db_callback_wrapper.Wait();
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // When the database is reset, observers are notified about this.
  FakeSystemTokenCertDbStorageObserver observer;
  SystemTokenCertDbStorage::Get()->AddObserver(&observer);

  SystemTokenCertDbStorage::Get()->ResetDatabase();

  EXPECT_TRUE(observer.HasBeenNotified());
  SystemTokenCertDbStorage::Get()->RemoveObserver(&observer);
}

// Tests that requesting a database after it has been reset fails.
TEST_F(SystemTokenCertDbStorageTest, RequestingDatabaseFailsAfterReset) {
  // Successfully request the database.
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  SetSystemSlotInStorage();
  get_system_token_cert_db_callback_wrapper.Wait();
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Now reset the database.
  SystemTokenCertDbStorage::Get()->ResetDatabase();

  // Requesting the database after that results in a failure.
  GetSystemTokenCertDbCallbackWrapper
      get_system_token_cert_db_callback_wrapper_2;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper_2.GetCallback());
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper_2.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper_2.IsDbRetrievalSucceeded());
}

}  // namespace ash

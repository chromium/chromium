// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager_impl.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::NiceMock;
using testing::Return;

class MockEntitySuppressionManagerObserver
    : public EntitySuppressionManager::Observer {
 public:
  MOCK_METHOD(void, OnEntitySuppressionsChanged, (), (override));
};

class EntitySuppressionManagerImplTest : public testing::Test {
 public:
  EntitySuppressionManagerImplTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}
  ~EntitySuppressionManagerImplTest() override = default;

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    auto bridge = std::make_unique<EntitySuppressionSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        encryptor_);
    ASSERT_TRUE(base::test::RunUntil([&]() { return bridge->IsLoaded(); }));
    manager_ =
        std::make_unique<EntitySuppressionManagerImpl>(std::move(bridge));
  }

 protected:
  EntitySuppressionManagerImpl& manager() { return *manager_; }
  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<EntitySuppressionManagerImpl> manager_;
};

// Tests that a new entity instance is initially not suppressed.
TEST_F(EntitySuppressionManagerImplTest, InitiallyNotSuppressed) {
  EntityInstance passport = test::GetPassportEntityInstance();

  EXPECT_FALSE(manager().IsSuppressed(passport));
}

// Tests that suppressing an entity instance marks it as suppressed.
TEST_F(EntitySuppressionManagerImplTest, SuppressEntityMarksAsSuppressed) {
  EntityInstance passport = test::GetPassportEntityInstance();

  EXPECT_TRUE(manager().SuppressEntity(passport));
  EXPECT_TRUE(manager().IsSuppressed(passport));
}

// Tests that re-suppressing an already suppressed entity returns false.
TEST_F(EntitySuppressionManagerImplTest, DuplicateSuppressReturnsFalse) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(manager().SuppressEntity(passport));

  EXPECT_FALSE(manager().SuppressEntity(passport));
}

// Tests that an entity is not suppressed if no merge constraints are satisfied.
TEST_F(EntitySuppressionManagerImplTest,
       IsNotSuppressedIfNoMergeConstraintsSatisfied) {
  // Passport requires either {number} or {name, country}. Setting only {name}
  // leaves no constraint satisfied.
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Alice",
                                  .number = nullptr,
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});

  EXPECT_FALSE(manager().SuppressEntity(passport));
  EXPECT_FALSE(manager().IsSuppressed(passport));
}

// Tests unsuppressing a previously suppressed entity instance.
TEST_F(EntitySuppressionManagerImplTest, UnsuppressEntity) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(manager().SuppressEntity(passport));
  ASSERT_TRUE(manager().IsSuppressed(passport));

  EXPECT_TRUE(manager().UnsuppressEntity(passport));

  EXPECT_FALSE(manager().IsSuppressed(passport));
}

// Tests that entities matching satisfied merge constraints are recognized as
// suppressed.
TEST_F(EntitySuppressionManagerImplTest, SuppressedIfConstraintMatches) {
  EntityInstance passport1 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"BOB", .number = u"P12345", .country = u"US"});
  EntityInstance passport2 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"B0B", .number = u"P12345", .country = u"US"});
  ASSERT_TRUE(manager().SuppressEntity(passport1));

  EXPECT_TRUE(manager().IsSuppressed(passport2));
}

// Tests that suppressing an entity does not suppress another entity when merge
// constraints differ.
TEST_F(EntitySuppressionManagerImplTest, NotSuppressedIfConstraintsDiffer) {
  EntityInstance passport1 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"Alice", .number = u"P12345", .country = u"US"});
  EntityInstance passport2 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"Bob", .number = u"P67890", .country = u"CA"});
  ASSERT_TRUE(manager().SuppressEntity(passport1));

  EXPECT_FALSE(manager().IsSuppressed(passport2));
}

// Tests that a masked entity can be suppressed.
TEST_F(EntitySuppressionManagerImplTest, MaskedEntitySuppression) {
  EntityInstance passport = test::MaskEntityInstance(
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = nullptr,
          .number = u"LR1234567",
          .country = nullptr,
          .record_type = EntityInstance::RecordType::kServerWallet}));

  EXPECT_TRUE(manager().SuppressEntity(passport));
  EXPECT_TRUE(manager().IsSuppressed(passport));
}

// Tests that entities matching different attribute types with identical values
// do not falsely match.
TEST_F(EntitySuppressionManagerImplTest,
       DoesNotSuppressDifferentAttributeTypesWithSameValue) {
  EntityInstance vehicle1 = test::GetVehicleEntityInstance(
      test::VehicleOptions{.plate = u"12345", .number = nullptr});
  EntityInstance vehicle2 = test::GetVehicleEntityInstance(
      test::VehicleOptions{.plate = nullptr, .number = u"12345"});

  ASSERT_TRUE(manager().SuppressEntity(vehicle1));

  EXPECT_FALSE(manager().IsSuppressed(vehicle2));
}

// Tests that an entity is considered suppressed if any of its satisfied merge
// constraints matches a suppressed entry.
TEST_F(EntitySuppressionManagerImplTest, SuppressedIfAnyConstraintMatches) {
  // Vehicle has two separate merge constraints: [Plate number] and [VIN].
  EntityInstance vehicle_to_suppress =
      test::GetVehicleEntityInstance(test::VehicleOptions{
          .plate = u"PLATE123",
          .number = u"VIN123",
      });
  EntityInstance vehicle_matching_plate =
      test::GetVehicleEntityInstance(test::VehicleOptions{
          .plate = u"PLATE123",
          .number = u"Different VIN",
      });
  EntityInstance vehicle_matching_vin =
      test::GetVehicleEntityInstance(test::VehicleOptions{
          .plate = u"Different Plate",
          .number = u"VIN123",
      });

  ASSERT_TRUE(manager().SuppressEntity(vehicle_to_suppress));

  EXPECT_TRUE(manager().IsSuppressed(vehicle_matching_plate));
  EXPECT_TRUE(manager().IsSuppressed(vehicle_matching_vin));
}

// Tests that observers are notified when an entity is successfully suppressed.
TEST_F(EntitySuppressionManagerImplTest,
       SuppressEntity_NotifiesObserversOnSuccess) {
  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EntityInstance passport = test::GetPassportEntityInstance();
  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(1);
  EXPECT_TRUE(manager().SuppressEntity(passport));
}

// Tests that observers are not notified when suppressing an entity that is
// already suppressed.
TEST_F(EntitySuppressionManagerImplTest,
       SuppressEntity_DoesNotNotifyObserversIfAlreadySuppressed) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(manager().SuppressEntity(passport));

  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(0);
  EXPECT_FALSE(manager().SuppressEntity(passport));
}

// Tests that observers are notified when an entity is successfully
// unsuppressed.
TEST_F(EntitySuppressionManagerImplTest,
       UnsuppressEntity_NotifiesObserversOnSuccess) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(manager().SuppressEntity(passport));

  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(1);
  EXPECT_TRUE(manager().UnsuppressEntity(passport));
}

// Tests that observers are not notified when unsuppressing an entity that is
// not suppressed.
TEST_F(EntitySuppressionManagerImplTest,
       UnsuppressEntity_DoesNotNotifyObserversIfNotSuppressed) {
  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EntityInstance passport = test::GetPassportEntityInstance();
  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(0);
  EXPECT_FALSE(manager().UnsuppressEntity(passport));
}

// Tests that remote sync or bridge-triggered updates notify manager observers.
TEST_F(EntitySuppressionManagerImplTest,
       SyncBridgeObserverNotifiesManagerObservers) {
  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(1);
  manager().OnSuppressionsChanged();
}

// Tests that GetSyncControllerDelegate forwards to the sync bridge.
TEST_F(EntitySuppressionManagerImplTest, GetSyncControllerDelegate) {
  EXPECT_CALL(mock_processor(), GetControllerDelegate());
  manager().GetSyncControllerDelegate();
}

// Tests that ClearAllSuppressions removes all suppressed entities and notifies
// observers.
TEST_F(EntitySuppressionManagerImplTest, ClearAllSuppressions) {
  EntityInstance passport = test::GetPassportEntityInstance();
  EntityInstance vehicle = test::GetVehicleEntityInstance();
  ASSERT_TRUE(manager().SuppressEntity(passport));
  ASSERT_TRUE(manager().SuppressEntity(vehicle));

  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(1);
  EXPECT_TRUE(manager().ClearAllSuppressions());

  EXPECT_FALSE(manager().IsSuppressed(passport));
  EXPECT_FALSE(manager().IsSuppressed(vehicle));
}

// Tests that ClearAllSuppressions returns false and does not notify observers
// when nothing is suppressed.
TEST_F(EntitySuppressionManagerImplTest, ClearAllSuppressions_Empty) {
  MockEntitySuppressionManagerObserver observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, OnEntitySuppressionsChanged()).Times(0);
  EXPECT_FALSE(manager().ClearAllSuppressions());
}

}  // namespace
}  // namespace autofill

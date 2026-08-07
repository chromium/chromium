// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

class InMemoryEntitySuppressionManagerTest : public testing::Test {
 public:
  InMemoryEntitySuppressionManagerTest() = default;
  ~InMemoryEntitySuppressionManagerTest() override = default;

 protected:
  InMemoryEntitySuppressionManager suppression_manager_;
};

// Tests that a new entity instance is initially not suppressed.
TEST_F(InMemoryEntitySuppressionManagerTest, InitiallyNotSuppressed) {
  EntityInstance passport = test::GetPassportEntityInstance();

  EXPECT_FALSE(suppression_manager_.IsSuppressed(passport));
}

// Tests that suppressing an entity instance marks it as suppressed.
TEST_F(InMemoryEntitySuppressionManagerTest, SuppressEntityMarksAsSuppressed) {
  EntityInstance passport = test::GetPassportEntityInstance();

  EXPECT_TRUE(suppression_manager_.SuppressEntity(passport));
  EXPECT_TRUE(suppression_manager_.IsSuppressed(passport));
}

// Tests that re-suppressing an already suppressed entity returns false.
TEST_F(InMemoryEntitySuppressionManagerTest, DuplicateSuppressReturnsFalse) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(suppression_manager_.SuppressEntity(passport));

  EXPECT_FALSE(suppression_manager_.SuppressEntity(passport));
}

// Tests that an entity is not suppressed if no merge constraints are satisfied.
TEST_F(InMemoryEntitySuppressionManagerTest,
       IsNotSuppressedIfNoMergeConstraintsSatisfied) {
  // Passport requires either {number} or {name, country}. Setting only {name}
  // leaves no constraint satisfied.
  EntityInstance passport = test::GetPassportEntityInstance(
      test::PassportEntityOptions{.name = u"Alice",
                                  .number = nullptr,
                                  .country = nullptr,
                                  .expiry_date = nullptr,
                                  .issue_date = nullptr});

  EXPECT_FALSE(suppression_manager_.SuppressEntity(passport));
  EXPECT_FALSE(suppression_manager_.IsSuppressed(passport));
}

// Tests unsuppressing a previously suppressed entity instance.
TEST_F(InMemoryEntitySuppressionManagerTest, UnsuppressEntity) {
  EntityInstance passport = test::GetPassportEntityInstance();
  ASSERT_TRUE(suppression_manager_.SuppressEntity(passport));
  ASSERT_TRUE(suppression_manager_.IsSuppressed(passport));

  EXPECT_TRUE(suppression_manager_.UnsuppressEntity(passport));

  EXPECT_FALSE(suppression_manager_.IsSuppressed(passport));
}

// Tests that entities matching satisfied merge constraints are recognized as
// suppressed.
TEST_F(InMemoryEntitySuppressionManagerTest, SuppressedIfConstraintMatches) {
  EntityInstance passport1 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"BOB", .number = u"P12345", .country = u"US"});
  EntityInstance passport2 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"B0B", .number = u"P12345", .country = u"US"});
  ASSERT_TRUE(suppression_manager_.SuppressEntity(passport1));

  EXPECT_TRUE(suppression_manager_.IsSuppressed(passport2));
}

// Tests that suppressing an entity does not suppress another entity when merge
// constraints differ.
TEST_F(InMemoryEntitySuppressionManagerTest, NotSuppressedIfConstraintsDiffer) {
  EntityInstance passport1 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"Alice", .number = u"P12345", .country = u"US"});
  EntityInstance passport2 =
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = u"Bob", .number = u"P67890", .country = u"CA"});
  ASSERT_TRUE(suppression_manager_.SuppressEntity(passport1));

  EXPECT_FALSE(suppression_manager_.IsSuppressed(passport2));
}

// Tests that a masked entity can be suppressed.
TEST_F(InMemoryEntitySuppressionManagerTest, MaskedEntitySuppression) {
  EntityInstance passport = test::MaskEntityInstance(
      test::GetPassportEntityInstance(test::PassportEntityOptions{
          .name = nullptr,
          .number = u"LR1234567",
          .country = nullptr,
          .record_type = EntityInstance::RecordType::kServerWallet}));

  EXPECT_TRUE(suppression_manager_.SuppressEntity(passport));
  EXPECT_TRUE(suppression_manager_.IsSuppressed(passport));
}

// Tests that entities matching different attribute types with identical values
// do not falsely match.
TEST_F(InMemoryEntitySuppressionManagerTest,
       DoesNotSuppressDifferentAttributeTypesWithSameValue) {
  EntityInstance vehicle1 = test::GetVehicleEntityInstance(
      test::VehicleOptions{.plate = u"12345", .number = nullptr});
  EntityInstance vehicle2 = test::GetVehicleEntityInstance(
      test::VehicleOptions{.plate = nullptr, .number = u"12345"});

  ASSERT_TRUE(suppression_manager_.SuppressEntity(vehicle1));

  EXPECT_FALSE(suppression_manager_.IsSuppressed(vehicle2));
}

}  // namespace
}  // namespace autofill

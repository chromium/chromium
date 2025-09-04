// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/virtual_card_enrollment_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_database_integrator_test_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class VirtualCardEnrollmentStrikeDatabaseTest : public ::testing::Test {
 public:
  VirtualCardEnrollmentStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ =
        std::make_unique<strike_database::StrikeDatabase>(db_provider_.get(),
                                                          temp_dir_.GetPath());

    strike_database_ = std::make_unique<VirtualCardEnrollmentStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of |strike_database_service_|'s components is posted
    // to a task runner, requires running the loop to complete.
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<strike_database::StrikeDatabase> strike_database_service_;
  std::unique_ptr<VirtualCardEnrollmentStrikeDatabase> strike_database_;
};

// Test to ensure that IsLastOffer works correctly.
TEST_F(VirtualCardEnrollmentStrikeDatabaseTest, IsLastOffer) {
  int max_strikes = strike_database_->GetMaxStrikesLimit();
  std::string instrument_id = "123";
  ASSERT_EQ(strike_database_->GetStrikes(instrument_id), 0);

  // Adds one strike and check IsLastOffer.
  strike_database_->AddStrike(instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 1);
  EXPECT_FALSE(strike_database_->IsLastOffer(instrument_id));

  // Removes existing strikes.
  strike_database_->RemoveStrike(instrument_id);
  ASSERT_EQ(strike_database_->GetStrikes(instrument_id), 0);

  // Adds |max_strikes - 1| strikes to the strike database and check
  // IsLastOffer.
  strike_database_->AddStrikes(max_strikes - 1, instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), max_strikes - 1);
  EXPECT_TRUE(strike_database_->IsLastOffer(instrument_id));
}

// Parameterized test that tests all arms for vcn strike optimization
// experiment with corresponding values of `kAutofillVcnEnrollStrikeExpiryTime`
// and `kAutofillVcnEnrollStrikeExpiryTimeDays` flags.
// Params of the VirtualCardEnrollmentStrikeOptimizationTest:
// -- bool is experiment enabled;
// -- bool value of `kAutofillVcnEnrollStrikeExpiryTimeDays`;
class VirtualCardEnrollmentStrikeOptimizationTest
    : public VirtualCardEnrollmentStrikeDatabaseTest,
      public testing::WithParamInterface<std::tuple<bool, int>> {
 public:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    if (IsVcnStrikeOptimizationExperimentEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutofillVcnEnrollStrikeExpiryTime,
          {{features::kAutofillVcnEnrollStrikeExpiryTimeDays.name,
            base::NumberToString(std::get<1>(GetParam()))}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillVcnEnrollStrikeExpiryTime);
    }

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ =
        std::make_unique<strike_database::StrikeDatabase>(db_provider_.get(),
                                                          temp_dir_.GetPath());

    strike_database_ = std::make_unique<VirtualCardEnrollmentStrikeDatabase>(
        strike_database_service_.get());
  }

  bool IsVcnStrikeOptimizationExperimentEnabled() {
    return std::get<0>(GetParam());
  }

  int GetExpectedStrikeExpiryTimeInDays() {
    // If experiment is not enabled, expected strike time should be set as
    // default i.e. 180 days.
    return IsVcnStrikeOptimizationExperimentEnabled()
               ? std::get<1>(GetParam())
               : VirtualCardEnrollmentStrikeDatabaseTraits::kExpiryTimeDelta
                     .InDays();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test to ensure that Strike optimization experiment is working as intended.
TEST_P(VirtualCardEnrollmentStrikeOptimizationTest, GetExpiryTimeDelta) {
  base::TimeDelta expiry_time_delta =
      strike_database_->GetExpiryTimeDelta().value();

  EXPECT_EQ(expiry_time_delta.InDays(), GetExpectedStrikeExpiryTimeInDays());
}

INSTANTIATE_TEST_SUITE_P(,
                         VirtualCardEnrollmentStrikeOptimizationTest,
                         testing::ValuesIn({
                             std::make_tuple(true, 120),
                             std::make_tuple(true, 60),
                             std::make_tuple(true, 30),
                             std::make_tuple(false, 0),
                         }));

}  // namespace

}  // namespace autofill

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_metrics_id_allocator.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_prefs.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountMetricsIdAllocatorTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    SigninPrefs::RegisterProfilePrefs(pref_service_->registry());
    signin_prefs_ = std::make_unique<SigninPrefs>(*pref_service_);
  }

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<SigninPrefs> signin_prefs_;
};

TEST_F(AccountMetricsIdAllocatorTest, SequentialAllocation) {
  for (int i = 0; i <= 99; ++i) {
    GaiaId gaia_id("gaia_" + base::NumberToString(i));
    std::optional<int> id =
        GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id.value(), i);
  }
}

TEST_F(AccountMetricsIdAllocatorTest, IdempotentAllocation) {
  GaiaId gaia_id("gaia_1");
  std::optional<int> id1 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
  ASSERT_TRUE(id1.has_value());
  EXPECT_EQ(id1.value(), 0);

  std::optional<int> id2 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
  ASSERT_TRUE(id2.has_value());
  EXPECT_EQ(id2.value(), 0);

  EXPECT_EQ(signin_prefs_->GetNextAccountMetricsUnassignedId(), 1);
}

TEST_F(AccountMetricsIdAllocatorTest, CapReached) {
  for (int i = 0; i <= 99; ++i) {
    GaiaId gaia_id("gaia_" + base::NumberToString(i));
    GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
  }

  EXPECT_EQ(signin_prefs_->GetNextAccountMetricsUnassignedId(), 100);

  GaiaId gaia_id_100("gaia_100");
  std::optional<int> id =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id_100);
  EXPECT_FALSE(id.has_value());

  EXPECT_EQ(signin_prefs_->GetNextAccountMetricsUnassignedId(), 100);
  EXPECT_FALSE(signin_prefs_->GetAccountMetricsId(gaia_id_100).has_value());
  EXPECT_TRUE(signin_prefs_->IsAccountMetricsIdCapped(gaia_id_100));
}

TEST_F(AccountMetricsIdAllocatorTest, TraceabilityCapReached) {
  base::HistogramTester histogram_tester;
  // Seed the global pref to 100.
  signin_prefs_->SetNextAccountMetricsUnassignedId(100);

  GaiaId gaia_id_101("gaia_101");
  std::optional<int> id =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id_101);
  EXPECT_FALSE(id.has_value());

  // Verify account pref is NOT set.
  EXPECT_FALSE(signin_prefs_->GetAccountMetricsId(gaia_id_101).has_value());
  // Verify account is marked as capped.
  EXPECT_TRUE(signin_prefs_->IsAccountMetricsIdCapped(gaia_id_101));

  // Verify that sample 100 was recorded (overflow).
  histogram_tester.ExpectUniqueSample("Signin.AccountInPref.AssignedId", 100,
                                      1);
}

TEST_F(AccountMetricsIdAllocatorTest, NoDuplicateLoggingAtCap) {
  // Seed the global pref to 100.
  signin_prefs_->SetNextAccountMetricsUnassignedId(100);

  GaiaId gaia_id_101("gaia_101");

  base::HistogramTester histogram_tester;

  // First call should log.
  std::optional<int> id1 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id_101);
  EXPECT_FALSE(id1.has_value());
  histogram_tester.ExpectUniqueSample("Signin.AccountInPref.AssignedId", 100,
                                      1);

  // Second call should NOT log.
  std::optional<int> id2 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id_101);
  EXPECT_FALSE(id2.has_value());
  // Total count should still be 1!
  histogram_tester.ExpectUniqueSample("Signin.AccountInPref.AssignedId", 100,
                                      1);
}

TEST_F(AccountMetricsIdAllocatorTest, MetricsLogging) {
  base::HistogramTester histogram_tester;
  GaiaId gaia_id("gaia_0");

  GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);

  histogram_tester.ExpectUniqueSample("Signin.AccountInPref.AssignedId", 0, 1);
}

TEST_F(AccountMetricsIdAllocatorTest, PersistenceAcrossInstances) {
  GaiaId gaia_id("gaia_1");
  std::optional<int> id1 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
  ASSERT_TRUE(id1.has_value());
  EXPECT_EQ(id1.value(), 0);

  // Call the free function again with the same prefs.
  std::optional<int> id2 =
      GetOrAllocateAccountMetricsId(*signin_prefs_, gaia_id);
  ASSERT_TRUE(id2.has_value());
  EXPECT_EQ(id2.value(), 0);
}

}  // namespace signin

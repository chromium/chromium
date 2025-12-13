// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_order.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

using ::testing::ElementsAre;

constexpr int kNumSessionsBetweenRotation = 3;

class NtpPromoOrderTest : public testing::Test {
 public:
  NtpPromoOrderTest() {
    order_policy_ = std::make_unique<NtpPromoOrderPolicy>(
        registry_, storage_service_, kNumSessionsBetweenRotation);
  }

 protected:
  // Puts the promo in the test registry, and initializes desired pref state.
  void RegisterPromo(NtpPromoIdentifier id,
                     base::flat_set<NtpPromoIdentifier> show_after,
                     int last_top_spot_session,
                     int top_spot_session_count,
                     base::Time completed_time) {
    registry_.AddPromo(NtpPromoSpecification(
        id, NtpPromoContent("", 0, 0),
        NtpPromoSpecification::EligibilityCallback(), base::DoNothing(),
        base::DoNothing(), std::move(show_after), user_education::Metadata()));

    NtpPromoData pref;
    pref.last_top_spot_session = last_top_spot_session;
    pref.top_spot_session_count = top_spot_session_count;
    pref.completed = completed_time;
    storage_service_.SaveNtpPromoData(id, pref);
  }

  // Generates the list of ordered promos, from everything put in the registry.
  // Eligible or completed state is ignored, since determining that is not
  // part of the ordering system.
  std::vector<NtpPromoIdentifier> Pending() {
    auto promos = registry_.GetNtpPromoIdentifiers();
    return order_policy_->OrderPendingPromos(std::move(promos));
  }

  std::vector<NtpPromoIdentifier> Completed() {
    auto promos = registry_.GetNtpPromoIdentifiers();
    return order_policy_->OrderCompletedPromos(std::move(promos));
  }

 private:
  NtpPromoRegistry registry_;
  test::TestUserEducationStorageService storage_service_;
  std::unique_ptr<NtpPromoOrderPolicy> order_policy_;
};

}  // namespace

TEST_F(NtpPromoOrderTest, PendingSinglePromo) {
  RegisterPromo("a", {}, 0, 0, base::Time());
  EXPECT_THAT(Pending(), ElementsAre("a"));
}

TEST_F(NtpPromoOrderTest, PendingNoPromo) {
  EXPECT_THAT(Pending(), testing::IsEmpty());
}

// The promo registry maintains registration order. Ensure this is preserved,
// in the absence of other ordering criteria.
TEST_F(NtpPromoOrderTest, PendingStableOrder) {
  RegisterPromo("c", {}, 0, 0, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());
  RegisterPromo("a", {}, 0, 0, base::Time());
  EXPECT_THAT(Pending(), ElementsAre("c", "b", "a"));
}

TEST_F(NtpPromoOrderTest, PendingCircularDependency) {
  RegisterPromo("a", {"b"}, 0, 0, base::Time());
  RegisterPromo("b", {"a"}, 0, 0, base::Time());
  EXPECT_CHECK_DEATH(Pending());
}

TEST_F(NtpPromoOrderTest, PendingShowAfter) {
  RegisterPromo("a", {"b"}, 0, 0, base::Time());
  RegisterPromo("b", {"c"}, 0, 0, base::Time());
  RegisterPromo("c", {}, 0, 0, base::Time());
  EXPECT_THAT(Pending(), ElementsAre("c", "b", "a"));
}

// Promos that have no dependencies should all be shown above any promos that
// have dependencies. The no-dependency promos form an effective group at the
// top of the list, so they need to be kept together.
TEST_F(NtpPromoOrderTest, PendingShowsAfterAllTopRanked) {
  RegisterPromo("a", {"b"}, 0, 0, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());
  RegisterPromo("c", {}, 0, 0, base::Time());

  EXPECT_THAT(Pending(), ElementsAre("b", "c", "a"));
}

TEST_F(NtpPromoOrderTest, PendingLastTopPromoStaysTop) {
  ASSERT_GT(kNumSessionsBetweenRotation, 1);

  RegisterPromo("a", {}, 100, 1, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());

  EXPECT_THAT(Pending(), ElementsAre("a", "b"));
}

TEST_F(NtpPromoOrderTest, PendingLastTopPromoStaysTopLastTime) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation - 1, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());

  EXPECT_THAT(Pending(), ElementsAre("a", "b"));
}

TEST_F(NtpPromoOrderTest, PendingLastTopPromoRotates) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());

  EXPECT_THAT(Pending(), ElementsAre("b", "a"));
}

TEST_F(NtpPromoOrderTest, PendingLastTopPromoRotatesWithDependencies) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation, base::Time());
  RegisterPromo("b", {}, 0, 0, base::Time());
  RegisterPromo("c", {"a"}, 0, 0, base::Time());

  EXPECT_THAT(Pending(), ElementsAre("b", "a", "c"));
}

TEST_F(NtpPromoOrderTest, CompletedSinglePromo) {
  RegisterPromo("a", {}, 0, 0, base::Time());

  EXPECT_THAT(Completed(), ElementsAre("a"));
}

TEST_F(NtpPromoOrderTest, CompletedPromosShowMostRecentFirst) {
  RegisterPromo("a", {}, 0, 0, base::Time::FromSecondsSinceUnixEpoch(2));
  RegisterPromo("b", {}, 0, 0, base::Time::FromSecondsSinceUnixEpoch(3));
  RegisterPromo("c", {}, 0, 0, base::Time::FromSecondsSinceUnixEpoch(1));
  EXPECT_THAT(Completed(), ElementsAre("b", "a", "c"));
}

}  // namespace user_education

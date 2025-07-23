// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_order.h"

#include <memory>

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
                     int top_spot_session_count) {
    registry_.AddPromo(NtpPromoSpecification(
        id, NtpPromoContent("", 0, 0),
        NtpPromoSpecification::EligibilityCallback(),
        NtpPromoSpecification::ActionCallback(), std::move(show_after),
        user_education::Metadata()));

    KeyedNtpPromoData pref;
    pref.last_top_spot_session = last_top_spot_session;
    pref.top_spot_session_count = top_spot_session_count;
    storage_service_.SaveNtpPromoData(id, pref);
  }

  // Generates the list of ordered promos, from everything put in the registry.
  std::vector<NtpPromoIdentifier> Pending() {
    auto promos = registry_.GetNtpPromoIdentifiers();
    return order_policy_->OrderPendingPromos(std::move(promos));
  }

 private:
  NtpPromoRegistry registry_;
  test::TestUserEducationStorageService storage_service_;
  std::unique_ptr<NtpPromoOrderPolicy> order_policy_;
};

}  // namespace

TEST_F(NtpPromoOrderTest, SinglePromo) {
  RegisterPromo("a", {}, 0, 0);
  EXPECT_THAT(Pending(), ElementsAre("a"));
}

TEST_F(NtpPromoOrderTest, NoPromo) {
  EXPECT_THAT(Pending(), testing::IsEmpty());
}

// The promo registry orders by key; ensure this is preserved.
TEST_F(NtpPromoOrderTest, StableOrder) {
  RegisterPromo("c", {}, 0, 0);
  RegisterPromo("b", {}, 0, 0);
  RegisterPromo("a", {}, 0, 0);
  EXPECT_THAT(Pending(), ElementsAre("a", "b", "c"));
}

TEST_F(NtpPromoOrderTest, CircularDependency) {
  RegisterPromo("a", {"b"}, 0, 0);
  RegisterPromo("b", {"a"}, 0, 0);
  EXPECT_CHECK_DEATH(Pending());
}

TEST_F(NtpPromoOrderTest, ShowAfter) {
  RegisterPromo("a", {"b"}, 0, 0);
  RegisterPromo("b", {"c"}, 0, 0);
  RegisterPromo("c", {}, 0, 0);
  EXPECT_THAT(Pending(), ElementsAre("c", "b", "a"));
}

// Promos that have no dependencies should all be shown above any promos that
// have dependencies. The no-dependency promos form an effective group at the
// top of the list, so they need to be kept together.
TEST_F(NtpPromoOrderTest, ShowsAfterAllTopRanked) {
  RegisterPromo("a", {"b"}, 0, 0);
  RegisterPromo("b", {}, 0, 0);
  RegisterPromo("c", {}, 0, 0);

  EXPECT_THAT(Pending(), ElementsAre("b", "c", "a"));
}

TEST_F(NtpPromoOrderTest, LastTopPromoStaysTop) {
  ASSERT_GT(kNumSessionsBetweenRotation, 1);

  RegisterPromo("a", {}, 100, 1);
  RegisterPromo("b", {}, 0, 0);

  EXPECT_THAT(Pending(), ElementsAre("a", "b"));
}

TEST_F(NtpPromoOrderTest, LastTopPromoStaysTopLastTime) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation - 1);
  RegisterPromo("b", {}, 0, 0);

  EXPECT_THAT(Pending(), ElementsAre("a", "b"));
}

TEST_F(NtpPromoOrderTest, LastTopPromoRotates) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation);
  RegisterPromo("b", {}, 0, 0);

  EXPECT_THAT(Pending(), ElementsAre("b", "a"));
}

TEST_F(NtpPromoOrderTest, LastTopPromoRotatesWithDependencies) {
  RegisterPromo("a", {}, 100, kNumSessionsBetweenRotation);
  RegisterPromo("b", {}, 0, 0);
  RegisterPromo("c", {"a"}, 0, 0);

  EXPECT_THAT(Pending(), ElementsAre("b", "a", "c"));
}

}  // namespace user_education

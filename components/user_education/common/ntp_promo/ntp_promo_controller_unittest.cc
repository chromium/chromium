// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/test/mock_callback.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kPromoId[] = "promo";

class NtpPromoControllerTest : public testing::Test {
 public:
  NtpPromoControllerTest() : controller_(registry_, storage_service_) {}

 protected:
  void RegisterPromo(
      NtpPromoIdentifier id,
      NtpPromoSpecification::EligibilityCallback eligibility_callback,
      NtpPromoSpecification::ActionCallback action_callback) {
    registry_.AddPromo(NtpPromoSpecification(
        id, NtpPromoContent("", 0, 0), eligibility_callback, action_callback,
        /*show_after=*/{}, user_education::Metadata()));
  }

  NtpPromoRegistry registry_;
  test::TestUserEducationStorageService storage_service_;
  NtpPromoController controller_;
};

}  // namespace

// Note: Parameterize these eligibility tests when there are more of them.
TEST_F(NtpPromoControllerTest, IneligiblePromoHidden) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kIneligible));

  const auto showable_promos = controller_.GetShowablePromos();
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, EligiblePromoShows) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kEligible));

  const auto showable_promos = controller_.GetShowablePromos();
  EXPECT_EQ(showable_promos.pending.size(), 1u);
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, CompletedPromoShows) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kCompleted));

  const auto showable_promos = controller_.GetShowablePromos();
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
}

TEST_F(NtpPromoControllerTest, MarkedCompletePromoShows) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kEligible));

  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GetShowablePromos();
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
}

TEST_F(NtpPromoControllerTest, ClickInvokesPromoAction) {
  base::MockRepeatingCallback<void(Browser*)> action_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                action_callback.Get());
  EXPECT_CALL(action_callback, Run(_));
  controller_.OnPromoClicked(kPromoId);
}

}  // namespace user_education

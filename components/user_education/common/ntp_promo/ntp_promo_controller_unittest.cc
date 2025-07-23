// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
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
constexpr char kPromo2Id[] = "promo2";

constexpr int kSessionNumber = 10;

class NtpPromoControllerTest : public testing::Test {
 public:
  NtpPromoControllerTest() : controller_(registry_, storage_service_) {
    auto session = storage_service_.ReadSessionData();
    session.session_number = kSessionNumber;
    storage_service_.SaveSessionData(session);
  }

 protected:
  void RegisterPromo(
      NtpPromoIdentifier id,
      NtpPromoSpecification::EligibilityCallback eligibility_callback,
      NtpPromoSpecification::ActionCallback action_callback) {
    registry_.AddPromo(
        NtpPromoSpecification(id, NtpPromoContent("", IDS_OK, IDS_CANCEL),
                              eligibility_callback, action_callback,
                              /*show_after=*/{}, user_education::Metadata()));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
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

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_EQ(showable_promos.pending.size(), 1u);
  EXPECT_TRUE(showable_promos.completed.empty());
}

// A promo that reports itself as complete, but was never clicked, should not
// be shown.
TEST_F(NtpPromoControllerTest, UnclickedCompletedPromoHidden) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kCompleted));

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, ClickedCompletedPromoShows) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kCompleted));
  // Simulate that the user clicked on the promo.
  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.last_clicked = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);

  // Ensure the completion time pref is recorded.
  const auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().completed, base::Time::Now());
}

// Once a promo has been declared completed, it should continue to show as
// completed even if the promo reverts to Eligible state (eg. a user signs out).
TEST_F(NtpPromoControllerTest, PreviouslyCompletedPromoShows) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kEligible));

  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
}

TEST_F(NtpPromoControllerTest, OldCompletedPromoHidden) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kEligible));

  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed =
      base::Time::Now() - controller_.GetCompletedPromoShowDurationForTest();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, FutureCompletedPromoHidden) {
  base::MockRepeatingCallback<NtpPromoSpecification::Eligibility(Profile*)>
      eligibility_callback;
  RegisterPromo(kPromoId, eligibility_callback.Get(),
                NtpPromoSpecification::ActionCallback());
  EXPECT_CALL(eligibility_callback, Run(_))
      .WillOnce(Return(NtpPromoSpecification::Eligibility::kEligible));

  // Verify that a pref saved with a nonsense timestamp doesn't end up
  // showing a completed promo indefinitely.
  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now() + base::Days(1);
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, PromoClicked) {
  base::MockRepeatingCallback<void(BrowserWindowInterface*)> action_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                action_callback.Get());
  EXPECT_CALL(action_callback, Run(_));
  controller_.OnPromoClicked(kPromoId, nullptr);

  const auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().last_clicked, base::Time::Now());
}

TEST_F(NtpPromoControllerTest, OnPromosShown_CompletedPromoOnly) {
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  controller_.OnPromosShown({}, {kPromoId});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(old_value, new_value);
}

TEST_F(NtpPromoControllerTest, OnPromosShown_EligiblePromo_NoPreviousData) {
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(std::nullopt, old_value);
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
}

TEST_F(NtpPromoControllerTest, OnPromosShown_EligiblePromo_PreviousData) {
  KeyedNtpPromoData old_value;
  old_value.last_top_spot_session = kSessionNumber - 1;
  old_value.top_spot_session_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(3, new_value->top_spot_session_count);
}

TEST_F(NtpPromoControllerTest, OnPromosShown_MultiplePromos) {
  const auto old_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);
  controller_.OnPromosShown({kPromoId, kPromo2Id}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  const auto new_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
  EXPECT_EQ(old_value2, new_value2);
}

}  // namespace user_education

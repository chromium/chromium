// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/test/bind.h"
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

constexpr auto kEligible = NtpPromoSpecification::Eligibility::kEligible;
constexpr auto kIneligible = NtpPromoSpecification::Eligibility::kIneligible;
constexpr auto kCompleted = NtpPromoSpecification::Eligibility::kCompleted;

class NtpPromoControllerTest : public testing::Test {
 public:
  NtpPromoControllerTest() : controller_(registry_, storage_service_) {
    auto session = storage_service_.ReadSessionData();
    session.session_number = kSessionNumber;
    storage_service_.SaveSessionData(session);
  }

 protected:
  // Register a promo with the supplied callbacks.
  void RegisterPromo(
      NtpPromoIdentifier id,
      NtpPromoSpecification::EligibilityCallback eligibility_callback,
      NtpPromoSpecification::ShowCallback show_callback,
      NtpPromoSpecification::ActionCallback action_callback) {
    registry_.AddPromo(NtpPromoSpecification(
        id, NtpPromoContent("", IDS_OK, IDS_CANCEL), eligibility_callback,
        show_callback, action_callback,
        /*show_after=*/{}, user_education::Metadata()));
  }

  // Register a promo of the the specified eligibility.
  void RegisterPromo(NtpPromoIdentifier id,
                     NtpPromoSpecification::Eligibility eligibility) {
    RegisterPromo(id, base::BindLambdaForTesting([=](Profile* profile) {
                    return eligibility;
                  }),
                  base::DoNothing(), base::DoNothing());
  }

  int ShowablePendingPromoCount() {
    const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
    return showable_promos.pending.size();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NtpPromoRegistry registry_;
  test::TestUserEducationStorageService storage_service_;
  NtpPromoController controller_;
};

}  // namespace

TEST_F(NtpPromoControllerTest, IneligiblePromoHidden) {
  RegisterPromo(kPromoId, kIneligible);
  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, EligiblePromoShows) {
  RegisterPromo(kPromoId, kEligible);
  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_EQ(showable_promos.pending.size(), 1u);
  EXPECT_TRUE(showable_promos.completed.empty());
}

// A promo that reports itself as complete, but was never clicked, should not
// be shown.
TEST_F(NtpPromoControllerTest, UnclickedCompletedPromoHidden) {
  RegisterPromo(kPromoId, kCompleted);
  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, ClickedCompletedPromoShows) {
  RegisterPromo(kPromoId, kCompleted);

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
  RegisterPromo(kPromoId, kEligible);
  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
}

TEST_F(NtpPromoControllerTest, OldCompletedPromoHidden) {
  RegisterPromo(kPromoId, kEligible);
  user_education::KeyedNtpPromoData keyed_data;
  keyed_data.completed =
      base::Time::Now() - controller_.GetCompletedPromoShowDurationForTest();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller_.GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, FutureCompletedPromoHidden) {
  RegisterPromo(kPromoId, kCompleted);

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
                base::DoNothing(), action_callback.Get());
  EXPECT_CALL(action_callback, Run(_));
  controller_.OnPromoClicked(kPromoId, nullptr);

  const auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().last_clicked, base::Time::Now());
}

TEST_F(NtpPromoControllerTest, ClickedPromoHiddenTemporarily) {
  RegisterPromo(kPromoId, kEligible);
  EXPECT_EQ(ShowablePendingPromoCount(), 1);

  controller_.OnPromoClicked(kPromoId, nullptr);
  EXPECT_EQ(ShowablePendingPromoCount(), 0);

  task_environment_.AdvanceClock(
      controller_.GetClickedPromoHideDurationForTest());
  EXPECT_EQ(ShowablePendingPromoCount(), 1);
}

TEST_F(NtpPromoControllerTest, CompletedPromoShown) {
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  controller_.OnPromosShown({}, {kPromoId});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(old_value, new_value);
}

TEST_F(NtpPromoControllerTest, TopSpotPromoShownFirstTime) {
  RegisterPromo(kPromoId, kEligible);
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(std::nullopt, old_value);
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
}

// When the shown top spot promo was previously in the top spot, during the
// same browsing session, prefs shouldn't change.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInSameSession) {
  RegisterPromo(kPromoId, kEligible);
  KeyedNtpPromoData old_value;
  old_value.last_top_spot_session = kSessionNumber;
  old_value.top_spot_session_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(2, new_value->top_spot_session_count);
}

// When the shown top spot promo was previously in the top spot, during the
// previous browsing session, the top spot session count should be incremented.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInNewSession) {
  RegisterPromo(kPromoId, kEligible);
  KeyedNtpPromoData old_value;
  old_value.last_top_spot_session = kSessionNumber - 1;
  old_value.top_spot_session_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(3, new_value->top_spot_session_count);
}

// When the shown top spot promo was not previously in the top spot, it should
// clear its top spot count to start a fresh stay at the top of the list.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownReclaimsTopSpot) {
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  // Have Promo2 be the most recent top-spot holder.
  KeyedNtpPromoData old_promo_2;
  old_promo_2.last_top_spot_session = kSessionNumber - 1;
  storage_service_.SaveNtpPromoData(kPromo2Id, old_promo_2);
  // Have Promo be a previous top-spot holder, before Promo2.
  KeyedNtpPromoData old_value;
  old_promo_2.last_top_spot_session = kSessionNumber - 2;
  old_promo_2.top_spot_session_count = 3;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);

  // Showing Promo should clear its top spot count and restart at 1.
  controller_.OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
}

TEST_F(NtpPromoControllerTest, OnMultiplePromosShown) {
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);
  const auto old_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);
  controller_.OnPromosShown({kPromoId, kPromo2Id}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  const auto new_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
  EXPECT_EQ(old_value2, new_value2);
}

TEST_F(NtpPromoControllerTest, ShownCallbackInvoked) {
  base::MockRepeatingCallback<void()> show_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                show_callback.Get(), base::DoNothing());
  EXPECT_CALL(show_callback, Run());
  controller_.OnPromosShown({kPromoId}, {});
}

}  // namespace user_education

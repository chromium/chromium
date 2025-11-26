// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

constexpr char kPromoId[] = "TestPromo";
constexpr char kPromo2Id[] = "TestPromo2";
constexpr char kPromo3Id[] = "TestPromo3";

constexpr int kSessionNumber = 10;

constexpr auto kEligible = NtpPromoSpecification::Eligibility::kEligible;
constexpr auto kIneligible = NtpPromoSpecification::Eligibility::kIneligible;
constexpr auto kCompleted = NtpPromoSpecification::Eligibility::kCompleted;

class NtpPromoControllerTest : public testing::Test {
 public:
  NtpPromoControllerTest() {
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
        /*show_after=*/{}, Metadata()));
  }

  // Register a promo of the the specified eligibility.
  void RegisterPromo(NtpPromoIdentifier id,
                     NtpPromoSpecification::Eligibility eligibility,
                     bool clicked = false) {
    RegisterPromo(
        id,
        base::BindLambdaForTesting(
            [=](const user_education::UserEducationContextPtr& context) {
              return eligibility;
            }),
        base::DoNothing(), base::DoNothing());
    if (clicked) {
      NtpPromoData promo_data =
          storage_service_.ReadNtpPromoData(id).value_or(NtpPromoData());
      promo_data.last_clicked = base::Time::Now();
      storage_service_.SaveNtpPromoData(id, promo_data);
    }
  }

  int ShowablePendingPromoCount() {
    const auto showable_promos = controller().GenerateShowablePromos(nullptr);
    return showable_promos.pending.size();
  }

  void CreateController(
      std::optional<NtpPromoControllerParams> feature_params = std::nullopt) {
    controller_ = std::make_unique<NtpPromoController>(
        registry_, storage_service_,
        feature_params.value_or(GetNtpPromoControllerParams()));
  }

  NtpPromoController& controller() { return *controller_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::TestUserEducationStorageService storage_service_;

 private:
  NtpPromoRegistry registry_;
  std::unique_ptr<NtpPromoController> controller_;
};

std::vector<NtpPromoIdentifier> IdsFromShowablePromos(
    const std::vector<NtpShowablePromo>& promos) {
  std::vector<NtpPromoIdentifier> ids;
  ids.reserve(promos.size());
  std::transform(promos.begin(), promos.end(), std::back_inserter(ids),
                 [](const NtpShowablePromo& promo) { return promo.id; });
  return ids;
}

}  // namespace

TEST_F(NtpPromoControllerTest, IneligiblePromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kIneligible);
  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, EligiblePromoShows) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(showable_promos.pending.size(), 1u);
  EXPECT_TRUE(showable_promos.completed.empty());
}

// A promo that reports itself as complete, but was never clicked, should not
// be shown.
TEST_F(NtpPromoControllerTest, UnclickedCompletedPromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);
  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, ClickedCompletedPromoShows) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);
  base::HistogramTester histogram_tester;

  // Simulate that the user clicked on the promo.
  NtpPromoData keyed_data;
  keyed_data.last_clicked = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  // Generating promos is currently where "completed promo" detection lives.
  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
  auto completion_time = base::Time::Now();

  // Ensure the completion is recorded.
  auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().completed, completion_time);
  histogram_tester.ExpectTotalCount(
      "UserEducation.NtpPromos.Promos.TestPromo.Completed", 1);

  // Generate promos again. Completion data shouldn't change.
  task_environment_.AdvanceClock(base::Days(1));
  controller().GenerateShowablePromos(nullptr);
  prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().completed, completion_time);
  histogram_tester.ExpectTotalCount(
      "UserEducation.NtpPromos.Promos.TestPromo.Completed", 1);
}

// Once a promo has been declared completed, it should continue to show as
// completed even if the promo reverts to Eligible state (eg. a user signs out).
TEST_F(NtpPromoControllerTest, PreviouslyCompletedPromoShows) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_EQ(showable_promos.completed.size(), 1u);
}

TEST_F(NtpPromoControllerTest, OldCompletedPromoHidden) {
  auto params = GetNtpPromoControllerParams();
  CreateController(params);
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now() - params.completed_show_duration;
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, FutureCompletedPromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);

  // Verify that a pref saved with a nonsense timestamp doesn't end up
  // showing a completed promo indefinitely.
  NtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now() + base::Days(1);
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  const auto showable_promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_TRUE(showable_promos.pending.empty());
  EXPECT_TRUE(showable_promos.completed.empty());
}

TEST_F(NtpPromoControllerTest, PromoClicked) {
  CreateController();
  base::MockRepeatingCallback<void(
      const user_education::UserEducationContextPtr& context)>
      action_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                base::DoNothing(), action_callback.Get());
  EXPECT_CALL(action_callback, Run(_));
  base::HistogramTester histogram_tester;

  controller().OnPromoClicked(kPromoId, nullptr);

  const auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().last_clicked, base::Time::Now());
  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo.Clicked", true, 1);
}

TEST_F(NtpPromoControllerTest, ClickedPromoHiddenTemporarily) {
  auto params = GetNtpPromoControllerParams();
  CreateController(params);
  RegisterPromo(kPromoId, kEligible);
  EXPECT_EQ(ShowablePendingPromoCount(), 1);

  controller().OnPromoClicked(kPromoId, nullptr);
  EXPECT_EQ(ShowablePendingPromoCount(), 0);

  task_environment_.AdvanceClock(params.clicked_hide_duration);
  EXPECT_EQ(ShowablePendingPromoCount(), 1);
}

TEST_F(NtpPromoControllerTest, CompletedPromoShown) {
  CreateController();
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  controller().OnPromosShown({}, {kPromoId});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(old_value, new_value);
}

TEST_F(NtpPromoControllerTest, TopSpotPromoShownFirstTime) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(std::nullopt, old_value);
  controller().OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
}

// When the shown top spot promo was previously in the top spot, during the
// same browsing session, prefs shouldn't change.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInSameSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData old_value;
  old_value.last_top_spot_session = kSessionNumber;
  old_value.top_spot_session_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller().OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(2, new_value->top_spot_session_count);
}

// When the shown top spot promo was previously in the top spot, during the
// previous browsing session, the top spot session count should be incremented.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInNewSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData old_value;
  old_value.last_top_spot_session = kSessionNumber - 1;
  old_value.top_spot_session_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller().OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(3, new_value->top_spot_session_count);
}

// When the shown top spot promo was not previously in the top spot, it should
// clear its top spot count to start a fresh stay at the top of the list.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownReclaimsTopSpot) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  // Have Promo2 be the most recent top-spot holder.
  NtpPromoData old_promo_2;
  old_promo_2.last_top_spot_session = kSessionNumber - 1;
  storage_service_.SaveNtpPromoData(kPromo2Id, old_promo_2);
  // Have Promo be a previous top-spot holder, before Promo2.
  NtpPromoData old_value;
  old_promo_2.last_top_spot_session = kSessionNumber - 2;
  old_promo_2.top_spot_session_count = 3;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);

  // Showing Promo should clear its top spot count and restart at 1.
  controller().OnPromosShown({kPromoId}, {});
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kSessionNumber, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
}

TEST_F(NtpPromoControllerTest, ShownPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);
  base::HistogramTester histogram_tester;
  const auto old_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);

  controller().OnPromosShown({kPromoId, kPromo2Id}, {});

  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  const auto new_value2 = storage_service_.ReadNtpPromoData(kPromo2Id);
  EXPECT_EQ(10, new_value->last_top_spot_session);
  EXPECT_EQ(1, new_value->top_spot_session_count);
  EXPECT_EQ(old_value2, new_value2);

  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo.ShownTopSpot", true, 1);
  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo2.Shown", true, 1);
  histogram_tester.ExpectTotalCount(
      "UserEducation.NtpPromos.Promos.TestPromo2.ShownTopSpot", 0);
}

TEST_F(NtpPromoControllerTest, ShownCallbackInvoked) {
  CreateController();
  base::MockRepeatingCallback<void()> show_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                show_callback.Get(), base::DoNothing());
  EXPECT_CALL(show_callback, Run());
  controller().OnPromosShown({kPromoId}, {});
}

TEST_F(NtpPromoControllerTest, HasShowablePromosNoCompleted) {
  CreateController();
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, false));
  RegisterPromo(kPromoId, kCompleted, true);
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, false));
  RegisterPromo(kPromo2Id, kEligible);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, false));
}

TEST_F(NtpPromoControllerTest,
       HasShowablePromosIncludeCompleted_CompletedOnly) {
  CreateController();
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, true));
  RegisterPromo(kPromoId, kCompleted, true);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
  RegisterPromo(kPromo2Id, kEligible);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
}

TEST_F(NtpPromoControllerTest,
       HasShowablePromosIncludeCompleted_EligibleThenCompleted) {
  CreateController();
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, true));
  RegisterPromo(kPromoId, kEligible);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
  RegisterPromo(kPromo2Id, kCompleted, true);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
}

TEST_F(NtpPromoControllerTest, SetAllPromosSnoozed) {
  CreateController();
  controller().SetAllPromosSnoozed(true);
  EXPECT_EQ(storage_service_.ReadNtpPromoPreferences().last_snoozed,
            storage_service_.GetCurrentTime());
}

TEST_F(NtpPromoControllerTest, SetAllPromosSnoozedUnsnoozed) {
  CreateController();
  controller().SetAllPromosSnoozed(true);
  controller().SetAllPromosSnoozed(false);
  EXPECT_EQ(storage_service_.ReadNtpPromoPreferences().last_snoozed,
            base::Time());
}

TEST_F(NtpPromoControllerTest, SetAllPromosDisabled) {
  CreateController();
  controller().SetAllPromosDisabled(true);
  const auto prefs = storage_service_.ReadNtpPromoPreferences();
  EXPECT_TRUE(prefs.disabled);
}

TEST_F(NtpPromoControllerTest, SetAllPromosDisabledClearsSnoozedState) {
  CreateController();
  controller().SetAllPromosSnoozed(true);
  controller().SetAllPromosDisabled(true);
  const auto prefs = storage_service_.ReadNtpPromoPreferences();
  EXPECT_EQ(base::Time(), prefs.last_snoozed);
}

TEST_F(NtpPromoControllerTest, SetAllPromosUndisabledClearsSnoozedState) {
  CreateController();
  controller().SetAllPromosDisabled(true);
  controller().SetAllPromosSnoozed(true);
  controller().SetAllPromosDisabled(false);
  const auto prefs = storage_service_.ReadNtpPromoPreferences();
  EXPECT_EQ(base::Time(), prefs.last_snoozed);
}

TEST_F(NtpPromoControllerTest, SetAllPromosDisabledUndisabled) {
  CreateController();
  controller().SetAllPromosDisabled(true);
  controller().SetAllPromosDisabled(false);
  EXPECT_FALSE(storage_service_.ReadNtpPromoPreferences().disabled);
}

TEST_F(NtpPromoControllerTest, SnoozeBlocksPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kCompleted, /*clicked=*/true);
  controller().SetAllPromosSnoozed(true);
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, true));
  const auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(0U, promos.pending.size());
  EXPECT_EQ(0U, promos.completed.size());
}

TEST_F(NtpPromoControllerTest, UnsnoozeRestoresPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kCompleted, /*clicked=*/true);
  controller().SetAllPromosSnoozed(true);
  controller().SetAllPromosSnoozed(false);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
  const auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(1U, promos.pending.size());
  EXPECT_EQ(1U, promos.completed.size());
}

TEST_F(NtpPromoControllerTest, SnoozeExpiresRestoresPromos) {
  auto params = GetNtpPromoControllerParams();
  CreateController(params);
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kCompleted, /*clicked=*/true);
  controller().SetAllPromosSnoozed(true);
  task_environment_.FastForwardBy(params.promos_snoozed_hide_duration +
                                  base::Minutes(1));
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
  auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(1U, promos.pending.size());
  EXPECT_EQ(1U, promos.completed.size());
}

TEST_F(NtpPromoControllerTest, DisableBlocksPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kCompleted, /*clicked=*/true);
  controller().SetAllPromosDisabled(true);
  EXPECT_FALSE(controller().HasShowablePromos(nullptr, true));
  const auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(0U, promos.pending.size());
  EXPECT_EQ(0U, promos.completed.size());
}

TEST_F(NtpPromoControllerTest, UndisableRestoresPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kCompleted, /*clicked=*/true);
  controller().SetAllPromosDisabled(true);
  controller().SetAllPromosDisabled(false);
  EXPECT_TRUE(controller().HasShowablePromos(nullptr, true));
  const auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_EQ(1U, promos.pending.size());
  EXPECT_EQ(1U, promos.completed.size());
}

TEST_F(NtpPromoControllerTest, SuppessedSinglePromo) {
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      user_education::features::kEnableNtpBrowserPromos,
      {{"suppress-list", kPromoId}});
  CreateController();
  auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_THAT(IdsFromShowablePromos(promos.pending), ElementsAre(kPromo2Id));
}

// This exercises the parsing of a comma-separated list in the feature param.
TEST_F(NtpPromoControllerTest, SuppessedMultiplePromos) {
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);
  RegisterPromo(kPromo3Id, kEligible);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      user_education::features::kEnableNtpBrowserPromos,
      {{"suppress-list", base::StrCat({kPromoId, ",", kPromo2Id})}});
  CreateController();
  auto promos = controller().GenerateShowablePromos(nullptr);
  EXPECT_THAT(IdsFromShowablePromos(promos.pending), ElementsAre(kPromo3Id));
}

}  // namespace user_education

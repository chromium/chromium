// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
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
using ::testing::Return;

constexpr char kPromoId[] = "TestPromo";
constexpr char kPromo2Id[] = "TestPromo2";
constexpr char kPromo3Id[] = "TestPromo3";

constexpr int kInitialSessionNumber = 10;

constexpr auto kEligible = NtpPromoSpecification::Eligibility::kEligible;
constexpr auto kIneligible = NtpPromoSpecification::Eligibility::kIneligible;
constexpr auto kCompleted = NtpPromoSpecification::Eligibility::kCompleted;

class NtpPromoControllerTest : public testing::Test {
 public:
  NtpPromoControllerTest() {
    auto session = storage_service_.ReadSessionData();
    session.session_number = kInitialSessionNumber;
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

  // Helper function that ensures a promo is showing, and continues to show
  // after being displayed.
  bool ShowsPromo(std::string_view expected_promo_id) {
    const auto showable = controller().GenerateShowablePromo(nullptr);
    bool shows = showable.has_value() && showable->id == expected_promo_id;
    EXPECT_TRUE(shows);
    if (shows) {
      // Ensure that this promo continues to show, after appearing on an NTP.
      controller().OnPromoShown(showable->id);
      const auto showable_again = controller().GenerateShowablePromo(nullptr);
      EXPECT_TRUE(showable_again.has_value() &&
                  showable_again->id == expected_promo_id);
    }
    return shows;
  }

  bool ShowsAnyPromo() {
    return controller().GenerateShowablePromo(nullptr).has_value();
  }

  void CreateController(
      std::optional<NtpPromoControllerParams> feature_params = std::nullopt) {
    controller_ = std::make_unique<NtpPromoController>(
        registry_, storage_service_,
        feature_params.value_or(GetNtpPromoControllerParams()));
  }

  NtpPromoController& controller() { return *controller_; }

  void AdvanceSession() {
    auto session_data = storage_service_.ReadSessionData();
    session_data.session_number++;
    storage_service_.SaveSessionData(session_data);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::TestUserEducationStorageService storage_service_;

 private:
  NtpPromoRegistry registry_;
  std::unique_ptr<NtpPromoController> controller_;
};

}  // namespace

TEST_F(NtpPromoControllerTest, IneligiblePromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kIneligible);
  EXPECT_FALSE(ShowsAnyPromo());
}

TEST_F(NtpPromoControllerTest, EligiblePromoShows) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  EXPECT_TRUE(ShowsPromo(kPromoId));
}

// A promo that reports itself as complete, but was never clicked, should not
// be shown.
TEST_F(NtpPromoControllerTest, UnclickedCompletedPromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);
  EXPECT_FALSE(ShowsAnyPromo());
}

TEST_F(NtpPromoControllerTest, ClickedCompletedPromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);
  base::HistogramTester histogram_tester;

  // Simulate that the user clicked on the promo.
  NtpPromoData keyed_data;
  keyed_data.last_clicked = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  // Generating promos is currently where "completed promo" detection lives.
  EXPECT_FALSE(ShowsAnyPromo());
  auto completion_time = base::Time::Now();

  // Ensure the completion is recorded.
  auto prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().completed, completion_time);
  histogram_tester.ExpectTotalCount(
      "UserEducation.NtpPromos.Promos.TestPromo.Completed", 1);

  // Generate promos again. Completion data shouldn't change.
  task_environment_.AdvanceClock(base::Days(1));
  controller().GenerateShowablePromo(nullptr);
  prefs = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(prefs.value().completed, completion_time);
  histogram_tester.ExpectTotalCount(
      "UserEducation.NtpPromos.Promos.TestPromo.Completed", 1);
}

// Once a promo has been declared completed, it should continue to not show
//  even if the promo reverts to Eligible state (eg. a user signs out).
TEST_F(NtpPromoControllerTest, PreviouslyCompletedPromoShows) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now();
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  EXPECT_FALSE(ShowsAnyPromo());
}

TEST_F(NtpPromoControllerTest, FutureCompletedPromoHidden) {
  CreateController();
  RegisterPromo(kPromoId, kCompleted);

  // Verify that a pref saved with a nonsense timestamp doesn't end up
  // showing a completed promo indefinitely.
  NtpPromoData keyed_data;
  keyed_data.completed = base::Time::Now() + base::Days(1);
  storage_service_.SaveNtpPromoData(kPromoId, keyed_data);

  EXPECT_FALSE(ShowsAnyPromo());
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
  EXPECT_TRUE(ShowsPromo(kPromoId));

  controller().OnPromoClicked(kPromoId, nullptr);
  EXPECT_FALSE(ShowsAnyPromo());

  task_environment_.AdvanceClock(params.cool_off_duration);
  EXPECT_TRUE(ShowsPromo(kPromoId));
}

TEST_F(NtpPromoControllerTest, TopSpotPromoShownFirstTime) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  const auto old_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(std::nullopt, old_value);
  controller().OnPromoShown(kPromoId);
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kInitialSessionNumber, new_value->last_session);
  EXPECT_EQ(1, new_value->session_count_in_term);
}

// When the shown promo was previously shown during the same browsing session,
// prefs shouldn't change.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInSameSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData old_value;
  old_value.last_session = kInitialSessionNumber;
  old_value.session_count_in_term = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller().OnPromoShown(kPromoId);
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kInitialSessionNumber, new_value->last_session);
  EXPECT_EQ(2, new_value->session_count_in_term);
}

// When the shown promo was also shown in the previous session, the top spot
// session count should be incremented.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownInNewSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  NtpPromoData old_value;
  old_value.last_session = kInitialSessionNumber - 1;
  old_value.session_count_in_term = 2;
  storage_service_.SaveNtpPromoData(kPromoId, old_value);
  controller().OnPromoShown(kPromoId);
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kInitialSessionNumber, new_value->last_session);
  EXPECT_EQ(3, new_value->session_count_in_term);
}

// When the shown promo was not previously the shown promo, it should
// clear its session count to start a fresh term on the NTP.
TEST_F(NtpPromoControllerTest, TopSpotPromoShownReclaimsTopSpot) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  // Have Promo2 be the most recent top-spot holder.
  NtpPromoData old_promo_2;
  old_promo_2.last_session = kInitialSessionNumber - 1;
  storage_service_.SaveNtpPromoData(kPromo2Id, old_promo_2);
  // Have Promo be a previous top-spot holder, before Promo2.

  NtpPromoData old_data;
  old_data.last_session = kInitialSessionNumber - 2;
  old_data.session_count_in_term = 3;
  old_data.term_count = 1;
  storage_service_.SaveNtpPromoData(kPromoId, old_data);

  // Showing the promo should clear its top spot count and restart at 1.
  controller().OnPromoShown(kPromoId);
  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(kInitialSessionNumber, new_value->last_session);
  EXPECT_EQ(1, new_value->session_count_in_term);
  EXPECT_EQ(2, new_value->term_count);
}

// When the shown promo was not previously the shown promo, it should
// clear its session count to start a fresh term on the NTP.
TEST_F(NtpPromoControllerTest, MaxTermsBlocksPromo) {
  auto params = GetNtpPromoControllerParams();
  params.max_terms = 2;
  params.max_sessions_per_term = 2;
  CreateController(params);

  RegisterPromo(kPromoId, kEligible);

  NtpPromoData data;
  data.last_session = kInitialSessionNumber - 1;
  data.session_count_in_term = 2;
  data.term_count = 2;
  storage_service_.SaveNtpPromoData(kPromoId, data);

  EXPECT_FALSE(ShowsAnyPromo());
}

TEST_F(NtpPromoControllerTest, DismissedBlocksPromo) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  base::HistogramTester histogram_tester;

  controller().OnPromoDismissed(kPromoId);

  EXPECT_FALSE(ShowsAnyPromo());
  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo.Dismissed", true, 1);
}

TEST_F(NtpPromoControllerTest, ShownPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  base::HistogramTester histogram_tester;

  controller().OnPromoShown(kPromoId);

  const auto new_value = storage_service_.ReadNtpPromoData(kPromoId);
  EXPECT_EQ(10, new_value->last_session);
  EXPECT_EQ(1, new_value->session_count_in_term);

  histogram_tester.ExpectUniqueSample(
      "UserEducation.NtpPromos.Promos.TestPromo.Shown", true, 1);
}

TEST_F(NtpPromoControllerTest, ShownCallbackInvoked) {
  CreateController();
  base::MockRepeatingCallback<void()> show_callback;
  RegisterPromo(kPromoId, NtpPromoSpecification::EligibilityCallback(),
                show_callback.Get(), base::DoNothing());
  EXPECT_CALL(show_callback, Run());
  controller().OnPromoShown(kPromoId);
}

// Verifies the HasShowablePromo() wrapper function.
TEST_F(NtpPromoControllerTest, HasShowablePromo) {
  CreateController();
  EXPECT_FALSE(controller().HasShowablePromo(nullptr));
  RegisterPromo(kPromoId, kEligible);
  EXPECT_TRUE(controller().HasShowablePromo(nullptr));
}

TEST_F(NtpPromoControllerTest, SetAllPromosDisabled) {
  CreateController();
  controller().SetAllPromosDisabled(true);
  const auto prefs = storage_service_.ReadNtpPromoPreferences();
  EXPECT_TRUE(prefs.disabled);
}

TEST_F(NtpPromoControllerTest, SetAllPromosDisabledUndisabled) {
  CreateController();
  controller().SetAllPromosDisabled(true);
  controller().SetAllPromosDisabled(false);
  EXPECT_FALSE(storage_service_.ReadNtpPromoPreferences().disabled);
}

TEST_F(NtpPromoControllerTest, DisableBlocksPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  controller().SetAllPromosDisabled(true);
  EXPECT_FALSE(controller().HasShowablePromo(nullptr));
  const auto showable = controller().GenerateShowablePromo(nullptr);
  EXPECT_FALSE(showable.has_value());
}

TEST_F(NtpPromoControllerTest, UndisableRestoresPromos) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  controller().SetAllPromosDisabled(true);
  controller().SetAllPromosDisabled(false);
  EXPECT_TRUE(controller().HasShowablePromo(nullptr));
  EXPECT_TRUE(ShowsPromo(kPromoId));
}

TEST_F(NtpPromoControllerTest, SuppessedSinglePromo) {
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      user_education::features::kEnableNtpBrowserPromos,
      {{"suppress-list", kPromoId}});
  CreateController();
  EXPECT_TRUE(ShowsPromo(kPromo2Id));
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
  EXPECT_TRUE(ShowsPromo(kPromo3Id));
}

TEST_F(NtpPromoControllerTest, ClickedPromoPreventsOtherPromosInSameSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  EXPECT_TRUE(ShowsPromo(kPromoId));
  controller().OnPromoClicked(kPromoId, nullptr);

  // No other promo should show in this session.
  EXPECT_FALSE(ShowsAnyPromo());

  AdvanceSession();

  // In the next session, the other promo can show.
  EXPECT_TRUE(ShowsPromo(kPromo2Id));
}

TEST_F(NtpPromoControllerTest, DismissedPromoPreventsOtherPromosInSameSession) {
  CreateController();
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  EXPECT_TRUE(ShowsPromo(kPromoId));
  controller().OnPromoDismissed(kPromoId);

  // No other promo should show in this session.
  EXPECT_FALSE(ShowsAnyPromo());

  AdvanceSession();

  // In the next session, the other promo can show.
  EXPECT_TRUE(ShowsPromo(kPromo2Id));
}

TEST_F(NtpPromoControllerTest, MaxShowTermsExhaustionCycle) {
  // This test walks through two promos burning through their session and term
  // limits, to ensure that they both eventually become non-showable. Two promos
  // are used to verify the expected ordering between them.
  RegisterPromo(kPromoId, kEligible);
  RegisterPromo(kPromo2Id, kEligible);

  auto params = GetNtpPromoControllerParams();
  params.max_sessions_per_term = 3;
  params.max_terms = 3;
  params.cool_off_duration = base::Days(180);
  CreateController(params);

  for (int term = 0; term < params.max_terms; ++term) {
    for (const auto& promo : {kPromoId, kPromo2Id}) {
      for (int session = 0; session < params.max_sessions_per_term; ++session) {
        AdvanceSession();
        EXPECT_TRUE(ShowsPromo(promo));
      }
    }

    // After both promos have finished their term, neither should show in the
    // next session because they are in their cool-off period.
    AdvanceSession();
    EXPECT_FALSE(ShowsAnyPromo());

    // Advance time to satisfy the cool-off period so the next term can begin.
    task_environment_.AdvanceClock(params.cool_off_duration);
  }

  // At this point, both promos have used up their term limit.
  // Verify that advancing sessions and clearing the cool-off duration no longer
  // allows them to show.
  AdvanceSession();
  task_environment_.AdvanceClock(params.cool_off_duration);
  EXPECT_FALSE(ShowsAnyPromo());
  EXPECT_FALSE(ShowsAnyPromo());
}

}  // namespace user_education

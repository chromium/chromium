// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_browsertest_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

namespace user_education {

namespace {

using test::kActionableAlertIPHFeature;
using test::kActionableAlertIPHFeature2;
using test::kAppName1;
using test::kAppName2;
using test::kCustomActionIPHFeature;
using test::kDefaultCustomActionIPHFeature;
using test::kKeyedPromoFeature;
using test::kKeyedPromoFeature2;
using test::kLegalNoticeFeature;
using test::kLegalNoticeFeature2;
using test::kRotatingPromoIPHFeature;
using test::kSnoozeIPHFeature;
using test::kTestIPHFeature;
using test::kTestTutorialIdentifier;
using test::kTutorialIPHFeature;

class BrowserFeaturePromoPolicyTestBaseWithPriority
    : public BrowserFeaturePromoControllerTestBase {
 public:
  BrowserFeaturePromoPolicyTestBaseWithPriority() { VerifyConstants(); }
  ~BrowserFeaturePromoPolicyTestBaseWithPriority() override = default;

 protected:
  void RegisterIPH() override {
    BrowserFeaturePromoControllerTestBase::RegisterIPH();

    FeaturePromoSpecification spec =
        DefaultPromoSpecification(kLegalNoticeFeature);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForTutorialPromo(
        kLegalNoticeFeature2, kToolbarAppMenuButtonElementId, IDS_OK,
        kTestTutorialIdentifier);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kActionableAlertIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
        IDS_OK, base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kActionableAlert);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kActionableAlertIPHFeature2, kToolbarAppMenuButtonElementId, IDS_CANCEL,
        IDS_OK, base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kActionableAlert);
    registry()->RegisterFeature(std::move(spec));
  }
};

class BrowserFeaturePromoControllerReshowTest
    : public BrowserFeaturePromoPolicyTestBaseWithPriority {
 public:
  BrowserFeaturePromoControllerReshowTest() = default;
  ~BrowserFeaturePromoControllerReshowTest() override = default;

  void RegisterIPH() override {
    BrowserFeaturePromoControllerTestBase::RegisterIPH();

    FeaturePromoSpecification spec =
        DefaultPromoSpecification(kLegalNoticeFeature);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    spec.SetReshowPolicy(base::Days(20), std::nullopt);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForTutorialPromo(
        kLegalNoticeFeature2, kToolbarAppMenuButtonElementId, IDS_OK,
        kTestTutorialIdentifier);
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    spec.SetReshowPolicy(base::Days(100), 2);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForCustomAction(
        kKeyedPromoFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL, IDS_OK,
        base::DoNothing());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kKeyedNotice);
    spec.SetReshowPolicy(base::Days(100), std::nullopt);
    registry()->RegisterFeature(std::move(spec));

    spec = FeaturePromoSpecification::CreateForToastPromo(
        kKeyedPromoFeature2, kToolbarAppMenuButtonElementId, IDS_CANCEL, IDS_OK,
        FeaturePromoSpecification::AcceleratorInfo());
    spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kKeyedNotice);
    spec.SetReshowPolicy(base::Days(20), 2);
    registry()->RegisterFeature(std::move(spec));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerReshowTest,
                       ReshowLegalNoticeWithNoLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(20)),
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo(),
                  // Promo cannot reshow again after a short time.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow again after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(20)),
                  MaybeShowPromo(kLegalNoticeFeature), ClosePromo());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerReshowTest,
                       ReshowLegalNoticeWithLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo(kLegalNoticeFeature2), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(100)),
                  MaybeShowPromo(kLegalNoticeFeature2), ClosePromo(),
                  // Promo cannot reshow again because it has reached the limit.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kPermanentlyDismissed),
                  AdvanceTime(std::nullopt, base::Days(100)),
                  MaybeShowPromo(kLegalNoticeFeature2,
                                 FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerReshowTest,
                       ReshowKeyedPromoNoLimit) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Promo can show initially.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1}), ClosePromo(),
                  // Promo cannot reshow immediately.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1},
                                 FeaturePromoResult::kBlockedByReshowDelay),

                  // Promo cannot reshow after a short period.
                  AdvanceTime(std::nullopt, base::Days(5)),
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1},
                                 FeaturePromoResult::kBlockedByReshowDelay),
                  // But for other app it can.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName2}), ClosePromo(),

                  // Promo can reshow after sufficient time.
                  AdvanceTime(std::nullopt, base::Days(99)),
                  MaybeShowPromo({kKeyedPromoFeature, kAppName1}), ClosePromo(),

                  // But other app cannot, since it has not been long enough.
                  MaybeShowPromo({kKeyedPromoFeature, kAppName2},
                                 FeaturePromoResult::kBlockedByReshowDelay));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerReshowTest,
                       ReshowKeyedPromoWithLimit) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Promo can show initially.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1}), ClosePromo(),
      // Promo cannot reshow immediately.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kBlockedByReshowDelay),

      // Promo cannot reshow after a short period.
      AdvanceTime(std::nullopt, base::Days(5)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kBlockedByReshowDelay),
      // But for other app it can.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2}), ClosePromo(),

      // Promo can reshow after sufficient time.
      AdvanceTime(std::nullopt, base::Days(19)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1}), ClosePromo(),

      // But other app cannot, since it has not been long enough.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2},
                     FeaturePromoResult::kBlockedByReshowDelay),

      // After additional time, the second app can show, but the
      // first has hit the limit.
      AdvanceTime(std::nullopt, base::Days(25)),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName1},
                     FeaturePromoResult::kPermanentlyDismissed),
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2}), ClosePromo(),

      // Both are now permanently dismissed.
      MaybeShowPromo({kKeyedPromoFeature2, kAppName2},
                     FeaturePromoResult::kPermanentlyDismissed));
}

class BrowserFeaturePromoControllerPolicyTest
    : public BrowserFeaturePromoPolicyTestBaseWithPriority {
 public:
  BrowserFeaturePromoControllerPolicyTest() = default;
  ~BrowserFeaturePromoControllerPolicyTest() override = default;

  void TearDownOnMainThread() override {
    help_bubble_.reset();
    BrowserFeaturePromoPolicyTestBaseWithPriority::TearDownOnMainThread();
  }

  auto SimulateSnoozes(const base::Feature& feature, int delta_from_max) {
    return Do([this, &feature, delta_from_max] {
      auto data = storage_service()->ReadPromoData(feature);
      if (!data) {
        data = FeaturePromoData();
      }
      data->show_count = data->snooze_count =
          user_education::features::GetMaxSnoozeCount() + delta_from_max;
      storage_service()->SavePromoData(feature, *data);
    });
  }

  bool is_help_bubble_open() const {
    return help_bubble_ && help_bubble_->is_open();
  }

  auto ShowHelpBubble() {
    return Check(
        [this]() {
          HelpBubbleParams bubble_params;
          bubble_params.body_text = l10n_util::GetStringUTF16(IDS_CHROME_TIP);
          help_bubble_ = bubble_factory()->CreateHelpBubble(
              GetAnchorElement(), std::move(bubble_params));
          return is_help_bubble_open();
        },
        "ShowHelpBubble()");
  }

 private:
  std::unique_ptr<user_education::HelpBubble> help_bubble_;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       TwoLowPriorityPromos) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod), MaybeShowPromo(kTestIPHFeature),
      ExpectShowingPromo(&kTestIPHFeature),
      MaybeShowPromo(kCustomActionIPHFeature,
                     FeaturePromoResult::kBlockedByPromo,
                     user_education::features::GetLowPriorityTimeout()),
      ExpectShowingPromo(&kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       ActionableAlertOverridesLowPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  MaybeShowPromo(kActionableAlertIPHFeature),
                  ExpectShowingPromo(&kActionableAlertIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       TwoActionableAlerts) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kActionableAlertIPHFeature),
      ExpectShowingPromo(&kActionableAlertIPHFeature),
      MaybeShowPromo(kActionableAlertIPHFeature2,
                     FeaturePromoResult::kBlockedByPromo,
                     user_education::features::GetMediumPriorityTimeout()),
      ExpectShowingPromo(&kActionableAlertIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       LegalNoticeOverridesLowPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature),
                  ExpectShowingPromo(&kLegalNoticeFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       LegalNoticeOverridesActionableAlert) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature),
                  ExpectShowingPromo(&kLegalNoticeFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       TwoLegalNotices) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kLegalNoticeFeature2),
      ExpectShowingPromo(&kLegalNoticeFeature2),
      MaybeShowPromo(kLegalNoticeFeature, FeaturePromoResult::kBlockedByPromo,
                     user_education::features::GetHighPriorityTimeout()),
      ExpectShowingPromo(&kLegalNoticeFeature2));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodBlocksHeavyweightInV2) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 FeaturePromoResult::kBlockedByGracePeriod));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodDoesNotBlockLightweightInV2) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod),
      MaybeShowPromo(kTestIPHFeature, FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodDoesNotBlockHeavyweightLegalNotice) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod),
      MaybeShowPromo(kLegalNoticeFeature2, FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodDoesNotBlockActionableAlert) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature2,
                                 FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodBlocksHeavyweightInV2AfterNewSession) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  AdvanceTime(kMoreThanNewSession),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 FeaturePromoResult::kBlockedByGracePeriod));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       GracePeriodDoesNotBlocksHeavyweightLongAfterNewSession) {
  RunTestSequence(
      ResetSessionData(base::Seconds(60)), AdvanceTime(kMoreThanNewSession),
      AdvanceTime(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       CooldownPreventsPromoInV2) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kCustomActionIPHFeature,
                                 FeaturePromoResult::kBlockedByCooldown));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       CooldownDoesNotPreventLightweightPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       LightweightPromoDoesNotTriggerCooldown) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod), MaybeShowPromo(kTestIPHFeature),
      ClosePromo(), AdvanceTime(kLessThanCooldown),
      AdvanceTime(kMoreThanGracePeriod), MaybeShowPromo(kTutorialIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       CooldownDoesNotPreventLegalNotice) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kLegalNoticeFeature2));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       CooldownDoesNotPreventActionableAlert) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kActionableAlertIPHFeature2));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       ExpiredCooldownDoesNotPreventPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kMoreThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kCustomActionIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       AbortedPromoDoesNotTriggerCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Show an immediately close the promo without user
                  // interaction.
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  // Immediately try another promo.
                  MaybeShowPromo(kCustomActionIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       AbortedPromoDoesTriggerIndividualCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  AdvanceTime(kLessThanAbortCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 FeaturePromoResult::kRecentlyAborted));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       AbortedPromoDoesNotTriggerSnooze) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
      AdvanceTime(kMoreThanAbortCooldown), AdvanceTime(kMoreThanGracePeriod),
      // V1 uses full snooze time for aborted promos.
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       SnoozeButtonDisappearsInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kSnoozeIPHFeature, -1),
      // Show a snoozeable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and verify that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsureNotPresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       TutorialSnoozeButtonChangesInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kTutorialIPHFeature, -1),
      // Show a snoozeable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),

      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and verify that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
                        &views::LabelButton::GetText,
                        l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       IdleAtStartupStillShowsPromo) {
  RunTestSequence(
      ResetSessionData(base::TimeDelta()),
      AdvanceTime(std::nullopt, kLessThanNewSession, true),
      AdvanceTime(base::Seconds(15), base::Milliseconds(100), false),
      MaybeShowPromo(kTutorialIPHFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPolicyTest,
                       IdleAtStartupPromoBlockedByNewSession) {
  RunTestSequence(
      ResetSessionData(base::TimeDelta()),
      AdvanceTime(std::nullopt, kMoreThanNewSession, true),
      AdvanceTime(base::Seconds(15), base::Milliseconds(100), false),
      MaybeShowPromo(kTutorialIPHFeature,
                     FeaturePromoResult::kBlockedByGracePeriod));
}

}  // namespace
}  // namespace user_education

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
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_browsertest_base.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/state_observer.h"

namespace user_education {

namespace {

using test::kActionableAlertIPHFeature;
using test::kActionableAlertIPHFeature2;
using test::kLegalNoticeFeature;
using test::kLegalNoticeFeature2;
using test::kSnoozeIPHFeature;
using test::kTestIPHFeature;
using test::kTestTutorialIdentifier;
using test::kTutorialIPHFeature;

class StartupCallbackObserver : public ui::test::StateObserver<bool> {
 public:
  StartupCallbackObserver(
      base::MockCallback<FeaturePromoController::ShowPromoResultCallback>*
          callback,
      FeaturePromoResult expected) {
    EXPECT_CALL(*callback, Run)
        .WillOnce([this, expected](FeaturePromoResult result) {
          ASSERT_EQ(expected, result);
          OnStateObserverStateChanged(true);
        });
  }
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(StartupCallbackObserver,
                                    kStartupCallbackState);

class RequiredNotice {
 public:
  explicit RequiredNotice(Browser* browser)
      : controller_(UserEducationServiceFactory::GetForBrowserContext(
                        browser->GetProfile())
                        ->product_messaging_controller()) {}
  RequiredNotice(const RequiredNotice&) = delete;
  void operator=(const RequiredNotice&) = delete;
  ~RequiredNotice() = default;

  void Request(ProductMessageKey key) {
    CHECK(!key_);
    CHECK(!handle_);
    key_ = key;
    controller_->QueueMessage(key_,
                              base::BindOnce(&RequiredNotice::OnPriority,
                                             weak_ptr_factory_.GetWeakPtr()));
  }

  void Release() {
    CHECK(handle_);
    CHECK(!key_);
    handle_.reset();
  }

  bool has_priority() const { return !!handle_; }

  auto AddOnPriorityCallback(base::OnceClosure callback) {
    return callbacks_.Add(std::move(callback));
  }

 private:
  void OnPriority(ProductMessagingHandle handle) {
    handle_ = std::move(handle);
    key_ = ProductMessageKey();
    callbacks_.Notify();
  }

  ProductMessageKey key_;
  ProductMessagingHandle handle_;
  base::OnceCallbackList<void()> callbacks_;
  const raw_ref<ProductMessagingController> controller_;
  base::WeakPtrFactory<RequiredNotice> weak_ptr_factory_{this};
};

class NoticeCallbackObserver : public ui::test::StateObserver<bool> {
 public:
  explicit NoticeCallbackObserver(RequiredNotice* notice)
      : sub_(notice->AddOnPriorityCallback(
            base::BindOnce(&NoticeCallbackObserver::OnNotice,
                           base::Unretained(this)))) {}

 private:
  void OnNotice() { OnStateObserverStateChanged(true); }

  base::CallbackListSubscription sub_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(NoticeCallbackObserver,
                                    kNoticeCallbackState);

DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kRequiredNoticeId,
                                 ProductMessageType::kLegalOrComplianceNotice);

class BrowserFeaturePromoControllerPriorityTest
    : public BrowserFeaturePromoControllerTestBase {
 public:
  BrowserFeaturePromoControllerPriorityTest() { VerifyConstants(); }
  ~BrowserFeaturePromoControllerPriorityTest() override = default;

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

  auto MaybeShowStartupPromo(FeaturePromoParams params) {
    // Must be computed before `Do()`, which consumes `params`.
    const std::string caller =
        base::StrCat({"MaybeShowStartupPromo( ", params.feature->name, " )"});
    auto steps = Steps(WithElement(
        kBrowserViewElementId,
        [this, p = std::move(params)](ui::TrackedElement* el) mutable {
          // This is insurance, a parameter could be added to
          // specify whether the feature is expected to check the
          // tracker or not.
          EXPECT_CALL(*mock_tracker(),
                      ShouldTriggerHelpUI(testing::Ref(*p.feature)))
              .WillRepeatedly(testing::Return(true));
          controller()->MaybeShowStartupPromo(std::move(p),
                                              user_education_context());
        }));
    AddDescriptionPrefix(steps, caller);
    return steps;
  }

  auto MaybeShowStartupPromo(const base::Feature& feature) {
    return MaybeShowStartupPromo(FeaturePromoParams(feature));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       MultipleStartupPromosHighPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowStartupPromo(kLegalNoticeFeature),
                  MaybeShowStartupPromo(kLegalNoticeFeature2),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature),
                  // This is required so we don't try to close on the same call
                  // stack as the bubble was shown on.
                  ClosePromo(),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature2),
                  // This is required so we don't try to close on the same call
                  // stack as the bubble was shown on.
                  ClosePromo());
}

IN_PROC_BROWSER_TEST_F(
    BrowserFeaturePromoControllerPriorityTest,
    MultipleStartupPromosHighPriorityToastThenLowPriorityAllowed) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  FeaturePromoParams second_params(kSnoozeIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(  // Since the second promo cannot show during grace period,
                    // assume this is a browser restart during a session.
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &second_promo_callback,
                   FeaturePromoResult::Success()),
      MaybeShowStartupPromo(kLegalNoticeFeature),
      MaybeShowStartupPromo(std::move(second_params)),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kLegalNoticeFeature),
      // This is required so we don't try to close on the same call
      // stack as the bubble was shown on.
      ClosePromo(), WaitForState(kStartupCallbackState, true),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kSnoozeIPHFeature), ClosePromo());
}

IN_PROC_BROWSER_TEST_F(
    BrowserFeaturePromoControllerPriorityTest,
    MultipleStartupPromosHighPriorityLowPriorityToastAllowedAfterHeavyweight) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  FeaturePromoParams second_params(kTestIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  ObserveState(kStartupCallbackState, &second_promo_callback,
                               FeaturePromoResult::Success()),
                  MaybeShowStartupPromo(kLegalNoticeFeature2),
                  MaybeShowStartupPromo(std::move(second_params)),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature2),
                  // This is required so we don't try to close on the same call
                  // stack as the bubble was shown on.
                  ClosePromo(), WaitForState(kStartupCallbackState, true),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kTestIPHFeature), ClosePromo());
}

IN_PROC_BROWSER_TEST_F(
    BrowserFeaturePromoControllerPriorityTest,
    MultipleStartupPromosHighPriorityLowPriorityBlockedAfterHeavyweight) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  FeaturePromoParams second_params(kSnoozeIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RunTestSequence(  // Since the second promo cannot show during grace period,
                    // assume this is a browser restart during a session.
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &second_promo_callback,
                   FeaturePromoResult::kBlockedByCooldown),
      MaybeShowStartupPromo(kLegalNoticeFeature2),
      MaybeShowStartupPromo(std::move(second_params)),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ExpectShowingPromo(&kLegalNoticeFeature2),
      // This is required so we don't try to close on the same call
      // stack as the bubble was shown on.
      ClosePromo(), WaitForState(kStartupCallbackState, true));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       RequiredNoticeDelaysLegalNotice) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         promo_callback);
  FeaturePromoParams params(kLegalNoticeFeature);
  params.show_promo_result_callback = promo_callback.Get();
  RequiredNotice notice(browser());
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      ObserveState(kStartupCallbackState, &promo_callback,
                   FeaturePromoResult::Success()),
      ObserveState(kNoticeCallbackState, &notice),
      // Request notice first before startup promo.
      Do([&notice]() { notice.Request(kRequiredNoticeId); }),
      MaybeShowStartupPromo(std::move(params)),
      // Wait for notice to pop and ensure that the startup promo wasn't shown.
      WaitForState(kNoticeCallbackState, true),
      WaitForState(kStartupCallbackState, false),
      // Release the notice and verify the promo shows.
      Do([&notice]() { notice.Release(); }),
      WaitForState(kStartupCallbackState, true),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      ClosePromo());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       LegalNoticeDelaysRequiredNotice) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         promo_callback);
  RequiredNotice notice(browser());
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  ObserveState(kNoticeCallbackState, &notice),
                  MaybeShowStartupPromo(kLegalNoticeFeature),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  Do([&notice]() { notice.Request(kRequiredNoticeId); }),
                  WaitForState(kNoticeCallbackState, false), ClosePromo(),
                  WaitForState(kNoticeCallbackState, true),
                  Do([&notice]() { notice.Release(); }));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       MultipleStartupPromosHighThenNoticeThenLow) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         second_promo_callback);
  FeaturePromoParams second_params(kTestIPHFeature);
  second_params.show_promo_result_callback = second_promo_callback.Get();
  RequiredNotice notice(browser());
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Observe the notice and the second callback.
                  ObserveState(kNoticeCallbackState, &notice),
                  ObserveState(kStartupCallbackState, &second_promo_callback,
                               FeaturePromoResult::Success()),
                  // Queue both promos and wait for the first to show.
                  MaybeShowStartupPromo(kLegalNoticeFeature2),
                  MaybeShowStartupPromo(std::move(second_params)),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kLegalNoticeFeature2),
                  // Request the notice and verify it doesn't pop.
                  Do([&notice]() { notice.Request(kRequiredNoticeId); }),
                  CheckState(kNoticeCallbackState, false),
                  // Close the promo and verify the notice (and not the other
                  // promo) pops. This is required so we don't try to close on
                  // the same call stack as the bubble was shown on.
                  ClosePromo(), WaitForState(kNoticeCallbackState, true),
                  WaitForState(kStartupCallbackState, false),
                  // Release the notice and verify the final promo is shown.
                  Do([&notice]() { notice.Release(); }),
                  WaitForState(kStartupCallbackState, true),
                  WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
                  ExpectShowingPromo(&kTestIPHFeature), ClosePromo());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       RegularPromoBlockedWhenPromoIsQueued) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowStartupPromo(kLegalNoticeFeature2),
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::kBlockedByPromo,
                     features::GetLowPriorityTimeout()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       LegalNoticeNotBlockedWhenPromoIsQueued) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowStartupPromo(kSnoozeIPHFeature),
                  MaybeShowPromo(kLegalNoticeFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerPriorityTest,
                       SecondPromoNotCanceledWhenFirstQueuedPromoIsOverridden) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowStartupPromo(kSnoozeIPHFeature),
      MaybeShowStartupPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),

      CheckPromoStatus(kSnoozeIPHFeature, FeaturePromoStatus::kBubbleShowing),
      InParallel(RunSubsequence(MaybeShowPromo(kLegalNoticeFeature)),
                 RunSubsequence(WaitForShow(
                     HelpBubbleView::kHelpBubbleElementIdForTesting, true))),

      CheckPromoStatus(kLegalNoticeFeature, FeaturePromoStatus::kBubbleShowing),
      CheckPromoStatus(kSnoozeIPHFeature, FeaturePromoStatus::kNotRunning),
      CheckPromoStatus(kTutorialIPHFeature, FeaturePromoStatus::kQueued));
}

}  // namespace

}  // namespace user_education

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
BASE_FEATURE(kFeaturePromoLifecycleTestPromo,
             "FeaturePromoLifecycleTestPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestPromo2,
             "FeaturePromoLifecycleTestPromo2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestPromo3,
             "FeaturePromoLifecycleTestPromo3",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

using TestBase = InteractiveBrowserTestT<web_app::WebAppControllerBrowserTest>;

class FeaturePromoLifecycleUiTest : public TestBase {
 public:
  FeaturePromoLifecycleUiTest() {
    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker));
    scoped_feature_list_.InitAndEnableFeatures(
        {kFeaturePromoLifecycleTestPromo});
    disable_active_checks_ = user_education::FeaturePromoControllerCommon::
        BlockActiveWindowCheckForTesting();
  }
  ~FeaturePromoLifecycleUiTest() override = default;

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    for (auto& promo : CreatePromos()) {
      GetPromoController(browser())->registry()->RegisterFeature(
          std::move(promo));
    }
  }

 protected:
  using PromoData = user_education::FeaturePromoData;

  using SpecList = std::vector<user_education::FeaturePromoSpecification>;
  virtual SpecList CreatePromos() {
    SpecList promos;
    promos.emplace_back(
        user_education::FeaturePromoSpecification::CreateForSnoozePromo(
            kFeaturePromoLifecycleTestPromo, kToolbarAppMenuButtonElementId,
            IDS_TAB_GROUPS_NEW_GROUP_PROMO));
    return promos;
  }

  auto InBrowser(base::OnceCallback<void(Browser*)> callback) {
    return WithView(kBrowserViewElementId,
                    base::BindOnce(
                        [](base::OnceCallback<void(Browser*)> callback,
                           BrowserView* browser_view) {
                          std::move(callback).Run(browser_view->browser());
                        },
                        std::move(callback)));
  }

  auto CheckBrowser(base::OnceCallback<bool(Browser*)> callback) {
    return CheckView(
        kBrowserViewElementId,
        base::BindOnce(
            [](base::OnceCallback<bool(Browser*)> callback,
               BrowserView* browser_view) {
              return std::move(callback).Run(browser_view->browser());
            },
            std::move(callback)));
  }

  auto CheckSnoozePrefs(bool is_dismissed, int show_count, int snooze_count) {
    return CheckBrowser(base::BindLambdaForTesting(
        [this, is_dismissed, show_count, snooze_count](Browser* browser) {
          auto data = GetStorageService(browser)->ReadPromoData(
              kFeaturePromoLifecycleTestPromo);

          if (!data.has_value()) {
            return false;
          }

          EXPECT_EQ(data->is_dismissed, is_dismissed);
          EXPECT_EQ(data->show_count, show_count);
          EXPECT_EQ(data->snooze_count, snooze_count);

          // last_show_time is only meaningful if a show has occurred.
          if (data->show_count > 0) {
            EXPECT_GE(data->last_show_time, last_show_time_.first);
            EXPECT_LE(data->last_show_time, last_show_time_.second);
          }

          // last_snooze_time is only meaningful if a snooze has occurred.
          if (data->snooze_count > 0) {
            EXPECT_GE(data->last_snooze_time, last_snooze_time_.first);
            EXPECT_LE(data->last_snooze_time, last_snooze_time_.second);
          }

          return !testing::Test::HasNonfatalFailure();
        }));
  }

  auto SetSnoozePrefs(const PromoData& data) {
    return InBrowser(base::BindLambdaForTesting([data](Browser* browser) {
      GetStorageService(browser)->SavePromoData(kFeaturePromoLifecycleTestPromo,
                                                data);
    }));
  }

  // Tries to show tab groups IPH by meeting the trigger conditions. If
  // |should_show| is true it checks that it was shown. If false, it
  // checks that it was not shown.
  auto AttemptIPH(
      bool should_show,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    return CheckBrowser(base::BindLambdaForTesting(
        [this, should_show, feature](Browser* browser) {
          auto* const tracker = GetTracker(browser);
          if (should_show) {
            last_show_time_.first = base::Time::Now();
            EXPECT_CALL(*tracker, ShouldTriggerHelpUI(Ref(*feature)))
                .WillOnce(Return(true));
          } else {
            EXPECT_CALL(*tracker, ShouldTriggerHelpUI(Ref(*feature))).Times(0);
          }

          if (should_show !=
              GetPromoController(browser)->MaybeShowPromo(*feature)) {
            LOG(ERROR) << "MaybeShowPromo did not return expected value.";
            return false;
          }

          if (should_show !=
              GetPromoController(browser)->IsPromoActive(*feature)) {
            LOG(ERROR) << "IsPromoActive did not return expected value.";
            return false;
          }

          // If shown, Tracker::Dismissed should be called eventually.
          if (should_show) {
            EXPECT_CALL(*tracker, Dismissed(Ref(*feature)));
            last_show_time_.second = base::Time::Now();
          }

          return true;
        }));
  }

  auto SnoozeIPH() {
    return Steps(
        Do(base::BindLambdaForTesting(
            [this]() { last_snooze_time_.first = base::Time::Now(); })),
        PressButton(
            user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        Do(base::BindLambdaForTesting(
            [this]() { last_snooze_time_.second = base::Time::Now(); })));
  }

  auto DismissIPH() {
    return Steps(
        PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        FlushEvents(), CheckBrowser(base::BindOnce([](Browser* browser) {
          auto* const promo = GetPromoController(browser)->current_promo_.get();
          return !promo || (!promo->is_promo_active() && !promo->help_bubble());
        })));
  }

  auto AbortIPH(
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    return InBrowser(base::BindLambdaForTesting([feature](Browser* browser) {
      GetPromoController(browser)->EndPromo(
          *feature, user_education::EndFeaturePromoReason::kAbortPromo);
    }));
  }

  auto CheckDismissed(
      bool dismissed,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    return CheckBrowser(
        base::BindLambdaForTesting([dismissed, feature](Browser* browser) {
          return GetPromoController(browser)->HasPromoBeenDismissed(*feature) ==
                 dismissed;
        }));
  }

  auto CheckDismissedWithReason(
      user_education::FeaturePromoClosedReason close_reason,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    return CheckBrowser(
        base::BindLambdaForTesting([close_reason, feature](Browser* browser) {
          user_education::FeaturePromoClosedReason actual_reason;
          return GetPromoController(browser)->HasPromoBeenDismissed(
                     *feature, &actual_reason) &&
                 actual_reason == close_reason;
        }));
  }

  static BrowserFeaturePromoController* GetPromoController(Browser* browser) {
    return static_cast<BrowserFeaturePromoController*>(
        browser->window()->GetFeaturePromoController());
  }

  static user_education::FeaturePromoStorageService* GetStorageService(
      Browser* browser) {
    return GetPromoController(browser)->storage_service();
  }

  static NiceMock<feature_engagement::test::MockTracker>* GetTracker(
      Browser* browser) {
    return static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser->profile()));
  }

 private:
  static void RegisterMockTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow any other IPH to call, but don't ever show them.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return mock_tracker;
  }

  std::pair<base::Time, base::Time> last_show_time_;
  std::pair<base::Time, base::Time> last_snooze_time_;

  feature_engagement::test::ScopedIphFeatureList scoped_feature_list_;
  base::CallbackListSubscription subscription_;
  user_education::FeaturePromoControllerCommon::TestLock disable_active_checks_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, DismissDoesNotSnooze) {
  RunTestSequence(AttemptIPH(true), DismissIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ true,
                                   /* show_count */ 1,
                                   /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, SnoozeSetsCorrectTime) {
  RunTestSequence(AttemptIPH(true), SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 1,
                                   /* snooze_count */ 1));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, HasPromoBeenDismissed) {
  RunTestSequence(CheckDismissed(false), AttemptIPH(true), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       HasPromoBeenDismissedWithReason) {
  RunTestSequence(AttemptIPH(true), DismissIPH(),
                  CheckDismissedWithReason(
                      user_education::FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, CanReSnooze) {
  // Simulate the user snoozing the IPH.
  PromoData data;
  data.is_dismissed = false;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_snooze_time =
      base::Time::Now() - user_education::features::GetSnoozeDuration();
  data.last_show_time = data.last_snooze_time - base::Seconds(1);

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(true), SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 2,
                                   /* snooze_count */ 2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, DoesNotShowIfDismissed) {
  PromoData data;
  data.is_dismissed = true;
  data.show_count = 1;
  data.snooze_count = 0;

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       DoesNotShowBeforeSnoozeDuration) {
  PromoData data;
  data.is_dismissed = false;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_snooze_time = base::Time::Now();
  data.last_show_time = data.last_snooze_time - base::Seconds(1);

  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, AbortPromoSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true), AbortIPH(),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, EndPromoSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true), InBrowser(base::BindOnce([](Browser* browser) {
        GetPromoController(browser)->EndPromo(
            kFeaturePromoLifecycleTestPromo,
            user_education::EndFeaturePromoReason::kFeatureEngaged);
      })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ true,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, WidgetCloseSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](user_education::HelpBubbleView* bubble) {
                 bubble->GetWidget()->CloseWithReason(
                     views::Widget::ClosedReason::kEscKeyPressed);
               })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, AnchorHideSetsPrefs) {
  RunTestSequence(
      AttemptIPH(true),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](user_education::HelpBubbleView* bubble) {
                 // This should yank the bubble out from under us.
                 bubble->GetAnchorView()->SetVisible(false);
               })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, WorkWithoutNonClickerData) {
  PromoData data;
  data.is_dismissed = false;
  data.snooze_count = 1;
  data.last_snooze_time =
      base::Time::Now() - user_education::features::GetSnoozeDuration();

  // Non-clicker policy shipped pref entries that don't exist before.
  // Make sure empty entries are properly handled.
  RunTestSequence(SetSnoozePrefs(data), AttemptIPH(true));
}

class FeaturePromoLifecycleAppUiTest : public FeaturePromoLifecycleUiTest {
 public:
  FeaturePromoLifecycleAppUiTest() = default;
  ~FeaturePromoLifecycleAppUiTest() override = default;

  static constexpr char kApp1Url[] = "http://example.org/";
  static constexpr char kApp2Url[] = "http://foo.com/";

  void SetUpOnMainThread() override {
    FeaturePromoLifecycleUiTest::SetUpOnMainThread();
    app1_id_ = InstallPWA(GURL(kApp1Url));
    app2_id_ = InstallPWA(GURL(kApp2Url));
  }

  auto CheckShownForApp() {
    return CheckBrowser(base::BindOnce([](Browser* browser) {
      const auto data = GetStorageService(browser)->ReadPromoData(
          kFeaturePromoLifecycleTestPromo);
      return base::Contains(data->shown_for_apps,
                            browser->app_controller()->app_id());
    }));
  }

 protected:
  webapps::AppId app1_id_;
  webapps::AppId app2_id_;

 private:
  SpecList CreatePromos() override {
    SpecList promos;
    promos.emplace_back(std::move(
        user_education::FeaturePromoSpecification::CreateForLegacyPromo(
            &kFeaturePromoLifecycleTestPromo, kToolbarAppMenuButtonElementId,
            IDS_TAB_GROUPS_NEW_GROUP_PROMO)
            .SetPromoSubtype(user_education::FeaturePromoSpecification::
                                 PromoSubtype::kPerApp)));
    return promos;
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForApp) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(app_browser->window()->GetElementContext(),
                           WaitForShow(kToolbarAppMenuButtonElementId),
                           AttemptIPH(true), DismissIPH(), CheckShownForApp());
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForAppThenBlocked) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(app_browser->window()->GetElementContext(),
                           WaitForShow(kToolbarAppMenuButtonElementId),
                           AttemptIPH(true), DismissIPH(), FlushEvents(),
                           AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, HasPromoBeenDismissed) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(app_browser->window()->GetElementContext(),
                           WaitForShow(kToolbarAppMenuButtonElementId),
                           CheckDismissed(false), AttemptIPH(true),
                           DismissIPH(), CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForTwoApps) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  Browser* const app_browser2 = LaunchWebAppBrowser(app2_id_);
  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(), AttemptIPH(true),
      WaitForShow(kToolbarAppMenuButtonElementId), DismissIPH(), FlushEvents(),
      InContext(app_browser2->window()->GetElementContext(),
                Steps(WaitForShow(kToolbarAppMenuButtonElementId),
                      AttemptIPH(true), DismissIPH(), CheckShownForApp())));
}

class FeaturePromoLifecycleCriticaUiTest : public FeaturePromoLifecycleUiTest {
 public:
  FeaturePromoLifecycleCriticaUiTest() = default;
  ~FeaturePromoLifecycleCriticaUiTest() override = default;

  auto CheckDismissed(
      bool dismissed,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    return CheckBrowser(
        base::BindLambdaForTesting([dismissed, feature](Browser* browser) {
          const auto data = GetStorageService(browser)->ReadPromoData(*feature);
          return (data && data->is_dismissed) == dismissed;
        }));
  }

 private:
  SpecList CreatePromos() override {
    SpecList result;
    result.emplace_back(
        user_education::FeaturePromoSpecification::CreateForLegacyPromo(
            &kFeaturePromoLifecycleTestPromo, kToolbarAppMenuButtonElementId,
            IDS_TAB_GROUPS_NEW_GROUP_PROMO));
    result.back().set_promo_subtype_for_testing(
        user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    result.emplace_back(
        user_education::FeaturePromoSpecification::CreateForLegacyPromo(
            &kFeaturePromoLifecycleTestPromo2, kToolbarAppMenuButtonElementId,
            IDS_TAB_GROUPS_NAMED_GROUP_TOOLTIP));
    result.back().set_promo_subtype_for_testing(
        user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    result.emplace_back(
        user_education::FeaturePromoSpecification::CreateForLegacyPromo(
            &kFeaturePromoLifecycleTestPromo3, kToolbarAppMenuButtonElementId,
            IDS_TAB_GROUPS_UNNAMED_GROUP_TOOLTIP));
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest, ShowCriticalPromo) {
  RunTestSequence(CheckDismissed(false), AttemptIPH(true), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest,
                       CannotRepeatDismissedPromo) {
  RunTestSequence(AttemptIPH(true), DismissIPH(), FlushEvents(),
                  AttemptIPH(false));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest, ReshowAfterAbort) {
  RunTestSequence(AttemptIPH(true), AbortIPH(), CheckDismissed(false),
                  AttemptIPH(true), DismissIPH(), CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest,
                       HasPromoBeenDismissed) {
  RunTestSequence(CheckDismissed(false), AttemptIPH(true), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest,
                       ShowSecondAfterDismiss) {
  RunTestSequence(
      AttemptIPH(true, &kFeaturePromoLifecycleTestPromo), DismissIPH(),
      CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
      AttemptIPH(true, &kFeaturePromoLifecycleTestPromo2), DismissIPH(),
      CheckDismissed(true, &kFeaturePromoLifecycleTestPromo2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest,
                       CriticalBlocksCritical) {
  RunTestSequence(AttemptIPH(true, &kFeaturePromoLifecycleTestPromo),
                  AttemptIPH(false, &kFeaturePromoLifecycleTestPromo2),
                  DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestPromo2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticaUiTest,
                       CriticalCancelsNormal) {
  RunTestSequence(AttemptIPH(true, &kFeaturePromoLifecycleTestPromo3),
                  AttemptIPH(true, &kFeaturePromoLifecycleTestPromo),
                  DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestPromo3));
}

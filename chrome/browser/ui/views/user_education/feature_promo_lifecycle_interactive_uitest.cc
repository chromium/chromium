// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <sstream>
#include <utility>

#include "build/build_config.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
BASE_FEATURE(kFeaturePromoLifecycleTestPromo,
             "TEST_FeaturePromoLifecycleTestPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestPromo2,
             "TEST_FeaturePromoLifecycleTestPromo2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestPromo3,
             "TEST_FeaturePromoLifecycleTestPromo3",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestAlert,
             "TEST_FeaturePromoLifecycleTestAlert",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeaturePromoLifecycleTestAlert2,
             "TEST_FeaturePromoLifecycleTestAlert2",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

using TestBase = InteractiveFeaturePromoTestT<web_app::WebAppBrowserTestBase>;
using user_education::FeaturePromoClosedReason;
using user_education::FeaturePromoResult;

class FeaturePromoLifecycleUiTest : public TestBase {
 public:
  FeaturePromoLifecycleUiTest()
      : TestBase(UseMockTracker(), ClockMode::kUseDefaultClock) {}
  ~FeaturePromoLifecycleUiTest() override = default;

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    RegisterPromos();
  }

 protected:
  using PromoData = user_education::FeaturePromoData;

  virtual void RegisterPromos() {
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForSnoozePromo(
            kFeaturePromoLifecycleTestPromo, kToolbarAppMenuButtonElementId,
            IDS_OK));
  }

  auto InBrowser(base::OnceCallback<void(Browser*)> callback) {
    return std::move(
        WithView(kBrowserViewElementId,
                 base::BindOnce(
                     [](base::OnceCallback<void(Browser*)> callback,
                        BrowserView* browser_view) {
                       std::move(callback).Run(browser_view->browser());
                     },
                     std::move(callback)))
            .SetDescription("InBrowser()"));
  }

  auto CheckBrowser(base::OnceCallback<bool(Browser*)> callback) {
    return std::move(
        CheckView(kBrowserViewElementId,
                  base::BindOnce(
                      [](base::OnceCallback<bool(Browser*)> callback,
                         BrowserView* browser_view) {
                        return std::move(callback).Run(browser_view->browser());
                      },
                      std::move(callback)))
            .SetDescription("CheckBrowser()"));
  }

  auto ShowPromoRecordingTime(const base::Feature& feature) {
    auto steps =
        Steps(Do([this]() { last_show_time_.first = base::Time::Now(); }),
              MaybeShowPromo(feature),
              Do([this]() { last_show_time_.second = base::Time::Now(); }));
    AddDescription(steps, "ShowPromoRecordingTime() - %s");
    return steps;
  }

  auto CheckSnoozePrefs(bool is_dismissed, int show_count, int snooze_count) {
    return std::move(
        CheckBrowser(
            base::BindLambdaForTesting([this, is_dismissed, show_count,
                                        snooze_count](Browser* browser) {
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
            }))
            .SetDescription(base::StringPrintf("CheckSnoozePrefs(%s, %d, %d)",
                                               is_dismissed ? "true" : "false",
                                               show_count, snooze_count)));
  }

  auto SetSnoozePrefs(const PromoData& data) {
    return InBrowser(base::BindLambdaForTesting([data](Browser* browser) {
      GetStorageService(browser)->SavePromoData(kFeaturePromoLifecycleTestPromo,
                                                data);
    }));
  }

  auto SnoozeIPH() {
    auto steps = Steps(
        Do([this]() { last_snooze_time_.first = base::Time::Now(); }),
        PressButton(
            user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        Do([this]() { last_snooze_time_.second = base::Time::Now(); }));
    AddDescription(steps, "SnoozeIPH(%s)");
    return steps;
  }

  auto DismissIPH() {
    auto steps = Steps(
        PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckBrowser(base::BindOnce([](Browser* browser) {
          auto* const promo = GetPromoController(browser)->current_promo_.get();
          return !promo || (!promo->is_promo_active() && !promo->help_bubble());
        })));
    AddDescription(steps, "DismissIPH(%s)");
    return steps;
  }

  auto CheckDismissed(
      bool dismissed,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo,
      const std::string& key = std::string()) {
    return std::move(
        CheckBrowser(base::BindLambdaForTesting([dismissed, feature,
                                                 key](Browser* browser) {
          return GetPromoController(browser)->HasPromoBeenDismissed(
                     {*feature, key}) == dismissed;
        }))
            .SetDescription(base::StringPrintf("CheckDismissed(%s, %s)",
                                               dismissed ? "true" : "false",
                                               feature->name)));
  }

  auto CheckDismissedWithReason(
      user_education::FeaturePromoClosedReason close_reason,
      const base::Feature* feature = &kFeaturePromoLifecycleTestPromo) {
    std::ostringstream desc;
    desc << "CheckDismissedWithReason(" << close_reason << ", " << feature->name
         << ")";
    return std::move(
        CheckBrowser(base::BindLambdaForTesting([close_reason,
                                                 feature](Browser* browser) {
          user_education::FeaturePromoClosedReason actual_reason;
          return GetPromoController(browser)->HasPromoBeenDismissed(
                     *feature, &actual_reason) &&
                 actual_reason == close_reason;
        })).SetDescription(desc.str()));
  }

  auto CheckMessageActionHistogram(const base::Feature& feature,
                                   FeaturePromoClosedReason bucket,
                                   int expected_count = 1) {
    const std::string name =
        base::StrCat({"UserEducation.MessageAction.", feature.name});
    return std::move(Do([this, name, bucket, expected_count]() {
                       histogram_tester_.ExpectBucketCount(name, bucket,
                                                           expected_count);
                     })
                         .SetDescription(base::StringPrintf(
                             "CheckHistogram(%s)", name.c_str())));
  }

  static BrowserFeaturePromoController* GetPromoController(Browser* browser) {
    return static_cast<BrowserFeaturePromoController*>(
        browser->window()->GetFeaturePromoControllerForTesting());
  }

  static user_education::FeaturePromoStorageService* GetStorageService(
      Browser* browser) {
    return GetPromoController(browser)->storage_service();
  }

 private:
  std::pair<base::Time, base::Time> last_show_time_;
  std::pair<base::Time, base::Time> last_snooze_time_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, DismissDoesNotSnooze) {
  RunTestSequence(ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
                  DismissIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ true,
                                   /* show_count */ 1,
                                   /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, SnoozeSetsCorrectTime) {
  RunTestSequence(ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
                  SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 1,
                                   /* snooze_count */ 1));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, HasPromoBeenDismissed) {
  RunTestSequence(CheckDismissed(false),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       HasPromoBeenDismissedWithReason) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
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

  RunTestSequence(SetSnoozePrefs(data),
                  ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
                  SnoozeIPH(),
                  CheckSnoozePrefs(/* is_dismiss */ false,
                                   /* show_count */ 2,
                                   /* snooze_count */ 2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, DoesNotShowIfDismissed) {
  PromoData data;
  data.is_dismissed = true;
  data.show_count = 1;
  data.snooze_count = 0;

  RunTestSequence(SetSnoozePrefs(data),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo,
                                 FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       DoesNotShowBeforeSnoozeDuration) {
  PromoData data;
  data.is_dismissed = false;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_snooze_time = base::Time::Now();
  data.last_show_time = data.last_snooze_time - base::Seconds(1);

  RunTestSequence(SetSnoozePrefs(data),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo,
                                 FeaturePromoResult::kSnoozed));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, AbortPromoSetsPrefs) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      AbortPromo(kFeaturePromoLifecycleTestPromo),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, EndPromoSetsPrefs) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      InBrowser(base::BindOnce([](Browser* browser) {
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
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](user_education::HelpBubbleView* bubble) {
                 bubble->GetWidget()->CloseWithReason(
                     views::Widget::ClosedReason::kUnspecified);
               })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, AnchorViewHiddenSetsPrefs) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               [](user_education::HelpBubbleView* bubble) {
                 // This should yank the bubble out from under us.
                 bubble->GetAnchorView()->SetVisible(false);
               }),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckSnoozePrefs(/* is_dismiss */ false,
                       /* show_count */ 1,
                       /* snooze_count */ 0));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, AnchorHideSetsPrefs) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
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
  RunTestSequence(SetSnoozePrefs(data),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       AbortPromoRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      AbortPromo(kFeaturePromoLifecycleTestPromo),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kAbortedByFeature));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       SnoozePromoRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo), SnoozeIPH(),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kSnooze));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       CancelPromoRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo), DismissIPH(),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       PressingEscRecordsHistogram) {
  const ui::Accelerator kEsc(ui::VKEY_ESCAPE, ui::MODIFIER_NONE);
  views::Widget* bubble_widget = nullptr;
  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      // Ensure that the bubble is active before trying to send an accelerator;
      // widgets cannot accept accelerators before they become active.
      // TODO(dfried): need to create a common WaitForActivation() verb.
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               [&bubble_widget](views::View* view) {
                 bubble_widget = view->GetWidget();
               }),
      If([&bubble_widget]() { return !bubble_widget->IsActive(); },
         WaitForState(
             views::test::kCurrentWidgetFocus,
             [&bubble_widget]() { return bubble_widget->GetNativeView(); })),
      SendAccelerator(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting, kEsc),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       DismissPromoRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      PressDefaultPromoButton(),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest, EndPromoRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      InBrowser(base::BindOnce([](Browser* browser) {
        GetPromoController(browser)->EndPromo(
            kFeaturePromoLifecycleTestPromo,
            user_education::EndFeaturePromoReason::kFeatureEngaged);
      })),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckMessageActionHistogram(kFeaturePromoLifecycleTestPromo,
                                  FeaturePromoClosedReason::kFeatureEngaged));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       WidgetClosedRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               [](user_education::HelpBubbleView* bubble) {
                 bubble->GetWidget()->CloseWithReason(
                     views::Widget::ClosedReason::kUnspecified);
               }),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),

      CheckMessageActionHistogram(
          kFeaturePromoLifecycleTestPromo,
          FeaturePromoClosedReason::kAbortedByBubbleDestroyed));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       AnchorHideRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
               [](user_education::HelpBubbleView* bubble) {
                 // This should yank the bubble out from under us.
                 bubble->GetAnchorView()->SetVisible(false);
               }),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckMessageActionHistogram(
          kFeaturePromoLifecycleTestPromo,
          FeaturePromoClosedReason::kAbortedByAnchorHidden));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleUiTest,
                       DismissInRegionRecordsHistogram) {
  RunTestSequence(
      ShowPromoRecordingTime(kFeaturePromoLifecycleTestPromo),
      WithView(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](user_education::HelpBubbleView* bubble) {
            BrowserFeaturePromoController::MaybeCloseOverlappingHelpBubbles(
                bubble);
          }),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckMessageActionHistogram(
          kFeaturePromoLifecycleTestPromo,
          FeaturePromoClosedReason::kOverrideForUIRegionConflict));
}

class FeaturePromoLifecycleAppUiTest : public FeaturePromoLifecycleUiTest {
 public:
  FeaturePromoLifecycleAppUiTest() = default;
  ~FeaturePromoLifecycleAppUiTest() override = default;

  static constexpr char kApp1Host[] = "example.org";
  static constexpr char kApp2Host[] = "foo.com";
  static constexpr char kAppPath[] = "/web_apps/no_manifest.html";

  void SetUpOnMainThread() override {
    FeaturePromoLifecycleUiTest::SetUpOnMainThread();
    CHECK(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    app1_id_ = InstallPWA(embedded_test_server()->GetURL(kApp1Host, kAppPath));
    app2_id_ = InstallPWA(embedded_test_server()->GetURL(kApp2Host, kAppPath));
    EXPECT_NE(app1_id_, app2_id_);
  }

  auto CheckShownForApp() {
    return std::move(
        CheckBrowser(base::BindOnce([](Browser* browser) {
          const auto data = GetStorageService(browser)->ReadPromoData(
              kFeaturePromoLifecycleTestPromo);
          return base::Contains(data->shown_for_keys,
                                browser->app_controller()->app_id());
        })).SetDescription("CheckShownForApp()"));
  }

 protected:
  webapps::AppId app1_id_;
  webapps::AppId app2_id_;

 private:
  void RegisterPromos() override {
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForLegacyPromo(
                &kFeaturePromoLifecycleTestPromo,
                kToolbarAppMenuButtonElementId, IDS_OK)
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kKeyedNotice)));
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForApp) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(),
      WaitForShow(kToolbarAppMenuButtonElementId),
      MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app1_id_}), DismissIPH(),
      CheckShownForApp());
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForAppThenBlocked) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(),
      WaitForShow(kToolbarAppMenuButtonElementId),
      MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app1_id_}), DismissIPH(),

      MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app1_id_},
                     FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, HasPromoBeenDismissed) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(),
      WaitForShow(kToolbarAppMenuButtonElementId), CheckDismissed(false),
      MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app1_id_}), DismissIPH(),
      CheckDismissed(true, &kFeaturePromoLifecycleTestPromo, app1_id_));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleAppUiTest, ShowForTwoApps) {
  Browser* const app_browser = LaunchWebAppBrowser(app1_id_);
  Browser* const app_browser2 = LaunchWebAppBrowser(app2_id_);
  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(),
      MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app1_id_}),
      WaitForShow(kToolbarAppMenuButtonElementId), DismissIPH(),
      InContext(
          app_browser2->window()->GetElementContext(),
          Steps(WaitForShow(kToolbarAppMenuButtonElementId),
                MaybeShowPromo({kFeaturePromoLifecycleTestPromo, app2_id_}),
                DismissIPH(), CheckShownForApp())));
}

class FeaturePromoLifecycleCriticalUiTest : public FeaturePromoLifecycleUiTest {
 public:
  FeaturePromoLifecycleCriticalUiTest() = default;
  ~FeaturePromoLifecycleCriticalUiTest() override = default;

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
  void RegisterPromos() override {
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForLegacyPromo(
                &kFeaturePromoLifecycleTestPromo,
                kToolbarAppMenuButtonElementId, IDS_OK)
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kLegalNotice)));
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForLegacyPromo(
                &kFeaturePromoLifecycleTestPromo2,
                kToolbarAppMenuButtonElementId, IDS_CANCEL)
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kLegalNotice)));
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForLegacyPromo(
            &kFeaturePromoLifecycleTestPromo3, kToolbarAppMenuButtonElementId,
            IDS_CLEAR));
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForCustomAction(
                kFeaturePromoLifecycleTestAlert, kToolbarAppMenuButtonElementId,
                IDS_OK, IDS_CLEAR, base::DoNothing())
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kActionableAlert)));
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForCustomAction(
                kFeaturePromoLifecycleTestAlert2,
                kToolbarAppMenuButtonElementId, IDS_CANCEL, IDS_OK,
                base::DoNothing())
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kActionableAlert)));
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       CannotRepeatDismissedPromo) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),

                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo,
                                 FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest, ReshowAfterAbort) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo),
                  AbortPromo(kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       HasPromoBeenDismissed) {
  RunTestSequence(CheckDismissed(false),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       ShowSecondAfterDismiss) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo2),
                  DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       CriticalBlocksCritical) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo2,
                                 FeaturePromoResult::kBlockedByPromo),
                  DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestPromo2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest, AlertBlocksAlert) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestAlert),
                  MaybeShowPromo(kFeaturePromoLifecycleTestAlert2,
                                 FeaturePromoResult::kBlockedByPromo),
                  DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestAlert),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestAlert2));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       CriticalCancelsAlert) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestAlert),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestAlert));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       CriticalCancelsNormal) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo3),
                  MaybeShowPromo(kFeaturePromoLifecycleTestPromo), DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestPromo),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestPromo3),

                  CheckMessageActionHistogram(
                      kFeaturePromoLifecycleTestPromo3,
                      FeaturePromoClosedReason::kOverrideForPrecedence));
}

IN_PROC_BROWSER_TEST_F(FeaturePromoLifecycleCriticalUiTest,
                       AlertCancelsNormal) {
  RunTestSequence(MaybeShowPromo(kFeaturePromoLifecycleTestPromo3),
                  MaybeShowPromo(kFeaturePromoLifecycleTestAlert), DismissIPH(),
                  CheckDismissed(true, &kFeaturePromoLifecycleTestAlert),
                  CheckDismissed(false, &kFeaturePromoLifecycleTestPromo3),

                  CheckMessageActionHistogram(
                      kFeaturePromoLifecycleTestPromo3,
                      FeaturePromoClosedReason::kOverrideForPrecedence));
}

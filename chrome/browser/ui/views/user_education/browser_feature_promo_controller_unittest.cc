// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_snooze_service.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {
BASE_FEATURE(kTestIPHFeature,
             "TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kOneOffIPHFeature,
             "AnyContextIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTutorialIPHFeature,
             "TutorialTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature,
             "CustomActionTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultCustomActionIPHFeature,
             "DefaultCustomActionTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionIPHFeature2,
             "CustomActionTestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kStringTestIPHFeature,
             "StringTestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr char kTestTutorialIdentifier[] = "Test Tutorial";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOneOffIPHElementId);
}  // namespace

using user_education::FeaturePromoController;
using user_education::FeaturePromoHandle;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSnoozeService;
using user_education::FeaturePromoSpecification;
using user_education::FeaturePromoStatus;
using user_education::HelpBubble;
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleFactoryRegistry;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;
using user_education::HelpBubbleViews;
using user_education::TutorialDescription;

class BrowserFeaturePromoControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    controller_ = browser_view()->GetFeaturePromoController();
    lock_ = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile()));

    registry()->ClearFeaturesForTesting();

    // Register placeholder tutorials and IPH journeys.

    auto* const user_education_service =
        UserEducationServiceFactory::GetForProfile(browser()->profile());

    // Create a dummy tutorial.
    // This is just the first two steps of the "create tab group" tutorial.
    TutorialDescription desc;
    desc.steps.emplace_back(
        TutorialDescription::BubbleStep(kTabStripElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopCenter));
    desc.steps.emplace_back(
        TutorialDescription::BubbleStep(kTabGroupEditorBubbleId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
            .AbortIfVisibilityLost(false));

    user_education_service->tutorial_registry().AddTutorial(
        kTestTutorialIdentifier, std::move(desc));

    registry()->RegisterFeature(DefaultBubbleParams());

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForTutorialPromo(
            kTutorialIPHFeature, kAppMenuButtonElementId, IDS_CHROME_TIP,
            kTestTutorialIdentifier));

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForCustomAction(
            kCustomActionIPHFeature, kAppMenuButtonElementId, IDS_CHROME_TIP,
            IDS_CHROME_TIP,
            base::BindRepeating(
                &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
                base::Unretained(this),
                base::Unretained(&kCustomActionIPHFeature))));

    auto default_custom = FeaturePromoSpecification::CreateForCustomAction(
        kDefaultCustomActionIPHFeature, kAppMenuButtonElementId, IDS_CHROME_TIP,
        IDS_CHROME_TIP,
        base::BindRepeating(
            &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
            base::Unretained(this),
            base::Unretained(&kDefaultCustomActionIPHFeature)));
    default_custom.SetCustomActionIsDefault(true);
    default_custom.SetCustomActionDismissText(IDS_NOT_NOW);
    registry()->RegisterFeature(std::move(default_custom));

    // Make sure the browser view is visible for the tests.
    browser_view()->GetWidget()->Show();
  }

  void TearDown() override {
    TestWithBrowserView::TearDown();
    lock_.reset();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(
            BrowserFeaturePromoControllerTest::MakeTestTracker));
    return factories;
  }

 protected:
  FeaturePromoSnoozeService* snooze_service() {
    return controller_->snooze_service();
  }

  FeaturePromoRegistry* registry() { return controller_->registry(); }

  HelpBubbleFactoryRegistry* bubble_factory() {
    return controller_->bubble_factory_registry();
  }

  HelpBubbleView* GetPromoBubble(HelpBubble* bubble) {
    return bubble ? bubble->AsA<HelpBubbleViews>()->bubble_view() : nullptr;
  }

  HelpBubbleView* GetPromoBubble() {
    return GetPromoBubble(controller_->promo_bubble());
  }

  HelpBubbleView* GetCriticalPromoBubble() {
    return GetPromoBubble(controller_->critical_promo_bubble());
  }

  views::View* GetAnchorView() {
    return browser_view()->toolbar()->app_menu_button();
  }

  ui::TrackedElement* GetAnchorElement() {
    auto* const result =
        views::ElementTrackerViews::GetInstance()->GetElementForView(
            GetAnchorView());
    CHECK(result);
    return result;
  }

  FeaturePromoSpecification DefaultBubbleParams() {
    return FeaturePromoSpecification::CreateForLegacyPromo(
        &kTestIPHFeature, kAppMenuButtonElementId, IDS_CHROME_TIP);
  }

  void OnCustomPromoAction(const base::Feature* feature,
                           ui::ElementContext context,
                           FeaturePromoHandle promo_handle) {
    ++custom_callback_count_;
    EXPECT_TRUE(promo_handle.is_valid());
    EXPECT_EQ(FeaturePromoStatus::kContinued,
              controller_->GetPromoStatus(*feature));
    EXPECT_EQ(browser()->window()->GetElementContext(), context);
    promo_handle.Release();
    EXPECT_EQ(FeaturePromoStatus::kNotRunning,
              controller_->GetPromoStatus(*feature));
  }

  raw_ptr<BrowserFeaturePromoController, DanglingUntriaged> controller_;
  raw_ptr<NiceMock<feature_engagement::test::MockTracker>, DanglingUntriaged>
      mock_tracker_;
  BrowserFeaturePromoController::TestLock lock_;
  int custom_callback_count_ = 0;

 private:
  static std::unique_ptr<KeyedService> MakeTestTracker(
      content::BrowserContext* context) {
    auto tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow other code to call into the tracker.
    EXPECT_CALL(*tracker, NotifyEvent(_)).Times(AnyNumber());
    EXPECT_CALL(*tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return tracker;
  }
};

using BubbleCloseCallback = BrowserFeaturePromoController::BubbleCloseCallback;

TEST_F(BrowserFeaturePromoControllerTest, GetForView) {
  EXPECT_EQ(controller_,
            BrowserFeaturePromoController::GetForView(GetAnchorView()));

  // For a view not in the BrowserView's hierarchy, it should return null.
  views::View orphan_view;
  EXPECT_EQ(nullptr, BrowserFeaturePromoController::GetForView(&orphan_view));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);

  EXPECT_FALSE(
      controller_->MaybeShowPromo(kTestIPHFeature, close_callback.Get()));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowStartupPromo) {
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([](feature_engagement::Tracker::OnInitializedCallback cb) {
        std::move(cb).Run(false);
      });

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::StartupPromoCallback,
                         callback);
  EXPECT_CALL_IN_SCOPE(
      callback, Run(Ref(kTestIPHFeature), false),
      controller_->MaybeShowStartupPromo(kTestIPHFeature, callback.Get()));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DoesNotAskBackendWhenShowingFromDemoPage) {
  EXPECT_TRUE(controller_->MaybeShowPromoForDemoPage(&kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_NE(nullptr, GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubble) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsStartupBubble) {
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([](feature_engagement::Tracker::OnInitializedCallback cb) {
        std::move(cb).Run(true);
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::StartupPromoCallback,
                         callback);

  EXPECT_CALL_IN_SCOPE(callback, Run(Ref(kTestIPHFeature), true),
                       EXPECT_TRUE(controller_->MaybeShowStartupPromo(
                           kTestIPHFeature, callback.Get())));
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowStartupBlockedWithAsyncCallback) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([&](feature_engagement::Tracker::OnInitializedCallback cb) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](feature_engagement::Tracker::OnInitializedCallback cb,
                   base::OnceClosure quit_closure) {
                  std::move(cb).Run(false);
                  std::move(quit_closure).Run();
                },
                std::move(cb), std::move(quit_closure)));
      });

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::StartupPromoCallback,
                         callback);

  EXPECT_TRUE(
      controller_->MaybeShowStartupPromo(kTestIPHFeature, callback.Get()));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(callback, Run(Ref(kTestIPHFeature), false),
                       run_loop.Run());
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, ShowStartupBubbleWithAsyncCallback) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([&](feature_engagement::Tracker::OnInitializedCallback cb) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](feature_engagement::Tracker::OnInitializedCallback cb,
                   base::OnceClosure quit_closure) {
                  std::move(cb).Run(true);
                  std::move(quit_closure).Run();
                },
                std::move(cb), std::move(quit_closure)));
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::StartupPromoCallback,
                         callback);

  EXPECT_TRUE(
      controller_->MaybeShowStartupPromo(kTestIPHFeature, callback.Get()));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(callback, Run(Ref(kTestIPHFeature), true),
                       run_loop.Run());
  EXPECT_EQ(FeaturePromoStatus::kBubbleShowing,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       ShowStartupBubbleFailsWhenAlreadyShowing) {
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([](feature_engagement::Tracker::OnInitializedCallback cb) {
        std::move(cb).Run(true);
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       ShowStartupBubbleFailsWhenAlreadyPending) {
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback).Times(1);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, CancelPromoBeforeStartup) {
  feature_engagement::Tracker::OnInitializedCallback callback;
  EXPECT_CALL(*mock_tracker_, AddOnInitializedCallback)
      .WillOnce([&](feature_engagement::Tracker::OnInitializedCallback cb) {
        callback = std::move(cb);
      });
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI).Times(0);

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  controller_->EndPromo(kTestIPHFeature);
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));

  // Now, indicate that startup has completed and verify that the promo does
  // not show.
  std::move(callback).Run(true);
  EXPECT_EQ(FeaturePromoStatus::kNotRunning,
            controller_->GetPromoStatus(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleAnyContext) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetInAnyContext(true)));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = browser_view()->GetWidget()->GetNativeWindow();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  widget->Show();

  const ui::ElementContext widget_context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());
  EXPECT_NE(browser_view()->GetElementContext(), widget_context);

  EXPECT_TRUE(controller_->MaybeShowPromo(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleWithFilter) {
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetAnchorElementFilter(base::BindLambdaForTesting(
                        [](const ui::ElementTracker::ElementList& elements) {
                          EXPECT_EQ(2U, elements.size());
                          return elements[0];
                        }))));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Add two random views to the browser with the same element ID.
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  EXPECT_TRUE(controller_->MaybeShowPromo(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest, ShowsBubbleWithFilterAnyContext) {
  ui::ElementContext widget_context;
  registry()->RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &kOneOffIPHFeature, kOneOffIPHElementId, IDS_CHROME_TIP)
                    .SetInAnyContext(true)
                    .SetAnchorElementFilter(base::BindLambdaForTesting(
                        [&](const ui::ElementTracker::ElementList& elements) {
                          EXPECT_EQ(3U, elements.size());
                          for (auto* element : elements) {
                            if (element->context() == widget_context) {
                              return element;
                            }
                          }
                          ADD_FAILURE() << "Did not find expected element.";
                          return (ui::TrackedElement*)(nullptr);
                        }))));

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kOneOffIPHFeature)))
      .WillOnce(Return(true));

  // Add two random views to the browser with the same element ID.
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  browser_view()
      ->toolbar()
      ->AddChildView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);

  // Create a second widget with an element with the target identifier.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = browser_view()->GetWidget()->GetNativeWindow();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<views::View>())
      ->SetProperty(views::kElementIdentifierKey, kOneOffIPHElementId);
  widget->Show();
  widget_context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());

  EXPECT_NE(browser_view()->GetElementContext(), widget_context);

  EXPECT_TRUE(controller_->MaybeShowPromo(kOneOffIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kOneOffIPHFeature));
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(widget_context,
            controller_->promo_bubble_for_testing()->GetContext());

  bubble->Close();
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_RegionDoesNotOverlap) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect non_overlapping_region(bounds.right() + 1, bounds.bottom() + 1, 10,
                                   10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(non_overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_RegionOverlaps) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  const gfx::Rect bounds =
      GetPromoBubble()->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_TRUE(result);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DismissNonCriticalBubbleInRegion_CriticalPromo) {
  const auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);
  const gfx::Rect bounds =
      GetPromoBubble(bubble.get())->GetWidget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(bounds.IsEmpty());
  gfx::Rect overlapping_region(bounds.x() + 1, bounds.y() + 1, 10, 10);
  const bool result =
      controller_->DismissNonCriticalBubbleInRegion(overlapping_region);
  EXPECT_FALSE(result);
  EXPECT_TRUE(bubble->is_open());
}

TEST_F(BrowserFeaturePromoControllerTest, SnoozeServiceBlocksPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  snooze_service()->OnUserDismiss(kTestIPHFeature);
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
  snooze_service()->Reset(kTestIPHFeature);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(close_callback, Run(),
                       EXPECT_TRUE(controller_->EndPromo(kTestIPHFeature)));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->EndPromo(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  EXPECT_FALSE(controller_->EndPromo(kTutorialIPHFeature));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(close_callback, Run(), {
    bubble->GetWidget()->Close();
    widget_observer.Wait();
  });

  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       ContinuedPromoDefersBackendDismissed) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, close_callback.Get()));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  // First check that CloseBubbleAndContinuePromo() actually closes the
  // bubble, but doesn't yet tell the backend the promo finished.

  FeaturePromoHandle promo_handle;
  EXPECT_CALL_IN_SCOPE(
      close_callback, Run(),
      promo_handle = controller_->CloseBubbleAndContinuePromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoStatus::kContinued,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();

  // Check handle destruction causes the backend to be notified.

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoHandleDismissesPromoOnRelease) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.
  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          FeaturePromoStatus::kContinued));
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoOnOverwrite) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle = FeaturePromoHandle();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoExactlyOnce) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle);
  promo_handle.Release();
  EXPECT_FALSE(promo_handle);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoAfterMoveConstruction) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2(std::move(promo_handle));
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PromoHandleDismissesPromoAfterMoveAssignment) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  // Check handle destruction causes the backend to be notified.

  EXPECT_TRUE(promo_handle);
  FeaturePromoHandle promo_handle2;
  promo_handle2 = std::move(promo_handle);
  EXPECT_TRUE(promo_handle2);
  EXPECT_FALSE(promo_handle);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  promo_handle2.Release();
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_FALSE(promo_handle2);
}

TEST_F(BrowserFeaturePromoControllerTest,
       PropertySetOnAnchorViewWhileBubbleOpen) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_TRUE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));

  controller_->EndPromo(kTestIPHFeature);
  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));
}

TEST_F(BrowserFeaturePromoControllerTest, TestCanBlockPromos) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);

  auto lock = controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, TestCanStopCurrentPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  auto lock = controller_->BlockPromosForTesting();
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CriticalPromoBlocksNormalPromo) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));

  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CriticalPromoPreemptsNormalPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  EXPECT_TRUE(
      controller_->MaybeShowPromo(kTestIPHFeature, close_callback.Get()));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  std::unique_ptr<user_education::HelpBubble> bubble;
  EXPECT_CALL_IN_SCOPE(close_callback, Run,
                       bubble = controller_->ShowCriticalPromo(
                           DefaultBubbleParams(), GetAnchorElement()));
  EXPECT_TRUE(bubble);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, FirstCriticalPromoHasPrecedence) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());

  auto bubble2 =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_FALSE(bubble2);
  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CloseBubbleForCriticalPromo) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleForCriticalPromoDoesNothingAfterClose) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);
  ASSERT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  auto* widget = GetPromoBubble(bubble.get())->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();

  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowNewCriticalPromoAfterClose) {
  auto bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble =
      controller_->ShowCriticalPromo(DefaultBubbleParams(), GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, FailsIfBubbleIsShowing) {
  HelpBubbleParams bubble_params;
  bubble_params.body_text = l10n_util::GetStringUTF16(IDS_CHROME_TIP);
  auto bubble = bubble_factory()->CreateHelpBubble(GetAnchorElement(),
                                                   std::move(bubble_params));
  EXPECT_TRUE(bubble);

  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  EXPECT_FALSE(controller_->MaybeShowPromo(kTestIPHFeature));
}

// Test that a feature promo can chain into a tutorial.
TEST_F(BrowserFeaturePromoControllerTest, StartsTutorial) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kTutorialIPHFeature));

  // Simulate clicking the "Show Tutorial" button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  // We should be running the tutorial now.
  auto& tutorial_service =
      UserEducationServiceFactory::GetForProfile(browser()->profile())
          ->tutorial_service();
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
  tutorial_service.CancelTutorialIfRunning();
}

// Test that a feature promo can perform a custom action.
TEST_F(BrowserFeaturePromoControllerTest, PerformsCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kCustomActionIPHFeature));

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();

  EXPECT_EQ(1, custom_callback_count_);
}

// Test that a feature promo can perform a custom action that is the default.
TEST_F(BrowserFeaturePromoControllerTest, PerformsCustomActionAsDefault) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kDefaultCustomActionIPHFeature));

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  auto* const button = bubble->GetNonDefaultButtonForTesting(0);
  ASSERT_TRUE(button);

  const std::u16string& text = button->GetText();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOT_NOW), text);

  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  EXPECT_EQ(1, custom_callback_count_);
}

// Test that a feature promo does not perform a custom action when the default
// "Got it" button is clicked.
TEST_F(BrowserFeaturePromoControllerTest, DoesNotPerformCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kCustomActionIPHFeature));

  // Simulate clicking the other button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetDefaultButtonForTesting());
  waiter.Wait();

  EXPECT_EQ(0, custom_callback_count_);
}

// Test that a feature promo does not perform a custom action when a non-default
// "Got it" button is clicked.
TEST_F(BrowserFeaturePromoControllerTest, DoesNotPerformDefaultCustomAction) {
  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kDefaultCustomActionIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kDefaultCustomActionIPHFeature));

  // Simulate clicking the other button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();

  EXPECT_EQ(0, custom_callback_count_);
}

// Test that the promo controller can handle the anchor view disappearing from
// under the bubble during the button callback.
TEST_F(BrowserFeaturePromoControllerTest, CustomActionHidesAnchorView) {
  FeaturePromoHandle promo_handle;
  registry()->RegisterFeature(FeaturePromoSpecification::CreateForCustomAction(
      kCustomActionIPHFeature2, kAppMenuButtonElementId, IDS_CHROME_TIP,
      IDS_CHROME_TIP,
      base::BindLambdaForTesting(
          [&](ui::ElementContext context, FeaturePromoHandle handle) {
            views::ElementTrackerViews::GetInstance()
                ->GetUniqueView(kAppMenuButtonElementId, context)
                ->SetVisible(false);
            promo_handle = std::move(handle);
          })));

  // Launch a feature promo that has a tutorial.
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(Ref(kCustomActionIPHFeature2)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kCustomActionIPHFeature2));

  // Simulate clicking the custom action button.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      bubble->GetNonDefaultButtonForTesting(0));
  waiter.Wait();
  EXPECT_TRUE(promo_handle.is_valid());

  // Promo is actually dismissed when the handle is released.
  EXPECT_CALL(*mock_tracker_, Dismissed(testing::Ref(kCustomActionIPHFeature2)))
      .Times(1);
  promo_handle.Release();
}

TEST_F(BrowserFeaturePromoControllerTest, GetAnchorContext) {
  EXPECT_EQ(browser_view()->GetElementContext(),
            controller_->GetAnchorContext());
}

TEST_F(BrowserFeaturePromoControllerTest, GetAcceleratorProvider) {
  EXPECT_EQ(browser_view(), controller_->GetAcceleratorProvider());
}

TEST_F(BrowserFeaturePromoControllerTest, GetFocusHelpBubbleScreenReaderHint) {
  EXPECT_TRUE(controller_
                  ->GetFocusHelpBubbleScreenReaderHint(
                      FeaturePromoSpecification::PromoType::kToast,
                      GetAnchorElement(), false)
                  .empty());
  EXPECT_FALSE(controller_
                   ->GetFocusHelpBubbleScreenReaderHint(
                       FeaturePromoSpecification::PromoType::kSnooze,
                       GetAnchorElement(), false)
                   .empty());

  // Target element is focusable so critical promo should also have a hint.
  EXPECT_FALSE(controller_
                   ->GetFocusHelpBubbleScreenReaderHint(
                       FeaturePromoSpecification::PromoType::kLegacy,
                       GetAnchorElement(), true)
                   .empty());
}

namespace {
const int kStringWithSingleSubstitution =
    IDS_APP_TABLE_COLUMN_SORTED_ASC_ACCNAME;
const int kStringWithMultipleSubstitutions =
    IDS_CONCAT_THREE_STRINGS_WITH_COMMA;
const int kStringWithPluralSubstitution = IDS_TIME_HOURS;
const std::u16string kSubstitution1{u"First"};
const std::u16string kSubstitution2{u"Second"};
const std::u16string kSubstitution3{u"Third"};
}  // namespace

class BrowserFeaturePromoControllerViewsTest
    : public views::test::InteractiveViewsTestT<
          BrowserFeaturePromoControllerTest> {
 public:
  BrowserFeaturePromoControllerViewsTest() = default;
  ~BrowserFeaturePromoControllerViewsTest() override = default;

  auto RegisterPromo(int body_string, int title_string = 0) {
    return Do([this, body_string, title_string]() {
      auto spec = FeaturePromoSpecification::CreateForLegacyPromo(
          &kStringTestIPHFeature, kAppMenuButtonElementId, body_string);
      if (title_string) {
        spec.SetBubbleTitleText(title_string);
      }
      registry()->RegisterFeature(std::move(spec));
    });
  }

  template <typename... Args>
  auto MaybeShowPromo(Args... args) {
    return Check([this, args...]() {
      EXPECT_CALL(*mock_tracker_,
                  ShouldTriggerHelpUI(Ref(kStringTestIPHFeature)))
          .WillOnce(Return(true));

      return controller_->MaybeShowPromo(kStringTestIPHFeature,
                                         base::DoNothing(), args...);
    });
  }
};

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_SingleString) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(kStringWithSingleSubstitution),
      MaybeShowPromo(kSubstitution1),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_MultipleStrings) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(kStringWithMultipleSubstitutions),
      MaybeShowPromo(
          user_education::FeaturePromoSpecification::StringSubstitutions{
              kSubstitution1, kSubstitution2, kSubstitution3}),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Singular) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(kStringWithPluralSubstitution), MaybeShowPromo(1),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Plural) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(kStringWithPluralSubstitution), MaybeShowPromo(3),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_SingleString) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(IDS_OK, kStringWithSingleSubstitution),
      MaybeShowPromo(
          user_education::FeaturePromoSpecification::NoSubstitution(),
          kSubstitution1),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_MultipleStrings) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(IDS_OK, kStringWithMultipleSubstitutions),
      MaybeShowPromo(
          user_education::FeaturePromoSpecification::NoSubstitution(),
          user_education::FeaturePromoSpecification::StringSubstitutions{
              kSubstitution1, kSubstitution2, kSubstitution3}),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Singular) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(
          user_education::FeaturePromoSpecification::NoSubstitution(), 1),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Plural) {
  RunTestSequenceInContext(
      browser_view()->GetElementContext(),
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(
          user_education::FeaturePromoSpecification::NoSubstitution(), 3),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

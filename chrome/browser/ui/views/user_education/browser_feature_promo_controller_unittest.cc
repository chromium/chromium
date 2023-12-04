// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/feature_promo_session_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
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
BASE_FEATURE(kSnoozeIPHFeature,
             "SnoozeIPHFeature",
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
using user_education::FeaturePromoData;
using user_education::FeaturePromoHandle;
using user_education::FeaturePromoPolicyData;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoResult;
using user_education::FeaturePromoSessionData;
using user_education::FeaturePromoSpecification;
using user_education::FeaturePromoStatus;
using user_education::FeaturePromoStorageService;
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
    if (UseV2()) {
      scoped_feature_list_.InitAndEnableFeature(
          user_education::features::kUserEducationExperienceVersion2);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          user_education::features::kUserEducationExperienceVersion2);
    }

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
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());

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

    RegisterIPH();

    // Make sure the browser view is visible for the tests.
    browser_view()->GetWidget()->Show();
  }

  virtual void RegisterIPH() {
    registry()->RegisterFeature(DefaultPromoSpecification());

    registry()->RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
        kSnoozeIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP));

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForTutorialPromo(
            kTutorialIPHFeature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
            kTestTutorialIdentifier));

    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateForCustomAction(
            kCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
            IDS_CHROME_TIP, IDS_CHROME_TIP,
            base::BindRepeating(
                &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
                base::Unretained(this),
                base::Unretained(&kCustomActionIPHFeature))));

    auto default_custom = FeaturePromoSpecification::CreateForCustomAction(
        kDefaultCustomActionIPHFeature, kToolbarAppMenuButtonElementId,
        IDS_CHROME_TIP, IDS_CHROME_TIP,
        base::BindRepeating(
            &BrowserFeaturePromoControllerTest::OnCustomPromoAction,
            base::Unretained(this),
            base::Unretained(&kDefaultCustomActionIPHFeature)));
    default_custom.SetCustomActionIsDefault(true);
    default_custom.SetCustomActionDismissText(IDS_NOT_NOW);
    registry()->RegisterFeature(std::move(default_custom));
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
  FeaturePromoController* controller() { return controller_.get(); }

  virtual bool UseV2() const { return false; }

  user_education::FeaturePromoParams MakeParams(
      const base::Feature& feature,
      user_education::FeaturePromoController::BubbleCloseCallback
          close_callback,
      user_education::FeaturePromoController::StartupPromoCallback
          startup_callback = base::NullCallback()) {
    user_education::FeaturePromoParams params(feature);
    params.close_callback = std::move(close_callback);
    params.startup_callback = std::move(startup_callback);
    return params;
  }

  FeaturePromoStorageService* storage_service() {
    return controller_->storage_service();
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

  FeaturePromoSpecification DefaultPromoSpecification(
      const base::Feature& feature = kTestIPHFeature) {
    return FeaturePromoSpecification::CreateForToastPromo(
        feature, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP, IDS_OK,
        FeaturePromoSpecification::AcceleratorInfo());
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

using BubbleCloseCallback = BrowserFeaturePromoController::BubbleCloseCallback;

TEST_F(BrowserFeaturePromoControllerTest, GetForView) {
  EXPECT_EQ(controller_,
            BrowserFeaturePromoController::GetForView(GetAnchorView()));

  // For a view not in the BrowserView's hierarchy, it should return null.
  views::View orphan_view;
  EXPECT_EQ(nullptr, BrowserFeaturePromoController::GetForView(&orphan_view));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendIfPromoShouldBeShown) {
  // If the backend says no, the controller says no.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig,
            controller_->CanShowPromo(kTestIPHFeature));

  // If the backend says yes, the controller says yes.
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->CanShowPromo(kTestIPHFeature));
}

TEST_F(BrowserFeaturePromoControllerTest, AsksBackendToShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(false));

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);

  EXPECT_FALSE(controller_->MaybeShowPromo(
      MakeParams(kTestIPHFeature, close_callback.Get())));
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
      callback,
      Run(Ref(kTestIPHFeature), FeaturePromoResult(FeaturePromoResult::kError)),
      controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
}

TEST_F(BrowserFeaturePromoControllerTest,
       DoesNotAskBackendWhenShowingFromDemoPage) {
  EXPECT_TRUE(controller_->MaybeShowPromoForDemoPage(kTestIPHFeature));
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

TEST_F(BrowserFeaturePromoControllerTest, BubbleBlocksCanShowPromo) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));
  EXPECT_EQ(FeaturePromoResult::kBlockedByPromo,
            controller_->CanShowPromo(kTutorialIPHFeature));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  EXPECT_TRUE(controller_->EndPromo(
      kTestIPHFeature, user_education::EndFeaturePromoReason::kFeatureEngaged));
  EXPECT_TRUE(controller_->CanShowPromo(kTutorialIPHFeature));
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

  EXPECT_CALL_IN_SCOPE(
      callback, Run(Ref(kTestIPHFeature), FeaturePromoResult::Success()),
      EXPECT_TRUE(controller_->MaybeShowStartupPromo(
          MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get()))));
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

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(
      MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(
      callback,
      Run(Ref(kTestIPHFeature), FeaturePromoResult(FeaturePromoResult::kError)),
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

  EXPECT_TRUE(controller_->MaybeShowStartupPromo(
      MakeParams(kTestIPHFeature, base::DoNothing(), callback.Get())));
  EXPECT_EQ(FeaturePromoStatus::kQueuedForStartup,
            controller_->GetPromoStatus(kTestIPHFeature));
  EXPECT_CALL_IN_SCOPE(callback,
                       Run(Ref(kTestIPHFeature), FeaturePromoResult::Success()),
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
  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
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
  const auto bubble = controller_->ShowCriticalPromo(
      DefaultPromoSpecification(), GetAnchorElement());
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
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTutorialIPHFeature)))
      .Times(0);
  // Simulate a snooze by writing data directly.
  FeaturePromoData data;
  data.show_count = 1;
  data.snooze_count = 1;
  data.last_show_time = base::Time::Now();
  data.last_snooze_time = base::Time::Now();
  storage_service()->SavePromoData(kTutorialIPHFeature, data);

  EXPECT_FALSE(controller_->MaybeShowPromo(kTutorialIPHFeature));
  EXPECT_FALSE(controller_->IsPromoActive(kTutorialIPHFeature));
  EXPECT_FALSE(GetPromoBubble());
  storage_service()->Reset(kTutorialIPHFeature);
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsWhenRequested) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(controller_->MaybeShowPromo(
      MakeParams(kTestIPHFeature, close_callback.Get())));

  // Only valid before the widget is closed.
  auto* const bubble = GetPromoBubble();
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  views::test::WidgetDestroyedWaiter widget_observer(bubble->GetWidget());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  EXPECT_CALL_IN_SCOPE(
      close_callback, Run(),
      EXPECT_TRUE(controller_->EndPromo(
          kTestIPHFeature,
          user_education::EndFeaturePromoReason::kAbortPromo)));
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_FALSE(GetPromoBubble());

  // Ensure the widget does close.
  widget_observer.Wait();
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfPromoNotShowing) {
  EXPECT_FALSE(controller_->EndPromo(
      kTestIPHFeature, user_education::EndFeaturePromoReason::kAbortPromo));
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleDoesNothingIfDifferentPromoShowing) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  EXPECT_FALSE(controller_->EndPromo(
      kTutorialIPHFeature, user_education::EndFeaturePromoReason::kAbortPromo));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, PromoEndsOnBubbleClosure) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(0);

  UNCALLED_MOCK_CALLBACK(BubbleCloseCallback, close_callback);
  ASSERT_TRUE(controller_->MaybeShowPromo(
      MakeParams(kTestIPHFeature, close_callback.Get())));

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
  ASSERT_TRUE(controller_->MaybeShowPromo(
      MakeParams(kTestIPHFeature, close_callback.Get())));

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

TEST_F(BrowserFeaturePromoControllerTest, ContinuedPromoDismissesOnForceEnd) {
  EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(kTestIPHFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  ASSERT_TRUE(controller_->MaybeShowPromo(kTestIPHFeature));

  FeaturePromoHandle promo_handle =
      controller_->CloseBubbleAndContinuePromo(kTestIPHFeature);

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);
  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature,
                                          FeaturePromoStatus::kContinued));
  EXPECT_CALL(*mock_tracker_, Dismissed).Times(0);
  promo_handle.Release();
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

  controller_->EndPromo(kTestIPHFeature,
                        user_education::EndFeaturePromoReason::kAbortPromo);
  EXPECT_FALSE(
      GetAnchorView()->GetProperty(user_education::kHasInProductHelpPromoKey));
}

TEST_F(BrowserFeaturePromoControllerTest, CriticalPromoBlocksNormalPromo) {
  auto bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                               GetAnchorElement());
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
  EXPECT_TRUE(controller_->MaybeShowPromo(
      MakeParams(kTestIPHFeature, close_callback.Get())));
  EXPECT_TRUE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetPromoBubble());

  EXPECT_CALL(*mock_tracker_, Dismissed(Ref(kTestIPHFeature))).Times(1);

  std::unique_ptr<user_education::HelpBubble> bubble;
  EXPECT_CALL_IN_SCOPE(close_callback, Run,
                       bubble = controller_->ShowCriticalPromo(
                           DefaultPromoSpecification(), GetAnchorElement()));
  EXPECT_TRUE(bubble);
  EXPECT_FALSE(controller_->IsPromoActive(kTestIPHFeature));
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, FirstCriticalPromoHasPrecedence) {
  auto bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                               GetAnchorElement());
  EXPECT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());

  auto bubble2 = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                                GetAnchorElement());
  EXPECT_FALSE(bubble2);
  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, CloseBubbleForCriticalPromo) {
  auto bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                               GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest,
       CloseBubbleForCriticalPromoDoesNothingAfterClose) {
  auto bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                               GetAnchorElement());
  ASSERT_TRUE(bubble);
  ASSERT_EQ(GetPromoBubble(bubble.get()), GetCriticalPromoBubble());
  auto* widget = GetPromoBubble(bubble.get())->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();

  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                          GetAnchorElement());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(GetCriticalPromoBubble());
}

TEST_F(BrowserFeaturePromoControllerTest, ShowNewCriticalPromoAfterClose) {
  auto bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                               GetAnchorElement());
  ASSERT_TRUE(bubble);

  EXPECT_TRUE(GetCriticalPromoBubble());
  bubble->Close();
  EXPECT_FALSE(GetCriticalPromoBubble());

  bubble = controller_->ShowCriticalPromo(DefaultPromoSpecification(),
                                          GetAnchorElement());
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
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
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
      kCustomActionIPHFeature2, kToolbarAppMenuButtonElementId, IDS_CHROME_TIP,
      IDS_CHROME_TIP,
      base::BindLambdaForTesting(
          [&](ui::ElementContext context, FeaturePromoHandle handle) {
            views::ElementTrackerViews::GetInstance()
                ->GetUniqueView(kToolbarAppMenuButtonElementId, context)
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

  void SetUp() override {
    InteractiveViewsTestT<BrowserFeaturePromoControllerTest>::SetUp();
    SetContextWidget(browser_view()->GetWidget());
  }

  auto RegisterPromo(int body_string, int title_string = 0) {
    return Do([this, body_string, title_string]() {
      auto spec = FeaturePromoSpecification::CreateForLegacyPromo(
          &kStringTestIPHFeature, kToolbarAppMenuButtonElementId, body_string);
      if (title_string) {
        spec.SetBubbleTitleText(title_string);
      }
      registry()->RegisterFeature(std::move(spec));
    });
  }

  auto MaybeShowPromo(
      user_education::FeaturePromoParams params,
      FeaturePromoResult expected = FeaturePromoResult::Success()) {
    std::ostringstream desc;
    desc << "MaybeShowPromo(" << params.feature->name << ", " << expected
         << ")";
    return CheckResult(
        [this, p = std::move(params), expected]() mutable {
          if (expected) {
            EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(*p.feature)))
                .WillOnce(Return(true));
          } else if (expected.failure() ==
                     FeaturePromoResult::kBlockedByConfig) {
            EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(*p.feature)))
                .WillOnce(Return(false));
          }

          return controller_->MaybeShowPromo(std::move(p));
        },
        expected, desc.str());
  }
};

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_SingleString) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       BodyTextSubstitution_MultipleStrings) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params =
      user_education::FeaturePromoSpecification::StringSubstitutions{
          kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Singular) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 1;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, BodyTextSubstitution_Plural) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.body_params = 3;

  RunTestSequence(
      RegisterPromo(kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_SingleString) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = kSubstitution1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithSingleSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithSingleSubstitution, kSubstitution1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest,
       TitleTextSubstitution_MultipleStrings) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params =
      user_education::FeaturePromoSpecification::StringSubstitutions{
          kSubstitution1, kSubstitution2, kSubstitution3};

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithMultipleSubstitutions),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringFUTF16(
                            kStringWithMultipleSubstitutions, kSubstitution1,
                            kSubstitution2, kSubstitution3)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Singular) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 1;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 1)));
}

TEST_F(BrowserFeaturePromoControllerViewsTest, TitleTextSubstitution_Plural) {
  user_education::FeaturePromoParams params(kStringTestIPHFeature);
  params.title_params = 3;

  RunTestSequence(
      RegisterPromo(IDS_OK, kStringWithPluralSubstitution),
      MaybeShowPromo(std::move(params)),
      CheckViewProperty(
          user_education::HelpBubbleView::kTitleTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetPluralStringFUTF16(kStringWithPluralSubstitution, 3)));
}

namespace {
BASE_FEATURE(kHighPriorityIPHFeature,
             "HighPriorityIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kHighPriorityIPHFeature2,
             "HighPriorityIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Somewhere around 2020.
const base::Time kSessionStartTime =
    base::Time::FromDeltaSinceWindowsEpoch(420 * base::Days(365));
}  // namespace

class BrowserFeaturePromoControllerPolicyTest
    : public BrowserFeaturePromoControllerViewsTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserFeaturePromoControllerPolicyTest() { VerifyConstants(); }

  ~BrowserFeaturePromoControllerPolicyTest() override = default;

  void TearDown() override {
    help_bubble_.reset();
    test_util_.reset();
    BrowserFeaturePromoControllerViewsTest::TearDown();
  }

  void RegisterIPH() override {
    BrowserFeaturePromoControllerViewsTest::RegisterIPH();

    auto high_priority_spec =
        DefaultPromoSpecification(kHighPriorityIPHFeature);
    high_priority_spec.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(high_priority_spec));

    auto high_priority_spec2 =
        FeaturePromoSpecification::CreateForTutorialPromo(
            kHighPriorityIPHFeature2, kToolbarAppMenuButtonElementId, IDS_OK,
            kTestTutorialIdentifier);
    high_priority_spec2.set_promo_subtype_for_testing(
        FeaturePromoSpecification::PromoSubtype::kLegalNotice);
    registry()->RegisterFeature(std::move(high_priority_spec2));
  }

  auto ResetSessionData(base::TimeDelta since_session_start,
                        base::TimeDelta idle_time = base::Seconds(1)) {
    return std::move(
        WithView(kBrowserViewElementId, [this, since_session_start,
                                         idle_time](BrowserView* browser_view) {
          FeaturePromoSessionData session_data;
          session_data.start_time = kSessionStartTime;
          session_data.most_recent_active_time =
              kSessionStartTime + since_session_start;
          now_ = session_data.most_recent_active_time + idle_time;
          FeaturePromoPolicyData policy_data;
          test_util_ = std::make_unique<
              user_education::test::FeaturePromoSessionTestUtil>(
              UserEducationServiceFactory::GetForBrowserContext(
                  browser_view->GetProfile())
                  ->feature_promo_session_manager(),
              session_data, policy_data, now_);
        }).SetDescription("ResetSessionData"));
  }

  auto AdvanceTime(base::TimeDelta until_last_active,
                   base::TimeDelta idle_time = base::Seconds(1),
                   bool screen_locked = false) {
    return Do([this, until_last_active, idle_time, screen_locked]() {
      const auto new_active_time = now_ + until_last_active;
      now_ = new_active_time + idle_time;
      test_util_->SetNow(now_);
      test_util_->UpdateIdleState(new_active_time, screen_locked);
    });
  }

  auto CheckSessionActive(bool expected) {
    return std::move(
        CheckView(
            kBrowserViewElementId,
            [](BrowserView* browser_view) {
              return UserEducationServiceFactory::GetForBrowserContext(
                         browser_view->GetProfile())
                  ->feature_promo_session_manager()
                  .IsApplicationActive();
            },
            expected)
            .SetDescription("CheckSessionActive()"));
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

  auto ExpectShowingPromo(const base::Feature* feature) {
    return CheckResult(
        [this]() { return controller()->GetCurrentPromoFeature(); }, feature,
        base::StringPrintf("ExpectShowingPromo %s",
                           feature ? feature->name : "[none]"));
  }

  auto ClosePromo() {
    return PressButton(
        user_education::HelpBubbleView::kCloseButtonIdForTesting);
  }

  auto AbortPromo() {
    return Steps(
        WithView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [](views::View* bubble) { bubble->GetWidget()->Close(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        FlushEvents());
  }

 protected:
  bool UseV2() const override { return GetParam(); }

  const base::TimeDelta kLessThanGracePeriod =
      user_education::features::GetSessionStartGracePeriod() / 4;
  const base::TimeDelta kMoreThanGracePeriod =
      user_education::features::GetSessionStartGracePeriod() + base::Minutes(5);
  const base::TimeDelta kLessThanCooldown =
      user_education::features::GetLowPriorityCooldown() / 4;
  const base::TimeDelta kMoreThanCooldown =
      user_education::features::GetLowPriorityCooldown() + base::Hours(1);
  const base::TimeDelta kMoreThanSnooze =
      user_education::features::GetSnoozeDuration() + base::Hours(1);
  const base::TimeDelta kLessThanAbortCooldown =
      user_education::features::GetAbortCooldown() / 2;
  const base::TimeDelta kMoreThanAbortCooldown =
      user_education::features::GetAbortCooldown() + base::Minutes(5);
  const base::TimeDelta kLessThanNewSession =
      user_education::features::GetIdleTimeBetweenSessions() / 4;
  const base::TimeDelta kMoreThanNewSession =
      user_education::features::GetIdleTimeBetweenSessions() + base::Hours(1);
  const base::TimeDelta kLessThanIdleTime =
      user_education::features::GetTimeToIdle() / 4;
  const base::TimeDelta kMoreThanIdleTime =
      user_education::features::GetTimeToIdle() + base::Seconds(5);

 private:
  // Ensures some basic orderings of values to avoid triggering unexpected
  // behavior.
  void VerifyConstants() {
    CHECK_GT(kLessThanCooldown, kMoreThanNewSession);
    CHECK_GT(kLessThanNewSession, kMoreThanIdleTime);
    CHECK_LT(kMoreThanGracePeriod + kLessThanCooldown,
             user_education::features::GetLowPriorityCooldown());
    CHECK_LT(kMoreThanAbortCooldown + kMoreThanGracePeriod,
             user_education::features::GetSnoozeDuration());
  }

  std::unique_ptr<user_education::HelpBubble> help_bubble_;
  std::unique_ptr<user_education::test::FeaturePromoSessionTestUtil> test_util_;
  base::Time now_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserFeaturePromoControllerPolicyTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& param) {
                           return param.param ? "V2" : "Legacy";
                         });

TEST_P(BrowserFeaturePromoControllerPolicyTest, TwoLowPriorityPromos) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  ExpectShowingPromo(&kTestIPHFeature),
                  MaybeShowPromo(kCustomActionIPHFeature,
                                 FeaturePromoResult::kBlockedByPromo),
                  ExpectShowingPromo(&kTestIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       HighPriorityOverridesLowPriority) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature),
                  MaybeShowPromo(kHighPriorityIPHFeature),
                  ExpectShowingPromo(&kHighPriorityIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, TwoHighPriorityPromos) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kHighPriorityIPHFeature),
                  ExpectShowingPromo(&kHighPriorityIPHFeature),
                  MaybeShowPromo(kHighPriorityIPHFeature2,
                                 FeaturePromoResult::kBlockedByPromo),
                  ExpectShowingPromo(&kHighPriorityIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodBlocksHeavyweightInV2) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod), CheckSessionActive(true),
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByGracePeriod
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlockLightweightInV2) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod), CheckSessionActive(true),
      MaybeShowPromo(kTestIPHFeature, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlockHeavyweightHighPriority) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod), CheckSessionActive(true),
      MaybeShowPromo(kHighPriorityIPHFeature2, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodBlocksHeavyweightInV2AfterNewSession) {
  RunTestSequence(
      ResetSessionData(kLessThanGracePeriod), AdvanceTime(kMoreThanNewSession),
      CheckSessionActive(true),
      MaybeShowPromo(kTutorialIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByGracePeriod
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       GracePeriodDoesNotBlocksHeavyweightLongAfterNewSession) {
  RunTestSequence(
      ResetSessionData(base::Seconds(60)), AdvanceTime(kMoreThanNewSession),
      AdvanceTime(kMoreThanGracePeriod), CheckSessionActive(true),
      MaybeShowPromo(kTutorialIPHFeature, FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, InactivePreventsPromoInV2) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  // Long idle time, but not locked.
                  AdvanceTime(kLessThanNewSession, kMoreThanIdleTime),
                  CheckSessionActive(false),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 UseV2() ? FeaturePromoResult::kBlockedByUi
                                         : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, LockedScreenPreventsPromoInV2) {
  RunTestSequence(ResetSessionData(kLessThanGracePeriod),
                  // Short idle time, but locked.
                  AdvanceTime(kLessThanNewSession, kLessThanIdleTime, true),
                  CheckSessionActive(false),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 UseV2() ? FeaturePromoResult::kBlockedByUi
                                         : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, CooldownPreventsPromoInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
      AdvanceTime(kLessThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      MaybeShowPromo(kCustomActionIPHFeature,
                     UseV2() ? FeaturePromoResult::kBlockedByCooldown
                             : FeaturePromoResult::Success()));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       CooldownDoesNotPreventLightweightPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTestIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       LightweightPromoDoesNotTriggerCooldown) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod), MaybeShowPromo(kTestIPHFeature),
      ClosePromo(), AdvanceTime(kLessThanCooldown),
      AdvanceTime(kMoreThanGracePeriod), MaybeShowPromo(kTutorialIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       CooldownDoesNotPreventHighPriorityPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kLessThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kHighPriorityIPHFeature2));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       ExpiredCooldownDoesNotPreventPromo) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), ClosePromo(),
                  AdvanceTime(kMoreThanCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kCustomActionIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesntTriggerCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  // Show an immediately close the promo without user
                  // interaction.
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  // Immediately try another promo.
                  MaybeShowPromo(kCustomActionIPHFeature));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesTriggerIndividualCooldown) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  AdvanceTime(kLessThanAbortCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 UseV2() ? FeaturePromoResult::kRecentlyAborted
                                         : FeaturePromoResult::kSnoozed));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       AbortedPromoDoesntTriggerSnooze) {
  RunTestSequence(ResetSessionData(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature), AbortPromo(),
                  AdvanceTime(kMoreThanAbortCooldown),
                  AdvanceTime(kMoreThanGracePeriod),
                  MaybeShowPromo(kTutorialIPHFeature,
                                 UseV2() ? FeaturePromoResult::Success()
                                         : FeaturePromoResult::kSnoozed));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest, SnoozeButtonDisappearsInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kSnoozeIPHFeature, -1),
      // Show a snoozable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and veirfy that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kSnoozeIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      If([this] { return UseV2(); },
         EnsureNotPresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
         EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting)));
}

TEST_P(BrowserFeaturePromoControllerPolicyTest,
       TutorialSnoozeButtonChangesInV2) {
  RunTestSequence(
      ResetSessionData(kMoreThanGracePeriod),
      // Simulate N-1 snoozes at some distant time in the past.
      SimulateSnoozes(kTutorialIPHFeature, -1),
      // Show a snoozable promo, verify the snooze button is
      // present, and press it.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      // Wait until after the snooze period expires. We should now
      // be at N snoozes.
      AdvanceTime(kMoreThanCooldown), AdvanceTime(kMoreThanGracePeriod),
      // Show the promo again and veirfy that in V2 the snooze
      // button is *not* present.
      MaybeShowPromo(kTutorialIPHFeature),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(
          HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(UseV2() ? IDS_PROMO_DISMISS_BUTTON
                                            : IDS_PROMO_SNOOZE_BUTTON)));
}

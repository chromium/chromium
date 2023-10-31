// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

using user_education::EndFeaturePromoReason;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSpecification;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);

BASE_FEATURE(kMenuPromoTestFeature,
             "MenuPromoTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

class BrowserAppMenuButtonInteractiveTest : public InteractiveBrowserTest {
 public:
  BrowserAppMenuButtonInteractiveTest() {
    subscription_ = BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker));
  }
  ~BrowserAppMenuButtonInteractiveTest() override = default;
  BrowserAppMenuButtonInteractiveTest(
      const BrowserAppMenuButtonInteractiveTest&) = delete;
  void operator=(const BrowserAppMenuButtonInteractiveTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    mock_tracker_ =
        static_cast<NiceMock<feature_engagement::test::MockTracker>*>(
            feature_engagement::TrackerFactory::GetForBrowserContext(
                browser()->profile()));
    ASSERT_TRUE(mock_tracker_);

    // Allow an unlimited number of calls to WouldTriggerHelpUI().
    EXPECT_CALL(*mock_tracker_, WouldTriggerHelpUI)
        .WillRepeatedly(Return(true));

    promo_controller_ = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());

    // Register test features.
    registry()->RegisterFeature(std::move(
        user_education::FeaturePromoSpecification::CreateForToastPromo(
            kMenuPromoTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetHighlightedMenuItem(
                BookmarkSubMenuModel::kShowBookmarkSidePanelItem)));
  }

  auto AttemptIPH(const base::Feature& iph_feature,
                  user_education::FeaturePromoResult expected_result,
                  base::OnceClosure on_close = base::DoNothing()) {
    return Check([this, &iph_feature, expected_result,
                  callback = std::move(on_close)]() mutable {
      if (expected_result) {
        EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(iph_feature)))
            .WillOnce(Return(true));
      } else {
        EXPECT_CALL(*mock_tracker_, ShouldTriggerHelpUI(Ref(iph_feature)))
            .Times(0);
      }

      user_education::FeaturePromoParams params(iph_feature);
      params.close_callback = std::move(callback);
      if (expected_result !=
          promo_controller_->MaybeShowPromo(std::move(params))) {
        LOG(ERROR) << "MaybeShowPromo() didn't return expected result.";
        return false;
      }
      if (expected_result != promo_controller_->IsPromoActive(iph_feature)) {
        LOG(ERROR) << "IsPromoActive() didn't return expected result.";
        return false;
      }

      // If shown, Tracker::Dismissed should be called eventually.
      if (expected_result) {
        EXPECT_CALL(*mock_tracker_, Dismissed(Ref(iph_feature)));
      }
      return true;
    });
  }

  auto AbortIPH(const base::Feature& iph_feature) {
    return Steps(Do(base::BindLambdaForTesting([this, &iph_feature]() {
      EXPECT_TRUE(promo_controller_->IsPromoActive(iph_feature));
      promo_controller_->EndPromo(iph_feature,
                                  EndFeaturePromoReason::kAbortPromo);

      EXPECT_FALSE(promo_controller_->IsPromoActive(iph_feature));
    })));
  }

  auto CheckAlertStatus(ui::ElementIdentifier element_id, bool is_alerted) {
    return Check([this, element_id, is_alerted]() mutable {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      auto* toolbar = browser_view->toolbar();
      auto* button = toolbar->app_menu_button();
      auto* model = button->app_menu_model();
      EXPECT_EQ(model->IsElementIdAlerted(element_id), is_alerted);
      return true;
    });
  }

  auto CloseMenu() {
    return Do([this]() mutable {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      auto* toolbar = browser_view->toolbar();
      auto* button = toolbar->app_menu_button();
      button->CloseMenu();
    });
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

  UserEducationService* factory() {
    return UserEducationServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  FeaturePromoRegistry* registry() {
    return &factory()->feature_promo_registry();
  }

  raw_ptr<NiceMock<feature_engagement::test::MockTracker>,
          AcrossTasksDanglingUntriaged>
      mock_tracker_;
  raw_ptr<BrowserFeaturePromoController, AcrossTasksDanglingUntriaged>
      promo_controller_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(BrowserAppMenuButtonInteractiveTest,
                       HighlightedIdentifierPromo) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      AttemptIPH(kMenuPromoTestFeature,
                 user_education::FeaturePromoResult::Success()),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      CheckAlertStatus(BookmarkSubMenuModel::kShowBookmarkSidePanelItem, true),
      CloseMenu(), PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      CheckAlertStatus(BookmarkSubMenuModel::kShowBookmarkSidePanelItem,
                       false));
}

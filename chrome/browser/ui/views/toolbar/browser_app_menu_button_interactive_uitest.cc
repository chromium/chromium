// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
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
#include "chrome/test/user_education/interactive_feature_promo_test.h"
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

class BrowserAppMenuButtonInteractiveTest : public InteractiveFeaturePromoTest {
 public:
  BrowserAppMenuButtonInteractiveTest()
      : InteractiveFeaturePromoTest(UseMockTracker()) {}
  ~BrowserAppMenuButtonInteractiveTest() override = default;
  BrowserAppMenuButtonInteractiveTest(
      const BrowserAppMenuButtonInteractiveTest&) = delete;
  void operator=(const BrowserAppMenuButtonInteractiveTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    // Register test features.
    registry()->RegisterFeature(std::move(
        user_education::FeaturePromoSpecification::CreateForToastPromo(
            kMenuPromoTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetHighlightedMenuItem(
                BookmarkSubMenuModel::kShowBookmarkSidePanelItem)));
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
  UserEducationService* factory() {
    return UserEducationServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  FeaturePromoRegistry* registry() {
    return &factory()->feature_promo_registry();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserAppMenuButtonInteractiveTest,
                       HighlightedIdentifierPromo) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      MaybeShowPromo(kMenuPromoTestFeature),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      CheckAlertStatus(BookmarkSubMenuModel::kShowBookmarkSidePanelItem, true),
      CloseMenu(), PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      CheckAlertStatus(BookmarkSubMenuModel::kShowBookmarkSidePanelItem,
                       false));
}

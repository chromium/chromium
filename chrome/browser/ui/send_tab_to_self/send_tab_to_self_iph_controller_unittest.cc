// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

constexpr char kEligibleUrl[] = "https://example.com";

std::unique_ptr<KeyedService> BuildStubSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

class SendTabToSelfIphControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ~SendTabToSelfIphControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildStubSyncService));

    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());

    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
  }

  void TearDown() override {
    DeleteContents();
    tab_strip_model_.reset();
    test_tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetEntryPointDisplayReason(
      std::optional<EntryPointDisplayReason> reason) {
    static_cast<StubSendTabToSelfSyncService*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->SetEntryPointDisplayReason(reason);
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return &mock_browser_window_interface_;
  }

  MockBrowserUserEducationInterface* user_education() {
    return &user_education_;
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  tabs::TabInterface* AddTab(const GURL& url) {
    std::unique_ptr<content::WebContents> contents = CreateWebContents();
    content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    content::WebContents* content_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), true);
    return tab_strip_model_->GetTabForWebContents(content_ptr);
  }

  tabs::TabInterface* AddTab(std::string_view url = kEligibleUrl) {
    return AddTab(GURL(url));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      send_tab_to_self::kSendTabToSelfEnhancedDesktopUI};
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  MockBrowserUserEducationInterface user_education_{
      &mock_browser_window_interface_};
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(SendTabToSelfIphControllerTest, PromoShownWhenEligibleOnConstruction) {
  AddTab(kEligibleUrl);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  SendTabToSelfIphController controller(browser_window_interface());
}

TEST_F(SendTabToSelfIphControllerTest, PromoShownOnTabAdded) {
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  // Adding the eligible tab triggers the promo via OnTabStripModelChanged.
  AddTab(kEligibleUrl);
}

TEST_F(SendTabToSelfIphControllerTest, PromoShownOnTabSelectionChanged) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Start with ineligible entry point.
  SetEntryPointDisplayReason(std::nullopt);
  AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());

  // Add a second tab while still ineligible.
  AddTab(kEligibleUrl);

  // Switch back to tab 0.
  tab_strip_model()->ActivateTabAt(0);

  // Now make entry point eligible. Tab 0 is active, tab 1 is in background.
  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  // Switching active tab to tab 1 triggers promo via OnTabStripModelChanged.
  tab_strip_model()->ActivateTabAt(1);
}

TEST_F(SendTabToSelfIphControllerTest, PromoShownOnTabChangedAt) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Add tab with an ineligible entry point initially.
  SetEntryPointDisplayReason(std::nullopt);
  AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());

  // Now make entry point eligible and trigger OnTabChangedAt.
  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  tabs::TabInterface* tab = tab_strip_model()->GetActiveTab();
  controller.OnTabChangedAt(tab, TabChangeType::kAll);
}

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownOnPartialTabChange) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Add tab with an ineligible entry point initially.
  SetEntryPointDisplayReason(std::nullopt);
  tabs::TabInterface* tab = AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);

  // TabChangeType::kLoadingOnly should not trigger promo.
  controller.OnTabChangedAt(tab, TabChangeType::kLoadingOnly);
}

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownOnInactiveTabChange) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Add two tabs with an ineligible entry point initially.
  SetEntryPointDisplayReason(std::nullopt);
  tabs::TabInterface* tab0 = AddTab(kEligibleUrl);
  AddTab(kEligibleUrl);  // Tab 1 is now active.

  SendTabToSelfIphController controller(browser_window_interface());

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);

  // Change notification on background tab (tab0) should not trigger promo.
  controller.OnTabChangedAt(tab0, TabChangeType::kAll);
}

TEST_F(SendTabToSelfIphControllerTest, PromoShownOnModelReady) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Start with no target device (ineligible entry point).
  SetEntryPointDisplayReason(std::nullopt);
  AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  // When model becomes ready with a valid target device, promo is shown.
  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);
  controller.OnModelReady();
}

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownWhenEnhancedUIDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      send_tab_to_self::kSendTabToSelfEnhancedDesktopUI);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());
  controller.OnModelReady();
}

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownWithoutTargetDevice) {
  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  SetEntryPointDisplayReason(std::nullopt);
  AddTab(kEligibleUrl);

  SendTabToSelfIphController controller(browser_window_interface());
  controller.OnModelReady();
}

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownForNonEligibleReasons) {
  const EntryPointDisplayReason non_eligible_reasons[] = {
      EntryPointDisplayReason::kInformNoTargetDevice,
      EntryPointDisplayReason::kOfferSignIn,
      EntryPointDisplayReason::kOfferReauth,
  };

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  for (auto reason : non_eligible_reasons) {
    SCOPED_TRACE(testing::Message() << "Reason: " << static_cast<int>(reason));
    SetEntryPointDisplayReason(reason);
    AddTab(kEligibleUrl);

    SendTabToSelfIphController controller(browser_window_interface());
    controller.OnModelReady();

    tab_strip_model()->CloseAllTabs();
  }
}

TEST_F(SendTabToSelfIphControllerTest, PromoTriggeredOnlyOnce) {
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)));

  // First tab triggers the promo and removes the observer.
  AddTab(kEligibleUrl);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);

  // Subsequent tab additions or model notifications do not trigger promo again.
  AddTab(kEligibleUrl);
  controller.OnModelReady();
}

TEST_F(SendTabToSelfIphControllerTest, DestructorSafelyUnregistersObserver) {
  SetEntryPointDisplayReason(std::nullopt);
  AddTab(kEligibleUrl);

  {
    SendTabToSelfIphController controller(browser_window_interface());
  }

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);

  EXPECT_CALL(*user_education(),
              MaybeShowStartupFeaturePromo(
                  user_education::test::MatchFeaturePromoParams(
                      feature_engagement::kIPHSendTabToSelfTutorialFeature)))
      .Times(0);
  AddTab(kEligibleUrl);
  tab_strip_model()->ActivateTabAt(0);
  tab_strip_model()->CloseAllTabs();
}

}  // namespace

}  // namespace send_tab_to_self

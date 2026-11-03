// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using feature_engagement::kIPHSendTabToSelfTutorialFeature;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using user_education::FeaturePromoParams;
using user_education::FeaturePromoResult;
using user_education::test::MatchFeaturePromoParams;

constexpr char kEligibleUrl[] = "https://example.com";
constexpr char kSecondEligibleUrl[] = "https://example.org";

std::unique_ptr<KeyedService> BuildStubSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

auto RespondWithPromoResult(FeaturePromoResult result) {
  return [result](FeaturePromoParams params) {
    if (params.show_promo_result_callback) {
      std::move(params.show_promo_result_callback).Run(result);
    }
    return static_cast<bool>(result);
  };
}

class SendTabToSelfIphControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildStubSyncService));

    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());

    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(user_education_, MaybeShowFeaturePromo(_))
        .WillByDefault(RespondWithPromoResult(FeaturePromoResult::Success()));
  }

  void TearDown() override {
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

  tabs::TabInterface* AddTab(std::string_view url) {
    std::unique_ptr<content::WebContents> contents = CreateWebContents();
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL(url));
    content::WebContents* content_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), true);
    return tab_strip_model_->GetTabForWebContents(content_ptr);
  }

  // Adds a session-restored tab whose page hasn't loaded yet.
  tabs::TabInterface* AddRestoredTab(std::string_view url, bool foreground) {
    std::unique_ptr<content::WebContents> contents = CreateWebContents();
    std::unique_ptr<content::NavigationEntry> entry =
        content::NavigationEntry::Create();
    entry->SetURL(GURL(url));
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(std::move(entry));
    contents->GetController().Restore(0, content::RestoreType::kRestored,
                                      &entries);
    content::WebContents* content_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), foreground);
    return tab_strip_model_->GetTabForWebContents(content_ptr);
  }

  void CommitRestoredNavigation(tabs::TabInterface* tab) {
    content::NavigationController& controller =
        tab->GetContents()->GetController();
    controller.LoadIfNecessary();
    content::NavigationSimulator::CreateFromPending(controller)->Commit();
  }

 protected:
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  MockBrowserUserEducationInterface user_education_{
      &mock_browser_window_interface_};
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(SendTabToSelfIphControllerTest, PromoNotShownOnSameDocumentNavigation) {
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);

  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  // Same-document navigations are ignored.
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#ref"),
      tab->GetContents()->GetPrimaryMainFrame())
      ->CommitSameDocument();
}

// First navigation is grace-blocked -> second navigation calls and gets
// Success -> third navigation makes no call.
TEST_F(SendTabToSelfIphControllerTest,
       RetriesAfterBlockedByGracePeriodOnSameTabNavigation) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)))
      .WillOnce(
          RespondWithPromoResult(FeaturePromoResult::kBlockedByGracePeriod));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)))
      .WillOnce(RespondWithPromoResult(FeaturePromoResult::Success()));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
}

// A non-user tab activation makes no call. Navigating the newly active tab
// calls; navigating the old background tab doesn't.
TEST_F(SendTabToSelfIphControllerTest,
       NonUserTabActivationDoesNotTriggerPromo) {
  tabs::TabInterface* tab0 = AddTab(kEligibleUrl);
  tabs::TabInterface* tab1 = AddTab(kSecondEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  // Activation without a user gesture makes no call.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  tab_strip_model()->ActivateTabAt(0);
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  // Navigating the background tab does not call.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab1->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  // Navigating the newly active tab calls.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  content::WebContentsTester::For(tab0->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
}

TEST_F(SendTabToSelfIphControllerTest,
       UserTabSwitchToEligibleTabTriggersPromo) {
  AddTab(kEligibleUrl);
  AddTab(kSecondEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
}

TEST_F(SendTabToSelfIphControllerTest, RestoredNavigationDoesNotTriggerPromo) {
  SendTabToSelfIphController controller(browser_window_interface());
  tabs::TabInterface* tab = AddRestoredTab(kEligibleUrl, /*foreground=*/true);

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  CommitRestoredNavigation(tab);
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  // Observers stay attached for a later navigation.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
}

TEST_F(SendTabToSelfIphControllerTest,
       UserSwitchToUnloadedRestoredTabTriggersPromo) {
  AddTab(kEligibleUrl);
  tabs::TabInterface* restored_tab =
      AddRestoredTab(kSecondEligibleUrl, /*foreground=*/false);
  SendTabToSelfIphController controller(browser_window_interface());
  ASSERT_TRUE(restored_tab->GetContents()
                  ->GetPrimaryMainFrame()
                  ->GetLastCommittedURL()
                  .is_empty());

  // The switch triggers before the tab loads; the restored load doesn't.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)))
      .WillOnce(
          RespondWithPromoResult(FeaturePromoResult::kBlockedByGracePeriod));
  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  CommitRestoredNavigation(restored_tab);
}

TEST_F(SendTabToSelfIphControllerTest,
       RendererNavigationRequiresTransientActivation) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  std::unique_ptr<content::NavigationSimulator> unactivated =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecondEligibleUrl), tab->GetContents()->GetPrimaryMainFrame());
  unactivated->SetHasUserGesture(false);
  unactivated->Commit();
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  std::unique_ptr<content::NavigationSimulator> activated =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kEligibleUrl), tab->GetContents()->GetPrimaryMainFrame());
  activated->SetHasUserGesture(true);
  activated->Commit();
}

TEST_F(SendTabToSelfIphControllerTest,
       StartupPageNavigationDoesNotTriggerPromo) {
  SendTabToSelfIphController controller(browser_window_interface());
  tabs::TabInterface* tab = AddTab(kEligibleUrl);

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL(kSecondEligibleUrl), tab->GetContents());
  navigation->SetTransition(ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  navigation->Commit();
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  // Observers stay attached for a later navigation.
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
}

TEST_F(SendTabToSelfIphControllerTest,
       SubframeOrAbortedNavigationDoesNotTriggerPromo) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(
          tab->GetContents()->GetPrimaryMainFrame())
          ->AppendChild("sub");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kSecondEligibleUrl), child);
  content::NavigationSimulator::CreateBrowserInitiated(GURL(kSecondEligibleUrl),
                                                       tab->GetContents())
      ->AbortCommit();
}

// A non-grace result (kBlockedByCooldown, kBlockedByPromo) stops observing
// navigations and tab switches.
TEST_F(SendTabToSelfIphControllerTest, StopsObservingOnTerminalPromoFailures) {
  for (FeaturePromoResult result : {FeaturePromoResult::kBlockedByCooldown,
                                    FeaturePromoResult::kBlockedByPromo}) {
    SCOPED_TRACE(result);
    AddTab(kSecondEligibleUrl);
    tabs::TabInterface* tab = AddTab(kEligibleUrl);
    SendTabToSelfIphController controller(browser_window_interface());

    EXPECT_CALL(*user_education(),
                MaybeShowFeaturePromo(
                    MatchFeaturePromoParams(kIPHSendTabToSelfTutorialFeature)))
        .WillOnce(RespondWithPromoResult(result));
    content::WebContentsTester::For(tab->GetContents())
        ->NavigateAndCommit(GURL(kSecondEligibleUrl));

    EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
    content::WebContentsTester::For(tab->GetContents())
        ->NavigateAndCommit(GURL(kEligibleUrl));
    tab_strip_model()->ActivateTabAt(
        0, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kMouse));

    ::testing::Mock::VerifyAndClearExpectations(user_education());
    tab_strip_model()->CloseAllTabs();
  }
}

// Display reason nullopt -> no call; then kOfferFeature + navigation -> call.
TEST_F(SendTabToSelfIphControllerTest, PromoDeferredWhileDisplayReasonNullopt) {
  SetEntryPointDisplayReason(std::nullopt);
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
}

// A non-kOfferFeature reason stops observing, parametrized over
// kOfferReauth, kOfferSignIn, kInformNoTargetDevice.
class SendTabToSelfIphControllerNonEligibleReasonTest
    : public SendTabToSelfIphControllerTest,
      public ::testing::WithParamInterface<EntryPointDisplayReason> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    SendTabToSelfIphControllerNonEligibleReasonTest,
    ::testing::Values(EntryPointDisplayReason::kOfferReauth,
                      EntryPointDisplayReason::kOfferSignIn,
                      EntryPointDisplayReason::kInformNoTargetDevice));

TEST_P(SendTabToSelfIphControllerNonEligibleReasonTest,
       NonEligibleReasonStopsObserving) {
  AddTab(kSecondEligibleUrl);
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  SetEntryPointDisplayReason(GetParam());
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);
  // Times(0) still holds: no promo with kOfferFeature as observers are gone.
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
  tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
}

// Pending guard: hold the result callback (don't run it synchronously),
// navigate -> no second call. Run the callback with kBlockedByGracePeriod,
// navigate -> call.
TEST_F(SendTabToSelfIphControllerTest, PendingGuardSuppressesConcurrentCall) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  user_education::FeaturePromoController::ShowPromoResultCallback held_callback;
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)))
      .WillOnce([&held_callback](FeaturePromoParams params) {
        held_callback = std::move(params.show_promo_result_callback);
        return true;
      });

  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
  ::testing::Mock::VerifyAndClearExpectations(user_education());
  ASSERT_FALSE(held_callback.is_null());

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  std::move(held_callback).Run(FeaturePromoResult::kBlockedByGracePeriod);

  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)));
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
}

TEST_F(SendTabToSelfIphControllerTest, DestroyWithPendingRequestIsSafe) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  auto controller =
      std::make_unique<SendTabToSelfIphController>(browser_window_interface());

  user_education::FeaturePromoController::ShowPromoResultCallback held_callback;
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(MatchFeaturePromoParams(
                                     kIPHSendTabToSelfTutorialFeature)))
      .WillOnce([&held_callback](FeaturePromoParams params) {
        held_callback = std::move(params.show_promo_result_callback);
        return true;
      });
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kSecondEligibleUrl));
  ASSERT_FALSE(held_callback.is_null());
  ::testing::Mock::VerifyAndClearExpectations(user_education());

  controller.reset();
  EXPECT_CALL(*user_education(), MaybeShowFeaturePromo(_)).Times(0);
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL(kEligibleUrl));
  tab_strip_model()->CloseAllTabs();
  std::move(held_callback).Run(FeaturePromoResult::kCanceled);
}

TEST_F(SendTabToSelfIphControllerTest, PromoTabInitiallyNotSet) {
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());
  EXPECT_FALSE(controller.IsPromoTab(tab->GetHandle()));
  EXPECT_FALSE(controller.IsPromoTab(tabs::TabHandle::Null()));
}

TEST_F(SendTabToSelfIphControllerTest, SetPromoTabUpdatesRecordedTab) {
  tabs::TabInterface* tab_a = AddTab(kEligibleUrl);
  tabs::TabInterface* tab_b = AddTab(kEligibleUrl);
  SendTabToSelfIphController controller(browser_window_interface());

  controller.SetPromoTab(tab_a->GetHandle());
  EXPECT_TRUE(controller.IsPromoTab(tab_a->GetHandle()));
  EXPECT_FALSE(controller.IsPromoTab(tab_b->GetHandle()));
}

TEST_F(SendTabToSelfIphControllerTest, PromoTabStaysRecordedAfterTabClosed) {
  tabs::TabInterface* tab_a = AddTab(kEligibleUrl);
  tabs::TabInterface* tab_b = AddTab(kEligibleUrl);
  const tabs::TabHandle handle_a = tab_a->GetHandle();
  SendTabToSelfIphController controller(browser_window_interface());

  controller.SetPromoTab(handle_a);
  ASSERT_TRUE(controller.IsPromoTab(handle_a));

  // Closing tab A invalidates its handle, but it stays recorded and no other
  // tab becomes the promo tab.
  tab_strip_model()->CloseWebContentsAt(tab_strip_model()->GetIndexOfTab(tab_a),
                                        TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(handle_a.Get(), nullptr);
  EXPECT_TRUE(controller.IsPromoTab(handle_a));
  EXPECT_FALSE(controller.IsPromoTab(tab_b->GetHandle()));
}

TEST_F(SendTabToSelfIphControllerTest, IsTabEligible) {
  SetEntryPointDisplayReason(std::nullopt);
  tabs::TabInterface* tab = AddTab(kEligibleUrl);
  EXPECT_FALSE(SendTabToSelfIphController::IsTabEligible(nullptr));
  EXPECT_FALSE(SendTabToSelfIphController::IsTabEligible(tab));

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferReauth);
  EXPECT_FALSE(SendTabToSelfIphController::IsTabEligible(tab));

  SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);
  EXPECT_TRUE(SendTabToSelfIphController::IsTabEligible(tab));
}

}  // namespace

}  // namespace send_tab_to_self

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/test/test_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/page_transition_types.h"

namespace autofill {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class AddressBubblesControllerBrowserTest : public InProcessBrowserTest {
 public:
  AddressBubblesControllerBrowserTest() = default;

  AddressBubblesControllerBrowserTest(
      const AddressBubblesControllerBrowserTest&) = delete;
  AddressBubblesControllerBrowserTest& operator=(
      const AddressBubblesControllerBrowserTest&) = delete;
  ~AddressBubblesControllerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SidePanelUI* const side_panel_ui = SidePanelUI::From(browser());
    side_panel_ui->SetNoDelaysForTesting(true);
    side_panel_ui->DisableAnimationsForTesting();
  }

 protected:
  content::WebContents* tab_web_contents() const {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  AddressBubblesController* tab_controller() {
    return AddressBubblesController::FromWebContents(tab_web_contents());
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_features_{
      features::kAutofillAddressUserDeclinedSaveSurvey};
};

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}

// This is testing that the callback is invoked when the dialog is triggered in
// the side panel. It covers the regression found in crbug.com/401068467.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DialogAcceptedInvokesCallbackForSidePanel) {
  SidePanelUI* const side_panel_ui = SidePanelUI::From(browser());
  content::WebContents* side_panel_web_contents =
      side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadingList);
  side_panel_ui->Show(SidePanelEntryId::kReadingList);
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      side_panel_web_contents, profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::FromWebContents(side_panel_web_contents)
      ->OnUserDecision(AutofillClient::AddressPromptUserDecision::kAccepted,
                       std::nullopt);
}

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DeclinedSaveTriggersSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  auto empty_profile = AutofillProfile(AddressCountryCode("US"));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), empty_profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _));
  EXPECT_CALL(
      callback,
      Run(AutofillClient::AddressPromptUserDecision::kDeclined, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       DeclinedSaveWithProfileDoesNotTriggerSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), test::GetFullProfile(), /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/true, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       AcceptedSaveDoesNotTriggerSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  auto empty_profile = AutofillProfile(AddressCountryCode("US"));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), empty_profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      callback,
      Run(AutofillClient::AddressPromptUserDecision::kAccepted, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}
#endif

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  CHECK_EQ(1, tab_strip_model->count());
  // There is only now tab open, so the active web contents, are the
  // controller's web contents.
  content::WebContents* controller_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Now add another tab, and close the controller tab to make sure the window
  // remains open. This should destroy the web contents of the controller and
  // invoke the callback with a decision kIgnored.
  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  // Close controller tab.
  int previous_tab_count = browser()->GetTabStripModel()->count();
  browser()->GetTabStripModel()->CloseWebContents(
      controller_web_contents, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->GetTabStripModel()->count());
}

// This is testing that the bubble is visible and active when shown.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       BubbleShouldBeVisibleByDefault) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(tab_controller()->GetBubbleView());
  EXPECT_TRUE(tab_controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerBrowserTest,
                       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());
  tab_controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace {

std::unique_ptr<KeyedService> BuildSignedOutTestSyncService(
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<NiceMock<syncer::TestSyncService>>();
  sync_service->SetSignedOut();
  return sync_service;
}

class FakeBubbleController : public BubbleControllerBase {
 public:
  explicit FakeBubbleController(BubbleType bubble_type)
      : bubble_type_(bubble_type) {}

  FakeBubbleController(const FakeBubbleController&) = delete;
  FakeBubbleController& operator=(const FakeBubbleController&) = delete;
  ~FakeBubbleController() override = default;

  // BubbleControllerBase:
  void ShowBubble() override { is_showing_ = true; }
  void HideBubble(bool /*initiated_by_bubble_manager*/) override {
    is_showing_ = false;
  }
  void OnBubbleDiscarded() override {}
  BubbleType GetBubbleType() const override { return bubble_type_; }
  bool IsShowingBubble() const override { return is_showing_; }
  bool IsMouseHovered() const override { return false; }
  bool CanBeReshown() const override { return true; }
  bool ShouldReshowOnTabVisible() const override { return false; }
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const BubbleType bubble_type_;
  bool is_showing_ = false;
  base::WeakPtrFactory<FakeBubbleController> weak_ptr_factory_{this};
};

}  // namespace

class AddressBubblesControllerSignInPromoBrowserTest
    : public AddressBubblesControllerBrowserTest {
 public:
  AddressBubblesControllerSignInPromoBrowserTest() = default;
  AddressBubblesControllerSignInPromoBrowserTest(
      const AddressBubblesControllerSignInPromoBrowserTest&) = delete;
  AddressBubblesControllerSignInPromoBrowserTest& operator=(
      const AddressBubblesControllerSignInPromoBrowserTest&) = delete;
  ~AddressBubblesControllerSignInPromoBrowserTest() override = default;

  // AddressBubblesControllerBrowserTest:
  void SetUpOnMainThread() override {
    AddressBubblesControllerBrowserTest::SetUpOnMainThread();
    mock_network_change_notifier_ =
        std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  }

  void TearDownOnMainThread() override {
    mock_network_change_notifier_.reset();
    AddressBubblesControllerBrowserTest::TearDownOnMainThread();
  }

 protected:
  // AddressBubblesControllerBrowserTest:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    AddressBubblesControllerBrowserTest::SetUpBrowserContextKeyedServices(
        context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildSignedOutTestSyncService));
  }

 private:
  std::unique_ptr<net::test::ScopedMockNetworkChangeNotifier>
      mock_network_change_notifier_;
};

// Tests that closing the sign-in promo via `HideBubble()` does not clobber a
// subsequent bubble when the sign-in promo widget finishes closing
// asynchronously.
IN_PROC_BROWSER_TEST_F(
    AddressBubblesControllerSignInPromoBrowserTest,
    SignInPromoAsyncWidgetCloseDoesNotClobberSubsequentBubble) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());
  AutofillBubbleBase* const first_save_bubble_view =
      tab_controller()->GetBubbleView();
  ASSERT_TRUE(first_save_bubble_view);

  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, profile);
  AutofillBubbleBase* const sign_in_promo_view =
      tab_controller()->GetBubbleView();
  ASSERT_TRUE(sign_in_promo_view);
  ASSERT_NE(sign_in_promo_view, first_save_bubble_view);
  ASSERT_TRUE(tab_controller()->IsShowingBubble());

  tab_controller()->HideBubble(/*initiated_by_bubble_manager=*/false);
  EXPECT_FALSE(tab_controller()->IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsBubbleActive());

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());
  AutofillBubbleBase* const second_save_bubble_view =
      tab_controller()->GetBubbleView();
  ASSERT_TRUE(second_save_bubble_view);
  ASSERT_NE(second_save_bubble_view, sign_in_promo_view);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tab_controller()->IsShowingBubble());
  EXPECT_TRUE(tab_controller()->IsBubbleActive());
  EXPECT_EQ(tab_controller()->GetBubbleView(), second_save_bubble_view);
}

// Tests that transitioning from the save-address bubble to the sign-in promo
// keeps lower-priority bubbles queued, hides the sign-in promo on tab switch,
// and does not reshow the sign-in promo when returning to the tab.
IN_PROC_BROWSER_TEST_F(
    AddressBubblesControllerSignInPromoBrowserTest,
    SignInPromoIsManagedByBubbleManagerAndHiddenOnTabSwitch) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());
  AddressBubblesController* const controller = tab_controller();
  AutofillBubbleBase* const save_bubble_view = controller->GetBubbleView();
  ASSERT_TRUE(save_bubble_view);
  BubbleManager* const bubble_manager =
      BubbleManager::GetForTab(browser()->GetActiveTabInterface());
  ASSERT_TRUE(bubble_manager);

  // Queue a lower-priority bubble while the save-address bubble is open.
  FakeBubbleController lower_priority_bubble(BubbleType::kWalletReminderNotice);
  bubble_manager->RequestShowController(lower_priority_bubble,
                                        /*force_show=*/false);
  EXPECT_FALSE(lower_priority_bubble.IsShowingBubble());

  // Accepting the save prompt transitions to the sign-in promo without
  // prematurely advancing the `BubbleManager` queue.
  controller->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, profile);
  EXPECT_TRUE(controller->IsShowingBubble());
  EXPECT_TRUE(controller->IsBubbleActive());
  EXPECT_NE(controller->GetBubbleView(), save_bubble_view);
  EXPECT_FALSE(lower_priority_bubble.IsShowingBubble());

  // Switching away from the tab synchronously hides the sign-in promo.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(controller->IsShowingBubble());
  EXPECT_FALSE(controller->IsBubbleActive());

  // Returning to the tab does not reshow the sign-in promo and instead shows
  // the queued lower-priority bubble.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  EXPECT_FALSE(controller->IsShowingBubble());
  EXPECT_FALSE(controller->IsBubbleActive());
  EXPECT_TRUE(lower_priority_bubble.IsShowingBubble());

  lower_priority_bubble.HideBubble(/*initiated_by_bubble_manager=*/false);
  bubble_manager->OnBubbleHiddenByController(lower_priority_bubble,
                                             /*show_next_bubble=*/false);
}

// Tests that preempting a visible sign-in promo with a higher-priority bubble
// does not re-queue the sign-in promo.
IN_PROC_BROWSER_TEST_F(AddressBubblesControllerSignInPromoBrowserTest,
                       SignInPromoPreemptedByHigherPriorityBubbleIsNotReshown) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());
  AutofillBubbleBase* const save_bubble_view =
      tab_controller()->GetBubbleView();
  ASSERT_TRUE(save_bubble_view);

  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, profile);
  ASSERT_TRUE(tab_controller()->IsShowingBubble());
  ASSERT_NE(tab_controller()->GetBubbleView(), save_bubble_view);

  BubbleManager* const bubble_manager =
      BubbleManager::GetForTab(browser()->GetActiveTabInterface());
  ASSERT_TRUE(bubble_manager);

  FakeBubbleController higher_priority_bubble(BubbleType::kPassword);
  bubble_manager->RequestShowController(higher_priority_bubble,
                                        /*force_show=*/false);
  EXPECT_TRUE(higher_priority_bubble.IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsBubbleActive());

  higher_priority_bubble.HideBubble(/*initiated_by_bubble_manager=*/false);
  bubble_manager->OnBubbleHiddenByController(higher_priority_bubble,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(tab_controller()->IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsBubbleActive());
}

// Tests that `MaybeShowSignInPromo()` neither preempts an active
// higher-priority bubble (e.g., if one appeared while the edit dialog was
// open), nor queues the sign-in promo in `BubbleManager`, nor emits
// `Autofill.Bubble.RequestShow.ForceShow`.
IN_PROC_BROWSER_TEST_F(
    AddressBubblesControllerSignInPromoBrowserTest,
    SignInPromoDoesNotPreemptHigherPriorityBubbleOrQueuePromo) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());

  // Simulate the save bubble being hidden while the modal edit dialog is open
  // and a higher-priority bubble becoming active before edit acceptance.
  tab_controller()->HideBubble(/*initiated_by_bubble_manager=*/false);
  BubbleManager* const bubble_manager =
      BubbleManager::GetForTab(browser()->GetActiveTabInterface());
  ASSERT_TRUE(bubble_manager);

  FakeBubbleController higher_priority_bubble(BubbleType::kPassword);
  bubble_manager->RequestShowController(higher_priority_bubble,
                                        /*force_show=*/false);
  ASSERT_TRUE(higher_priority_bubble.IsShowingBubble());

  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kEditAccepted, profile);
  EXPECT_TRUE(higher_priority_bubble.IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsBubbleActive());
  histogram_tester.ExpectTotalCount("Autofill.Bubble.RequestShow.ForceShow", 0);

  // `OnIconClicked()` only reopens undecided save/update prompts and must not
  // show a sign-in promo.
  tab_controller()->OnIconClicked();
  EXPECT_TRUE(higher_priority_bubble.IsShowingBubble());
  EXPECT_FALSE(tab_controller()->IsShowingBubble());
  histogram_tester.ExpectTotalCount("Autofill.Bubble.RequestShow.ForceShow", 0);

  // Closing the higher-priority bubble does not reshow the skipped promo, and
  // a subsequent save-address prompt is not auto-declined.
  higher_priority_bubble.HideBubble(/*initiated_by_bubble_manager=*/false);
  bubble_manager->OnBubbleHiddenByController(higher_priority_bubble,
                                             /*show_next_bubble=*/true);
  EXPECT_FALSE(tab_controller()->IsShowingBubble());

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/false, base::DoNothing());
  EXPECT_TRUE(tab_controller()->IsShowingBubble());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace autofill

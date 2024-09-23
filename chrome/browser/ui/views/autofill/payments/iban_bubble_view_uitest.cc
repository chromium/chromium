// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

const char kIbanForm[] = "/autofill_iban_form.html";
constexpr char kIbanValue[] = "DE91 1000 0000 0123 4567 89";
constexpr char kIbanValueWithoutWhitespaces[] = "DE91100000000123456789";
constexpr char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforcreatepaymentinstrument";
constexpr char kResponseGetUploadDetailsSuccess[] =
    "{\"iban_details\":{\"validation_regex\":"
    "\"^[A-Z]{2}[0-9]{2}[A-Z0-9]{4}[0-9]{7}[A-Z0-9]{0,18}$\"},"
    "\"legal_message\":{\"line\":[{\"template\":\"Legal message template with"
    " link: {0}.\",\"template_parameter\":[{\"display_text\":\"Link\","
    "\"url\":\"https://www.example.com/\"}]}]},\"context_token\":"
    "\"dummy_context_token\"}";
constexpr char kURLUploadIbanRequest[] =
    "https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    "createpaymentinstrument"
    "?s7e_suffix=chromewallet";
constexpr char kResponsePaymentsSuccess[] = "Success";
constexpr char kResponsePaymentsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

class IbanBubbleViewFullFormBrowserTest
    : public SyncTest,
      public IbanSaveManager::ObserverForTest,
      public IbanBubbleControllerImpl::ObserverForTest {
 protected:
  IbanBubbleViewFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {}

 public:
  IbanBubbleViewFullFormBrowserTest(const IbanBubbleViewFullFormBrowserTest&) =
      delete;
  IbanBubbleViewFullFormBrowserTest& operator=(
      const IbanBubbleViewFullFormBrowserTest&) = delete;
  ~IbanBubbleViewFullFormBrowserTest() override = default;

 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    OFFERED_UPLOAD_SAVE,
    ACCEPT_SAVE_IBAN_COMPLETE,
    DECLINE_SAVE_IBAN_COMPLETE,
    ON_RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    REQUESTED_UPLOAD_SAVE,
    ACCEPT_UPLOAD_SAVE_IBAN_COMPLETE,
    ACCEPT_UPLOAD_SAVE_IBAN_FAILED,
    BUBBLE_SHOWN,
    ICON_SHOWN
  };

  // SyncTest::SetUpOnMainThread:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ASSERT_TRUE(SetupClients());

    // It's important to use the blank tab here and not some arbitrary page.
    // This causes the RenderFrameHost to stay the same when navigating to the
    // HTML pages in tests. Since ContentAutofillDriver is per RFH, the driver
    // that this method starts observing will also be the one to notify later.
    AddBlankTabAndShow(GetBrowser(0));

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile(0));

    // Set up the URL loader factory for the PaymentsNetworkInterface so we can
    // intercept those network requests.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    autofill_manager()
        ->client()
        .GetPaymentsAutofillClient()
        ->GetPaymentsNetworkInterface()
        ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

    // Set up this class as the ObserverForTest implementation.
    iban_save_manager_ =
        test_api(*autofill_manager()->client().GetFormDataImporter())
            .iban_save_manager();
    iban_save_manager_->SetEventObserverForTesting(this);
    AddEventObserverToController();
  }

  void TearDownOnMainThread() override {
    // Explicitly set to null to avoid that it becomes dangling.
    iban_save_manager_ = nullptr;

    SyncTest::TearDownOnMainThread();
  }

  // The primary main frame's AutofillManager.
  TestAutofillManager* autofill_manager() {
    return autofill_manager_injector_[GetActiveWebContents()];
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  // IbanSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnOfferUploadSave() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::OFFERED_UPLOAD_SAVE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnAcceptSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnDeclineSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::DECLINE_SAVE_IBAN_COMPLETE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(
          DialogEvent::ON_RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnSentUploadRequest() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::REQUESTED_UPLOAD_SAVE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnAcceptUploadSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ACCEPT_UPLOAD_SAVE_IBAN_COMPLETE);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnAcceptUploadSaveIbanFailed() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ACCEPT_UPLOAD_SAVE_IBAN_FAILED);
    }
  }

  // IbanBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
    }
  }

  // IbanSaveManager::ObserverForTest:
  void OnIconShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ICON_SHOWN);
    }
  }

  void NavigateToAndWaitForForm(const std::string& file_path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        GetBrowser(0), embedded_test_server()->GetURL(file_path)));
    ASSERT_TRUE(autofill_manager()->WaitForFormsSeen(1));
  }

  void SubmitFormAndWaitForIbanLocalSaveBubble() {
    ResetEventWaiterForSequence(
        {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecJs(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for autofill_iban_form.html.
  void FillForm(std::optional<std::string> iban_value = std::nullopt) {
    NavigateToAndWaitForForm(kIbanForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    if (iban_value.has_value()) {
      std::string set_value_js =
          "(function() { document.getElementById('iban').value ='" +
          iban_value.value() + "';})();";
      ASSERT_TRUE(content::ExecJs(web_contents, set_value_js));
    }
  }

  views::View* FindViewInBubbleById(DialogViewId view_id) {
    LocationBarBubbleDelegateView* iban_bubble_view =
        GetIbanBubbleDelegateView();
    views::View* specified_view =
        iban_bubble_view->GetViewByID(static_cast<int>(view_id));
    if (!specified_view) {
      // Many of the save IBAN bubble's inner views are not child views but
      // rather contained by the dialog. If we didn't find what we were
      // looking for, check there as well.
      specified_view =
          iban_bubble_view->GetWidget()->GetRootView()->GetViewByID(
              static_cast<int>(view_id));
    }
    return specified_view;
  }

  void ClickOnSaveButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        FindViewInBubbleById(DialogViewId::OK_BUTTON));
  }

  void ClickOnCancelButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
  }

  void ClickOnCloseButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        save_iban_bubble_views->GetBubbleFrameView()->close_button());
    CHECK(!GetSaveIbanBubbleView());
  }

  SaveIbanBubbleView* GetSaveIbanBubbleView() {
    AutofillBubbleBase* iban_bubble_view = GetIbanBubbleView();
    if (!iban_bubble_view) {
      return nullptr;
    }
    return static_cast<SaveIbanBubbleView*>(iban_bubble_view);
  }

  ManageSavedIbanBubbleView* GetManageSavedIbanBubbleView() {
    AutofillBubbleBase* iban_bubble_view = GetIbanBubbleView();
    if (!iban_bubble_view) {
      return nullptr;
    }
    return static_cast<ManageSavedIbanBubbleView*>(iban_bubble_view);
  }

  IbanBubbleType GetBubbleType() {
    IbanBubbleController* iban_bubble_controller =
        IbanBubbleController::GetOrCreate(GetActiveWebContents());
    if (!iban_bubble_controller) {
      return IbanBubbleType::kInactive;
    }
    return iban_bubble_controller->GetBubbleType();
  }

  SavePaymentIconView* GetSaveIbanIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(GetBrowser(0));
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kSaveIban);
    CHECK(browser_view->GetLocationBarView()->Contains(icon));
    return static_cast<SavePaymentIconView*>(icon);
  }

  content::WebContents* GetActiveWebContents() {
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  }

  void AddEventObserverToController() {
    IbanBubbleControllerImpl* save_iban_bubble_controller_impl =
        static_cast<IbanBubbleControllerImpl*>(
            IbanBubbleController::GetOrCreate(GetActiveWebContents()));
    CHECK(save_iban_bubble_controller_impl);
    save_iban_bubble_controller_impl->SetEventObserverForTesting(this);
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void ClickOnView(views::View* view) {
    CHECK(view);
    ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(),
                       gfx::Point(), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnDialogView(views::View* view) {
    GetIbanBubbleDelegateView()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetIbanBubbleDelegateView()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
  }

  void ClickOnDialogViewAndWaitForWidgetDestruction(views::View* view) {
    EXPECT_TRUE(GetIbanBubbleView());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetSaveIbanBubbleView()->GetWidget());
    ClickOnDialogView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetIbanBubbleView());
  }

  views::Textfield* nickname_input() {
    return static_cast<views::Textfield*>(
        FindViewInBubbleById(DialogViewId::NICKNAME_TEXTFIELD));
  }

  [[nodiscard]] testing::AssertionResult WaitForObservedEvent() {
    return event_waiter_->Wait();
  }

  raw_ptr<IbanSaveManager> iban_save_manager_ = nullptr;

 private:
  LocationBarBubbleDelegateView* GetIbanBubbleDelegateView() {
    LocationBarBubbleDelegateView* iban_bubble_view = nullptr;
    switch (GetBubbleType()) {
      case IbanBubbleType::kLocalSave:
      case IbanBubbleType::kUploadSave: {
        iban_bubble_view = GetSaveIbanBubbleView();
        CHECK(iban_bubble_view);
        break;
      }
      case IbanBubbleType::kManageSavedIban: {
        iban_bubble_view = GetManageSavedIbanBubbleView();
        CHECK(iban_bubble_view);
        break;
      }
      case IbanBubbleType::kUploadCompleted:
      case IbanBubbleType::kInactive:
        NOTREACHED();
    }
    return iban_bubble_view;
  }

  AutofillBubbleBase* GetIbanBubbleView() {
    IbanBubbleController* iban_bubble_controller =
        IbanBubbleController::GetOrCreate(GetActiveWebContents());
    if (!iban_bubble_controller) {
      return nullptr;
    }
    return iban_bubble_controller->GetPaymentBubbleView();
  }

  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
};

// Tests the local save bubble. Ensures that clicking the 'No thanks' button
// successfully causes the bubble to go away, and causes a strike to be added.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  base::HistogramTester histogram_tester;
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  // Clicking 'No thanks' should cancel and close it.
  ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  EXPECT_EQ(
      1, iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()->GetStrikes(
             IbanSaveManager::GetPartialIbanHashString(
                 kIbanValueWithoutWhitespaces)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kCancelled, 1);
}

// Tests the local save bubble. Ensures that clicking the [X] button
// successfully causes the bubble to go away, and causes a strike to be added.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingXIconClosesBubble) {
  base::HistogramTester histogram_tester;
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  // Clicking [X] should close the bubble.
  ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
  ClickOnCloseButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  EXPECT_EQ(
      1, iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()->GetStrikes(
             IbanSaveManager::GetPartialIbanHashString(
                 kIbanValueWithoutWhitespaces)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kClosed, 1);
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt max times and ensuring that the
// offer-to-save bubble does not appear on the next try. Then, ensures that no
// strikes are added if the IBAN already has max strikes.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  base::HistogramTester histogram_tester;
  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0; i < iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()
                          ->GetMaxStrikesLimit();
       ++i) {
    FillForm(kIbanValue);
    SubmitFormAndWaitForIbanLocalSaveBubble();

    ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
    ClickOnCancelButton();
    ASSERT_TRUE(WaitForObservedEvent());
  }
  EXPECT_EQ(
      iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()->GetStrikes(
          IbanSaveManager::GetPartialIbanHashString(
              kIbanValueWithoutWhitespaces)),
      iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()
          ->GetMaxStrikesLimit());
  // Submit the form a fourth time. Since the IBAN now has maximum strikes,
  // the bubble should not be shown.
  FillForm(kIbanValue);
  ResetEventWaiterForSequence(
      {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN});
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_TRUE(
      iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()
          ->ShouldBlockFeature(IbanSaveManager::GetPartialIbanHashString(
              kIbanValueWithoutWhitespaces)));

  EXPECT_TRUE(GetSaveIbanIconView()->GetVisible());
  EXPECT_FALSE(GetSaveIbanBubbleView());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.Reshows",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);

  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kNotShownMaxStrikesReached, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 3);
}

// Tests the local save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", false, 1);
}

// Tests the local save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble_WithNickname) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();
  nickname_input()->SetText(u"My doctor's IBAN");

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", true, 1);
}

// Tests the local save bubble. Ensures that clicking the [X] button will still
// see the omnibox icon.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingClosesBubbleStillShowOmnibox) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ClickOnCloseButton();
  EXPECT_TRUE(GetSaveIbanIconView()->GetVisible());
  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kClosed, 1);
}

// Tests the local save bubble. Ensures that clicking the omnibox icon opens
// manage saved IBAN bubble with IBAN nickname.
// crbug.com/330725101: disabled because it's flaky
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       DISABLED_Local_SavedIbanHasNickname) {
  const std::u16string kNickname = u"My doctor's IBAN";
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();
  nickname_input()->SetText(kNickname);

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Open up manage IBANs bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  const views::Label* nickname_label = static_cast<views::Label*>(
      FindViewInBubbleById(DialogViewId::NICKNAME_LABEL));
  EXPECT_TRUE(nickname_label);
  EXPECT_EQ(nickname_label->GetText(), kNickname);
  // Verify the bubble type is manage saved IBAN.
  ASSERT_EQ(GetBubbleType(), IbanBubbleType::kManageSavedIban);
}

// Tests the local save bubble. Ensures that clicking the omnibox icon opens
// manage saved IBAN bubble without IBAN nickname.
// crbug.com/330725101: disabled because it's flaky
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       DISABLED_Local_SavedIbanNoNickname) {
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Open up manage IBANs bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::NICKNAME_LABEL));
  // Verify the bubble type is manage saved IBAN.
  ASSERT_EQ(GetBubbleType(), IbanBubbleType::kManageSavedIban);
}

// Sets up Chrome with Sync-the-transport mode enabled, with the Wallet datatype
// as enabled type.
class IbanBubbleViewSyncTransportFullFormBrowserTest
    : public IbanBubbleViewFullFormBrowserTest {
 public:
  IbanBubbleViewSyncTransportFullFormBrowserTest(
      const IbanBubbleViewSyncTransportFullFormBrowserTest&) = delete;
  IbanBubbleViewSyncTransportFullFormBrowserTest& operator=(
      const IbanBubbleViewSyncTransportFullFormBrowserTest&) = delete;
  ~IbanBubbleViewSyncTransportFullFormBrowserTest() override = default;

  void SetUpOnMainThread() override {
    IbanBubbleViewFullFormBrowserTest::SetUpOnMainThread();
  }

 protected:
  IbanBubbleViewSyncTransportFullFormBrowserTest() {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableServerIban);
  }

  void SetUpForSyncTransportModeTest() {
    // On ChromeOS, the test profile starts with a primary account already set,
    // so below tests doesn't apply.
#if !BUILDFLAG(IS_CHROMEOS)
    // Signing in (without granting sync consent or explicitly setting up Sync)
    // should trigger starting the Sync machinery in standalone transport mode.
    secondary_account_helper::SignInUnconsentedAccount(
        GetProfile(0), test_url_loader_factory(), "user@gmail.com");
#endif  // !BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
    ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
              GetSyncService(0)->GetTransportState());

    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
              GetSyncService(0)->GetTransportState());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  }

  void SubmitFormAndWaitForUploadSaveBubble() {
    SetUploadDetailsRpcPaymentsSucceeds();
    ResetEventWaiterForSequence(
        {DialogEvent::ON_RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
         DialogEvent::BUBBLE_SHOWN, DialogEvent::OFFERED_UPLOAD_SAVE});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->GetVisible());

    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::LEGAL_MESSAGE_VIEW)->GetVisible());
  }

  void SubmitFormAndPreflightCallFailThenWaitForLocalSaveBubble() {
    SetUploadDetailsRpcPaymentsFails();
    ResetEventWaiterForSequence(
        {DialogEvent::ON_RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
         DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SetUploadDetailsRpcPaymentsSucceeds() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsFails() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponsePaymentsFailure);
  }

  void SetUploadIbanRpcPaymentsSucceeds() {
    test_url_loader_factory()->AddResponse(kURLUploadIbanRequest,
                                           kResponsePaymentsSuccess);
  }

  void SetUploadIbanRpcPaymentsFails() {
    test_url_loader_factory()->AddResponse(kURLUploadIbanRequest,
                                           kResponsePaymentsFailure);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the upload save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewSyncTransportFullFormBrowserTest,
                       Upload_ClickingSaveClosesBubble_Success) {
  SetUploadIbanRpcPaymentsSucceeds();
  SetUpForSyncTransportModeTest();

  // Submitting the form should trigger the flow of asking Payments if
  // Chrome should offer to upload save.
  FillForm(kIbanValue);
  SubmitFormAndWaitForUploadSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE,
                               DialogEvent::ACCEPT_UPLOAD_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away. Also, verify that a failed IBAN
// upload adds a strike to the strike database.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewSyncTransportFullFormBrowserTest,
                       Upload_ClickingSaveClosesBubble_Fail) {
  SetUploadIbanRpcPaymentsFails();
  SetUpForSyncTransportModeTest();

  // Submitting the form should trigger the flow of asking Payments if
  // Chrome should offer to upload save.
  FillForm(kIbanValue);
  SubmitFormAndWaitForUploadSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE,
                               DialogEvent::ACCEPT_UPLOAD_SAVE_IBAN_FAILED});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_EQ(
      1, iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()->GetStrikes(
             IbanSaveManager::GetPartialIbanHashString(
                 kIbanValueWithoutWhitespaces)));
}

// Tests the upload save bubble. Ensures that clicking the 'No thanks' button
// successfully causes the bubble to go away. Also, verify that a failed IBAN
// upload adds a strike to the strike database.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewSyncTransportFullFormBrowserTest,
                       Upload_ClickingDeclineClosesBubble) {
  SetUpForSyncTransportModeTest();

  // Submitting the form should trigger the flow of asking Payments if
  // Chrome should offer to upload save.
  FillForm(kIbanValue);
  SubmitFormAndWaitForUploadSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_EQ(
      1, iban_save_manager_->GetIbanSaveStrikeDatabaseForTesting()->GetStrikes(
             IbanSaveManager::GetPartialIbanHashString(
                 kIbanValueWithoutWhitespaces)));
}

// Test that upload save should not be offered when the preflight call failed.
// In this case, local save should be offered.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewSyncTransportFullFormBrowserTest,
                       Upload_FailedPreflightCallThenLocalSave) {
  SetUpForSyncTransportModeTest();

  // Submitting the form should trigger local save flow because preflight call
  // fails.
  FillForm();
  SubmitFormAndPreflightCallFailThenWaitForLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());
}

}  // namespace
}  // namespace autofill

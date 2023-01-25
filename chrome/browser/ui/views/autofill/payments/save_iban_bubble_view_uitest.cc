// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/save_iban_bubble_controller_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database_integrator_base.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {
const char kIbanForm[] = "/autofill_iban_form.html";
constexpr char kIbanValue[] = "DE91 1000 0000 0123 4567 89";
}  // namespace

namespace autofill {

class SaveIbanBubbleViewFullFormBrowserTest
    : public SyncTest,
      public IBANSaveManager::ObserverForTest,
      public SaveIbanBubbleControllerImpl::ObserverForTest {
 protected:
  SaveIbanBubbleViewFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::kAutofillFillIbanFields,
                              autofill::features::kAutofillParseIBANFields},
        /*disabled_features=*/{});
  }

 public:
  SaveIbanBubbleViewFullFormBrowserTest(
      const SaveIbanBubbleViewFullFormBrowserTest&) = delete;
  SaveIbanBubbleViewFullFormBrowserTest& operator=(
      const SaveIbanBubbleViewFullFormBrowserTest&) = delete;
  ~SaveIbanBubbleViewFullFormBrowserTest() override = default;

 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    TestAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver,
                                 client,
                                 "en-US",
                                 EnableDownloadManager(false)) {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {&AutofillManager::Observer::OnAfterFormsSeen}};
  };

  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    ACCEPT_SAVE_IBAN_COMPLETE,
    DECLINE_SAVE_IBAN_COMPLETE,
    BUBBLE_SHOWN,
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
    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
            GetActiveWebContents());

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile(0));

    // Set up this class as the ObserverForTest implementation.
    iban_save_manager_ = ContentAutofillDriver::GetForRenderFrameHost(
                             GetActiveWebContents()->GetPrimaryMainFrame())
                             ->autofill_manager()
                             ->client()
                             ->GetFormDataImporter()
                             ->iban_save_manager_for_testing();
    iban_save_manager_->SetEventObserverForTesting(this);
    AddEventObserverToController();
  }

  // The primary main frame's AutofillManager.
  TestAutofillManager* GetAutofillManager() {
    DCHECK(autofill_manager_injector_);
    return autofill_manager_injector_->GetForPrimaryMainFrame();
  }

  // IBANSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
    }
  }

  // IBANSaveManager::ObserverForTest:
  void OnAcceptSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE);
    }
  }

  // IBANSaveManager::ObserverForTest:
  void OnDeclineSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::DECLINE_SAVE_IBAN_COMPLETE);
    }
  }

  // SaveIbanBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
    }
  }

  void NavigateToAndWaitForForm(const std::string& file_path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        GetBrowser(0), embedded_test_server()->GetURL(file_path)));
    ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
  }

  void SubmitFormAndWaitForIbanLocalSaveBubble() {
    ResetEventWaiterForSequence(
        {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    WaitForObservedEvent();
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for autofill_iban_form.html.
  void FillForm(absl::optional<std::string> iban_value = absl::nullopt) {
    NavigateToAndWaitForForm(kIbanForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    if (iban_value.has_value()) {
      std::string set_value_js =
          "(function() { document.getElementById('iban').value ='" +
          iban_value.value() + "';})();";
      ASSERT_TRUE(content::ExecuteScript(web_contents, set_value_js));
    }
  }

  views::View* FindViewInBubbleById(DialogViewId view_id) {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);

    views::View* specified_view =
        save_iban_bubble_views->GetViewByID(static_cast<int>(view_id));

    if (!specified_view) {
      // Many of the save IBAN bubble's inner Views are not child views but
      // rather contained by the dialog. If we didn't find what we were looking
      // for, check there as well.
      specified_view =
          save_iban_bubble_views->GetWidget()->GetRootView()->GetViewByID(
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

  SaveIbanBubbleView* GetSaveIbanBubbleView() {
    SaveIbanBubbleController* save_iban_bubble_controller =
        SaveIbanBubbleController::GetOrCreate(GetActiveWebContents());
    if (!save_iban_bubble_controller) {
      return nullptr;
    }

    AutofillBubbleBase* save_iban_bubble_view =
        save_iban_bubble_controller->GetSaveBubbleView();
    if (!save_iban_bubble_view) {
      return nullptr;
    }

    return static_cast<SaveIbanBubbleView*>(save_iban_bubble_view);
  }

  content::WebContents* GetActiveWebContents() {
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  }

  void AddEventObserverToController() {
    SaveIbanBubbleControllerImpl* save_iban_bubble_controller_impl =
        static_cast<SaveIbanBubbleControllerImpl*>(
            SaveIbanBubbleController::GetOrCreate(GetActiveWebContents()));
    CHECK(save_iban_bubble_controller_impl);
    save_iban_bubble_controller_impl->SetEventObserverForTesting(this);
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void ClickOnView(views::View* view) {
    CHECK(view);
    ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnDialogView(views::View* view) {
    GetSaveIbanBubbleView()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetSaveIbanBubbleView()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
  }

  void ClickOnDialogViewAndWaitForWidgetDestruction(views::View* view) {
    EXPECT_TRUE(GetSaveIbanBubbleView());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetSaveIbanBubbleView()->GetWidget());
    ClickOnDialogView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetSaveIbanBubbleView());
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  raw_ptr<IBANSaveManager, DanglingUntriaged> iban_save_manager_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
};

// Tests the local save bubble. Ensures that clicking the 'No thanks' button
// successfully causes the bubble to go away, and causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveIbanBubbleViewFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  // Clicking 'No thanks' should cancel and close it.
  ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
  ClickOnCancelButton();
  WaitForObservedEvent();

  EXPECT_FALSE(GetSaveIbanBubbleView());
  EXPECT_EQ(
      1, iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()->GetStrikes(
             kIbanValue));
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt max times and ensuring that the
// offer-to-save bubble does not appear on the next try. Then, ensures that no
// strikes are added if the IBAN already has max strikes.
IN_PROC_BROWSER_TEST_F(SaveIbanBubbleViewFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0; i < iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
                          ->GetMaxStrikesLimit();
       ++i) {
    FillForm(kIbanValue);
    SubmitFormAndWaitForIbanLocalSaveBubble();

    ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
    ClickOnCancelButton();
    WaitForObservedEvent();
  }
  EXPECT_EQ(
      iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()->GetStrikes(
          kIbanValue),
      iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
          ->GetMaxStrikesLimit());
  // Submit the form a fourth time. Since the IBAN now has maximum strikes,
  // the bubble should not be shown.
  FillForm(kIbanValue);
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  SubmitForm();
  WaitForObservedEvent();

  EXPECT_TRUE(iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
                  ->ShouldBlockFeature(kIbanValue));
  EXPECT_FALSE(GetSaveIbanBubbleView());
}

// Tests the local save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveIbanBubbleViewFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  WaitForObservedEvent();

  EXPECT_FALSE(GetSaveIbanBubbleView());
}

}  // namespace autofill

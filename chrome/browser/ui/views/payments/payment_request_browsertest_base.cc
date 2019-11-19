// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/address_combobox_model.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"

namespace payments {

namespace {
const auto kBillingAddressType = autofill::ADDRESS_BILLING_LINE1;

// This is preferred to SelectValue, since only SetSelectedRow fires the events
// as if done by a user.
void SelectComboboxRowForValue(views::Combobox* combobox,
                               const base::string16& text) {
  int i;
  for (i = 0; i < combobox->GetRowCount(); i++) {
    if (combobox->GetTextForRow(i) == text)
      break;
  }
  DCHECK(i < combobox->GetRowCount()) << "Combobox does not contain " << text;
  combobox->SetSelectedRow(i);
}

}  // namespace

PersonalDataLoadedObserverMock::PersonalDataLoadedObserverMock() = default;
PersonalDataLoadedObserverMock::~PersonalDataLoadedObserverMock() = default;

PaymentRequestBrowserTestBase::PaymentRequestBrowserTestBase() = default;
PaymentRequestBrowserTestBase::~PaymentRequestBrowserTestBase() = default;

void PaymentRequestBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // HTTPS server only serves a valid cert for localhost, so this is needed to
  // load pages from "a.com" without an interstitial.
  command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
}

void PaymentRequestBrowserTestBase::SetUpOnMainThread() {
  // Setup the https server.
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(https_server_->InitializeAndListen());
  https_server_->ServeFilesFromSourceDirectory("components/test/data/payments");
  https_server_->StartAcceptingConnections();

  Observe(GetActiveWebContents());

  // Starting now, PaymentRequest Mojo messages sent by the renderer will
  // create PaymentRequest objects via this test's CreatePaymentRequestForTest,
  // allowing the test to inject itself as a dialog observer.
  payments::SetPaymentRequestFactoryForTesting(base::BindRepeating(
      &PaymentRequestBrowserTestBase::CreatePaymentRequestForTest,
      base::Unretained(this)));

  // Set a test sync service so that all types of cards work.
  GetDataManager()->SetSyncServiceForTest(&sync_service_);

  // Register all prefs with our pref testing service.
  payments::RegisterProfilePrefs(prefs_.registry());
}

void PaymentRequestBrowserTestBase::NavigateTo(const std::string& file_path) {
  if (file_path.find("data:") == 0U) {
    ui_test_utils::NavigateToURL(browser(), GURL(file_path));
  } else {
    ui_test_utils::NavigateToURL(browser(),
                                 https_server()->GetURL("a.com", file_path));
  }
}

void PaymentRequestBrowserTestBase::SetIncognito() {
  is_incognito_ = true;
}

void PaymentRequestBrowserTestBase::SetInvalidSsl() {
  is_valid_ssl_ = false;
}

void PaymentRequestBrowserTestBase::SetBrowserWindowInactive() {
  is_browser_window_active_ = false;
}

void PaymentRequestBrowserTestBase::SetSkipUiForForBasicCard() {
  skip_ui_for_basic_card_ = true;
}

void PaymentRequestBrowserTestBase::OnCanMakePaymentCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CAN_MAKE_PAYMENT_CALLED);
}

void PaymentRequestBrowserTestBase::OnCanMakePaymentReturned() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CAN_MAKE_PAYMENT_RETURNED);
}

void PaymentRequestBrowserTestBase::OnHasEnrolledInstrumentCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED);
}

void PaymentRequestBrowserTestBase::OnHasEnrolledInstrumentReturned() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED);
}

void PaymentRequestBrowserTestBase::OnNotSupportedError() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::NOT_SUPPORTED_ERROR);
}

void PaymentRequestBrowserTestBase::OnConnectionTerminated() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::DIALOG_CLOSED);
}

void PaymentRequestBrowserTestBase::OnAbortCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::ABORT_CALLED);
}

void PaymentRequestBrowserTestBase::OnDialogOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::DIALOG_OPENED);
}

void PaymentRequestBrowserTestBase::OnOrderSummaryOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::ORDER_SUMMARY_OPENED);
}

void PaymentRequestBrowserTestBase::OnPaymentMethodOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PAYMENT_METHOD_OPENED);
}

void PaymentRequestBrowserTestBase::OnShippingAddressSectionOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SHIPPING_ADDRESS_SECTION_OPENED);
}

void PaymentRequestBrowserTestBase::OnShippingOptionSectionOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SHIPPING_OPTION_SECTION_OPENED);
}

void PaymentRequestBrowserTestBase::OnCreditCardEditorOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
}

void PaymentRequestBrowserTestBase::OnShippingAddressEditorOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
}

void PaymentRequestBrowserTestBase::OnContactInfoEditorOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CONTACT_INFO_EDITOR_OPENED);
}

void PaymentRequestBrowserTestBase::OnBackNavigation() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BACK_NAVIGATION);
}

void PaymentRequestBrowserTestBase::OnBackToPaymentSheetNavigation() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);
}

void PaymentRequestBrowserTestBase::OnContactInfoOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CONTACT_INFO_OPENED);
}

void PaymentRequestBrowserTestBase::OnEditorViewUpdated() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::EDITOR_VIEW_UPDATED);
}

void PaymentRequestBrowserTestBase::OnErrorMessageShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::ERROR_MESSAGE_SHOWN);
}

void PaymentRequestBrowserTestBase::OnSpecDoneUpdating() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::SPEC_DONE_UPDATING);
}

void PaymentRequestBrowserTestBase::OnCvcPromptShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::CVC_PROMPT_SHOWN);
}

void PaymentRequestBrowserTestBase::OnProcessingSpinnerShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PROCESSING_SPINNER_SHOWN);
}

void PaymentRequestBrowserTestBase::OnProcessingSpinnerHidden() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PROCESSING_SPINNER_HIDDEN);
}

void PaymentRequestBrowserTestBase::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe, render_frame_host);
}

void PaymentRequestBrowserTestBase::InvokePaymentRequestUI() {
  ResetEventWaiterForDialogOpened();

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('buy').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
}

void PaymentRequestBrowserTestBase::ExpectBodyContains(
    const std::vector<std::string>& expected_strings) {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string extract_contents_js =
      "(function() { "
      "window.domAutomationController.send(window.document.body.textContent); "
      "})()";
  std::string contents;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, extract_contents_js, &contents));
  for (const std::string& expected_string : expected_strings) {
    EXPECT_NE(std::string::npos, contents.find(expected_string))
        << "String \"" << expected_string
        << "\" is not present in the content \"" << contents << "\"";
  }
}

void PaymentRequestBrowserTestBase::OpenOrderSummaryScreen() {
  ResetEventWaiter(DialogEvent::ORDER_SUMMARY_OPENED);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION);
}

void PaymentRequestBrowserTestBase::OpenPaymentMethodScreen() {
  ResetEventWaiter(DialogEvent::PAYMENT_METHOD_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingAddressSectionScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_SECTION_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingOptionSectionScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_OPTION_SECTION_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
}

void PaymentRequestBrowserTestBase::OpenContactInfoScreen() {
  ResetEventWaiter(DialogEvent::CONTACT_INFO_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenCreditCardEditorScreen() {
  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingAddressEditorScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_SHIPPING_BUTTON));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenContactInfoEditorScreen() {
  ResetEventWaiter(DialogEvent::CONTACT_INFO_EDITOR_OPENED);

  views::View* view = delegate_->dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ADD_CONTACT_BUTTON));
  if (!view) {
    view = delegate_->dialog_view()->GetViewByID(static_cast<int>(
        DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON));
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::ClickOnBackArrow() {
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);

  ClickOnDialogViewAndWait(DialogViewID::BACK_BUTTON);
}

void PaymentRequestBrowserTestBase::ClickOnCancel() {
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON, false);
}

content::WebContents* PaymentRequestBrowserTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

const std::vector<PaymentRequest*>
PaymentRequestBrowserTestBase::GetPaymentRequests(
    content::WebContents* web_contents) {
  PaymentRequestWebContentsManager* manager =
      PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents);
  if (!manager)
    return std::vector<PaymentRequest*>();

  std::vector<PaymentRequest*> payment_requests_ptrs;
  for (const auto& p : manager->payment_requests_)
    payment_requests_ptrs.push_back(p.first);
  return payment_requests_ptrs;
}

autofill::PersonalDataManager* PaymentRequestBrowserTestBase::GetDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
}

void PaymentRequestBrowserTestBase::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  size_t profile_count = personal_data_manager->GetProfiles().size();

  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&data_loop));
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .Times(testing::AnyNumber());
  personal_data_manager->AddProfile(profile);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(profile_count + 1, personal_data_manager->GetProfiles().size());
}

void PaymentRequestBrowserTestBase::AddCreditCard(
    const autofill::CreditCard& card) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  if (card.record_type() != autofill::CreditCard::LOCAL_CARD) {
    personal_data_manager->AddServerCreditCardForTest(
        std::make_unique<autofill::CreditCard>(card));
    return;
  }
  size_t card_count = personal_data_manager->GetCreditCards().size();

  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&data_loop));
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .Times(testing::AnyNumber());

  personal_data_manager->AddCreditCard(card);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer);
  EXPECT_EQ(card_count + 1, personal_data_manager->GetCreditCards().size());
}

void PaymentRequestBrowserTestBase::WaitForOnPersonalDataChanged() {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .Times(testing::AnyNumber());
  run_loop.Run();
}

void PaymentRequestBrowserTestBase::CreatePaymentRequestForTest(
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);
  std::unique_ptr<TestChromePaymentRequestDelegate> delegate =
      std::make_unique<TestChromePaymentRequestDelegate>(
          web_contents, this /* observer */, &prefs_, is_incognito_,
          is_valid_ssl_, is_browser_window_active_, skip_ui_for_basic_card_);
  delegate_ = delegate.get();
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentRequest(web_contents->GetMainFrame(), web_contents,
                             std::move(delegate), std::move(receiver), this);
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    DialogViewID view_id,
    bool wait_for_animation) {
  ClickOnDialogViewAndWait(view_id, delegate_->dialog_view(),
                           wait_for_animation);
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    DialogViewID view_id,
    PaymentRequestDialogView* dialog_view,
    bool wait_for_animation) {
  views::View* view = dialog_view->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  ClickOnDialogViewAndWait(view, dialog_view, wait_for_animation);
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    views::View* view,
    bool wait_for_animation) {
  ClickOnDialogViewAndWait(view, delegate_->dialog_view(), wait_for_animation);
}

void PaymentRequestBrowserTestBase::ClickOnDialogViewAndWait(
    views::View* view,
    PaymentRequestDialogView* dialog_view,
    bool wait_for_animation) {
  DCHECK(view);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(pressed);
  ui::MouseEvent released_event = ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(released_event);

  if (wait_for_animation)
    WaitForAnimation(dialog_view);

  WaitForObservedEvent();
}

void PaymentRequestBrowserTestBase::ClickOnChildInListViewAndWait(
    size_t child_index,
    size_t total_num_children,
    DialogViewID list_view_id,
    bool wait_for_animation) {
  views::View* list_view =
      dialog_view()->GetViewByID(static_cast<int>(list_view_id));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(total_num_children, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[child_index],
                           wait_for_animation);
}

std::vector<base::string16>
PaymentRequestBrowserTestBase::GetProfileLabelValues(
    DialogViewID parent_view_id) {
  std::vector<base::string16> line_labels;
  views::View* parent_view =
      dialog_view()->GetViewByID(static_cast<int>(parent_view_id));
  EXPECT_TRUE(parent_view);

  views::View* view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_1));
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_2));
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_3));
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());

  return line_labels;
}

std::vector<base::string16>
PaymentRequestBrowserTestBase::GetShippingOptionLabelValues(
    DialogViewID parent_view_id) {
  std::vector<base::string16> labels;
  views::View* parent_view =
      dialog_view()->GetViewByID(static_cast<int>(parent_view_id));
  EXPECT_TRUE(parent_view);

  views::View* view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_OPTION_DESCRIPTION));
  DCHECK(view);
  labels.push_back(static_cast<views::Label*>(view)->GetText());
  view = parent_view->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_OPTION_AMOUNT));
  DCHECK(view);
  labels.push_back(static_cast<views::Label*>(view)->GetText());
  return labels;
}

void PaymentRequestBrowserTestBase::OpenCVCPromptWithCVC(
    const base::string16& cvc) {
  OpenCVCPromptWithCVC(cvc, delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::OpenCVCPromptWithCVC(
    const base::string16& cvc,
    PaymentRequestDialogView* dialog_view) {
  ResetEventWaiter(DialogEvent::CVC_PROMPT_SHOWN);
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view);

  views::Textfield* cvc_field =
      static_cast<views::Textfield*>(dialog_view->GetViewByID(
          static_cast<int>(DialogViewID::CVC_PROMPT_TEXT_FIELD)));
  cvc_field->InsertOrReplaceText(cvc);
}

void PaymentRequestBrowserTestBase::PayWithCreditCardAndWait(
    const base::string16& cvc) {
  PayWithCreditCardAndWait(cvc, delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::PayWithCreditCardAndWait(
    const base::string16& cvc,
    PaymentRequestDialogView* dialog_view) {
  OpenCVCPromptWithCVC(cvc, dialog_view);

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON,
                           dialog_view);
}

void PaymentRequestBrowserTestBase::PayWithCreditCard(
    const base::string16& cvc) {
  OpenCVCPromptWithCVC(cvc, delegate_->dialog_view());

  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_SHOWN);
  ClickOnDialogViewAndWait(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON,
                           delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::RetryPaymentRequest(
    const std::string& validation_errors,
    PaymentRequestDialogView* dialog_view) {
  EXPECT_EQ(2U, dialog_view->view_stack_for_testing()->size());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION});

  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "retry(" + validation_errors + ");"));

  WaitForObservedEvent();
}

void PaymentRequestBrowserTestBase::RetryPaymentRequest(
    const std::string& validation_errors,
    const DialogEvent& dialog_event,
    PaymentRequestDialogView* dialog_view) {
  EXPECT_EQ(2U, dialog_view->view_stack_for_testing()->size());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::SPEC_DONE_UPDATING,
       DialogEvent::PROCESSING_SPINNER_HIDDEN,
       DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION, dialog_event});

  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "retry(" + validation_errors + ");"));

  WaitForObservedEvent();
}

base::string16 PaymentRequestBrowserTestBase::GetEditorTextfieldValue(
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  return textfield->GetText();
}

void PaymentRequestBrowserTestBase::SetEditorTextfieldValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  textfield->SetText(base::string16());
  textfield->InsertText(value);
  textfield->OnBlur();
}

base::string16 PaymentRequestBrowserTestBase::GetComboboxValue(
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  return combobox->model()->GetItemAt(combobox->GetSelectedIndex());
}

void PaymentRequestBrowserTestBase::SetComboboxValue(
    const base::string16& value,
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  SelectComboboxRowForValue(combobox, value);
  combobox->OnBlur();
}

void PaymentRequestBrowserTestBase::SelectBillingAddress(
    const std::string& billing_address_id) {
  views::Combobox* address_combobox(
      static_cast<views::Combobox*>(dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(kBillingAddressType))));
  ASSERT_NE(address_combobox, nullptr);
  autofill::AddressComboboxModel* address_combobox_model(
      static_cast<autofill::AddressComboboxModel*>(address_combobox->model()));
  address_combobox->SetSelectedRow(
      address_combobox_model->GetIndexOfIdentifier(billing_address_id));
  address_combobox->OnBlur();
}

bool PaymentRequestBrowserTestBase::IsEditorTextfieldInvalid(
    autofill::ServerFieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  return textfield->GetInvalid();
}

bool PaymentRequestBrowserTestBase::IsEditorComboboxInvalid(
    autofill::ServerFieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  return combobox->GetInvalid();
}

bool PaymentRequestBrowserTestBase::IsPayButtonEnabled() {
  views::Button* button =
      static_cast<views::Button*>(delegate_->dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::PAY_BUTTON)));
  DCHECK(button);
  return button->GetEnabled();
}

void PaymentRequestBrowserTestBase::WaitForAnimation() {
  WaitForAnimation(delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::WaitForAnimation(
    PaymentRequestDialogView* dialog_view) {
  ViewStack* view_stack = dialog_view->view_stack_for_testing();
  if (view_stack->slide_in_animator_->IsAnimating()) {
    view_stack->slide_in_animator_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
    view_stack->slide_in_animator_->SetAnimationDelegate(
        view_stack->top(), std::unique_ptr<gfx::AnimationDelegate>(
                               new gfx::TestAnimationDelegate()));
    base::RunLoop().Run();
  } else if (view_stack->slide_out_animator_->IsAnimating()) {
    view_stack->slide_out_animator_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
    view_stack->slide_out_animator_->SetAnimationDelegate(
        view_stack->top(), std::unique_ptr<gfx::AnimationDelegate>(
                               new gfx::TestAnimationDelegate()));
    base::RunLoop().Run();
  }
}

const base::string16& PaymentRequestBrowserTestBase::GetLabelText(
    DialogViewID view_id) {
  views::View* view = dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  return static_cast<views::Label*>(view)->GetText();
}

const base::string16& PaymentRequestBrowserTestBase::GetStyledLabelText(
    DialogViewID view_id) {
  views::View* view = dialog_view()->GetViewByID(static_cast<int>(view_id));
  DCHECK(view);
  return static_cast<views::StyledLabel*>(view)->GetText();
}

const base::string16& PaymentRequestBrowserTestBase::GetErrorLabelForType(
    autofill::ServerFieldType type) {
  views::View* view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) + type);
  DCHECK(view);
  return static_cast<views::Label*>(view)->GetText();
}

void PaymentRequestBrowserTestBase::SetCanMakePaymentEnabledPref(
    bool can_make_payment_enabled) {
  prefs_.SetBoolean(kCanMakePaymentEnabled, can_make_payment_enabled);
}

void PaymentRequestBrowserTestBase::ResetEventWaiter(DialogEvent event) {
  event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
      std::list<DialogEvent>{event});
}

void PaymentRequestBrowserTestBase::ResetEventWaiterForSequence(
    std::list<DialogEvent> event_sequence) {
  event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
      std::move(event_sequence));
}

void PaymentRequestBrowserTestBase::ResetEventWaiterForDialogOpened() {
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
}

void PaymentRequestBrowserTestBase::WaitForObservedEvent() {
  event_waiter_->Wait();
}

}  // namespace payments

std::ostream& operator<<(
    std::ostream& out,
    payments::PaymentRequestBrowserTestBase::DialogEvent event) {
  using DialogEvent = payments::PaymentRequestBrowserTestBase::DialogEvent;
  switch (event) {
    case DialogEvent::DIALOG_OPENED:
      out << "DIALOG_OPENED";
      break;
    case DialogEvent::DIALOG_CLOSED:
      out << "DIALOG_CLOSED";
      break;
    case DialogEvent::ORDER_SUMMARY_OPENED:
      out << "ORDER_SUMMARY_OPENED";
      break;
    case DialogEvent::PAYMENT_METHOD_OPENED:
      out << "PAYMENT_METHOD_OPENED";
      break;
    case DialogEvent::SHIPPING_ADDRESS_SECTION_OPENED:
      out << "SHIPPING_ADDRESS_SECTION_OPENED";
      break;
    case DialogEvent::SHIPPING_OPTION_SECTION_OPENED:
      out << "SHIPPING_OPTION_SECTION_OPENED";
      break;
    case DialogEvent::CREDIT_CARD_EDITOR_OPENED:
      out << "CREDIT_CARD_EDITOR_OPENED";
      break;
    case DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED:
      out << "SHIPPING_ADDRESS_EDITOR_OPENED";
      break;
    case DialogEvent::CONTACT_INFO_EDITOR_OPENED:
      out << "CONTACT_INFO_EDITOR_OPENED";
      break;
    case DialogEvent::BACK_NAVIGATION:
      out << "BACK_NAVIGATION";
      break;
    case DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION:
      out << "BACK_TO_PAYMENT_SHEET_NAVIGATION";
      break;
    case DialogEvent::CONTACT_INFO_OPENED:
      out << "CONTACT_INFO_OPENED";
      break;
    case DialogEvent::EDITOR_VIEW_UPDATED:
      out << "EDITOR_VIEW_UPDATED";
      break;
    case DialogEvent::CAN_MAKE_PAYMENT_CALLED:
      out << "CAN_MAKE_PAYMENT_CALLED";
      break;
    case DialogEvent::CAN_MAKE_PAYMENT_RETURNED:
      out << "CAN_MAKE_PAYMENT_RETURNED";
      break;
    case DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED:
      out << "HAS_ENROLLED_INSTRUMENT_CALLED";
      break;
    case DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED:
      out << "HAS_ENROLLED_INSTRUMENT_RETURNED";
      break;
    case DialogEvent::ERROR_MESSAGE_SHOWN:
      out << "ERROR_MESSAGE_SHOWN";
      break;
    case DialogEvent::SPEC_DONE_UPDATING:
      out << "SPEC_DONE_UPDATING";
      break;
    case DialogEvent::CVC_PROMPT_SHOWN:
      out << "CVC_PROMPT_SHOWN";
      break;
    case DialogEvent::NOT_SUPPORTED_ERROR:
      out << "NOT_SUPPORTED_ERROR";
      break;
    case DialogEvent::ABORT_CALLED:
      out << "ABORT_CALLED";
      break;
    case DialogEvent::PROCESSING_SPINNER_SHOWN:
      out << "PROCESSING_SPINNER_SHOWN";
      break;
    case DialogEvent::PROCESSING_SPINNER_HIDDEN:
      out << "PROCESSING_SPINNER_HIDDEN";
      break;
  }
  return out;
}

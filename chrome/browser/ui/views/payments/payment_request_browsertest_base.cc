// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "chrome/test/payments/payment_app_install_util.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/ui/address_combobox_model.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/views_switches.h"

namespace payments {

namespace {
// This is preferred to SelectValue, since only SetSelectedRow fires the events
// as if done by a user.
void SelectComboboxRowForValue(views::Combobox* combobox,
                               const std::u16string& text) {
  size_t i;
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

base::WeakPtr<PaymentRequestBrowserTestBase>
PaymentRequestBrowserTestBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentRequestBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // HTTPS server only serves a valid cert for localhost, so this is needed to
  // load pages from "a.com" without an interstitial.
  command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);

  // Clicks from tests should always be allowed, even on dialogs that have
  // protection against accidental double-clicking/etc.
  command_line->AppendSwitch(
      views::switches::kDisableInputEventActivationProtectionForTesting);
}

void PaymentRequestBrowserTestBase::SetUpOnMainThread() {
  // Setup the https server.
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  host_resolver()->AddRule("*", "127.0.0.1");
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
  NavigateTo("a.com", file_path);
}

void PaymentRequestBrowserTestBase::NavigateTo(const std::string& hostname,
                                               const std::string& file_path) {
  if (file_path.find("data:") == 0U) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(file_path)));
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL(hostname, file_path)));
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

void PaymentRequestBrowserTestBase::OnConnectionTerminated() {}

void PaymentRequestBrowserTestBase::OnPayCalled() {}

void PaymentRequestBrowserTestBase::OnAbortCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::ABORT_CALLED);
}

void PaymentRequestBrowserTestBase::OnDialogOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::DIALOG_OPENED);
}

void PaymentRequestBrowserTestBase::OnDialogClosed() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::DIALOG_CLOSED);
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

void PaymentRequestBrowserTestBase::OnProcessingSpinnerShown() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PROCESSING_SPINNER_SHOWN);
}

void PaymentRequestBrowserTestBase::OnProcessingSpinnerHidden() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PROCESSING_SPINNER_HIDDEN);
}

void PaymentRequestBrowserTestBase::OnPaymentHandlerWindowOpened() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED);
}

void PaymentRequestBrowserTestBase::OnPaymentHandlerTitleSet() {
  if (event_waiter_)
    event_waiter_->OnEvent(DialogEvent::PAYMENT_HANDLER_TITLE_SET);
}

// Install the payment app specified by `hostname`, e.g., "a.com". Specify the
// filename of the service worker with `service_worker_filename`. Note that
// the origin has to be initialized first to be supported here. The payment
// method of the installed payment app will be outputted in
// `url_method_output`, e.g., "https://a.com:12345".
void PaymentRequestBrowserTestBase::InstallPaymentApp(
    const std::string& hostname,
    const std::string& service_worker_filename,
    std::string* url_method_output) {
  *url_method_output = PaymentAppInstallUtil::InstallPaymentApp(
      *GetActiveWebContents(), *https_server(), hostname,
      service_worker_filename, PaymentAppInstallUtil::IconInstall::kWithIcon);
  ASSERT_FALSE(url_method_output->empty()) << "Failed to install payment app";
}

void PaymentRequestBrowserTestBase::InstallPaymentAppWithoutIcon(
    const std::string& hostname,
    const std::string& service_worker_filename,
    std::string* url_method_output) {
  *url_method_output = PaymentAppInstallUtil::InstallPaymentApp(
      *GetActiveWebContents(), *https_server(), hostname,
      service_worker_filename,
      PaymentAppInstallUtil::IconInstall::kWithoutIcon);
  ASSERT_FALSE(url_method_output->empty()) << "Failed to install payment app";
}

void PaymentRequestBrowserTestBase::InvokePaymentRequestUI() {
  InvokePaymentRequestUIWithJs(
      "(function() { document.getElementById('buy').click(); })();");
}

void PaymentRequestBrowserTestBase::InvokePaymentRequestUIWithJs(
    const std::string& click_buy_button_js) {
  ResetEventWaiterForDialogOpened();

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(web_contents, click_buy_button_js,
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  ASSERT_TRUE(WaitForObservedEvent());

  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
}

void PaymentRequestBrowserTestBase::ExpectBodyContains(
    const std::vector<std::string>& expected_strings) {
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string extract_contents_js = "window.document.body.textContent;";
  std::string contents =
      content::EvalJs(web_contents, extract_contents_js).ExtractString();
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

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingAddressSectionScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_SECTION_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingOptionSectionScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_OPTION_SECTION_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);

  ClickOnDialogViewAndWait(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
}

void PaymentRequestBrowserTestBase::OpenContactInfoScreen() {
  ResetEventWaiter(DialogEvent::CONTACT_INFO_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenCreditCardEditorScreen() {
  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_METHOD_ADD_CARD_BUTTON);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenShippingAddressEditorScreen() {
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_METHOD_ADD_SHIPPING_BUTTON);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON);
  }

  EXPECT_TRUE(view);
  ClickOnDialogViewAndWait(view);
}

void PaymentRequestBrowserTestBase::OpenContactInfoEditorScreen() {
  ResetEventWaiter(DialogEvent::CONTACT_INFO_EDITOR_OPENED);

  views::View* view =
      GetByDialogViewID(DialogViewID::PAYMENT_METHOD_ADD_CONTACT_BUTTON);
  if (!view) {
    view = GetByDialogViewID(
        DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON);
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
PaymentRequestBrowserTestBase::GetPaymentRequests() {
  std::vector<PaymentRequest*> ptrs;
  ptrs.reserve(requests_.size());
  for (const auto& weak : requests_) {
    if (weak)
      ptrs.push_back(&*weak);
  }
  return ptrs;
}

autofill::PersonalDataManager* PaymentRequestBrowserTestBase::GetDataManager() {
  return autofill::PersonalDataManagerFactory::GetForBrowserContext(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
}

void PaymentRequestBrowserTestBase::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  size_t profile_count =
      personal_data_manager->address_data_manager().GetProfiles().size();
  autofill::PersonalDataChangedWaiter waiter(*personal_data_manager);
  personal_data_manager->address_data_manager().AddProfile(profile);
  std::move(waiter).Wait();
  EXPECT_EQ(profile_count + 1,
            personal_data_manager->address_data_manager().GetProfiles().size());
}

void PaymentRequestBrowserTestBase::AddCreditCard(
    const autofill::CreditCard& card) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  size_t card_count =
      personal_data_manager->payments_data_manager().GetCreditCards().size();
  autofill::PersonalDataChangedWaiter waiter(*personal_data_manager);
  if (card.record_type() == autofill::CreditCard::RecordType::kLocalCard) {
    personal_data_manager->payments_data_manager().AddCreditCard(card);
  } else {
    test_api(personal_data_manager->payments_data_manager())
        .AddServerCreditCard(card);
  }
  std::move(waiter).Wait();
  EXPECT_EQ(
      card_count + 1,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
}

void PaymentRequestBrowserTestBase::WaitForOnPersonalDataChanged() {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  autofill::PersonalDataChangedWaiter(*personal_data_manager).Wait();
}

void PaymentRequestBrowserTestBase::CreatePaymentRequestForTest(
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsActive());

  auto delegate =
      std::make_unique<TestChromePaymentRequestDelegate>(render_frame_host);
  delegate->set_payment_request_dialog_view_observer_for_test(GetWeakPtr());
  delegate->OverridePrefService(&prefs_);
  delegate->OverrideOffTheRecord(is_incognito_);
  delegate->OverrideValidSSL(is_valid_ssl_);
  delegate->OverrideBrowserWindowActive(is_browser_window_active_);
  delegate_ = delegate.get();

  auto* request = new PaymentRequest(std::move(delegate), std::move(receiver));
  request->set_observer_for_test(GetWeakPtr());
  requests_.push_back(request->GetWeakPtr());
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
  views::View* view = GetChildByDialogViewID(dialog_view, view_id);
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
  ClickOnDialogView(view);
  if (wait_for_animation) {
    WaitForAnimation(dialog_view);
  }
  ASSERT_TRUE(WaitForObservedEvent());
}

void PaymentRequestBrowserTestBase::ClickOnDialogView(views::View* view) {
  DCHECK(view);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(pressed);
  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(released_event);
}

void PaymentRequestBrowserTestBase::ClickOnChildInListViewAndWait(
    size_t child_index,
    size_t total_num_children,
    DialogViewID list_view_id,
    bool wait_for_animation) {
  views::View* list_view = GetByDialogViewID(list_view_id);
  EXPECT_TRUE(list_view);
  EXPECT_EQ(total_num_children, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[child_index],
                           wait_for_animation);
}

std::vector<std::u16string>
PaymentRequestBrowserTestBase::GetProfileLabelValues(
    DialogViewID parent_view_id) {
  std::vector<std::u16string> line_labels;
  views::View* parent_view = GetByDialogViewID(parent_view_id);
  EXPECT_TRUE(parent_view);

  views::View* view =
      GetChildByDialogViewID(parent_view, DialogViewID::PROFILE_LABEL_LINE_1);
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view =
      GetChildByDialogViewID(parent_view, DialogViewID::PROFILE_LABEL_LINE_2);
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view =
      GetChildByDialogViewID(parent_view, DialogViewID::PROFILE_LABEL_LINE_3);
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());
  view = GetChildByDialogViewID(parent_view, DialogViewID::PROFILE_LABEL_ERROR);
  if (view)
    line_labels.push_back(static_cast<views::Label*>(view)->GetText());

  return line_labels;
}

std::vector<std::u16string>
PaymentRequestBrowserTestBase::GetShippingOptionLabelValues(
    DialogViewID parent_view_id) {
  std::vector<std::u16string> labels;
  views::View* parent_view = GetByDialogViewID(parent_view_id);
  EXPECT_TRUE(parent_view);

  views::View* view = GetChildByDialogViewID(
      parent_view, DialogViewID::SHIPPING_OPTION_DESCRIPTION);
  DCHECK(view);
  labels.push_back(static_cast<views::Label*>(view)->GetText());
  view =
      GetChildByDialogViewID(parent_view, DialogViewID::SHIPPING_OPTION_AMOUNT);
  DCHECK(view);
  labels.push_back(static_cast<views::Label*>(view)->GetText());
  return labels;
}

void PaymentRequestBrowserTestBase::OpenCVCPromptWithCVC(
    const std::u16string& cvc,
    PaymentRequestDialogView* dialog_view) {
  ResetEventWaiter(DialogEvent::CVC_PROMPT_SHOWN);
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view);

  views::Textfield* cvc_field = static_cast<views::Textfield*>(
      GetChildByDialogViewID(dialog_view, DialogViewID::CVC_PROMPT_TEXT_FIELD));
  cvc_field->InsertOrReplaceText(cvc);
}

void PaymentRequestBrowserTestBase::PayWithCreditCard(
    const std::u16string& cvc) {
  OpenCVCPromptWithCVC(cvc, delegate_->dialog_view());

  ResetEventWaiter(DialogEvent::PROCESSING_SPINNER_SHOWN);
  ClickOnDialogViewAndWait(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON,
                           delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::RetryPaymentRequest(
    const std::string& validation_errors,
    PaymentRequestDialogView* dialog_view) {
  EXPECT_EQ(2U, dialog_view->view_stack_for_testing()->GetSize());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION});

  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry(" + validation_errors + ");"));

  ASSERT_TRUE(WaitForObservedEvent());
}

void PaymentRequestBrowserTestBase::RetryPaymentRequest(
    const std::string& validation_errors,
    const DialogEvent& dialog_event,
    PaymentRequestDialogView* dialog_view) {
  EXPECT_EQ(2U, dialog_view->view_stack_for_testing()->GetSize());
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::SPEC_DONE_UPDATING,
       DialogEvent::PROCESSING_SPINNER_HIDDEN,
       DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION, dialog_event});

  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry(" + validation_errors + ");"));

  ASSERT_TRUE(WaitForObservedEvent());
}

bool PaymentRequestBrowserTestBase::IsViewVisible(DialogViewID view_id) const {
  return IsViewVisible(view_id, dialog_view());
}

bool PaymentRequestBrowserTestBase::IsViewVisible(
    DialogViewID view_id,
    views::View* dialog_view) const {
  views::View* view = GetChildByDialogViewID(dialog_view, view_id);
  return view && view->GetVisible();
}

std::u16string PaymentRequestBrowserTestBase::GetEditorTextfieldValue(
    autofill::FieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  return textfield->GetText();
}

void PaymentRequestBrowserTestBase::SetEditorTextfieldValue(
    const std::u16string& value,
    autofill::FieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  textfield->SetText(std::u16string());
  textfield->InsertText(
      value,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  textfield->OnBlur();
}

std::u16string PaymentRequestBrowserTestBase::GetComboboxValue(
    autofill::FieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  return combobox->GetModel()->GetItemAt(combobox->GetSelectedIndex().value());
}

void PaymentRequestBrowserTestBase::SetComboboxValue(
    const std::u16string& value,
    autofill::FieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  SelectComboboxRowForValue(combobox, value);
  combobox->OnBlur();
}

void PaymentRequestBrowserTestBase::SelectBillingAddress(
    const std::string& billing_address_id) {
  views::Combobox* address_combobox(static_cast<views::Combobox*>(
      dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
          autofill::ADDRESS_HOME_LINE1))));
  ASSERT_NE(address_combobox, nullptr);
  autofill::AddressComboboxModel* address_combobox_model(
      static_cast<autofill::AddressComboboxModel*>(
          address_combobox->GetModel()));
  address_combobox->SetSelectedRow(
      address_combobox_model->GetIndexOfIdentifier(billing_address_id));
  address_combobox->OnBlur();
}

bool PaymentRequestBrowserTestBase::IsEditorTextfieldInvalid(
    autofill::FieldType type) {
  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(textfield);
  return textfield->GetInvalid();
}

bool PaymentRequestBrowserTestBase::IsEditorComboboxInvalid(
    autofill::FieldType type) {
  ValidatingCombobox* combobox =
      static_cast<ValidatingCombobox*>(delegate_->dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(type)));
  DCHECK(combobox);
  return combobox->GetInvalid();
}

bool PaymentRequestBrowserTestBase::IsPayButtonEnabled() {
  views::Button* button =
      static_cast<views::Button*>(GetByDialogViewID(DialogViewID::PAY_BUTTON));
  DCHECK(button);
  return button->GetEnabled();
}

std::u16string PaymentRequestBrowserTestBase::GetPrimaryButtonLabel() const {
  return static_cast<views::MdTextButton*>(
             GetByDialogViewID(DialogViewID::PAY_BUTTON))
      ->GetText();
}

void PaymentRequestBrowserTestBase::WaitForAnimation() {
  WaitForAnimation(delegate_->dialog_view());
}

void PaymentRequestBrowserTestBase::WaitForAnimation(
    PaymentRequestDialogView* dialog_view) {
  base::RunLoop loop;
  ViewStack* view_stack = dialog_view->view_stack_for_testing();
  if (view_stack->slide_in_animator_->IsAnimating()) {
    view_stack->slide_in_animator_->SetAnimationDuration(base::Milliseconds(1));
    view_stack->slide_in_animator_->SetAnimationDelegate(
        view_stack->top(),
        std::unique_ptr<gfx::AnimationDelegate>(
            new gfx::TestAnimationDelegate(loop.QuitWhenIdleClosure())));
    loop.Run();
  } else if (view_stack->slide_out_animator_->IsAnimating()) {
    view_stack->slide_out_animator_->SetAnimationDuration(
        base::Milliseconds(1));
    view_stack->slide_out_animator_->SetAnimationDelegate(
        view_stack->top(),
        std::unique_ptr<gfx::AnimationDelegate>(
            new gfx::TestAnimationDelegate(loop.QuitWhenIdleClosure())));
    loop.Run();
  }
}

views::View* PaymentRequestBrowserTestBase::GetByDialogViewID(
    DialogViewID id) const {
  return GetChildByDialogViewID(dialog_view(), id);
}

views::View* PaymentRequestBrowserTestBase::GetChildByDialogViewID(
    views::View* parent,
    DialogViewID id) const {
  return parent->GetViewByID(static_cast<int>(id));
}

const std::u16string& PaymentRequestBrowserTestBase::GetLabelText(
    DialogViewID view_id) {
  return GetLabelText(view_id, dialog_view());
}

const std::u16string& PaymentRequestBrowserTestBase::GetLabelText(
    DialogViewID view_id,
    views::View* dialog_view) {
  views::View* view = GetChildByDialogViewID(dialog_view, view_id);
  DCHECK(view);
  return static_cast<views::Label*>(view)->GetText();
}

const std::u16string& PaymentRequestBrowserTestBase::GetStyledLabelText(
    DialogViewID view_id) {
  views::View* view = GetByDialogViewID(view_id);
  DCHECK(view);
  return static_cast<views::StyledLabel*>(view)->GetText();
}

const std::u16string& PaymentRequestBrowserTestBase::GetErrorLabelForType(
    autofill::FieldType type) {
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

testing::AssertionResult PaymentRequestBrowserTestBase::WaitForObservedEvent() {
  return event_waiter_->Wait();
}

base::WeakPtr<CSPChecker>
PaymentRequestBrowserTestBase::GetCSPCheckerForTests() {
  return const_csp_checker_.GetWeakPtr();
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
    case DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED:
      out << "PAYMENT_HANDLER_WINDOW_OPENED";
      break;
    case DialogEvent::PAYMENT_HANDLER_TITLE_SET:
      out << "PAYMENT_HANDLER_TITLE_SET";
      break;
  }
  return out;
}

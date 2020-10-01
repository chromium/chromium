// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/credit_card_save_strike_database.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using base::Bucket;
using testing::ElementsAre;

namespace {
const char kCreditCardAndAddressUploadForm[] =
    "/credit_card_upload_form_address_and_cc.html";
const char kCreditCardUploadForm[] = "/credit_card_upload_form_cc.html";
const char kCreditCardAndShippingUploadForm[] =
    "/credit_card_upload_form_shipping_address.html";
const char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
const char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
const char kURLUploadCardRequest[] =
    "https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    "savecard"
    "?s7e_suffix=chromewallet";
const char kResponsePaymentsSuccess[] =
    "{ \"credit_card_id\": \"InstrumentData:1\" }";
const char kResponsePaymentsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

const double kFakeGeolocationLatitude = 1.23;
const double kFakeGeolocationLongitude = 4.56;
}  // namespace

namespace autofill {

class SaveCardBubbleViewsFullFormBrowserTest
    : public SyncTest,
      public AutofillHandler::ObserverForTest,
      public CreditCardSaveManager::ObserverForTest,
      public SaveCardBubbleControllerImpl::ObserverForTest {
 protected:
  SaveCardBubbleViewsFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {}

 public:
  SaveCardBubbleViewsFullFormBrowserTest(
      const SaveCardBubbleViewsFullFormBrowserTest&) = delete;
  SaveCardBubbleViewsFullFormBrowserTest& operator=(
      const SaveCardBubbleViewsFullFormBrowserTest&) = delete;

 protected:
  ~SaveCardBubbleViewsFullFormBrowserTest() override = default;

  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    REQUESTED_UPLOAD_SAVE,
    DYNAMIC_FORM_PARSED,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_UPLOAD_CARD_REQUEST,
    RECEIVED_UPLOAD_CARD_RESPONSE,
    SHOW_CARD_SAVED_FEEDBACK,
    STRIKE_CHANGE_COMPLETE,
    BUBBLE_SHOWN,
    BUBBLE_CLOSED
  };

  // SyncTest::SetUpOnMainThread:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile())
        ->OverrideNetworkForTest(
            fake_server::CreateFakeServerHttpPostProviderFactory(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In ChromeOS browser tests, the profile may already by authenticated with
    // stub account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(browser()->profile())
            ->GetPrimaryAccountInfo();
    username = info.email;

    // Install the Settings App.
    web_app::WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
#endif
    if (username.empty())
      username = "user@gmail.com";

    harness_ = ProfileSyncServiceHarness::Create(
        browser()->profile(), username, "password",
        ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

    // Set up the URL loader factory for the payments client so we can intercept
    // those network requests too.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    ContentAutofillDriver::GetForRenderFrameHost(
        GetActiveWebContents()->GetMainFrame())
        ->autofill_manager()
        ->client()
        ->GetPaymentsClient()
        ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

    // Set up this class as the ObserverForTest implementation.
    credit_card_save_manager_ = ContentAutofillDriver::GetForRenderFrameHost(
                                    GetActiveWebContents()->GetMainFrame())
                                    ->autofill_manager()
                                    ->client()
                                    ->GetFormDataImporter()
                                    ->credit_card_save_manager_.get();
    credit_card_save_manager_->SetEventObserverForTesting(this);

    // Set up this class as the ObserverForTest implementation.
    AutofillHandler* autofill_handler =
        ContentAutofillDriver::GetForRenderFrameHost(
            GetActiveWebContents()->GetMainFrame())
            ->autofill_handler();
    autofill_handler->SetEventObserverForTesting(this);

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);
  }

  // AutofillHandler::ObserverForTest:
  void OnFormParsed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DYNAMIC_FORM_PARSED);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnDecideToRequestUploadSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::REQUESTED_UPLOAD_SAVE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnSentUploadCardRequest() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SENT_UPLOAD_CARD_REQUEST);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnReceivedUploadCardResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_UPLOAD_CARD_RESPONSE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnShowCardSavedFeedback() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SHOW_CARD_SAVED_FEEDBACK);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnStrikeChangeComplete() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::STRIKE_CHANGE_COMPLETE);
  }

  // SaveCardBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
  }

  // SaveCardBubbleControllerImpl::ObserverForTest:
  void OnBubbleClosed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::BUBBLE_CLOSED);
  }

  inline views::Combobox* month_input() {
    return static_cast<views::Combobox*>(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH));
  }

  inline views::Combobox* year_input() {
    return static_cast<views::Combobox*>(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR));
  }

  void VerifyExpirationDateDropdownsAreVisible() {
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->GetVisible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->GetVisible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_VIEW)->GetVisible());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR)
                    ->GetVisible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH)
            ->GetVisible());
    EXPECT_FALSE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_LABEL));
  }

  void SetUpForEditableExpirationDate() {
    // Start sync.
    harness_->SetupSync();
  }

  void NavigateTo(const std::string& file_path) {
    if (file_path.find("data:") == 0U) {
      ui_test_utils::NavigateToURL(browser(), GURL(file_path));
    } else {
      ui_test_utils::NavigateToURL(browser(),
                                   embedded_test_server()->GetURL(file_path));
    }
  }

  void SetAccountFullName(const std::string& full_name) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    CoreAccountInfo core_info =
        PersonalDataManagerFactory::GetForProfile(browser()->profile())
            ->GetAccountInfoForPaymentsServer();

    AccountInfo account_info;
    account_info.account_id = core_info.account_id;
    account_info.gaia = core_info.gaia;
    account_info.email = core_info.email;
    account_info.is_under_advanced_protection =
        core_info.is_under_advanced_protection;
    account_info.full_name = full_name;
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void SubmitFormAndWaitForCardLocalSaveBubble() {
    ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
    SubmitForm();
    WaitForObservedEvent();
    WaitForAnimationToEnd();
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SubmitFormAndWaitForCardUploadSaveBubble() {
    // Set up the Payments RPC.
    SetUploadDetailsRpcPaymentsAccepts();
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_UPLOAD_SAVE,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    SubmitForm();
    WaitForObservedEvent();
    WaitForAnimationToEnd();
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->GetVisible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->GetVisible());
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillForm() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));
  }

  // Should be called for credit_card_upload_form_cc.html.
  void FillAndChangeForm() {
    NavigateTo(kCreditCardUploadForm);
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    const std::string click_add_fields_button_js =
        "(function() { document.getElementById('add_fields').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), click_fill_button_js));
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       click_add_fields_button_js));
  }

  void FillFormWithCardDetailsOnly() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_card_button_js =
        "(function() { document.getElementById('fill_card_only').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_fill_card_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutCvc() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_cvc_button_js =
        "(function() { document.getElementById('clear_cvc').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_cvc_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithInvalidCvc() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_fill_invalid_cvc_button_js =
        "(function() { document.getElementById('fill_invalid_cvc').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_fill_invalid_cvc_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutName() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_name_button_js =
        "(function() { document.getElementById('clear_name').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_name_button_js));
  }

  // Should be called for credit_card_upload_form_shipping_address.html.
  void FillFormWithConflictingName() {
    NavigateTo(kCreditCardAndShippingUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_conflicting_name_button_js =
        "(function() { document.getElementById('conflicting_name').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_conflicting_name_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutExpirationDate() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithExpirationMonthOnly(const std::string& month) {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_month_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithExpirationYearOnly(const std::string& year) {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents,
                                       click_clear_expiration_date_button_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_year_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithSpecificExpirationDate(const std::string& month,
                                          const std::string& year) {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_month_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, set_year_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutAddress() {
    NavigateTo(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_clear_address_button_js =
        "(function() { document.getElementById('clear_address').click(); })();";
    ASSERT_TRUE(
        content::ExecuteScript(web_contents, click_clear_address_button_js));
  }

  // Should be called for credit_card_upload_form_shipping_address.html.
  void FillFormWithConflictingPostalCode() {
    NavigateTo(kCreditCardAndShippingUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    const std::string click_conflicting_postal_code_button_js =
        "(function() { "
        "document.getElementById('conflicting_postal_code').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(
        web_contents, click_conflicting_postal_code_button_js));
  }

  void SetUploadDetailsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsDeclines() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponsePaymentsFailure);
  }

  void SetUploadDetailsRpcServerError() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess,
                                           net::HTTP_INTERNAL_SERVER_ERROR);
  }

  void SetUploadCardRpcPaymentsSucceeds() {
    test_url_loader_factory()->AddResponse(kURLUploadCardRequest,
                                           kResponsePaymentsSuccess);
  }

  void SetUploadCardRpcPaymentsFails() {
    test_url_loader_factory()->AddResponse(kURLUploadCardRequest,
                                           kResponsePaymentsFailure);
  }

  void ClickOnView(views::View* view) {
    DCHECK(view);
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
    GetSaveCardBubbleViews()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetSaveCardBubbleViews()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
  }

  void ClickOnDialogViewAndWait(views::View* view) {
    EXPECT_TRUE(GetSaveCardBubbleViews());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetSaveCardBubbleViews()->GetWidget());
    ClickOnDialogView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetSaveCardBubbleViews());
  }

  void ClickOnDialogViewWithId(DialogViewId view_id) {
    ClickOnDialogView(FindViewInBubbleById(view_id));
  }

  void ClickOnDialogViewWithIdAndWait(DialogViewId view_id) {
    ClickOnDialogViewAndWait(FindViewInBubbleById(view_id));
  }

  views::View* FindViewInBubbleById(DialogViewId view_id) {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);

    views::View* specified_view =
        save_card_bubble_views->GetViewByID(static_cast<int>(view_id));

    if (!specified_view) {
      // Many of the save card bubble's inner Views are not child views but
      // rather contained by the dialog. If we didn't find what we were looking
      // for, check there as well.
      specified_view =
          save_card_bubble_views->GetWidget()->GetRootView()->GetViewByID(
              static_cast<int>(view_id));
    }
    if (!specified_view) {
      // Additionally, the save card bubble's footnote view is not part of its
      // main bubble, and contains elements such as the legal message links.
      // If we didn't find what we were looking for, check there as well.
      views::View* footnote_view =
          save_card_bubble_views->GetFootnoteViewForTesting();
      if (footnote_view) {
        specified_view = footnote_view->GetViewByID(static_cast<int>(view_id));
      }
    }

    return specified_view;
  }

  void ClickOnCancelButton(bool strike_expected = false) {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);
    if (strike_expected) {
      ResetEventWaiterForSequence(
          {DialogEvent::STRIKE_CHANGE_COMPLETE, DialogEvent::BUBBLE_CLOSED});
    } else {
      ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
    }
    ClickOnDialogViewWithIdAndWait(DialogViewId::CANCEL_BUTTON);
    DCHECK(!GetSaveCardBubbleViews());
  }

  void ClickOnCloseButton() {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    DCHECK(save_card_bubble_views);
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
    ClickOnDialogViewAndWait(save_card_bubble_views->GetBubbleFrameView()
                                 ->GetCloseButtonForTesting());
    DCHECK(!GetSaveCardBubbleViews());
  }

  SaveCardBubbleViews* GetSaveCardBubbleViews() {
    SaveCardBubbleController* save_card_bubble_controller =
        SaveCardBubbleController::GetOrCreate(GetActiveWebContents());
    if (!save_card_bubble_controller)
      return nullptr;
    SaveCardBubbleView* save_card_bubble_view =
        save_card_bubble_controller->GetSaveCardBubbleView();
    if (!save_card_bubble_view)
      return nullptr;
    return static_cast<SaveCardBubbleViews*>(save_card_bubble_view);
  }

  SavePaymentIconView* GetSaveCardIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kSaveCard);
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableToolbarStatusChip)) {
      DCHECK(
          browser_view->toolbar()->toolbar_account_icon_container()->Contains(
              icon));
    } else {
      DCHECK(browser_view->GetLocationBarView()->Contains(icon));
    }
    return static_cast<SavePaymentIconView*>(icon);
  }

  void OpenSettingsFromManageCardsPrompt() {
    FillForm();
    SubmitFormAndWaitForCardLocalSaveBubble();

    // Adding an event observer to the controller so we can wait for the bubble
    // to show.
    AddEventObserverToController();
    ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
    ResetEventWaiterForSequence(
        {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

    // Click [Save] should close the offer-to-save bubble and show "Card saved"
    // animation -- followed by the sign-in promo (if not on Chrome OS).
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
    // Wait for and then close the promo.
    WaitForObservedEvent();
    ClickOnCloseButton();
#endif

    // Open up Manage Cards prompt.
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    ClickOnView(GetSaveCardIconView());
    WaitForObservedEvent();

    // Click on the redirect button.
    ClickOnDialogViewWithId(DialogViewId::MANAGE_CARDS_BUTTON);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AddEventObserverToController() {
    SaveCardBubbleControllerImpl* save_card_bubble_controller_impl =
        SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
    DCHECK(save_card_bubble_controller_impl);
    save_card_bubble_controller_impl->SetEventObserverForTesting(this);
  }

  void ReduceAnimationTime() {
    GetSaveCardIconView()->ReduceAnimationTimeForTesting();
    auto* const animating_layout = GetAnimatingLayoutManager();
    if (animating_layout) {
      animating_layout->SetAnimationDuration(
          base::TimeDelta::FromMilliseconds(1));
    }
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

  void WaitForAnimationToEnd() {
    auto* const animating_layout = GetAnimatingLayoutManager();
    if (animating_layout)
      views::test::WaitForAnimatingLayoutManager(animating_layout);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  std::unique_ptr<ProfileSyncServiceHarness> harness_;

  CreditCardSaveManager* credit_card_save_manager_ = nullptr;

 private:
  views::AnimatingLayoutManager* GetAnimatingLayoutManager() {
    if (!base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableToolbarStatusChip)) {
      return nullptr;
    }

    return views::test::GetAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->toolbar_account_icon_container());
  }

  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

// TODO(crbug.com/932818): Remove this class after experiment flag is cleaned
// up. Otherwise we need it because the toolbar is init-ed before each test is
// set up. Thus need to enable the feature in the general browsertest SetUp().
class SaveCardBubbleViewsFullFormBrowserTestForStatusChip
    : public SaveCardBubbleViewsFullFormBrowserTest {
 protected:
  SaveCardBubbleViewsFullFormBrowserTestForStatusChip()
      : SaveCardBubbleViewsFullFormBrowserTest() {}
  ~SaveCardBubbleViewsFullFormBrowserTestForStatusChip() override {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillCreditCardUploadFeedback,
                              features::kAutofillEnableToolbarStatusChip,
                              features::kAutofillUpstream},
        /*disabled_features=*/{});

    SaveCardBubbleViewsFullFormBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

// Tests the sign in promo bubble. Ensures that clicking the [Save] button
// on the local save bubble successfully causes the sign in promo to show.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveShowsSigninPromo) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Sign-in promo should be showing and user actions should have recorded
  // impression.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::SIGN_IN_PROMO_VIEW)->GetVisible());
}
#endif

class SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream
    : public SaveCardBubbleViewsFullFormBrowserTest {
 public:
  SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream() {
    feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the sign in promo bubble. Ensures that the sign-in promo
// is not shown when the user is signed-in and syncing, even if the local save
// bubble is shown.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Local_NoSigninPromoShowsWhenUserIsSyncing) {
  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());

  // Click [Save] should close the offer-to-save bubble
  // but no sign-in promo should show because user is signed in.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing and no sign-in impression should have been
  // recorded.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}

// Tests the sign in promo bubble. Ensures that signin impression is recorded
// when promo is shown.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_SigninImpressionSigninPromo) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
}
#endif

// Tests the sign in promo bubble. Ensures that signin action is recorded when
// user accepts promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_Metrics_AcceptingSigninPromo) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Click on [Sign in] button.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::SIGN_IN_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromSaveCardBubble"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromSaveCardBubble"));
}

IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       DiceBubbleSyncPromoViewAlertAccessibleEvent) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}
#endif

class SaveCardBubbleViewsFullFormBrowserTestSettings
    : public SaveCardBubbleViewsFullFormBrowserTest {
 public:
  SaveCardBubbleViewsFullFormBrowserTestSettings() {
#if !defined(OS_CHROMEOS)
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillCreditCardUploadFeedback,
                               features::kAutofillEnableToolbarStatusChip});
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the manage cards bubble. Ensures that clicking the [Manage cards]
// button redirects properly.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestSettings,
                       Local_ManageCardsButtonRedirects) {
  base::HistogramTester histogram_tester;
  OpenSettingsFromManageCardsPrompt();

  // Another tab should have opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Metrics should have been recorded correctly.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, 1)));
}

// Tests the local save bubble. Ensures that the bubble behaves correctly if
// dismissed and then immediately torn down (e.g. by closing browser window)
// before the asynchronous close completes. Regression test for
// https://crbug.com/842577 .
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_SynchronousCloseAfterAsynchronousClose) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  SaveCardBubbleViews* bubble = GetSaveCardBubbleViews();
  EXPECT_TRUE(bubble);
  views::Widget* bubble_widget = bubble->GetWidget();
  EXPECT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());
  bubble->Hide();
  EXPECT_FALSE(bubble_widget->IsVisible());

  // The bubble is immediately hidden, but it can still receive events here.
  // Simulate an OS event arriving to destroy the Widget.
  bubble_widget->CloseNow();
  // |bubble| and |bubble_widget| now point to deleted objects.

  // Simulate closing the browser window.
  browser()->tab_strip_model()->CloseAllTabs();

  // Process the asynchronous close (which should do nothing).
  base::RunLoop().RunUntilIdle();
}

// Tests the upload save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away and sends an UploadCardRequest RPC
// to Google Payments.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ClickingSaveClosesBubble) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [Save] should accept and close it, then send an UploadCardRequest
  // to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// On Chrome OS, the test profile starts with a primary account already set, so
// sync-the-transport tests don't apply.
#if !defined(OS_CHROMEOS)

// Sets up Chrome with Sync-the-transport mode enabled, with the Wallet datatype
// as enabled type.
class SaveCardBubbleViewsSyncTransportFullFormBrowserTest
    : public SaveCardBubbleViewsFullFormBrowserTest {
 protected:
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest() {
    // Set up Sync the transport mode, so that sync starts on content-area
    // signins. Also add wallet data type to the list of enabled types.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillUpstream,
                              features::kAutofillEnableAccountWalletStorage},
        /*disabled_features=*/{});
  }

 public:
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest(
      const SaveCardBubbleViewsSyncTransportFullFormBrowserTest&) = delete;
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest& operator=(
      const SaveCardBubbleViewsSyncTransportFullFormBrowserTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(test_url_loader_factory());

    SaveCardBubbleViewsFullFormBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpForSyncTransportModeTest() {
    // Signing in (without making the account Chrome's primary one or explicitly
    // setting up Sync) causes the Sync machinery to start up in standalone
    // transport mode.
    secondary_account_helper::SignInSecondaryAccount(
        browser()->profile(), test_url_loader_factory(), "user@gmail.com");
    ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
              harness_->service()->GetTransportState());

    ASSERT_TRUE(harness_->AwaitSyncTransportActive());
    ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
              harness_->service()->GetTransportState());
    ASSERT_FALSE(harness_->service()->IsSyncFeatureEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;
};

// Tests the upload save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away and sends an UploadCardRequest RPC
// to Google Payments.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
                       Upload_TransportMode_ClickingSaveClosesBubble) {
  SetUpForSyncTransportModeTest();
  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [Save] should accept and close it, then send an UploadCardRequest
  // to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the implicit sync state. Ensures that the (i) info icon appears for
// upload save offers.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
                       Upload_TransportMode_InfoTextIconExists) {
  SetUpForSyncTransportModeTest();
  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // As this is an upload save for a Butter-enabled user, there should be a
  // hoverable (i) icon in the extra view explaining the functionality.
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::UPLOAD_EXPLANATION_TOOLTIP));
}

// Tests the implicit sync state. Ensures that the (i) info icon does not appear
// for local save offers.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
                       Local_TransportMode_InfoTextIconDoesNotExist) {
  SetUpForSyncTransportModeTest();
  FillForm();

  // Declining upload save will fall back to local save.
  SetUploadDetailsRpcPaymentsDeclines();
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());

  // Even though this is a Butter-enabled user, as this is a local save, there
  // should NOT be a hoverable (i) icon in the extra view.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::UPLOAD_EXPLANATION_TOOLTIP));
}

// Tests the upload save bubble when sync transport for Wallet data is active.
// Ensures that if cardholder name is explicitly requested, it is prefilled with
// the name from the user's Google Account.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
    Upload_TransportMode_RequestedCardholderNameTextfieldIsPrefilledWithFocusName) {
  // Signing in (without making the account Chrome's primary one or explicitly
  // setting up Sync) causes the Sync machinery to start up in standalone
  // transport mode.
  secondary_account_helper::SignInSecondaryAccount(
      browser()->profile(), test_url_loader_factory(), "user@gmail.com");
  SetAccountFullName("John Smith");

  ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
            harness_->service()->GetTransportState());

  ASSERT_TRUE(harness_->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            harness_->service()->GetTransportState());
  ASSERT_FALSE(harness_->service()->IsSyncFeatureEnabled());

  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be prefilled with the name on the user's Google
  // Account.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_EQ(cardholder_name_textfield->GetText(),
            base::ASCIIToUTF16("John Smith"));
}

#endif  // !OS_CHROMEOS

// Tests the fully-syncing state. Ensures that the Butter (i) info icon does not
// appear for fully-syncing users.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_NotTransportMode_InfoTextIconDoesNotExist) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Even though this is an upload save, as this is a fully-syncing user, there
  // should NOT be a hoverable (i) icon in the extra view.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::UPLOAD_EXPLANATION_TOOLTIP));
}

// Tests the upload save bubble. Ensures that clicking the [No thanks] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ClickingNoThanksClosesBubble) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

// Tests the upload save bubble. Ensures that clicking the top-right [X] close
// button successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ClickingCloseClosesBubble) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();
}

// Tests the upload save bubble. Ensures that the bubble does not surface the
// cardholder name textfield if it is not needed.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ShouldNotRequestCardholderNameInHappyPath) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Assert that cardholder name was not explicitly requested in the bubble.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithMissingNamesRequestsCardholderNameIfExpOn) {
  // Start sync.
  harness_->SetupSync();

  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is conflicting.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithConflictingNamesRequestsCardholderNameIfExpOn) {
  // Start sync.
  harness_->SetupSync();

  // Submit first shipping address form with a conflicting name.
  FillFormWithConflictingName();
  SubmitForm();

  // Submitting the second form should still show the upload save bubble and
  // legal footer, along with a textfield requesting the cardholder name.
  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that if the cardholder name textfield
// is empty, the user is not allowed to click [Save] and close the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SaveButtonIsDisabledIfNoCardholderNameAndCardholderNameRequested) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clearing out the cardholder name textfield should disable the [Save]
  // button.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(base::ASCIIToUTF16(""));
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Setting a cardholder name should enable the [Save] button.
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, filling it and clicking [Save] closes the dialog.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_EnteringCardholderNameAndClickingSaveClosesBubbleIfCardholderNameRequested) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Entering a cardholder name and clicking [Save] should accept and close
  // the bubble, then send an UploadCardRequest to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("John Smith"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance for both regular save UMA and
  // the ".RequestingCardholderName" subhistogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, it is prefilled with the name from the user's Google Account.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_RequestedCardholderNameTextfieldIsPrefilledWithFocusName) {
  base::HistogramTester histogram_tester;

  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be prefilled with the name on the user's Google
  // Account, and UMA should have logged its value's existence. Because the
  // textfield has a value, the tooltip explaining that the name came from the
  // user's Google Account should also be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_EQ(cardholder_name_textfield->GetText(),
            base::ASCIIToUTF16("John Smith"));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", true, 1);
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested but the name on the user's Google Account is unable to be fetched
// for any reason, the textfield is left blank.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_RequestedCardholderNameTextfieldIsNotPrefilledWithFocusNameIfMissing) {
  base::HistogramTester histogram_tester;

  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be blank, and UMA should have logged its value's
  // absence. Because the textfield is blank, the tooltip explaining that the
  // name came from the user's Google Account should NOT be visible.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_TRUE(cardholder_name_textfield->GetText().empty());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNamePrefilled", false, 1);
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TOOLTIP));
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog without changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_CardholderNameRequested_SubmittingPrefilledValueLogsUneditedMetric) {
  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clicking [Save] should accept and close the bubble, logging that the name
  // was not edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", false, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog after changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_CardholderNameRequested_SubmittingChangedValueLogsEditedMetric) {
  // Start sync.
  harness_->SetupSync();
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Changing the name then clicking [Save] should accept and close the bubble,
  // logging that the name was edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(
      base::ASCIIToUTF16("Jane Doe"));
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", true, 1);
}

// TODO(jsaul): Only *part* of the legal message StyledLabel is clickable, and
//              the NOTREACHED() in SaveCardBubbleViews::StyledLabelLinkClicked
//              prevents us from being able to click it unless we know the exact
//              gfx::Range of the link. When/if that can be worked around,
//              create an Upload_ClickingTosLinkClosesBubble test.

// Tests the upload save logic. Ensures that Chrome offers a local save when the
// data is complete, even if Payments rejects the data.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldOfferLocalSaveIfPaymentsDeclines) {
  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
}

// Tests the upload save logic. Ensures that Chrome offers a local save when the
// data is complete, even if the Payments upload fails unexpectedly.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldOfferLocalSaveIfPaymentsFails) {
  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcServerError();

  // Submitting the form and having the call to Payments fail should show the
  // local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE});
  NavigateTo(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and offers to upload save the card if Payments allows it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_CanOfferToSaveEvenIfNothingFoundIfPaymentsAccepts) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says yes, and the offer to save bubble
  // should be shown.
  FillFormWithCardDetailsOnly();
  SubmitFormAndWaitForCardUploadSaveBubble();
}

// Tests the upload save logic. Ensures that Chrome offers a upload save for
// dynamic change form.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_CanOfferToSaveDynamicForm) {
  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the the dynamic change form, offer to save bubble should be
  // shown.
  ResetEventWaiterForSequence({DialogEvent::DYNAMIC_FORM_PARSED});
  FillAndChangeForm();
  WaitForObservedEvent();
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_TRUE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and still does not surface the offer to upload save dialog
// if Payments declines it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldNotOfferToSaveIfNothingFoundAndPaymentsDeclines) {
  // Start sync.
  harness_->SetupSync();

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form, even with only card number and expiration date, should
  // start the flow of asking Payments if Chrome should offer to save the card
  // to Google. In this case, Payments says no, so the offer to save bubble
  // should not be shown.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  FillFormWithCardDetailsOnly();
  SubmitForm();
  WaitForObservedEvent();
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if CVC is not detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfCvcNotFound) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though CVC is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutCvc();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if the detected CVC is invalid.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfInvalidCvcFound) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the provided
  // CVC is invalid.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithInvalidCvc();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if address/cardholder name is not
// detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfNameNotFound) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though name is
  // missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutName();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting names are
// detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfNamesConflict) {
  // Start sync.
  harness_->SetupSync();

  // Submit first shipping address form with a conflicting name.
  FillFormWithConflictingName();
  SubmitForm();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the name
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if billing address is not detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfAddressNotFound) {
  // Start sync.
  harness_->SetupSync();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though billing address
  // is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutAddress();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting billing address
// postal codes are detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfPostalCodesConflict) {
  // Start sync.
  harness_->SetupSync();

  // Submit first shipping address form with a conflicting postal code.
  FillFormWithConflictingPostalCode();
  SubmitForm();

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the postal code
  // conflicts with the previous form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
}

// Tests UMA logging for the upload save bubble. Ensures that if the user
// declines upload, Autofill.UploadAcceptedCardOrigin is not logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_DecliningUploadDoesNotLogUserAcceptedCardOriginUMA) {
  base::HistogramTester histogram_tester;

  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Ensure that UMA was logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadOfferedCardOrigin",
      AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD, 1);
  histogram_tester.ExpectTotalCount("Autofill.UploadAcceptedCardOrigin", 0);
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithMissingExpirationDateRequestsExpirationDate) {
  SetUpForEditableExpirationDate();
  FillFormWithoutExpirationDate();
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is expired.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithExpiredExpirationDateRequestsExpirationDate) {
  SetUpForEditableExpirationDate();
  FillFormWithSpecificExpirationDate("08", "2000");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();
}

// Tests the upload save bubble. Ensures that the bubble does not surface the
// expiration date dropdowns if it is not needed.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ShouldNotRequestExpirationDateInHappyPath) {
  SetUpForEditableExpirationDate();
  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_LABEL)->GetVisible());

  // Assert that expiration date was not explicitly requested in the bubble.
  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_VIEW));
  EXPECT_FALSE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_YEAR));
  EXPECT_FALSE(
      FindViewInBubbleById(DialogViewId::EXPIRATION_DATE_DROPBOX_MONTH));
}

// Tests the upload save bubble. Ensures that if the expiration date drop down
// box is changing, [Save] button will change status correctly.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SaveButtonStatusResetBetweenExpirationDateSelectionChanges) {
  SetUpForEditableExpirationDate();
  FillFormWithoutExpirationDate();
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // [Save] button is disabled by default when requesting expiration date,
  // because there are no preselected values in the dropdown lists.
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Selecting only month or year will disable [Save] button.
  year_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  year_input()->SetSelectedRow(0);
  month_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);

  // Selecting both month and year will enable [Save] button.
  month_input()->SetSelectedRow(2);
  year_input()->SetSelectedRow(2);
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if the user is selecting an
// expired expiration date, it is not allowed to click [Save].
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SaveButtonIsDisabledIfExpiredExpirationDateAndExpirationDateRequested) {
  SetUpForEditableExpirationDate();
  FillFormWithoutExpirationDate();
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));

  // Set now to next month. Setting test_clock will not affect the dropdown to
  // be selected, so selecting the current January will always be expired.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());
  test_clock.Advance(base::TimeDelta::FromDays(40));
  // Selecting expired date will disable [Save] button.
  month_input()->SetSelectedRow(1);
  year_input()->SetSelectedRow(1);
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date with year pre-populated if year is valid
// but month is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithMissingExpirationDateMonthAndWithValidYear) {
  SetUpForEditableExpirationDate();
  // Submit the form with a year value, but not a month value.
  FillFormWithExpirationYearOnly(test::NextYear());
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the next year is pre-populated but month is not checked.
  EXPECT_EQ(0, month_input()->GetSelectedIndex());
  EXPECT_EQ(base::ASCIIToUTF16(test::NextYear()),
            year_input()->GetTextForRow(year_input()->GetSelectedIndex()));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date with month pre-populated if month is
// detected but year is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithMissingExpirationDateYearAndWithMonth) {
  SetUpForEditableExpirationDate();
  // Submit the form with a month value, but not a year value.
  FillFormWithExpirationMonthOnly("12");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the December is pre-populated but year is not checked.
  EXPECT_EQ(base::ASCIIToUTF16("12"),
            month_input()->GetTextForRow(month_input()->GetSelectedIndex()));
  EXPECT_EQ(0, year_input()->GetSelectedIndex());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if month is missing and year is detected
// but out of the range of dropdown.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithExpirationDateMonthAndWithYearIsOutOfRange) {
  SetUpForEditableExpirationDate();
  // Fill form but with an expiration year ten years in the future which is out
  // of the range of |year_input_dropdown_|.
  FillFormWithExpirationYearOnly(test::TenYearsFromNow());
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure no pre-populated expiration date.
  EXPECT_EQ(0, month_input()->GetSelectedIndex());
  EXPECT_EQ(0, year_input()->GetSelectedRow());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date month is missing and
// year is detected but passed.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithExpirationDateMonthAndYearExpired) {
  SetUpForEditableExpirationDate();
  // Fill form with a valid month but a passed year.
  FillFormWithSpecificExpirationDate("08", "2000");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure no pre-populated expiration date.
  EXPECT_EQ(base::ASCIIToUTF16("08"),
            month_input()->GetTextForRow(month_input()->GetSelectedIndex()));
  EXPECT_EQ(0, year_input()->GetSelectedRow());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is expired but is
// current year.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithExpirationDateMonthAndCurrentYear) {
  SetUpForEditableExpirationDate();
  const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  // Fill form with a valid month but a passed year.
  FillFormWithSpecificExpirationDate("03", "2017");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure pre-populated expiration date.
  EXPECT_EQ(base::ASCIIToUTF16("03"),
            month_input()->GetTextForRow(month_input()->GetSelectedIndex()));
  EXPECT_EQ(base::ASCIIToUTF16("2017"),
            year_input()->GetTextForRow(year_input()->GetSelectedIndex()));
}

// TODO(crbug.com/884817): Investigate combining local vs. upload tests using a
//                         boolean to branch local vs. upload logic.

// Tests StrikeDatabase interaction with the local save bubble. Ensures that a
// strike is added if the bubble is ignored.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleIgnored) {
  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Add an event observer to the controller to detect strike changes.
  AddEventObserverToController();

  base::HistogramTester histogram_tester;

  // Wait long enough to avoid bubble stickiness, then navigate away from the
  // page.
  test_clock.Advance(kCardBubbleSurviveNavigationTime);
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  NavigateTo(kCreditCardAndAddressUploadForm);
  WaitForObservedEvent();

  // Ensure that a strike was added due to the bubble being ignored.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/1, /*count=*/1);
}

// Tests StrikeDatabase interaction with the upload save bubble. Ensures that a
// strike is added if the bubble is ignored.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_AddStrikeIfBubbleIgnored) {
  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());

  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking the [X] close button should dismiss the bubble.
  ClickOnCloseButton();

  // Add an event observer to the controller to detect strike changes.
  AddEventObserverToController();

  base::HistogramTester histogram_tester;

  // Wait long enough to avoid bubble stickiness, then navigate away from the
  // page.
  test_clock.Advance(kCardBubbleSurviveNavigationTime);
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  NavigateTo(kCreditCardAndAddressUploadForm);
  WaitForObservedEvent();

  // Ensure that a strike was added due to the bubble being ignored.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/1, /*count=*/1);
}

// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleDeclined) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests the upload save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_AddStrikeIfBubbleDeclined) {
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ClickOnCancelButton(/*strike_expected=*/true);

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  bool controller_observer_set = false;

  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0;
       i < credit_card_save_manager_->GetCreditCardSaveStrikeDatabase()
               ->GetMaxStrikesLimit();
       ++i) {
    FillForm();
    SubmitFormAndWaitForCardLocalSaveBubble();

    if (!controller_observer_set) {
      // Add an event observer to the controller.
      AddEventObserverToController();
      ReduceAnimationTime();
      controller_observer_set = true;
    }

    base::HistogramTester histogram_tester;
    ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
    ClickOnCancelButton(/*strike_expected=*/true);
    WaitForObservedEvent();

    // Ensure that a strike was added due to the bubble being declined.
    // The sample logged is the Nth strike added, or (i+1).
    histogram_tester.ExpectUniqueSample(
        "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
        /*sample=*/(i + 1), /*count=*/1);
  }

  base::HistogramTester histogram_tester;

  // Submit the form a fourth time. Since the card now has maximum strikes (3),
  // the icon should be shown but the bubble should not.
  ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE});
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
  WaitForAnimationToEnd();
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());

  ClickOnCancelButton();

  // Ensure that no strike was added because the card already had max strikes.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

// Tests overall StrikeDatabase interaction with the upload save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_FullFlowTest) {
  bool controller_observer_set = false;

  // Start sync.
  harness_->SetupSync();

  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0;
       i < credit_card_save_manager_->GetCreditCardSaveStrikeDatabase()
               ->GetMaxStrikesLimit();
       ++i) {
    FillForm();
    SubmitFormAndWaitForCardUploadSaveBubble();

    if (!controller_observer_set) {
      // Add an event observer to the controller.
      AddEventObserverToController();
      ReduceAnimationTime();
      controller_observer_set = true;
    }

    base::HistogramTester histogram_tester;

    ClickOnCancelButton(/*strike_expected=*/true);
    WaitForObservedEvent();

    // Ensure that a strike was added due to the bubble being declined.
    // The sample logged is the Nth strike added, or (i+1).
    histogram_tester.ExpectUniqueSample(
        "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
        /*sample=*/(i + 1), /*count=*/1);
  }

  base::HistogramTester histogram_tester;

  // Submit the form a fourth time. Since the card now has maximum strikes (3),
  // the icon should be shown but the bubble should not.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
  NavigateTo(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  WaitForObservedEvent();
  WaitForAnimationToEnd();
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                  ->GetVisible());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW)->GetVisible());

  ClickOnCancelButton();

  // Ensure that no strike was added because the card already had max strikes.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", 0);

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

// Tests to ensure the card nickname is shown correctly in the Upstream bubble.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    LocalCardHasNickname) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetCreditCard();
  // Set card number to match the number to be filled in the form.
  card.SetNumber(base::ASCIIToUTF16("5454545454545454"));
  card.SetNickname(base::ASCIIToUTF16("nickname"));
  AddTestCreditCard(browser(), card);

  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_EQ(GetSaveCardBubbleViews()->GetCardIdentifierString(),
            card.NicknameAndLastFourDigitsForTesting());
}

IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    LocalCardHasNoNickname) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetCreditCard();
  // Set card number to match the number to be filled in the form.
  card.SetNumber(base::ASCIIToUTF16("5454545454545454"));
  AddTestCreditCard(browser(), card);

  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_EQ(GetSaveCardBubbleViews()->GetCardIdentifierString(),
            card.NetworkAndLastFourDigits());
}

// TODO(crbug.com/932818): Remove the condition once the experiment is enabled
// on ChromeOS.
#if !defined(OS_CHROMEOS)
// Ensures that the credit card icon will show in status chip.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       CreditCardIconShownInStatusChip) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();
  EXPECT_TRUE(GetSaveCardIconView());
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
}

// Ensures that the clicking on the credit card icon will reshow bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       ClickingOnCreditCardIconInStatusChipReshowsBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();
  ClickOnCloseButton();
  AddEventObserverToController();
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  EXPECT_TRUE(GetSaveCardBubbleViews());
  EXPECT_TRUE(GetSaveCardBubbleViews()->GetVisible());
}

#if defined(OS_MAC)
// TODO(crbug.com/823543): Widget activation doesn't work on Mac.
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  DISABLED_ActivateFirstInactiveBubbleForAccessibility
#else
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  ActivateFirstInactiveBubbleForAccessibility
#endif
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       MAYBE_ActivateFirstInactiveBubbleForAccessibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarView* toolbar_view = browser_view->toolbar();
  EXPECT_FALSE(toolbar_view->toolbar_account_icon_container()
                   ->page_action_icon_controller()
                   ->ActivateFirstInactiveBubbleForAccessibility());

  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Ensures the bubble's widget is visible, but inactive. Active widgets are
  // focused by accessibility, so not of concern.
  views::Widget* widget = GetSaveCardBubbleViews()->GetWidget();
  widget->Deactivate();
  widget->ShowInactive();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  EXPECT_TRUE(toolbar_view->toolbar_account_icon_container()
                  ->page_action_icon_controller()
                  ->ActivateFirstInactiveBubbleForAccessibility());

  // Ensure the bubble's widget refreshed appropriately.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->IsActive());
}

// Ensures the credit card icon updates its visibility when switching between
// tabs.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       IconAndBubbleVisibilityAfterTabSwitching) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Ensures flow is triggered, and bubble and icon view are visible.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_TRUE(GetSaveCardBubbleViews()->GetVisible());

  AddTabAtIndex(1, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  WaitForAnimationToEnd();

  // Ensures bubble and icon go away if user navigates to another tab.
  EXPECT_FALSE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  tab_model->ActivateTabAt(0, {TabStripModel::GestureType::kOther});
  WaitForAnimationToEnd();

  // If the user navigates back, shows only the icon not the bubble.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Ensures the card saving throbber animation in the status chip behaves
// correctly during credit card upload process. Also ensures the credit card
// icon goes away when upload succeeds.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       Feedback_Success) {
  base::HistogramTester histogram_tester;
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Ensures icon is visible and animation is not.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(
      GetSaveCardIconView()->loading_indicator_for_testing()->IsAnimating());

  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Ensures icon and the animation are visible.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_TRUE(
      GetSaveCardIconView()->loading_indicator_for_testing()->IsAnimating());

  SetUploadCardRpcPaymentsSucceeds();
  ResetEventWaiterForSequence({DialogEvent::RECEIVED_UPLOAD_CARD_RESPONSE,
                               DialogEvent::SHOW_CARD_SAVED_FEEDBACK});
  WaitForObservedEvent();

  // Ensures icon is not visible.
  EXPECT_FALSE(GetSaveCardIconView()->GetVisible());

  // UMA should have been logged.
  histogram_tester.ExpectTotalCount("Autofill.CreditCardUploadFeedback", 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_LOADING_ANIMATION_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_ICON_SHOWN, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_BUBBLE_SHOWN, 0);
}

// Ensures the card saving throbber animation in the status chip behaves
// correctly during credit card upload process. Also ensures the credit card
// icon and the save card failure bubble behave correctly when upload failed.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       Feedback_Failure) {
  base::HistogramTester histogram_tester;
  // Start sync.
  harness_->SetupSync();

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Ensures icon is visible and animation is not.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(
      GetSaveCardIconView()->loading_indicator_for_testing()->IsAnimating());

  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Ensures icon and the animation are visible.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_TRUE(
      GetSaveCardIconView()->loading_indicator_for_testing()->IsAnimating());

  SetUploadCardRpcPaymentsFails();
  ResetEventWaiterForSequence({DialogEvent::RECEIVED_UPLOAD_CARD_RESPONSE,
                               DialogEvent::STRIKE_CHANGE_COMPLETE,
                               DialogEvent::SHOW_CARD_SAVED_FEEDBACK});
  WaitForObservedEvent();

  // Ensures icon is visible and the animation is not animating.
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(
      GetSaveCardIconView()->loading_indicator_for_testing()->IsAnimating());

  // UMA should have been logged.
  histogram_tester.ExpectTotalCount("Autofill.CreditCardUploadFeedback", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_LOADING_ANIMATION_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_ICON_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_BUBBLE_SHOWN, 0);

  // Click on the icon.
  AddEventObserverToController();
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // UMA should have been logged.
  histogram_tester.ExpectTotalCount("Autofill.CreditCardUploadFeedback", 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_LOADING_ANIMATION_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_ICON_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.CreditCardUploadFeedback",
      AutofillMetrics::CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_BUBBLE_SHOWN, 1);
}

// Tests the sign in promo bubble. Ensures that clicking the [Save] button
// on the local save bubble successfully causes the sign in promo to show from
// the avatar toolbar button.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                       Local_ClickingSaveShowsSigninPromo) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Ensures the credit card icon is not visible.
  EXPECT_FALSE(GetSaveCardIconView()->GetVisible());
  // Sign-in promo should be showing.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::SIGN_IN_PROMO_VIEW)->GetVisible());
}

// Tests the manage cards bubble. Ensures that it will not pop up after the
// sign-in promo is closed.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
    Local_ClosingSigninPromoDoesNotShowNeitherIconNorManageCardsBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close the sign-in promo.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_CLOSED});
  ClickOnCloseButton();
  WaitForObservedEvent();

  // Ensures the neither credit card icon nor the manage cards bubble is
  // showing.
  EXPECT_FALSE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());
}
#endif  // !defined(OS_CHROMEOS)

// TODO(crbug.com/932818): Remove this once the experiment is fully launched.
class SaveCardBubbleViewsFullFormBrowserTestForManageCard
    : public SaveCardBubbleViewsFullFormBrowserTest {
 protected:
  SaveCardBubbleViewsFullFormBrowserTestForManageCard()
      : SaveCardBubbleViewsFullFormBrowserTest() {}
  ~SaveCardBubbleViewsFullFormBrowserTestForManageCard() override {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillCreditCardUploadFeedback,
                               features::kAutofillEnableToolbarStatusChip});

    SaveCardBubbleViewsFullFormBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the local save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForManageCard,
                       Local_ClickingSaveClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [Save] should accept and close it.
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);

  // The local save bubble should not be visible, but the card icon should
  // remain visible for the clickable [Manage cards] option.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
}

// Tests the manage cards bubble. Ensures that sign-in impression is recorded
// correctly.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForManageCard,
                       Local_Metrics_SigninImpressionManageCards) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // User actions should have recorded impression.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
}
#endif

// Tests the manage cards bubble. Ensures that it shows up by clicking the
// credit card icon.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForManageCard,
                       Local_ClickingIconShowsManageCards) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Bubble should be showing.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MANAGE_CARDS_VIEW)->GetVisible());
  histogram_tester.ExpectUniqueSample("Autofill.ManageCardsPrompt.Local",
                                      AutofillMetrics::MANAGE_CARDS_SHOWN, 1);
}

// Tests the manage cards bubble. Ensures that clicking the [Done]
// button closes the bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForManageCard,
                       Local_ManageCardsDoneButtonClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();

#if !defined(OS_CHROMEOS)
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

#if !defined(OS_CHROMEOS)
  // Wait for and then close the promo.
  WaitForObservedEvent();
  ClickOnCloseButton();
#endif

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on the [Done] button.
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing now and metrics should be recorded correctly.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_DONE, 1)));
}

// Tests the Manage Cards bubble. Ensures that signin action is recorded when
// user accepts footnote promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTestForManageCard,
                       Local_Metrics_AcceptingFootnotePromoManageCards) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Adding an event observer to the controller so we can wait for the bubble to
  // show.
  AddEventObserverToController();
  ReduceAnimationTime();
  ResetEventWaiterForSequence(
      {DialogEvent::BUBBLE_CLOSED, DialogEvent::BUBBLE_SHOWN});

  // Click [Save] should close the offer-to-save bubble
  // and pop up the sign-in promo.
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  WaitForObservedEvent();

  // Close promo.
  ClickOnCloseButton();

  // Open up Manage Cards prompt.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveCardIconView());
  WaitForObservedEvent();

  // Click on [Sign in] button in footnote.
  ClickOnDialogView(static_cast<DiceBubbleSyncPromoView*>(
                        FindViewInBubbleById(DialogViewId::FOOTNOTE_VIEW))
                        ->GetSigninButtonForTesting());

  // User actions should have recorded impression and click.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Impression_FromManageCardsBubble"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Signin_Signin_FromManageCardsBubble"));
}
#endif

}  // namespace autofill

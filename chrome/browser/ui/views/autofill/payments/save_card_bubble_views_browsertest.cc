// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/credit_card_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#endif

namespace autofill {
namespace {

using base::Bucket;
using testing::ElementsAre;
using testing::WithParamInterface;

const char kCreditCardAndAddressUploadForm[] =
    "/credit_card_upload_form_address_and_cc.html";
const char kCreditCardUploadForm[] = "/credit_card_upload_form_cc.html";
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
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

// Param of the test indicates whether the experiment to reposition the bubble
// ToS message is enabled.
class SaveCardBubbleViewsFullFormBrowserTest
    : public SyncTest,
      public CreditCardSaveManager::ObserverForTest,
      public PageActionIconViewObserver {
 public:
  SaveCardBubbleViewsFullFormBrowserTest(
      const SaveCardBubbleViewsFullFormBrowserTest&) = delete;
  SaveCardBubbleViewsFullFormBrowserTest& operator=(
      const SaveCardBubbleViewsFullFormBrowserTest&) = delete;
  ~SaveCardBubbleViewsFullFormBrowserTest() override = default;

 protected:
  SaveCardBubbleViewsFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {
  }

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
    REQUESTED_UPLOAD_SAVE,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_UPLOAD_CARD_REQUEST,
    RECEIVED_UPLOAD_CARD_RESPONSE,
    SHOW_CARD_SAVED_FEEDBACK,
    STRIKE_CHANGE_COMPLETE,
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

    // Set up the URL loader factory for the PaymentsNetworkInterface so we can
    // intercept those network requests too.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    autofill_manager()
        ->client()
        .GetPaymentsAutofillClient()
        ->GetPaymentsNetworkInterface()
        ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile(0));

    // Set up this class as the ObserverForTest implementation.
    credit_card_save_manager()->SetEventObserverForTesting(this);
    GetSaveCardIconView()->AddPageIconViewObserver(this);

    any_widget_observer_ = std::make_unique<views::AnyWidgetObserver>(
        views::test::AnyWidgetTestPasskey{});
    any_widget_observer_->set_shown_callback(base::BindRepeating(
        &SaveCardBubbleViewsFullFormBrowserTest::OnWidgetShown,
        base::Unretained(this)));

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);
  }

  void TearDownOnMainThread() override {
     if (!closed_all_tabs_) {
       GetSaveCardIconView()->RemovePageIconViewObserver(this);
       // credit_card_save_manager() will be null if the active web contents
       // have changed since the test began.
       if (credit_card_save_manager()) {
         credit_card_save_manager()->SetEventObserverForTesting(nullptr);
       }
    }
    SyncTest::TearDownOnMainThread();
  }

  // The primary main frame's AutofillManager.
  TestAutofillManager* autofill_manager() {
    return autofill_manager_injector_[GetActiveWebContents()];
  }

  CreditCardSaveManager* credit_card_save_manager() {
    return autofill_manager() ? autofill_manager()
                                    ->client()
                                    .GetFormDataImporter()
                                    ->GetCreditCardSaveManager()
                              : nullptr;
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
  }

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferUploadSave() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::OFFERED_UPLOAD_SAVE);
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

  void OnWidgetShown(views::Widget* widget) {
    if (widget->GetName() == "SaveCardBubble" && event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
    }
  }

  // PageActionIconViewObserver:
  void OnPageActionIconViewShown(PageActionIconView* view) override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::ICON_SHOWN);
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
        FindViewInBubbleById(DialogViewId::LEGAL_MESSAGE_VIEW)->GetVisible());
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
    ASSERT_TRUE(SetupSync());
  }

  void CloseAllTabs() {
    closed_all_tabs_ = true;
    GetBrowser(0)->tab_strip_model()->CloseAllTabs();
  }

  void NavigateToAndWaitForForm(const std::string& file_path) {
    if (file_path.find("data:") == 0U) {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(0), GURL(file_path)));
    } else {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          GetBrowser(0), embedded_test_server()->GetURL(file_path)));
    }
    ASSERT_TRUE(autofill_manager()->WaitForFormsSeen(1));
  }

  void SetAccountFullName(const std::string& full_name) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    CoreAccountInfo core_info =
        PersonalDataManagerFactory::GetForBrowserContext(GetProfile(0))
            ->payments_data_manager()
            .GetAccountInfoForPaymentsServer();

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
    // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
    ResetEventWaiterForSequence({DialogEvent::OFFERED_LOCAL_SAVE,
                                 DialogEvent::ICON_SHOWN,
                                 DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SubmitFormAndWaitForCardUploadSaveBubble() {
    // Set up the Payments RPC.
    SetUploadDetailsRpcPaymentsAccepts();
    // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_UPLOAD_SAVE,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
         DialogEvent::OFFERED_UPLOAD_SAVE, DialogEvent::ICON_SHOWN,
         DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                    ->GetVisible());
    EXPECT_TRUE(
        FindViewInBubbleById(DialogViewId::LEGAL_MESSAGE_VIEW)->GetVisible());
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecJs(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillForm() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));
  }

  // Should be called for credit_card_upload_form_cc.html.
  void FillAndChangeForm() {
    NavigateToAndWaitForForm(kCreditCardUploadForm);
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    const std::string click_add_fields_button_js =
        "(function() { document.getElementById('add_fields').click(); })();";
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), click_fill_button_js));
    ASSERT_TRUE(
        content::ExecJs(GetActiveWebContents(), click_add_fields_button_js));
  }

  void FillFormWithCardDetailsOnly() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_card_button_js =
        "(function() { document.getElementById('fill_card_only').click(); "
        "})();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_card_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutCvc() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_cvc_button_js =
        "(function() { document.getElementById('clear_cvc').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_clear_cvc_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithInvalidCvc() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_fill_invalid_cvc_button_js =
        "(function() { document.getElementById('fill_invalid_cvc').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecJs(web_contents, click_fill_invalid_cvc_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutName() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_name_button_js =
        "(function() { document.getElementById('clear_name').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_clear_name_button_js));
  }

  // Should be called for credit_card_upload_form_shipping_address.html.
  void FillFormWithConflictingName() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_conflicting_name_button_js =
        "(function() { "
        "document.getElementById('fill_conflicting_name').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecJs(web_contents, click_conflicting_name_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutExpirationDate() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecJs(web_contents, click_clear_expiration_date_button_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithExpirationMonthOnly(const std::string& month) {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecJs(web_contents, click_clear_expiration_date_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecJs(web_contents, set_month_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithExpirationYearOnly(const std::string& year) {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_expiration_date_button_js =
        "(function() { "
        "document.getElementById('clear_expiration_date').click(); "
        "})();";
    ASSERT_TRUE(
        content::ExecJs(web_contents, click_clear_expiration_date_button_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecJs(web_contents, set_year_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithSpecificExpirationDate(const std::string& month,
                                          const std::string& year) {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    std::string set_month_js =
        "(function() { document.getElementById('cc_month_exp_id').value =" +
        month + ";})();";
    ASSERT_TRUE(content::ExecJs(web_contents, set_month_js));

    std::string set_year_js =
        "(function() { document.getElementById('cc_year_exp_id').value =" +
        year + ";})();";
    ASSERT_TRUE(content::ExecJs(web_contents, set_year_js));
  }

  // Should be called for credit_card_upload_form_address_and_cc.html.
  void FillFormWithoutAddress() {
    NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string click_clear_address_button_js =
        "(function() { document.getElementById('clear_address').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_clear_address_button_js));
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

  void ClickSavePaymentIconView(SavePaymentIconView* icon_view) {
    CHECK(icon_view);
    ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi test_api(icon_view);
    test_api.NotifyClick(e);
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
    CHECK(save_card_bubble_views);

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

  void ClickOnCancelButton() {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    CHECK(save_card_bubble_views);
    ClickOnDialogViewWithIdAndWait(DialogViewId::CANCEL_BUTTON);
    CHECK(!GetSaveCardBubbleViews());
  }

  void ClickOnCloseButton() {
    SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
    CHECK(save_card_bubble_views);
    ClickOnDialogViewAndWait(
        save_card_bubble_views->GetBubbleFrameView()->close_button());
    CHECK(!GetSaveCardBubbleViews());
  }

  SaveCardBubbleViews* GetSaveCardBubbleViews() {
    SaveCardBubbleControllerImpl::CreateForWebContents(GetActiveWebContents());
    SaveCardBubbleControllerImpl* save_card_bubble_controller =
        SaveCardBubbleControllerImpl::FromWebContents(GetActiveWebContents());
    if (!save_card_bubble_controller)
      return nullptr;
    AutofillBubbleBase* save_card_bubble_view =
        save_card_bubble_controller->GetPaymentBubbleView();
    if (!save_card_bubble_view)
      return nullptr;
    return static_cast<SaveCardBubbleViews*>(save_card_bubble_view);
  }

  SavePaymentIconView* GetSaveCardIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(GetBrowser(0));
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kSaveCard);
    CHECK(browser_view->GetLocationBarView()->Contains(icon));
    return static_cast<SavePaymentIconView*>(icon);
  }

  content::WebContents* GetActiveWebContents() {
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  [[nodiscard]] testing::AssertionResult WaitForObservedEvent() {
    return event_waiter_->Wait();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  bool closed_all_tabs_ = false;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  std::unique_ptr<views::AnyWidgetObserver> any_widget_observer_;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  base::AutoReset<bool> ignore_window_activation_for_testing_{
      SaveCardBubbleControllerImpl::IgnoreWindowActivationForTesting()};
};

// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.FirstShow",
      autofill_metrics::SaveCardPromptResult::kCancelled, 1);
}

class SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream
    : public SaveCardBubbleViewsFullFormBrowserTest {
 public:
  SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream() {
    feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

class SaveCardBubbleViewsFullFormBrowserTestSettings
    : public SaveCardBubbleViewsFullFormBrowserTest {
 public:
  SaveCardBubbleViewsFullFormBrowserTestSettings() = default;

  void SetUpOnMainThread() override {
    SaveCardBubbleViewsFullFormBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // OpenSettingsFromManageCardsPrompt() tries to retrieve the PhoneHubManager
    // keyed service, whose factory implementation relies on ChromeOS having a
    // single profile, and consequently a single service instance. Creating a
    // service for both browser()->profile() and GetProfile(0) hits a CHECK,
    // so prevent this by disabling the feature.
    GetProfile(0)->GetPrefs()->SetBoolean(
        ash::multidevice_setup::kPhoneHubAllowedPrefName, false);
#endif
  }

  void OpenSettingsFromManageCardsPrompt() {
    FillForm();
    SubmitFormAndWaitForCardLocalSaveBubble();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
#endif

    // Click [Save] should close the offer-to-save bubble and show "Card saved"
    // animation.
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

    // Open up Manage Cards prompt.
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    ClickSavePaymentIconView(GetSaveCardIconView());
    ASSERT_TRUE(WaitForObservedEvent());

    // Click on the redirect button.
    ClickOnDialogViewWithId(DialogViewId::MANAGE_CARDS_BUTTON);
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
  EXPECT_EQ(2, GetBrowser(0)->tab_strip_model()->count());

  // Metrics should have been recorded correctly.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 1),
                  Bucket(ManageCardsPromptMetric::kManageCardsManageCards, 1)));
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
  CloseAllTabs();

  // Process the asynchronous close (which should do nothing).
  base::RunLoop().RunUntilIdle();
}

// Tests the upload save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away and sends an UploadCardRequest RPC
// to Google Payments.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    // TODO(crbug.com/40913383): Flaky on multiple platforms.
    DISABLED_Upload_ClickingSaveClosesBubble) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [Save] should accept and close it, then send an UploadCardRequest
  // to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

// On Chrome OS, the test profile starts with a primary account already set, so
// sync-the-transport tests don't apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Sets up Chrome with Sync-the-transport mode enabled, with the Wallet datatype
// as enabled type.
class SaveCardBubbleViewsSyncTransportFullFormBrowserTest
    : public SaveCardBubbleViewsFullFormBrowserTest {
 protected:
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest() {
    // Add wallet data type to the list of enabled types.
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kAutofillUpstream};
    std::vector<base::test::FeatureRef> disabled_features = {};
    // Since server card saves upload address information, they are only offered
    // when addresses are being synced. Enable CONTACT_INFO in transport mode.
    enabled_features.push_back(switches::kExplicitBrowserSigninUIOnDesktop);
    enabled_features.push_back(
        syncer::kSyncEnableContactInfoDataTypeInTransportMode);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 public:
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest(
      const SaveCardBubbleViewsSyncTransportFullFormBrowserTest&) = delete;
  SaveCardBubbleViewsSyncTransportFullFormBrowserTest& operator=(
      const SaveCardBubbleViewsSyncTransportFullFormBrowserTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(test_url_loader_factory());

    SaveCardBubbleViewsFullFormBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpForSyncTransportModeTest() {
    // Signing in (without granting sync consent or explicitly setting up Sync)
    // should trigger starting the Sync machinery in standalone transport mode.
    secondary_account_helper::SignInUnconsentedAccount(
        GetProfile(0), test_url_loader_factory(), "user@gmail.com");
    ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
              GetSyncService(0)->GetTransportState());

    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
              GetSyncService(0)->GetTransportState());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription test_signin_client_subscription_;
};

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
  // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN,
       DialogEvent::BUBBLE_SHOWN});
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
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
  // Signing in (without granting sync consent or explicitly setting up Sync)
  // should trigger starting the Sync machinery in standalone transport mode.
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), test_url_loader_factory(), "user@gmail.com");
  SetAccountFullName("John Smith");

  ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // The textfield should be prefilled with the name on the user's Google
  // Account.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  EXPECT_EQ(cardholder_name_textfield->GetText(), u"John Smith");
}

class SaveCardBubbleViewsSyncTransportFullFormBrowserTestParameterized
    : public SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
      /*save_card_loading_and_confirmation*/ public WithParamInterface<bool> {
 public:
  SaveCardBubbleViewsSyncTransportFullFormBrowserTestParameterized() {
    feature_list_.InitWithFeatureState(
        features::kAutofillEnableSaveCardLoadingAndConfirmation, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the upload save bubble. Ensures that clicking the "Save" button
// successfully accepts the bubble and sends an UploadCardRequest RPC to
// Google Payments.
IN_PROC_BROWSER_TEST_P(
    SaveCardBubbleViewsSyncTransportFullFormBrowserTestParameterized,
    Upload_TransportMode_ClickingSaveAcceptsBubble) {
  SetUpForSyncTransportModeTest();
  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking "Save" should accept it and then send an UploadCardRequest to
  // Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  if (GetParam()) {
    // When loading and confirmation is enabled, dialog waits for confirmation
    // after "Save" button is clicked. So no need to wait for the dialog to
    // close.
    ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  } else {
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  }
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SaveCardBubbleViewsSyncTransportFullFormBrowserTest,
    SaveCardBubbleViewsSyncTransportFullFormBrowserTestParameterized,
    /*save_card_loading_and_confirmation=*/testing::Bool());

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests the fully-syncing state. Ensures that the Butter (i) info icon does not
// appear for fully-syncing users.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_NotTransportMode_InfoTextIconDoesNotExist) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // UMA should have recorded bubble rejection.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kCancelled, 1);
}

// Tests the upload save bubble. Ensures that clicking the top-right [X] close
// button successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_ClickingCloseClosesBubble) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a textfield
// requesting cardholder name if cardholder name is conflicting.
// TODO(crbug.com/40196025)
// This test is not applicable for explicit address save dialogs.
// The test relies on the following sequence of events: First a credit card is
// imported first, but the upload fails. Subsequently, an address profile is
// imported which is than used to complement the information needed to offer a
// local save prompt. With explicit address save prompts, the storage of a new
// address is omitted if a credit card can be stored to avoid showing two
// dialogs at the same time.
// To make test work one need to inject an existing address into the
// PersonalDataManager. Alternatively, the import logic should try to get an
// address candidate from the form even though no address was imported yet.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    DISABLED_Upload_SubmittingFormWithConflictingNamesRequestsCardholderNameIfExpOn) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clearing out the cardholder name textfield should disable the [Save]
  // button.
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(u"");
  views::LabelButton* save_button = static_cast<views::LabelButton*>(
      FindViewInBubbleById(DialogViewId::OK_BUTTON));
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_DISABLED);
  // Setting a cardholder name should enable the [Save] button.
  cardholder_name_textfield->InsertOrReplaceText(u"John Smith");
  EXPECT_EQ(save_button->GetState(),
            views::LabelButton::ButtonState::STATE_NORMAL);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, it is prefilled with the name from the user's Google Account.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_RequestedCardholderNameTextfieldIsPrefilledWithFocusName) {
  base::HistogramTester histogram_tester;

  // Start sync.
  ASSERT_TRUE(SetupSync());
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
  EXPECT_EQ(cardholder_name_textfield->GetText(), u"John Smith");
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
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsDeclines();

  // Submitting the form and having Payments decline offering to save should
  // show the local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN,
       DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
}

// Tests the upload save logic. Ensures that Chrome offers a local save when the
// data is complete, even if the Payments upload fails unexpectedly.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldOfferLocalSaveIfPaymentsFails) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Set up the Payments RPC.
  SetUploadDetailsRpcServerError();

  // Submitting the form and having the call to Payments fail should show the
  // local save bubble.
  // (Must wait for response from Payments before accessing the controller.)
  // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN,
       DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and offers to upload save the card if Payments allows it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_CanOfferToSaveEvenIfNothingFoundIfPaymentsAccepts) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(SetupSync());

  // Set up the Payments RPC.
  SetUploadDetailsRpcPaymentsAccepts();

  // Submitting the the dynamic change form, offer to save bubble should be
  // shown.
  // Relative order of ICON_SHOWN and BUBBLE_SHOWN does not matter.
  FillAndChangeForm();
  ResetEventWaiterForSequence(
      {DialogEvent::REQUESTED_UPLOAD_SAVE,
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_UPLOAD_SAVE, DialogEvent::ICON_SHOWN,
       DialogEvent::BUBBLE_SHOWN});
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome delegates the offer-to-save
// call to Payments, and still does not surface the offer to upload save dialog
// if Payments declines it.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldNotOfferToSaveIfNothingFoundAndPaymentsDeclines) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_FALSE(GetSaveCardBubbleViews());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if CVC is not detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfCvcNotFound) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though CVC is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutCvc();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if the detected CVC is invalid.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfInvalidCvcFound) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the provided
  // CVC is invalid.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithInvalidCvc();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if address/cardholder name is not
// detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    // TODO(crbug.com/40913383): Flaky on multiple platforms.
    DISABLED_Logic_ShouldAttemptToOfferToSaveIfNameNotFound) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though name is
  // missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutName();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting names are
// detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfNamesConflict) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though the name
  // in the credit card form conflicts with the one in the address form.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithConflictingName();
  SubmitForm();

  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if billing address is not detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfAddressNotFound) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still start the flow of asking Payments if
  // Chrome should offer to save the card to Google, even though billing address
  // is missing.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillFormWithoutAddress();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests the upload save logic. Ensures that Chrome lets Payments decide whether
// upload save should be offered, even if multiple conflicting billing address
// postal codes are detected.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Logic_ShouldAttemptToOfferToSaveIfPostalCodesConflict) {
  // Start sync.
  ASSERT_TRUE(SetupSync());
  // Add one address to the profile. This address should have a different
  // zipcode than the one to be filled in the form below.
  AutofillProfile address_profile = test::GetFullProfile();
  address_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"91111");
  PersonalDataManagerFactory::GetForBrowserContext(GetProfile(0))
      ->address_data_manager()
      .AddProfile(address_profile);

  // Submitting the form should start the flow of asking Payments if Chrome
  // should offer to save the card to Google, even though the postal codes in
  // the two known addresses conflict - the address filled in the form has
  // zipcode of 94043, comparing to the pre-existing profile zipcode of 91111.
  ResetEventWaiterForSequence({DialogEvent::REQUESTED_UPLOAD_SAVE});
  FillForm();
  SubmitForm();

  ASSERT_TRUE(WaitForObservedEvent());
}

// Tests UMA logging for the upload save bubble. Ensures that if the user
// declines upload, Autofill.UploadAcceptedCardOrigin is not logged.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_DecliningUploadDoesNotLogUserAcceptedCardOriginUMA) {
  base::HistogramTester histogram_tester;

  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  test_clock.Advance(base::Days(40));
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
    // TODO(crbug.com/40913383): Flaky on multiple platforms.
    DISABLED_Upload_SubmittingFormWithMissingExpirationDateMonthAndWithValidYear) {
  SetUpForEditableExpirationDate();
  // Submit the form with a year value, but not a month value.
  FillFormWithExpirationYearOnly(test::NextYear());
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the next year is pre-populated but month is not checked.
  EXPECT_EQ(0u, month_input()->GetSelectedIndex());
  EXPECT_EQ(
      base::ASCIIToUTF16(test::NextYear()),
      year_input()->GetTextForRow(year_input()->GetSelectedIndex().value()));
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date with month pre-populated if month is
// detected but year is missing.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    // TODO(crbug.com/40913383): Flaky on multiple platforms.
    DISABLED_Upload_SubmittingFormWithMissingExpirationDateYearAndWithMonth) {
  SetUpForEditableExpirationDate();
  // Submit the form with a month value, but not a year value.
  FillFormWithExpirationMonthOnly("12");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure the December is pre-populated but year is not checked.
  EXPECT_EQ(u"12", month_input()->GetTextForRow(
                       month_input()->GetSelectedIndex().value()));
  EXPECT_EQ(0u, year_input()->GetSelectedIndex());
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
  EXPECT_EQ(0u, month_input()->GetSelectedIndex());
  EXPECT_EQ(0u, year_input()->GetSelectedRow());
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
  EXPECT_EQ(u"08", month_input()->GetTextForRow(
                       month_input()->GetSelectedIndex().value()));
  EXPECT_EQ(0u, year_input()->GetSelectedRow());
}

// Tests the upload save bubble. Ensures that the bubble surfaces a pair of
// dropdowns requesting expiration date if expiration date is expired but is
// current year.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    Upload_SubmittingFormWithExpirationDateMonthAndCurrentYear) {
  SetUpForEditableExpirationDate();
  const base::Time kJune2017 =
      base::Time::FromSecondsSinceUnixEpoch(1497552271);
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  // Fill form with a valid month but a passed year.
  FillFormWithSpecificExpirationDate("03", "2017");
  SubmitFormAndWaitForCardUploadSaveBubble();
  VerifyExpirationDateDropdownsAreVisible();

  // Ensure pre-populated expiration date.
  EXPECT_EQ(u"03", month_input()->GetTextForRow(
                       month_input()->GetSelectedIndex().value()));
  EXPECT_EQ(u"2017", year_input()->GetTextForRow(
                         year_input()->GetSelectedIndex().value()));
}

// TODO(crbug.com/40594007): Investigate combining local vs. upload tests using
// a
//                         boolean to branch local vs. upload logic.
// Tests the local save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleDeclined) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests the local save bubble. Ensures that clicking the [X] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_AddStrikeIfBubbleIgnored) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [X] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCloseButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*expected_bucket_count=*/1);
}

// Tests the upload save bubble. Ensures that clicking the [No thanks] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_AddStrikeIfBubbleDeclined) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [No thanks] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*count=*/1);
}

// Tests the upload save bubble. Ensures that clicking the [X] button
// successfully causes a strike to be added.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_AddStrikeIfBubbleIgnored) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Clicking [X] should cancel and close it.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
  ClickOnCloseButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Ensure that a strike was added.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave",
      /*sample=*/(1), /*expected_bucket_count=*/1);
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0; i < credit_card_save_manager()
                          ->GetCreditCardSaveStrikeDatabase()
                          ->GetMaxStrikesLimit();
       ++i) {
    FillForm();
    SubmitFormAndWaitForCardLocalSaveBubble();

    base::HistogramTester histogram_tester;
    ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
    ClickOnCancelButton();
    ASSERT_TRUE(WaitForObservedEvent());

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
      {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN});
  FillForm();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickSavePaymentIconView(GetSaveCardIconView());
  ASSERT_TRUE(WaitForObservedEvent());
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
      "Autofill.SaveCreditCardPromptOffer.Local.FirstShow",
      autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached, 1);
}

// Tests overall StrikeDatabase interaction with the upload save bubble. Runs an
// example of declining the prompt three times and ensuring that the
// offer-to-save bubble does not appear on the fourth try. Then, ensures that no
// strikes are added if the card already has max strikes.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    StrikeDatabase_Upload_FullFlowTest) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0; i < credit_card_save_manager()
                          ->GetCreditCardSaveStrikeDatabase()
                          ->GetMaxStrikesLimit();
       ++i) {
    FillForm();
    SubmitFormAndWaitForCardUploadSaveBubble();

    base::HistogramTester histogram_tester;

    ResetEventWaiterForSequence({DialogEvent::STRIKE_CHANGE_COMPLETE});
    ClickOnCancelButton();
    ASSERT_TRUE(WaitForObservedEvent());

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
       DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
       DialogEvent::OFFERED_UPLOAD_SAVE, DialogEvent::ICON_SHOWN});
  NavigateToAndWaitForForm(kCreditCardAndAddressUploadForm);
  FillForm();
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
  EXPECT_FALSE(GetSaveCardBubbleViews());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickSavePaymentIconView(GetSaveCardIconView());
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_UPLOAD)
                  ->GetVisible());
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::LEGAL_MESSAGE_VIEW)->GetVisible());

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
      "Autofill.SaveCreditCardPromptOffer.Upload.FirstShow",
      autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached, 1);
}

// Tests to ensure the card nickname is shown correctly in the Upstream bubble.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    LocalCardHasNickname) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetCreditCard();
  // Set card number to match the number to be filled in the form.
  card.SetNumber(u"5454545454545454");
  card.SetNickname(u"nickname");
  AddTestCreditCard(GetProfile(0), card);

  // Start sync.
  ASSERT_TRUE(SetupSync());

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
  card.SetNumber(u"5454545454545454");
  AddTestCreditCard(GetProfile(0), card);

  // Start sync.
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  EXPECT_EQ(GetSaveCardBubbleViews()->GetCardIdentifierString(),
            card.NetworkAndLastFourDigits());
}

class SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized
    : public SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
      /*save_card_loading_and_confirmation*/ public WithParamInterface<bool> {
 public:
  SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized() {
    feature_list_.InitWithFeatureState(
        features::kAutofillEnableSaveCardLoadingAndConfirmation, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog after changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_P(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized,
    Upload_CardholderNameRequested_SubmittingChangedValueLogsEditedMetric) {
  // Start sync.
  ASSERT_TRUE(SetupSync());
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Changing the name then clicking "Save" should accept the bubble and log
  // that the name was edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(u"Jane Doe");
  base::HistogramTester histogram_tester;
  if (GetParam()) {
    // When loading and confirmation is enabled, dialog waits for confirmation
    // after "Save" button is clicked. So no need to wait for the dialog to
    // close.
    ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  } else {
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", true, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested and the user accepts the dialog without changing it, the correct
// metric is logged.
IN_PROC_BROWSER_TEST_P(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized,
    Upload_CardholderNameRequested_SubmittingPrefilledValueLogsUneditedMetric) {
  // Start sync.
  ASSERT_TRUE(SetupSync());
  // Set the user's full name.
  SetAccountFullName("John Smith");

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Clicking "Save" should accept the bubble and log that the name was not
  // edited.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  base::HistogramTester histogram_tester;
  if (GetParam()) {
    // When loading and confirmation is enabled, dialog waits for confirmation
    // after "Save" button is clicked. So no need to wait for the dialog to
    // close.
    ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  } else {
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardCardholderNameWasEdited", false, 1);
}

// Tests the upload save bubble. Ensures that if cardholder name is explicitly
// requested, filling it and clicking "Save" logs dialog's acceptance.
IN_PROC_BROWSER_TEST_P(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized,
    Upload_EnteringCardholderNameAndClickingSaveAcceptsBubbleIfCardholderNameRequested) {
  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Submitting the form should still show the upload save bubble and legal
  // footer, along with a textfield specifically requesting the cardholder name.
  FillFormWithoutName();
  SubmitFormAndWaitForCardUploadSaveBubble();
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));

  // Entering a cardholder name and clicking "Save" should accept the bubble and
  // then send an UploadCardRequest to Google Payments.
  ResetEventWaiterForSequence({DialogEvent::SENT_UPLOAD_CARD_REQUEST});
  views::Textfield* cardholder_name_textfield = static_cast<views::Textfield*>(
      FindViewInBubbleById(DialogViewId::CARDHOLDER_NAME_TEXTFIELD));
  cardholder_name_textfield->InsertOrReplaceText(u"John Smith");
  base::HistogramTester histogram_tester;
  if (GetParam()) {
    // When loading and confirmation is enabled, dialog waits for confirmation
    // after "Save" button is clicked. So no need to wait for the dialog to
    // close.
    ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);
  } else {
    ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  }
  // UMA should have recorded bubble acceptance for both regular save UMA and
  // the ".RequestingCardholderName" subhistogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow."
      "RequestingCardholderName",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstreamParameterized,
    /*save_card_loading_and_confirmation=*/testing::Bool());

class SaveCardBubbleViewsFullFormBrowserTestWithLoadingAndConfirmation
    : public SaveCardBubbleViewsFullFormBrowserTest {
 public:
  SaveCardBubbleViewsFullFormBrowserTestWithLoadingAndConfirmation() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableSaveCardLoadingAndConfirmation);
  }
 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the upload save bubble. Ensures that clicking the [Save] button
// does not close the bubble, causes a loading throbber to appear and hides the
// other dialog buttons.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithLoadingAndConfirmation,
    Upload_ClickingSave_ShowsLoadingView) {
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  SaveCardBubbleViews* save_card_bubble_views = GetSaveCardBubbleViews();
  views::View* loading_throbber =
      FindViewInBubbleById(DialogViewId::LOADING_THROBBER);
  EXPECT_FALSE(loading_throbber->IsDrawn());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::OK_BUTTON)->IsDrawn());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON)->IsDrawn());

  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);

  EXPECT_TRUE(save_card_bubble_views->GetWidget()->IsVisible());
  EXPECT_TRUE(loading_throbber->IsDrawn());
  EXPECT_EQ(FindViewInBubbleById(DialogViewId::OK_BUTTON), nullptr);
  EXPECT_EQ(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON), nullptr);
}

// Tests the local save bubble. Ensures that clicking the [Save] button
// closes the bubble.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithLoadingAndConfirmation,
    Local_ClickingSave_ClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  auto* bubble_widget = GetSaveCardBubbleViews()->GetWidget();
  views::test::TestWidgetObserver bubble_observer(bubble_widget);

  EXPECT_TRUE(bubble_widget->IsVisible());

  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  EXPECT_TRUE(bubble_observer.widget_closed());
}

// Tests that when the bubble view is created while the controller is in an
// UPLOAD_IN_PROGRESS state, the loading view will be shown.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithLoadingAndConfirmation,
    Upload_InProgress_ShowsLoadingView) {
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  // Start the save card upload. The save card controller bubble type should be
  // in an UPLOAD_IN_PROGRESS state.
  ClickOnDialogViewWithId(DialogViewId::OK_BUTTON);

  // Focus onto the bubble view and then focus onto the main frame to hide the
  // bubble view.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetSaveCardBubbleViews()->GetWidget());
  GetSaveCardBubbleViews()->GetWidget()->Activate();
  BrowserView::GetBrowserViewForBrowser(GetBrowser(0))->Activate();
  destroyed_waiter.Wait();

  // Wait for the bounds of the save payment icon view to be ready before
  // clicking. Due to how the bounds are set asynchronously, the icon can be
  // visible but un-clickable due to its unset bounds.
  ui_test_utils::ViewBoundsWaiter save_card_icon_view_waiter(
      GetSaveCardIconView());
  save_card_icon_view_waiter.WaitForNonEmptyBounds();

  // Click on the save card icon to reshow the bubble view.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickSavePaymentIconView(GetSaveCardIconView());
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(GetSaveCardBubbleViews()->IsDrawn());

  // Expect that the loading view is correctly shown.
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::LOADING_THROBBER)->IsDrawn());
  EXPECT_EQ(FindViewInBubbleById(DialogViewId::OK_BUTTON), nullptr);
  EXPECT_EQ(FindViewInBubbleById(DialogViewId::CANCEL_BUTTON), nullptr);
}

// Tests the local save bubble. Ensures that clicking the [Save] button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

  // Clicking [Save] should accept and close it.
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);
  // UMA should have recorded bubble acceptance.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);

  // The local save bubble should not be visible, but the card icon should
  // remain visible for the clickable [Manage cards] option.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_TRUE(GetSaveCardIconView()->GetVisible());
}

// Tests the manage cards bubble. Ensures that it shows up by clicking the
// credit card icon.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ClickingIconShowsManageCards) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation -- followed by the sign-in promo (if not on Chrome OS).
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickSavePaymentIconView(GetSaveCardIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  // Bubble should be showing.
  EXPECT_TRUE(
      FindViewInBubbleById(DialogViewId::MANAGE_CARDS_VIEW)->GetVisible());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ManageCardsPrompt", ManageCardsPromptMetric::kManageCardsShown,
      1);
}

// Tests the manage cards bubble. Ensures that clicking the [Done]
// button closes the bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       Local_ManageCardsDoneButtonClosesBubble) {
  FillForm();
  SubmitFormAndWaitForCardLocalSaveBubble();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
#endif

  // Click [Save] should close the offer-to-save bubble and show "Card saved"
  // animation.
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // Open up Manage Cards prompt.
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickSavePaymentIconView(GetSaveCardIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  // Click on the [Done] button.
  ClickOnDialogViewWithIdAndWait(DialogViewId::OK_BUTTON);

  // No bubble should be showing now and metrics should be recorded correctly.
  EXPECT_EQ(nullptr, GetSaveCardBubbleViews());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 1),
                  Bucket(ManageCardsPromptMetric::kManageCardsDone, 1)));
}

IN_PROC_BROWSER_TEST_F(SaveCardBubbleViewsFullFormBrowserTest,
                       IconViewAccessibleName) {
  EXPECT_EQ(GetSaveCardIconView()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD));
  EXPECT_EQ(GetSaveCardIconView()->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD));
}

// Test to verify the account chip footer is displayed correctly on the upload
// save bubble. User label information contains the user avatar and email.
IN_PROC_BROWSER_TEST_F(
    SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
    UploadBubble_CheckForAccountChipFooter) {
  ASSERT_TRUE(SetupSync());

  FillForm();
  SubmitFormAndWaitForCardUploadSaveBubble();

  views::View* view = FindViewInBubbleById(DialogViewId::USER_INFORMATION_VIEW);
  ASSERT_NE(nullptr, view);
  EXPECT_TRUE(view->GetVisible());
}

}  // namespace autofill

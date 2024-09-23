// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_loading_indicator_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/payments/local_card_migration_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

namespace autofill {
namespace {

using base::Bucket;
using testing::ElementsAre;

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

constexpr char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
constexpr char kURLMigrateCardRequest[] =
    "https://payments.google.com/payments/apis-secure/chromepaymentsservice/"
    "migratecards"
    "?s7e_suffix=chromewallet";

constexpr char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
constexpr char kResponseGetUploadDetailsSuccessLong[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Long message 1 long message "
    "2 long message 3 long message 4 long message 5 long message 6 long "
    "message 7 long message 8 long message 9 long message 10 long message 11 "
    "long message 12 long message 13 long message "
    "14\"}]},\"context_token\":\"dummy_context_token\"}";
constexpr char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

constexpr char kResponseMigrateCardSuccess[] =
    "{\"save_result\":[{\"unique_id\":\"0\", \"status\":\"SUCCESS\"}, "
    "{\"unique_id\":\"1\", \"status\":\"SUCCESS\"}], "
    "\"value_prop_display_text\":\"example message.\"}";

constexpr char kCreditCardFormURL[] =
    "/credit_card_upload_form_address_and_cc.html";

constexpr char kFirstCardNumber[] = "5428424047572420";   // Mastercard
constexpr char kSecondCardNumber[] = "4782187095085933";  // Visa
constexpr char kThirdCardNumber[] = "4111111111111111";   // Visa
constexpr char kInvalidCardNumber[] = "4444444444444444";
constexpr char kMaskedCardNumber[] = "2420";

constexpr double kFakeGeolocationLatitude = 1.23;
constexpr double kFakeGeolocationLongitude = 4.56;

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class LocalCardMigrationBrowserTest
    : public SyncTest,
      public LocalCardMigrationManager::ObserverForTest {
 public:
  LocalCardMigrationBrowserTest(const LocalCardMigrationBrowserTest&) = delete;
  LocalCardMigrationBrowserTest& operator=(
      const LocalCardMigrationBrowserTest&) = delete;

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
  enum class DialogEvent : int {
    REQUESTED_LOCAL_CARD_MIGRATION,
    RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
    SENT_MIGRATE_CARDS_REQUEST,
    RECEIVED_MIGRATE_CARDS_RESPONSE
  };

  LocalCardMigrationBrowserTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitAndDisableFeature(
        features::kAutofillEnableNewCardArtAndNetworkImages);
  }

  ~LocalCardMigrationBrowserTest() override {}

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ASSERT_TRUE(SetupClients());
    chrome::NewTab(GetBrowser(0));

    // Set up the URL loader factory for the PaymentsNetworkInterface so we can
    // intercept those network requests too.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    ContentAutofillClient* client =
        ContentAutofillClient::FromWebContents(GetActiveWebContents());
    client->GetPaymentsAutofillClient()
        ->GetPaymentsNetworkInterface()
        ->set_url_loader_factory_for_testing(test_shared_loader_factory_);

    // Set up this class as the ObserverForTest implementation.
    client->GetFormDataImporter()
        ->local_card_migration_manager()
        ->SetEventObserverForTesting(this);
    personal_data_ =
        PersonalDataManagerFactory::GetForBrowserContext(GetProfile(0));

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile(0));

    // Set up the fake geolocation data.
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            kFakeGeolocationLatitude, kFakeGeolocationLongitude);

    ASSERT_TRUE(SetupSync());

    // Set the billing_customer_number to designate existence of a Payments
    // account.
    const PaymentsCustomerData data =
        PaymentsCustomerData(/*customer_id=*/"123456");
    SetPaymentsCustomerData(data);

    SetUploadDetailsRpcPaymentsAccepts();
    SetUpMigrateCardsRpcPaymentsAccepts();
  }

  void TearDownOnMainThread() override {
    personal_data_ = nullptr;

    SyncTest::TearDownOnMainThread();
  }

  void SetPaymentsCustomerDataOnDBSequence(
      AutofillWebDataService* wds,
      const PaymentsCustomerData& customer_data) {
    DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
    PaymentsAutofillTable::FromWebDatabase(wds->GetDatabase())
        ->SetPaymentsCustomerData(&customer_data);
  }

  void SetPaymentsCustomerData(const PaymentsCustomerData& customer_data) {
    scoped_refptr<AutofillWebDataService> wds =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            GetProfile(0), ServiceAccessType::EXPLICIT_ACCESS);
    base::RunLoop loop;
    wds->GetDBTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &LocalCardMigrationBrowserTest::SetPaymentsCustomerDataOnDBSequence,
            base::Unretained(this), base::Unretained(wds.get()), customer_data),
        base::BindOnce(&base::RunLoop::Quit, base::Unretained(&loop)));
    loop.Run();
    WaitForOnPersonalDataChanged();
  }

  void WaitForOnPersonalDataChanged() {
    personal_data_->AddObserver(&personal_data_observer_);
    personal_data_->Refresh();

    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&personal_data_observer_);
    personal_data_->RemoveObserver(&personal_data_observer_);
  }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[GetActiveWebContents()];
  }

  void NavigateToAndWaitForForm(const std::string& file_path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        GetBrowser(0), file_path.find("data:") == 0U
                           ? GURL(file_path)
                           : embedded_test_server()->GetURL(file_path)));
    ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
  }

  void OnDecideToRequestLocalCardMigration() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION);
  }

  void OnReceivedGetUploadDetailsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE);
  }

  void OnSentMigrateCardsRequest() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::SENT_MIGRATE_CARDS_REQUEST);
  }

  void OnReceivedMigrateCardsResponse() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE);
  }

  CreditCard SaveLocalCard(std::string card_number,
                           bool set_as_expired_card = false,
                           bool set_nickname = false) {
    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "John Smith", card_number.c_str(),
                            "12",
                            set_as_expired_card ? test::LastYear().c_str()
                                                : test::NextYear().c_str(),
                            "1");
    local_card.set_guid("00000000-0000-0000-0000-" + card_number.substr(0, 12));
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);
    if (set_nickname)
      local_card.SetNickname(u"card nickname");

    AddTestCreditCard(GetProfile(0), local_card);
    return local_card;
  }

  CreditCard SaveServerCard(std::string card_number) {
    CreditCard server_card;
    test::SetCreditCardInfo(&server_card, "John Smith", card_number.c_str(),
                            "12", test::NextYear().c_str(), "1");
    server_card.set_guid("00000000-0000-0000-0000-" +
                         card_number.substr(0, 12));
    server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
    server_card.set_server_id("full_id_" + card_number);
    server_card.SetNetworkForMaskedCard(kVisaCard);
    AddTestServerCreditCard(GetProfile(0), server_card);
    return server_card;
  }

  void UseCardAndWaitForMigrationOffer(std::string card_number) {
    // Reusing a card should show the migration offer bubble.
    ResetEventWaiterForSequence(
        {DialogEvent::REQUESTED_LOCAL_CARD_MIGRATION,
         DialogEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE});
    FillAndSubmitFormWithCard(card_number);
    ASSERT_TRUE(WaitForObservedEvent());
  }

  void ClickOnSaveButtonAndWaitForMigrationResults() {
    ResetEventWaiterForSequence({DialogEvent::SENT_MIGRATE_CARDS_REQUEST,
                                 DialogEvent::RECEIVED_MIGRATE_CARDS_RESPONSE});
    ClickOnOkButton(GetLocalCardMigrationMainDialogView());
    ASSERT_TRUE(WaitForObservedEvent());
  }

  void FillAndSubmitFormWithCard(std::string card_number) {
    NavigateToAndWaitForForm(kCreditCardFormURL);
    content::WebContents* web_contents = GetActiveWebContents();

    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecJs(web_contents, click_fill_button_js));

    const std::string fill_cc_number_js =
        "(function() { document.getElementsByName(\"cc_number\")[0].value = " +
        card_number + "; })();";
    ASSERT_TRUE(content::ExecJs(web_contents, fill_cc_number_js));

    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecJs(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  void SetUploadDetailsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsSuccess);
  }

  void SetUploadDetailsRpcPaymentsDeclines() {
    test_url_loader_factory()->AddResponse(kURLGetUploadDetailsRequest,
                                           kResponseGetUploadDetailsFailure);
  }

  void SetUpMigrateCardsRpcPaymentsAccepts() {
    test_url_loader_factory()->AddResponse(kURLMigrateCardRequest,
                                           kResponseMigrateCardSuccess);
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

  void ClickOnDialogViewAndWait(
      views::View* view,
      views::BubbleDialogDelegateView* local_card_migration_view) {
    CHECK(local_card_migration_view);
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        local_card_migration_view->GetWidget());
    local_card_migration_view->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(
            local_card_migration_view->GetWidget()
                ->non_client_view()
                ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
  }

  views::View* FindViewInDialogById(
      DialogViewId view_id,
      views::BubbleDialogDelegateView* local_card_migration_view) {
    CHECK(local_card_migration_view);

    views::View* specified_view =
        local_card_migration_view->GetViewByID(static_cast<int>(view_id));

    if (!specified_view) {
      specified_view =
          local_card_migration_view->GetWidget()->GetRootView()->GetViewByID(
              static_cast<int>(view_id));
    }

    return specified_view;
  }

  void ClickOnOkButton(
      views::BubbleDialogDelegateView* local_card_migration_view) {
    views::View* ok_button = local_card_migration_view->GetOkButton();

    ClickOnDialogViewAndWait(ok_button, local_card_migration_view);
  }

  void ClickOnCancelButton(
      views::BubbleDialogDelegateView* local_card_migration_view) {
    views::View* cancel_button = local_card_migration_view->GetCancelButton();
    ClickOnDialogViewAndWait(cancel_button, local_card_migration_view);
  }

  LocalCardMigrationBubbleViews* GetLocalCardMigrationOfferBubbleViews() {
    LocalCardMigrationBubbleControllerImpl*
        local_card_migration_bubble_controller_impl =
            LocalCardMigrationBubbleControllerImpl::FromWebContents(
                GetActiveWebContents());
    if (!local_card_migration_bubble_controller_impl)
      return nullptr;
    return static_cast<LocalCardMigrationBubbleViews*>(
        local_card_migration_bubble_controller_impl
            ->local_card_migration_bubble_view());
  }

  views::BubbleDialogDelegateView* GetLocalCardMigrationMainDialogView() {
    LocalCardMigrationDialogControllerImpl*
        local_card_migration_dialog_controller_impl =
            LocalCardMigrationDialogControllerImpl::FromWebContents(
                GetActiveWebContents());
    if (!local_card_migration_dialog_controller_impl)
      return nullptr;
    return static_cast<LocalCardMigrationDialogView*>(
        local_card_migration_dialog_controller_impl
            ->local_card_migration_dialog_view());
  }

  PageActionIconView* GetLocalCardMigrationIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(GetBrowser(0));
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kLocalCardMigration);
    EXPECT_TRUE(browser_view->GetLocationBarView()->Contains(icon));
    return icon;
  }

  views::View* close_button() {
    LocalCardMigrationBubbleViews* local_card_migration_bubble_views =
        static_cast<LocalCardMigrationBubbleViews*>(
            GetLocalCardMigrationOfferBubbleViews());
    CHECK(local_card_migration_bubble_views);
    return local_card_migration_bubble_views->GetBubbleFrameView()
        ->close_button();
  }

  views::View* GetCardListView() {
    return static_cast<LocalCardMigrationDialogView*>(
               GetLocalCardMigrationMainDialogView())
        ->card_list_view_;
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

  void WaitForCardDeletion() { WaitForPersonalDataChange(GetProfile(0)); }

  raw_ptr<PersonalDataManager> personal_data_ = nullptr;
  PersonalDataLoadedObserverMock personal_data_observer_;

 private:
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  base::test::ScopedFeatureList feature_list_;
};

class LocalCardMigrationBrowserUiTest
    : public SupportsTestDialog<LocalCardMigrationBrowserTest> {
 public:
  LocalCardMigrationBrowserUiTest(const LocalCardMigrationBrowserUiTest&) =
      delete;
  LocalCardMigrationBrowserUiTest& operator=(
      const LocalCardMigrationBrowserUiTest&) = delete;

 protected:
  LocalCardMigrationBrowserUiTest() = default;
  ~LocalCardMigrationBrowserUiTest() override = default;

  // SupportsTestDialog:
  void ShowUi(const std::string& name) override {
    test_url_loader_factory()->AddResponse(
        kURLGetUploadDetailsRequest, kResponseGetUploadDetailsSuccessLong);
    SaveLocalCard(kFirstCardNumber);
    SaveLocalCard(kSecondCardNumber);
    UseCardAndWaitForMigrationOffer(kFirstCardNumber);
    ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  }
};

// Ensures that migration is not offered when user saves a new card.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_UsingNewCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kSecondCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that migration is not offered when payments declines the cards.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_IntermediateMigrationOfferDoesNotShowWhenPaymentsDeclines) {
  base::HistogramTester histogram_tester;
  SetUploadDetailsRpcPaymentsDeclines();

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a saved server card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ReusingServerCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kMaskedCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is shown after reusing
// a saved server card, if there is at least one card to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ReusingServerCardWithMigratableLocalCardShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveServerCard(kMaskedCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfServerCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
}

// Ensures that the intermediate migration bubble is not shown after reusing
// a previously saved local card, if there are no other cards to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ReusingLocalCardDoesNotShowIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  FillAndSubmitFormWithCard(kFirstCardNumber);

  // No migration bubble should be showing, because the single card upload
  // bubble should be displayed instead.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // No metrics are recorded when migration is not offered.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

// Ensures that the intermediate migration bubble is triggered after reusing
// a saved local card, if there are multiple local cards available to migrate.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ReusingLocalCardShowsIntermediateMigrationOffer) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  // The intermediate migration bubble should show.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.FirstShow"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
}

// Ensures that clicking [X] on the offer bubble makes the bubble disappear.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClickingCloseClosesBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
}

// Ensures that the credit card icon will show in location bar.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_CreditCardIconShownInLocationBar) {
  SaveServerCard(kMaskedCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);

  EXPECT_TRUE(GetLocalCardMigrationIconView()->GetVisible());
}

// Ensures that clicking on the credit card icon in the omnibox reopens the
// offer bubble after closing it.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClickingOmniboxIconReshowsBubble) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleOffer.Reshows"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, 1),
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, 1)));
}

// Ensures that accepting the intermediate migration offer opens up the main
// migration dialog.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClickingContinueOpensDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  // Dialog should be visible.
  EXPECT_TRUE(FindViewInDialogById(
                  DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_OFFER_DIALOG,
                  GetLocalCardMigrationMainDialogView())
                  ->GetVisible());
  // Intermediate bubble should be gone.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(autofill_metrics::MAIN_DIALOG_SHOWN, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalCardMigrationBubbleResult.FirstShow"),
              ElementsAre(Bucket(
                  autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.LocalCardMigrationDialogOffer"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_DIALOG_SHOWN, 1)));
}

// Ensures that the migration dialog contains all the valid card stored in
// Chrome browser local storage.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_DialogContainsAllValidMigratableCard) {
  base::HistogramTester histogram_tester;

  CreditCard first_card = SaveLocalCard(kFirstCardNumber);
  CreditCard second_card = SaveLocalCard(kSecondCardNumber);
  SaveLocalCard(kThirdCardNumber, /*set_as_expired_card=*/true);
  SaveLocalCard(kInvalidCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  views::View* card_list_view = GetCardListView();
  EXPECT_TRUE(card_list_view->GetVisible());
  ASSERT_EQ(2u, card_list_view->children().size());
  // Cards will be added to database in a reversed order.
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[0])
                ->GetCardIdentifierString(),
            second_card.CardNameAndLastFourDigits());
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[1])
                ->GetCardIdentifierString(),
            first_card.CardNameAndLastFourDigits());
}

// Ensures that rejecting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClickingCancelClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(autofill_metrics::MAIN_DIALOG_SHOWN, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalCardMigrationDialogUserInteraction"),
              ElementsAre(Bucket(
                  autofill_metrics::
                      LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED,
                  1)));
}

// Ensures that accepting the main migration dialog closes the dialog.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClickingSaveClosesDialog) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();

  // No dialog should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationMainDialogView());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationOrigin.UseOfLocalCard"),
      ElementsAre(Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN, 1),
                  Bucket(autofill_metrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1),
                  Bucket(autofill_metrics::MAIN_DIALOG_SHOWN, 1),
                  Bucket(autofill_metrics::MAIN_DIALOG_ACCEPTED, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalCardMigrationDialogUserInteraction"),
              ElementsAre(Bucket(
                  autofill_metrics::
                      LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED,
                  1)));
}

// Ensures local cards will be deleted from browser local storage after being
// successfully migrated.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_DeleteSuccessfullyMigratedCardsFromLocal) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button in the bubble.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button in the dialog.
  ClickOnSaveButtonAndWaitForMigrationResults();
  WaitForCardDeletion();

  EXPECT_EQ(nullptr,
            personal_data_->payments_data_manager().GetCreditCardByNumber(
                kFirstCardNumber));
  EXPECT_EQ(nullptr,
            personal_data_->payments_data_manager().GetCreditCardByNumber(
                kSecondCardNumber));
}

// Ensures that accepting the main migration dialog adds strikes.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_AcceptingDialogAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Save] button, should add and log strikes.
  ClickOnSaveButtonAndWaitForMigrationResults();

  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed, 1)));
}

// Ensures that rejecting the main migration dialog adds strikes.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_RejectingDialogAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());
  // Click the [Cancel] button, should add and log strikes.
  ClickOnCancelButton(GetLocalCardMigrationMainDialogView());

  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed, 1)));
}

// Ensures that rejecting the migration bubble adds strikes.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosingBubbleAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(Bucket(
          LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed, 1)));
}

// Ensures that rejecting the migration bubble repeatedly adds strikes every
// time, even for the same tab. Currently, it adds 3 strikes (out of 6), so this
// test can reliably test it being added twice.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosingBubbleAgainAddsLocalCardMigrationStrikes) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());
  // Do it again for the same tab.
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());

  // No bubble should be showing.
  EXPECT_EQ(nullptr, GetLocalCardMigrationOfferBubbleViews());
  // Metrics: Added 3 strikes each time, for totals of 3 then 6.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.StrikeDatabase.NthStrikeAdded.LocalCardMigration"),
      ElementsAre(
          Bucket(
              LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed,
              1),
          Bucket(
              LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed *
                  2,
              1)));
}

// Ensures that reshowing and closing bubble after previously closing it does
// not add strikes.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ReshowingBubbleDoesNotAddStrikes) {
  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());
  base::HistogramTester histogram_tester;
  ClickOnView(GetLocalCardMigrationIconView());

  // Clicking the icon should reshow the bubble.
  EXPECT_TRUE(
      FindViewInDialogById(DialogViewId::MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,
                           GetLocalCardMigrationOfferBubbleViews())
          ->GetVisible());

  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());

  // Metrics
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationBubbleOffer.FirstShow", 0);
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosedReason_BubbleAccepted) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalCardMigrationBubbleResult.FirstShow"),
              ElementsAre(Bucket(
                  autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED, 1)));
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosedReason_BubbleClosed) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  ClickOnDialogViewAndWait(close_button(),
                           GetLocalCardMigrationOfferBubbleViews());

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalCardMigrationBubbleResult.FirstShow"),
              ElementsAre(Bucket(
                  autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED, 1)));
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosedReason_BubbleNotInteracted) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetLocalCardMigrationOfferBubbleViews()->GetWidget());
  GetBrowser(0)->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleResult.FirstShow"),
      ElementsAre(Bucket(
          autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED, 1)));
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_ClosedReason_BubbleLostFocus) {
  base::HistogramTester histogram_tester;

  SaveLocalCard(kFirstCardNumber);
  SaveLocalCard(kSecondCardNumber);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetLocalCardMigrationOfferBubbleViews()->GetWidget());
  GetLocalCardMigrationOfferBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter.Wait();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.LocalCardMigrationBubbleResult.FirstShow"),
      ElementsAre(
          Bucket(autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS, 1)));
}

// Tests to ensure the card nickname is shown correctly in the local card
// migration dialog.
IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_CardIdentifierString) {
  base::HistogramTester histogram_tester;

  CreditCard first_card = SaveLocalCard(
      kFirstCardNumber, /*set_as_expired_card=*/false, /*set_nickname=*/true);
  CreditCard second_card = SaveLocalCard(
      kSecondCardNumber, /*set_as_expired_card=*/false, /*set_nickname=*/false);
  UseCardAndWaitForMigrationOffer(kFirstCardNumber);
  // Click the [Continue] button.
  ClickOnOkButton(GetLocalCardMigrationOfferBubbleViews());

  views::View* card_list_view = GetCardListView();
  EXPECT_TRUE(card_list_view->GetVisible());
  ASSERT_EQ(2u, card_list_view->children().size());
  // Cards will be added to database in a reversed order.
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[0])
                ->GetCardIdentifierString(),
            second_card.NetworkAndLastFourDigits());
  EXPECT_EQ(static_cast<MigratableCardView*>(card_list_view->children()[1])
                ->GetCardIdentifierString(),
            first_card.NicknameAndLastFourDigitsForTesting());
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_IconViewAccessibleName) {
  EXPECT_EQ(
      GetLocalCardMigrationIconView()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(IDS_TOOLTIP_MIGRATE_LOCAL_CARD));
  EXPECT_EQ(
      GetLocalCardMigrationIconView()->GetTextForTooltipAndAccessibleName(),
      l10n_util::GetStringUTF16(IDS_TOOLTIP_MIGRATE_LOCAL_CARD));
}

IN_PROC_BROWSER_TEST_F(
    LocalCardMigrationBrowserUiTest,
    // TODO(crbug.com/40649134): Flaky, but feature should soon be removed.
    DISABLED_InvokeUi_default) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/41422186):
// - Add more tests for feedback dialog.

}  // namespace autofill

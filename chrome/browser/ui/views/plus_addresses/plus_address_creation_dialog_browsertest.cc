// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {

namespace {

constexpr char kFakeEmailAddress[] = "alice@email.com";
constexpr char kFakePlusAddressManagementUrl[] = "https://manage.com";
constexpr char kFakeOauthScope[] = "https://foo.example";
constexpr char kPlusAddressModalEventHistogram[] =
    "Autofill.PlusAddresses.Modal.Events";

}  // namespace

class ScopedPlusAddressFeatureList {
 public:
  ScopedPlusAddressFeatureList() {
    features_.InitAndEnableFeatureWithParameters(
        kFeature,
        {// This must be overridden by calling Reinit(server_url). A dummy is
         // provided here to bypass any checks on this during service creation.
         {"server-url", {"https://override-me-please.example"}},
         {"oauth-scope", {kFakeOauthScope}},
         {"manage-url", {kFakePlusAddressManagementUrl}}});
  }

  void Reinit(const std::string& server_url) {
    CHECK(!server_url.empty());
    features_.Reset();
    // Don't enable the 'sync-with-server' param so that the dialog is the
    // only way to trigger requests to the server.
    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{"server-url", {server_url}},
                   {"oauth-scope", {kFakeOauthScope}},
                   {"manage-url", {kFakePlusAddressManagementUrl}}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

class PlusAddressCreationDialogTest : public DialogBrowserTest {
 public:
  PlusAddressCreationDialogTest()
      : override_profile_selections_(
            PlusAddressServiceFactory::GetInstance(),
            PlusAddressServiceFactory::CreateProfileSelections()) {}

  void SetUpInProcessBrowserTestFixture() override {
    unused_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Required to use IdentityTestEnvironmentAdaptor.
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

  void SetUpOnMainThread() override {
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    reserve_controllable_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/v1/profiles/reserve",
            /* relative_url_is_prefix= */ false);
    confirm_controllable_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/v1/profiles/create",
            /* relative_url_is_prefix= */ false);
    embedded_test_server()->StartAcceptingConnections();
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kFakeEmailAddress,
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // Reinit `feature_list_` here since the test server URL isn't ready at the
    // time we must first initialize the ScopedFeatureList.
    feature_list_.Reinit(embedded_test_server()->base_url().spec());
    DialogBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
  }

  enum RequestType { kReserve = 0, kConfirm = 1 };

  // This blocks twice:
  // - until the request of 'type' is made, and then fulfills it.
  // - until the UI has been updated to show the result of the request.
  void FulfillRequestAndBlockUntilUiShows(RequestType type, bool succeeds) {
    CHECK(type == RequestType::kReserve || type == RequestType::kConfirm);

    std::unique_ptr<net::test_server::ControllableHttpResponse> controllable =
        type == RequestType::kReserve
            ? std::move(reserve_controllable_response_)
            : std::move(confirm_controllable_response_);

    controllable->WaitForRequest();
    if (!succeeds) {
      controllable->Send(net::HttpStatusCode::HTTP_NOT_FOUND);
    } else {
      controllable->Send(
          net::HttpStatusCode::HTTP_OK, std::string("application/json"),
          type == RequestType::kReserve ? reserve_response : confirm_response);
    }
    controllable->Done();

    // Block until the result has been shown in the UI.
    PlusAddressCreationView* view =
        desktop_controller()->get_view_for_testing();
    view->WaitUntilResultShownForTesting();
  }

  void ShowUi(const std::string& name) override {
    PlusAddressCreationController* controller =
        PlusAddressCreationController::GetOrCreate(
            browser()->tab_strip_model()->GetActiveWebContents());
    controller->OfferCreation(facet, base::DoNothing());
  }

  PlusAddressCreationControllerDesktop* desktop_controller() {
    return PlusAddressCreationControllerDesktop::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription unused_subscription_;

  // Use two ControllableHttpResponses since each handles at most 1 request.
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      confirm_controllable_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      reserve_controllable_response_;

  base::HistogramTester histogram_tester;
  const url::Origin facet = url::Origin::Create(GURL("https://test.example"));
  const std::string fake_plus_address = "plus@plus.plus";
  const std::string reserve_response =
      plus_addresses::test::MakeCreationResponse(
          PlusProfile{.facet = facet.Serialize(),
                      .plus_address = fake_plus_address,
                      .is_confirmed = false});
  const std::string confirm_response =
      plus_addresses::test::MakeCreationResponse(
          PlusProfile{.facet = facet.Serialize(),
                      .plus_address = fake_plus_address,
                      .is_confirmed = true});

 protected:
  // Keep the order of these two scoped member variables.
  ScopedPlusAddressFeatureList feature_list_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
};

// Show a placeholder & disable the Confirm button while Reserve() is pending.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, InitialUi) {
  ShowUi("initial_dialog");
  // Make Reserve() load forever (note: there's actually a timeout after 5s).
  reserve_controllable_response_->WaitForRequest();

  EXPECT_TRUE(VerifyUi());
  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  EXPECT_FALSE(view->GetConfirmButtonEnabledForTesting());
  EXPECT_EQ(view->GetPlusAddressLabelTextForTesting(),
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER));
  DismissUi();
  reserve_controllable_response_->Done();

  // Verify expected metrics.
  histogram_tester.ExpectUniqueSample(
      kPlusAddressModalEventHistogram,
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown, 1);
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, CloseWebContents) {
  // First, show the UI normally.
  ShowUi(std::string());
  // Close the web contents, ensuring there aren't issues with teardown.
  // See crbug.com/1502957.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, DoubleInit) {
  // First, show the UI normally.
  ShowUi(std::string());
  FulfillRequestAndBlockUntilUiShows(RequestType::kReserve, /*succeeds=*/true);
  ASSERT_TRUE(VerifyUi());

  // Then, manually re-trigger the UI, while the modal is still open, passing
  // another callback. The second callback should not be run on confirmation in
  // the modal.
  base::test::TestFuture<const std::string&> future;
  PlusAddressCreationController* controller =
      PlusAddressCreationController::GetOrCreate(
          browser()->tab_strip_model()->GetActiveWebContents());
  controller->OfferCreation(url::Origin::Create(GURL("https://test.example")),
                            future.GetCallback());
  controller->OnConfirmed();
  FulfillRequestAndBlockUntilUiShows(RequestType::kConfirm, /*succeeds=*/true);
  EXPECT_FALSE(future.IsReady());
}

// If Reserve() request fails, show an error message.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, ReserveRequestFails) {
  ShowUi("reserve_fails");
  FulfillRequestAndBlockUntilUiShows(RequestType::kReserve, /*succeeds=*/false);
  ASSERT_TRUE(VerifyUi());

  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  EXPECT_FALSE(view->GetConfirmButtonEnabledForTesting());
  EXPECT_EQ(view->GetPlusAddressLabelTextForTesting(),
            l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_ERROR_MESSAGE));
  DismissUi();

  // Verify expected metrics.
  histogram_tester.ExpectUniqueSample(
      kPlusAddressModalEventHistogram,
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Reserve.ResponseCode",
      net::HttpStatusCode::HTTP_NOT_FOUND, 1);
}

// If Reserve() succeeds, enable the button and show the plus address.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, ReserveRequestSucceeds) {
  ShowUi("reserve_succeeds");
  FulfillRequestAndBlockUntilUiShows(RequestType::kReserve, /*succeeds=*/true);
  EXPECT_TRUE(VerifyUi());

  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  EXPECT_TRUE(view->GetConfirmButtonEnabledForTesting());
  EXPECT_EQ(view->GetPlusAddressLabelTextForTesting(),
            base::UTF8ToUTF16(fake_plus_address));
  DismissUi();

  // Verify expected metrics.
  histogram_tester.ExpectUniqueSample(
      kPlusAddressModalEventHistogram,
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Reserve.ResponseCode",
      net::HttpStatusCode::HTTP_OK, 1);
}

// If Confirm() request fails, disable the button & show an error message.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, ConfirmRequestFails) {
  ShowUi("confirm_fails");
  FulfillRequestAndBlockUntilUiShows(RequestType::kReserve, /*succeeds=*/true);
  ASSERT_TRUE(VerifyUi());

  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  view->ClickButtonForTesting(PlusAddressViewButtonType::kConfirm);
  FulfillRequestAndBlockUntilUiShows(RequestType::kConfirm, /*succeeds=*/false);
  EXPECT_FALSE(view->GetConfirmButtonEnabledForTesting());
  EXPECT_EQ(view->GetPlusAddressLabelTextForTesting(),
            l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_ERROR_MESSAGE));
  DismissUi();

  // Verify expected metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressModalEventHistogram),
      base::BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Reserve.ResponseCode",
      net::HttpStatusCode::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Create.ResponseCode",
      net::HttpStatusCode::HTTP_NOT_FOUND, 1);
}

// User presses confirm button, the request succeeds, and the dialog closes.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, ConfirmRequestSucceeds) {
  ShowUi("confirm_succeeds");
  FulfillRequestAndBlockUntilUiShows(RequestType::kReserve, /*succeeds=*/true);

  // Verify UI elements before button is pressed.
  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  ASSERT_TRUE(view->GetConfirmButtonEnabledForTesting());
  ASSERT_EQ(view->GetPlusAddressLabelTextForTesting(),
            base::UTF8ToUTF16(fake_plus_address));

  view->ClickButtonForTesting(PlusAddressViewButtonType::kConfirm);
  confirm_controllable_response_->WaitForRequest();

  // Verify the UI elements while the request is pending.
  VerifyUi();
  EXPECT_TRUE(view->ShowsLoadingIndicatorForTesting());

  // Unblock the network request.
  confirm_controllable_response_->Send(net::HttpStatusCode::HTTP_OK,
                                       std::string("application/json"),
                                       confirm_response);
  confirm_controllable_response_->Done();

  view->WaitUntilResultShownForTesting();

  // Verify expected metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressModalEventHistogram),
      base::BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Reserve.ResponseCode",
      net::HttpStatusCode::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PlusAddresses.NetworkRequest.Create.ResponseCode",
      net::HttpStatusCode::HTTP_OK, 1);
}

// User opens the dialog and closes it with the "x" button.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, DialogClosed) {
  ShowUi("dialog_closed");
  ASSERT_TRUE(VerifyUi());

  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  view->ClickButtonForTesting(PlusAddressViewButtonType::kClose);

  // Verify expected metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressModalEventHistogram),
      base::BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
}

// User opens the dialog and selects the "Cancel" button.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogTest, DialogCanceled) {
  ShowUi("dialog_canceled");
  ASSERT_TRUE(VerifyUi());

  PlusAddressCreationView* view = desktop_controller()->get_view_for_testing();
  view->ClickButtonForTesting(PlusAddressViewButtonType::kCancel);

  // Verify expected metrics.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressModalEventHistogram),
      base::BucketsAre(
          base::Bucket(PlusAddressMetrics::PlusAddressModalEvent::kModalShown,
                       1),
          base::Bucket(
              PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled, 1)));
}

}  // namespace plus_addresses

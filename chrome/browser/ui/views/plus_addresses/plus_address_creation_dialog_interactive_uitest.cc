// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/base_i18n_switches.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/mock_plus_address_setting_service.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using PlusAddressModalCompletionStatus =
    metrics::PlusAddressModalCompletionStatus;
using ::testing::Return;

constexpr char kFakeEmailAddress[] = "alice@email.example";
constexpr char16_t kFakeEmailAddressU16[] = u"alice@email.example";
constexpr char kFakeManagementUrl[] = "https://manage.example/";
constexpr char kFakeOauthScope[] = "https://foo.example";
constexpr char kFakeErrorReportUrl[] = "https://error-link.example/";
constexpr char kFakeLearnMoreUrl[] = "https://learn-more.stuff";
constexpr char kReservePath[] = "/v1/profiles/reserve";
constexpr char kConfirmPath[] = "/v1/profiles/create";

constexpr char kFakePlusAddress[] = "plus@plus.plus";
constexpr char16_t kFakePlusAddressU16[] = u"plus@plus.plus";
constexpr char kFakePlusAddressRefresh[] = "plus-refresh@plus.plus";
constexpr char16_t kFakePlusAddressRefreshU16[] = u"plus-refresh@plus.plus";

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";
// Histogram names.
constexpr std::string_view kPlusAddressModalEventHistogram =
    "PlusAddresses.Modal.Events";
constexpr std::string_view kPlusAddressModalWithNoticeEventHistogram =
    "PlusAddresses.ModalWithNotice.Events";

std::string FormatHistogramNameFor(PlusAddressNetworkRequestType type) {
  return base::ReplaceStringPlaceholders(
      "PlusAddresses.NetworkRequest.$1.ResponseCode",
      {metrics::PlusAddressNetworkRequestTypeToString(type)},
      /*offsets=*/nullptr);
}

std::string FormatDurationHistogramNameFor(
    metrics::PlusAddressModalCompletionStatus status,
    bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.ShownDuration"
                   : "PlusAddresses.Modal.$1.ShownDuration",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

std::string FormatRefreshHistogramNameFor(
    metrics::PlusAddressModalCompletionStatus status,
    bool notice_shown) {
  return base::ReplaceStringPlaceholders(
      notice_shown ? "PlusAddresses.ModalWithNotice.$1.Refreshes"
                   : "PlusAddresses.Modal.$1.Refreshes",
      {metrics::PlusAddressModalCompletionStatusToString(status)},
      /*offsets=*/nullptr);
}

void RegisterTestingFactories(content::BrowserContext* context) {
  // Required to use IdentityTestEnvironmentAdaptor.
  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  PlusAddressSettingServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockPlusAddressSettingService>();
      }));
}

class ScopedPlusAddressFeatureList {
 public:
  ScopedPlusAddressFeatureList() {
    features_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled, plus_addresses_enabled_params_);
  }

  void Reinit(const std::string& server_url,
              bool enable_onboarding = false,
              bool enable_updated_error_states = false) {
    CHECK(!server_url.empty());
    features_.Reset();
      // Don't enable the 'sync-with-server' param so that the dialog is the
      // only way to trigger requests to the server.
    base::FieldTrialParams plus_addresses_enabled_params_with_server =
        plus_addresses_enabled_params_;
    plus_addresses_enabled_params_with_server["server-url"] = server_url;
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kPlusAddressesEnabled,
         plus_addresses_enabled_params_with_server}};
    std::vector<base::test::FeatureRef> disabled_features;

    if (enable_onboarding) {
      enabled_features.push_back(
          {features::kPlusAddressUserOnboardingEnabled, {}});
    } else {
      disabled_features.push_back(features::kPlusAddressUserOnboardingEnabled);
    }

    if (enable_updated_error_states) {
      enabled_features.push_back(
          {features::kPlusAddressUpdatedErrorStatesInOnboardingModal, {}});
    } else {
      disabled_features.push_back(
          features::kPlusAddressUpdatedErrorStatesInOnboardingModal);
    }

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

 private:
  // Feature parameters used for the `kPlusAddressEnabled` feature.
  const base::FieldTrialParams plus_addresses_enabled_params_ = {
      {"server-url", {"https://override-me-please.example"}},
      {"oauth-scope", {kFakeOauthScope}},
      {"manage-url", {kFakeManagementUrl}},
      {"error-report-url", {kFakeErrorReportUrl}},
      {"learn-more", {kFakeLearnMoreUrl}}};
  base::test::ScopedFeatureList features_;
};

class PlusAddressCreationDialogInteractiveTest : public InteractiveBrowserTest {
 public:
  PlusAddressCreationDialogInteractiveTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    unused_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&RegisterTestingFactories));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kFakeEmailAddress,
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // Reinit `feature_list_` here since the test server URL isn't ready at the
    // time we must first initialize the ScopedFeatureList.
    feature_list_.Reinit(embedded_test_server()->base_url().spec());
    InteractiveBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  // Respond to request immediately with PlusProfile and OK status.
  // TODO: crbug.com/354865745 - Introduce a run loop here to simulate waiting
  // for network responses. This would allow reliably verifying loading status.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequestWithSuccess(
      const net::test_server::HttpRequest& request) {
    // Ignore unrecognized path.
    if (request.GetURL().path() != kReservePath &&
        request.GetURL().path() != kConfirmPath) {
      return nullptr;
    }

    bool is_refresh = [&]() {
      std::optional<base::Value> body = base::JSONReader::Read(request.content);
      if (!body || !body->is_dict()) {
        return false;
      }
      return body->GetIfDict()
          ->FindBool("refresh_email_address")
          .value_or(false);
    }();
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(PlusAddressResponseContent(
        request.GetURL().path() == kConfirmPath,
        is_refresh ? kFakePlusAddressRefresh : kFakePlusAddress));
    return http_response;
  }

 protected:
  std::string PlusAddressResponseContent(bool confirmed,
                                         std::string plus_address) {
    return plus_addresses::test::MakeCreationResponse(PlusProfile(
        /*profile_id=*/"123",
        affiliations::FacetURI::FromPotentiallyInvalidSpec(
            facet.GetURL().spec()),
        PlusAddress(std::move(plus_address)), confirmed));
  }

  InteractiveTestApi::StepBuilder ShowModal() {
    return Do([this]() {
      PlusAddressCreationController* controller =
          PlusAddressCreationController::GetOrCreate(
              browser()->tab_strip_model()->GetActiveWebContents());
      controller->OfferCreation(facet, future_.GetCallback());
      ASSERT_FALSE(future_.IsReady());
    });
  }

  InteractiveTestApi::StepBuilder CheckHistogramUniqueSample(
      std::string_view name,
      auto sample,
      int expected_count) {
    return Do([this, name, sample, expected_count]() {
      histogram_tester_.ExpectUniqueSample(name, sample, expected_count);
    });
  }

  // Check count of record for histogram disregarding the actual sample. Used
  // for `ModalShownDuration` where the actual time delta is not mocked.
  InteractiveTestApi::StepBuilder CheckHistogramTotalCount(
      std::string_view name,
      int expected_count) {
    return Do([this, name, expected_count]() {
      histogram_tester_.ExpectTotalCount(name, expected_count);
    });
  }

  InteractiveTestApi::StepBuilder CheckModalOutcomeHistograms(
      PlusAddressModalCompletionStatus status,
      int refresh_count,
      bool notice_shown) {
    return Do([=, this]() {
      histogram_tester_.ExpectTotalCount(
          FormatDurationHistogramNameFor(status, notice_shown), 1);
      histogram_tester_.ExpectUniqueSample(
          FormatRefreshHistogramNameFor(status, notice_shown), refresh_count,
          1);
    });
  }

  InteractiveTestApi::StepBuilder CheckModalEventHistogramBuckets(
      int shown,
      int confirmed,
      int canceled,
      bool notice_shown) {
    return Do([=, this]() {
      EXPECT_THAT(
          histogram_tester_.GetAllSamples(
              notice_shown ? kPlusAddressModalWithNoticeEventHistogram
                           : kPlusAddressModalEventHistogram),
          base::BucketsAre(
              base::Bucket(metrics::PlusAddressModalEvent::kModalShown, shown),
              base::Bucket(metrics::PlusAddressModalEvent::kModalConfirmed,
                           confirmed),
              base::Bucket(metrics::PlusAddressModalEvent::kModalCanceled,
                           canceled)));
    });
  }

  const url::Origin facet = url::Origin::Create(GURL("https://test.example"));
  base::CallbackListSubscription unused_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::HistogramTester histogram_tester_;
  base::test::TestFuture<const std::string&> future_;
  // Keep the order of these two scoped member variables.
  ScopedPlusAddressFeatureList feature_list_;
};

// An interactive UI test to exercise successful plus address user flow.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       ConfirmPlusAddressSucceeds) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  // RegisterRequestHandler must be called before server starts.
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      // Wait for modal to be shown and plus address reservation to complete.
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              &views::Label::GetText, kFakePlusAddressU16),
          // Ensure hidden elements are not present.
          EnsureNotPresent(
              PlusAddressCreationView::kPlusAddressErrorTextElementId),
          EnsureNotPresent(PlusAddressCreationView::kPlusAddressProgressBarId),
          // Simulate confirming plus address.
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          // Successful confirmation should close the modal.
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      // Flush remaining instructions to ensure that all metrics are
      // recorded.
      Check([&] {
        return future_.IsReady() && future_.Get() == kFakePlusAddress;
      }),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/0, /*notice_shown=*/false),
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kReserve),
          net::HttpStatusCode::HTTP_OK, 1),
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kCreate),
          net::HttpStatusCode::HTTP_OK, 1),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*refresh_count=*/0, /*notice_shown=*/false));
}

// An interactive UI test to exercise successful plus address user flow.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       ConfirmPlusAddressSucceedsAfterRefresh) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  // RegisterRequestHandler must be called before server starts.
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      // Wait for modal to be shown and plus address reservation to complete.
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              &views::Label::GetText, kFakePlusAddressU16),
          // Ensure hidden elements are not present.
          EnsureNotPresent(
              PlusAddressCreationView::kPlusAddressErrorTextElementId),
          EnsureNotPresent(PlusAddressCreationView::kPlusAddressProgressBarId),
          // Simulate refresh.
          PressButton(
              PlusAddressCreationView::kPlusAddressRefreshButtonElementId),
          WaitForViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              views::Label, Text, kFakePlusAddressRefreshU16),
          WaitForViewProperty(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
              views::View, Enabled, true),
          // Simulate confirming plus address.
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          // Successful confirmation should close the modal.
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      // Flush remaining instructions to ensure that all metrics are
      // recorded.
      Check([&] {
        return future_.IsReady() && future_.Get() == kFakePlusAddress;
      }),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/0, /*notice_shown=*/false),
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kReserve),
          net::HttpStatusCode::HTTP_OK, 2),
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kCreate),
          net::HttpStatusCode::HTTP_OK, 1),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*refresh_count=*/1, /*notice_shown=*/false));
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       ReserveFailsFromNoResponse_ShowsPlaceholderAndTimesOut) {
  // Simulate server not responding.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForShow(
          PlusAddressCreationView::kPlusAddressGenerationMessageElementId)),
      InSameContext(Steps(
          // Ensure that modal shows a placeholder & disables the confirm button
          // while `Reserve()` is pending.
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressGenerationMessageElementId,
              &views::Label::GetText,
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MODAL_GENERATION_TEMPORARY_LABEL_CONTENT)),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
              &views::View::GetEnabled, false),
          // UI should time out and eventually show error state.
          WaitForShow(PlusAddressCreationView::kPlusAddressErrorTextElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId),
          // Simulate canceling after reservation failure.
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      // Flush remaining instructions to ensure that all metrics are
      // recorded.

      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/0,
                                      /*canceled=*/1, /*notice_shown=*/false),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kReservePlusAddressError,
          /*refresh_count=*/0, /*notice_shown=*/false));
}

IN_PROC_BROWSER_TEST_F(
    PlusAddressCreationDialogInteractiveTest,
    ConfirmFailsFromNoResponse_ShowsProgressIndicatorAndTimesout) {
  // Simulate server not responding after successful plus address reservation.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() == kReservePath) {
          std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
              new net::test_server::BasicHttpResponse);
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("application/json");
          http_response->set_content(
              PlusAddressResponseContent(false, kFakePlusAddress));
          return http_response;
        }
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          // Ensure that progress indicator is shown while waiting for response
          // to confirm request.
          WaitForHide(PlusAddressCreationView::kPlusAddressProgressBarId, true),
          // UI should time out and eventually show error state.
          WaitForShow(PlusAddressCreationView::kPlusAddressErrorTextElementId),
          // Simulate canceling after confirm failure.
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      // Flush remaining instructions to ensure that all metrics are
      // recorded.
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kReserve),
          net::HttpStatusCode::HTTP_OK, 1),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/1, /*notice_shown=*/false),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*refresh_count=*/0, /*notice_shown=*/false));
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       ConfirmFails_ShowsErrorState) {
  // Confirm request fails with `HTTP_NOT_FOUND`.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
            new net::test_server::BasicHttpResponse);
        http_response->set_content_type("application/json");
        if (request.GetURL().path() == kReservePath) {
          http_response->set_code(net::HTTP_OK);
          http_response->set_content(
              PlusAddressResponseContent(false, kFakePlusAddress));

        } else {
          http_response->set_code(net::HTTP_NOT_FOUND);
        }
        return http_response;
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          WaitForShow(PlusAddressCreationView::kPlusAddressErrorTextElementId),
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressErrorTextElementId))),
      // Flush remaining instructions to ensure that all metrics are
      // recorded.

      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kReserve),
          net::HttpStatusCode::HTTP_OK, 1),
      CheckHistogramUniqueSample(
          FormatHistogramNameFor(PlusAddressNetworkRequestType::kCreate),
          net::HttpStatusCode::HTTP_NOT_FOUND, 1),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/1, /*notice_shown=*/false),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kConfirmPlusAddressError,
          /*refresh_count=*/0, /*notice_shown=*/false));
}

// Ensure modal handles error report link click when modal encounters error and
// open a new tab.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       ErrorReportLinkClicked_OpensNewTab) {
  // Simulate server not responding.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId);
  RunTestSequence(
      InstrumentNextTab(kTabElementId, AnyBrowser()), ShowModal(),
      InAnyContext(Steps(
          WaitForShow(PlusAddressCreationView::kPlusAddressErrorTextElementId),
          // EnsurePresent here is necessary to ensure error message is fully
          // rendered and link can be clicked.
          EnsurePresent(
              PlusAddressCreationView::kPlusAddressErrorTextElementId))),
      InAnyContext(
          WithElement(
              PlusAddressCreationView::kPlusAddressErrorTextElementId,
              [](ui::TrackedElement* el) {
                AsView<views::StyledLabel>(el)->ClickFirstLinkForTesting();
              })
              .SetMustRemainVisible(false)),
      InAnyContext(WaitForWebContentsNavigation(kTabElementId,
                                                GURL(kFakeErrorReportUrl))));
}

// User opens the dialog and presses the "Cancel" button.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       DialogCanceled) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForShow(
          PlusAddressCreationView::kPlusAddressCancelButtonElementId)),
      InSameContext(Steps(
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId))),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/0,
                                      /*canceled=*/1, /*notice_shown=*/false),
      CheckHistogramTotalCount(
          FormatDurationHistogramNameFor(
              PlusAddressModalCompletionStatus::kModalCanceled,
              /*notice_shown=*/false),
          1));
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest,
                       WebContentsClosed) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      // First, show the UI normally.
      ShowModal(),
      InAnyContext(Steps(
          WaitForShow(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId),
          Do([this]() {
            // Close the web contents, ensuring there aren't issues with
            // teardown. See crbug.com/1502957.
            EXPECT_EQ(1, browser()->tab_strip_model()->count());
            browser()->tab_strip_model()->GetActiveWebContents()->Close();
          }))),

      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/0,
                                      /*canceled=*/0, /*notice_shown=*/false));
}

IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogInteractiveTest, DoubleInit) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  base::test::TestFuture<const std::string&> double_init_future;
  RunTestSequence(
      // First, show the UI normally.
      ShowModal(),
      InAnyContext(Steps(
          WaitForShow(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId),
          Do([&]() {
            // Then, manually re-trigger the UI, while the modal is still open,
            // passing another callback. This should not create another modal.
            PlusAddressCreationController* controller =
                PlusAddressCreationController::GetOrCreate(
                    browser()->tab_strip_model()->GetActiveWebContents());
            controller->OfferCreation(facet, double_init_future.GetCallback());
          }),
          WaitForViewProperty(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
              views::View, Enabled, true),
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),

      // The second callback should not be run on confirmation on
      // the modal.
      Check([&] { return !double_init_future.IsReady(); }), Check([&] {
        return future_.IsReady() && future_.Get() == kFakePlusAddress;
      }),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/0, /*notice_shown=*/false));
}

// A test fixture that has the feature to show the onboarding notice turned on.
class PlusAddressCreationDialogWithNoticeTest
    : public PlusAddressCreationDialogInteractiveTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // Reinit `feature_list_` here since the test server URL isn't ready at the
    // time we must first initialize the ScopedFeatureList.
    feature_list_.Reinit(embedded_test_server()->base_url().spec(),
                         /*enable_onboarding=*/true);
    InteractiveBrowserTest::SetUp();
  }

 protected:
  MockPlusAddressSettingService& setting_service() {
    return static_cast<MockPlusAddressSettingService&>(
        *PlusAddressSettingServiceFactory::GetForBrowserContext(
            browser()->profile()));
  }
};

// Tests showing and cancelling the creation dialog for a user that has not yet
// accepted the notice.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogWithNoticeTest,
                       ShowNoticeAndCancel) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  ON_CALL(setting_service(), GetHasAcceptedNotice).WillByDefault(Return(false));
  EXPECT_CALL(setting_service(), SetHasAcceptedNotice).Times(0);

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              &views::Label::GetText, kFakePlusAddressU16),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressTitleElementId,
              &views::StyledLabel::GetText,
              l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE_NOTICE)),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId,
              &views::StyledLabel::GetText,
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MODAL_DESCRIPTION_NOTICE)),
          EnsurePresent(PlusAddressCreationView::kPlusAddressNoticeElementId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PlusAddressCreationView::kTopViewId,
                     /*screenshot_name=*/"notice",
                     /*baseline_cl=*/"5671781"),
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/0,
                                      /*canceled=*/1, /*notice_shown=*/true),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kModalCanceled,
          /*refresh_count=*/0, /*notice_shown=*/true));
}

// Tests that the notice is not shown if it has already been accepted.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogWithNoticeTest,
                       NoticePreviouslyAccepted) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  ON_CALL(setting_service(), GetHasAcceptedNotice).WillByDefault(Return(true));
  EXPECT_CALL(setting_service(), SetHasAcceptedNotice).Times(0);

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              &views::Label::GetText, kFakePlusAddressU16),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressTitleElementId,
              &views::StyledLabel::GetText,
              l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE)),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId,
              &views::StyledLabel::GetText,
              l10n_util::GetStringFUTF16(IDS_PLUS_ADDRESS_MODAL_DESCRIPTION,
                                         kFakeEmailAddressU16)),
          EnsureNotPresent(
              PlusAddressCreationView::kPlusAddressNoticeElementId),
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/0, /*notice_shown=*/false));
}

// Tests showing and accepting the creation dialog for a user that has not yet
// accepted the notice.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogWithNoticeTest,
                       ShowNoticeAndAccept) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  ON_CALL(setting_service(), GetHasAcceptedNotice).WillByDefault(Return(false));
  EXPECT_CALL(setting_service(), SetHasAcceptedNotice);

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId,
              &views::Label::GetText, kFakePlusAddressU16),
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))),
      // Flush remaining instructions to ensure that all metrics are recorded.
      Check([&] {
        return future_.IsReady() && future_.Get() == kFakePlusAddress;
      }),
      CheckModalEventHistogramBuckets(/*shown=*/1, /*confirmed=*/1,
                                      /*canceled=*/0, /*notice_shown=*/true),
      CheckModalOutcomeHistograms(
          PlusAddressModalCompletionStatus::kModalConfirmed,
          /*refresh_count=*/0, /*notice_shown=*/true));
}

// Tests that clicking the "learn more" link on the notice screen opens a new
// tab with the correct URL.
IN_PROC_BROWSER_TEST_F(PlusAddressCreationDialogWithNoticeTest,
                       ClickLearnMoreLink) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      base::Unretained(this)));
  embedded_test_server()->StartAcceptingConnections();

  ON_CALL(setting_service(), GetHasAcceptedNotice).WillByDefault(Return(false));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId);
  RunTestSequence(
      InstrumentNextTab(kTabElementId, AnyBrowser()), ShowModal(),
      InAnyContext(Steps(
          WaitForShow(PlusAddressCreationView::kPlusAddressNoticeElementId),
          // EnsurePresent here is necessary to ensure that the link can be
          // clicked.
          EnsurePresent(PlusAddressCreationView::kPlusAddressNoticeElementId))),
      InAnyContext(
          WithElement(
              PlusAddressCreationView::kPlusAddressNoticeElementId,
              [](ui::TrackedElement* el) {
                AsView<views::StyledLabel>(el)->ClickFirstLinkForTesting();
              })
              .SetMustRemainVisible(false)),
      InAnyContext(WaitForWebContentsNavigation(kTabElementId,
                                                GURL(kFakeLearnMoreUrl))));
}

class PlusAddressCreationDialogUiVariationsTestBase
    : public PlusAddressCreationDialogInteractiveTest,
      public testing::WithParamInterface<std::tuple</*force_dark_mode=*/bool,
                                                    /*is_rtl=*/bool>> {
 public:
  static std::string GetTextSuffix(
      const ::testing::TestParamInfo<std::tuple</*force_dark_mode=*/bool,
                                                /*is_rtl=*/bool>>& info) {
    return base::StrCat(
        {std::get<0>(info.param) ? "DarkMode_" : "LightMode_",
         std::get<1>(info.param) ? "RightToLeft" : "LeftToRight"});
  }

  bool IsDarkModeOn() { return std::get<0>(GetParam()); }

  bool IsBrowserLanguageRTL() { return std::get<1>(GetParam()); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn()) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
    if (IsBrowserLanguageRTL()) {
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
    }
  }
};

class PlusAddressCreationDialogUiVariationsTest
    : public PlusAddressCreationDialogUiVariationsTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // Reinit `feature_list_` here since the test server URL isn't ready at the
    // time we must first initialize the ScopedFeatureList.
    feature_list_.Reinit(embedded_test_server()->base_url().spec());
    InteractiveBrowserTest::SetUp();
  }
};

// An interactive UI test to exercise successful plus address user flow.
IN_PROC_BROWSER_TEST_P(PlusAddressCreationDialogUiVariationsTest,
                       ConfirmPlusAddressSucceeds) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PlusAddressCreationDialogInteractiveTest::HandleRequestWithSuccess,
      // It is safe to use base::Unretained(this) because the
      // embedded_test_server is shutdown as part of `TearDownOnMainThread`.
      base::Unretained(this)));
  // RegisterRequestHandler must be called before server starts.
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      // Wait for modal to be shown and plus address reservation to complete.
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          // Ensure hidden elements are not present.
          EnsureNotPresent(
              PlusAddressCreationView::kPlusAddressErrorTextElementId),
          EnsureNotPresent(PlusAddressCreationView::kPlusAddressProgressBarId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PlusAddressCreationView::kTopViewId,
                     /*screenshot_name=*/"confirm_succeeds",
                     /*baseline_cl=*/"5621234"),
          // Simulate confirming plus address.
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          // Successful confirmation should close the modal.
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))));
}

IN_PROC_BROWSER_TEST_P(PlusAddressCreationDialogUiVariationsTest,
                       ReserveFailsShowsPlaceholderAndTimesOut) {
  // Simulate server not responding.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForShow(
          PlusAddressCreationView::kPlusAddressGenerationMessageElementId)),
      InSameContext(Steps(
          // Ensure that modal shows a placeholder & disables the confirm button
          // while `Reserve()` is pending.
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressGenerationMessageElementId,
              &views::Label::GetText,
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MODAL_GENERATION_TEMPORARY_LABEL_CONTENT)),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
              &views::View::GetEnabled, false),
          // UI should time out and eventually show error state.
          WaitForShow(PlusAddressCreationView::kPlusAddressErrorTextElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PlusAddressCreationView::kTopViewId,
                     /*screenshot_name=*/"reserve_fails",
                     /*baseline_cl=*/"5621234"),
          // Simulate canceling after reservation failure.
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PlusAddressCreationDialogUiVariationsTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    PlusAddressCreationDialogUiVariationsTest::GetTextSuffix);

class PlusAddressCreationDialogUiVariationsOnboardingErrorStatesTest
    : public PlusAddressCreationDialogUiVariationsTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // Reinit `feature_list_` here since the test server URL isn't ready at the
    // time we must first initialize the ScopedFeatureList.
    feature_list_.Reinit(embedded_test_server()->base_url().spec(),
                         /*enable_onboarding=*/true,
                         /*enable_updated_error_states=*/true);
    InteractiveBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(
    PlusAddressCreationDialogUiVariationsOnboardingErrorStatesTest,
    ReserveTimeout) {
  // Simulate server not responding.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForShow(
          PlusAddressCreationView::kPlusAddressGenerationMessageElementId)),
      InSameContext(Steps(
          // Ensure that modal shows a placeholder & disables the confirm button
          // while `Reserve()` is pending.
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressGenerationMessageElementId,
              &views::Label::GetText,
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MODAL_GENERATION_TEMPORARY_LABEL_CONTENT)),
          CheckViewProperty(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
              &views::View::GetEnabled, false),
          // UI should time out and eventually show an error state.
          WaitForShow(PlusAddressCreationView::kPlusAddressReserveErrorId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressSuggestedEmailElementId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PlusAddressCreationView::kTopViewId,
                     /*screenshot_name=*/"reserve_timeout",
                     /*baseline_cl=*/"5860415"),
          // Simulate canceling after reservation failure.
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))));
}

IN_PROC_BROWSER_TEST_P(
    PlusAddressCreationDialogUiVariationsOnboardingErrorStatesTest,
    CreateTimeout) {
  // Simulate server not responding after successful plus address reservation.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() == kReservePath) {
          std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
              new net::test_server::BasicHttpResponse);
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("application/json");
          http_response->set_content(
              PlusAddressResponseContent(false, kFakePlusAddress));
          return http_response;
        }
        return std::make_unique<net::test_server::HungResponse>();
      }));
  embedded_test_server()->StartAcceptingConnections();

  RunTestSequence(
      ShowModal(),
      InAnyContext(WaitForViewProperty(
          PlusAddressCreationView::kPlusAddressConfirmButtonElementId,
          views::View, Enabled, true)),
      InSameContext(Steps(
          PressButton(
              PlusAddressCreationView::kPlusAddressConfirmButtonElementId),
          // Ensure that progress indicator is shown while waiting for response
          // to confirm request.
          WaitForHide(PlusAddressCreationView::kPlusAddressProgressBarId, true),
          // UI should time out and eventually show an error state.
          WaitForShow(PlusAddressCreationView::kPlusAddressCreateErrorId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(PlusAddressCreationView::kTopViewId,
                     /*screenshot_name=*/"create_timeout",
                     /*baseline_cl=*/"5860697"),
          // Simulate canceling after confirm failure.
          PressButton(
              PlusAddressCreationView::kPlusAddressCancelButtonElementId),
          WaitForHide(
              PlusAddressCreationView::kPlusAddressDescriptionTextElementId))));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PlusAddressCreationDialogUiVariationsOnboardingErrorStatesTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    PlusAddressCreationDialogUiVariationsOnboardingErrorStatesTest::
        GetTextSuffix);

}  // namespace
}  // namespace plus_addresses

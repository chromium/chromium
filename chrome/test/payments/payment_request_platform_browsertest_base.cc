// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"

#include <algorithm>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_app_install_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

namespace payments {

PaymentRequestPlatformBrowserTestBase::PaymentRequestPlatformBrowserTestBase() {
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  test_controller_.SetObserver(this);
}
PaymentRequestPlatformBrowserTestBase::
    ~PaymentRequestPlatformBrowserTestBase() = default;

void PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread() {
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  // Map all out-going DNS lookups to the local server.
  host_resolver()->AddRule("*", "127.0.0.1");

  // Setup the https server.
  https_server_->ServeFilesFromSourceDirectory("components/test/data/payments");
  ASSERT_TRUE(https_server_->Start());

  test_controller_.SetUpOnMainThread();
  PlatformBrowserTest::SetUpOnMainThread();
}

void PaymentRequestPlatformBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void PaymentRequestPlatformBrowserTestBase::
    TearDownInProcessBrowserTestFixture() {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void PaymentRequestPlatformBrowserTestBase::NavigateTo(
    const std::string& file_path) {
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     https_server_->GetURL(file_path)));
}

void PaymentRequestPlatformBrowserTestBase::NavigateTo(
    const std::string& hostname,
    const std::string& file_path) {
  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), https_server_->GetURL(hostname, file_path)));
}

void PaymentRequestPlatformBrowserTestBase::InstallPaymentApp(
    const std::string& hostname,
    const std::string& service_worker_filename,
    std::string* url_method_output) {
  *url_method_output = PaymentAppInstallUtil::InstallPaymentApp(
      *GetActiveWebContents(), *https_server(), hostname,
      service_worker_filename, PaymentAppInstallUtil::IconInstall::kWithIcon);
  ASSERT_FALSE(url_method_output->empty()) << "Failed to install payment app";
}

void PaymentRequestPlatformBrowserTestBase::ExpectBodyContains(
    const std::string& expected_string) {
  EXPECT_THAT(content::EvalJs(GetActiveWebContents(),
                              "window.document.body.textContent")
                  .ExtractString(),
              ::testing::HasSubstr(expected_string));
}

content::WebContents*
PaymentRequestPlatformBrowserTestBase::GetActiveWebContents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

void PaymentRequestPlatformBrowserTestBase::
    SetDownloaderAndIgnorePortInOriginComparisonForTestingInFrame(
        const std::vector<std::pair<const std::string&,
                                    net::EmbeddedTestServer*>>& payment_methods,
        content::RenderFrameHost* frame) {
  // Set up test manifest downloader that knows how to fake origin.
  content::BrowserContext* context =
      GetActiveWebContents()->GetBrowserContext();
  auto downloader = std::make_unique<TestDownloader>(
      GetCSPCheckerForTests(), context->GetDefaultStoragePartition()
                                   ->GetURLLoaderFactoryForBrowserProcess());
  for (const auto& method : payment_methods) {
    downloader->AddTestServerURL("https://" + method.first + "/",
                                 method.second->GetURL(method.first, "/"));
  }
  ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(frame)
      ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
          std::move(downloader));
}

void PaymentRequestPlatformBrowserTestBase::
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        const std::vector<
            std::pair<const std::string&, net::EmbeddedTestServer*>>&
            payment_methods) {
  SetDownloaderAndIgnorePortInOriginComparisonForTestingInFrame(
      payment_methods, GetActiveWebContents()->GetPrimaryMainFrame());
}

void PaymentRequestPlatformBrowserTestBase::OnCanMakePaymentCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kCanMakePaymentCalled);
}
void PaymentRequestPlatformBrowserTestBase::OnCanMakePaymentReturned() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kCanMakePaymentReturned);
}
void PaymentRequestPlatformBrowserTestBase::OnHasEnrolledInstrumentCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kHasEnrolledInstrumentCalled);
}
void PaymentRequestPlatformBrowserTestBase::OnHasEnrolledInstrumentReturned() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kHasEnrolledInstrumentReturned);
}
void PaymentRequestPlatformBrowserTestBase::OnConnectionTerminated() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kConnectionTerminated);
}
void PaymentRequestPlatformBrowserTestBase::OnNotSupportedError() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kNotSupportedError);
}
void PaymentRequestPlatformBrowserTestBase::OnAbortCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kAbortCalled);
}
void PaymentRequestPlatformBrowserTestBase::OnAppListReady() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kAppListReady);
}
void PaymentRequestPlatformBrowserTestBase::OnErrorDisplayed() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kErrorDisplayed);
}
void PaymentRequestPlatformBrowserTestBase::OnCompleteCalled() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kPaymentCompleted);
}

void PaymentRequestPlatformBrowserTestBase::OnUIDisplayed() {
  if (event_waiter_)
    event_waiter_->OnEvent(TestEvent::kUIDisplayed);
}

void PaymentRequestPlatformBrowserTestBase::ResetEventWaiterForSingleEvent(
    TestEvent event) {
  event_waiter_ = std::make_unique<EventWaiter>(
      std::list<TestEvent>{event}, true /* wait_for_single_event*/);
}

void PaymentRequestPlatformBrowserTestBase::ResetEventWaiterForEventSequence(
    std::list<TestEvent> event_sequence) {
  event_waiter_ = std::make_unique<EventWaiter>(
      std::move(event_sequence), false /* wait_for_single_event*/);
}

base::WeakPtr<CSPChecker>
PaymentRequestPlatformBrowserTestBase::GetCSPCheckerForTests() {
  return const_csp_checker_.GetWeakPtr();
}

void PaymentRequestPlatformBrowserTestBase::WaitForObservedEvent() {
  event_waiter_->Wait();
}

autofill::AutofillProfile
PaymentRequestPlatformBrowserTestBase::CreateAndAddAutofillProfile() {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  AddAutofillProfile(profile);
  return profile;
}
void PaymentRequestPlatformBrowserTestBase::AddAutofillProfile(
    const autofill::AutofillProfile& profile) {
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           profile);
}

autofill::CreditCard
PaymentRequestPlatformBrowserTestBase::CreateAndAddCreditCardForProfile(
    const autofill::AutofillProfile& profile) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);
  return card;
}

autofill::CreditCard
PaymentRequestPlatformBrowserTestBase::CreatCreditCardForProfile(
    const autofill::AutofillProfile& profile) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  return card;
}

void PaymentRequestPlatformBrowserTestBase::AddCreditCard(
    const autofill::CreditCard& card) {
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(), card);
}

std::string PaymentRequestPlatformBrowserTestBase::ClearPortNumber(
    const std::string& may_contain_method_url) {
  std::string before;
  std::string method;
  std::string after;
  GURL::Replacements port;
  port.ClearPort();
  return re2::RE2::FullMatch(
             may_contain_method_url,
             "(.*\"supportedMethods\":\")(https://.*)(\",\"total\".*)", &before,
             &method, &after)
             ? before +
                   url::Origin::Create(GURL(method).ReplaceComponents(port))
                       .Serialize() +
                   after
             : may_contain_method_url;
}

}  // namespace payments

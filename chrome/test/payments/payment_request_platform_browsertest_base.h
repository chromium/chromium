// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_PLATFORM_BROWSERTEST_BASE_H_
#define CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_PLATFORM_BROWSERTEST_BASE_H_

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "chrome/test/payments/personal_data_manager_test_util.h"
#include "chrome/test/payments/test_event_waiter.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

// Base class for any PaymentRequest test that is shared between Android and
// Desktop platforms.
class PaymentRequestPlatformBrowserTestBase
    : public PlatformBrowserTest,
      public PaymentRequestTestObserver {
 protected:
  PaymentRequestPlatformBrowserTestBase();
  ~PaymentRequestPlatformBrowserTestBase() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Navigates |window| to the URL to a server based on the given |file_path|
  // (relative to components/test/data/payments) using |hostname| or 127.0.0.1.
  void NavigateTo(const std::string& file_path);
  void NavigateTo(const std::string& hostname, const std::string& file_path);

  // Will expect that the expected string is present in output.
  void ExpectBodyContains(const std::string& expected_string);

  content::WebContents* GetActiveWebContents();

  // Set up test manifest downloader in |frame| that knows how to fake origin
  // for each 'payment method' and 'server' pair, (e.g. {"google.com",
  // &gpay_server_}). Must be called while on the page that will invoke the
  // PaymentRequest API, because the test manifest downloader is owned by
  // ServiceWorkerPaymentAppFinder, which in turn is owned by the |frame|.
  void SetDownloaderAndIgnorePortInOriginComparisonForTestingInFrame(
      const std::vector<std::pair<const std::string&,
                                  net::EmbeddedTestServer*>>& payment_methods,
      content::RenderFrameHost* frame);

  // Same as above, but uses the top-level frame of the web contents.
  void SetDownloaderAndIgnorePortInOriginComparisonForTesting(
      const std::vector<std::pair<const std::string&,
                                  net::EmbeddedTestServer*>>& payment_methods);

  // PaymentRequestTestObserver:
  void OnCanMakePaymentCalled() override;
  void OnCanMakePaymentReturned() override;
  void OnHasEnrolledInstrumentCalled() override;
  void OnHasEnrolledInstrumentReturned() override;
  void OnConnectionTerminated() override;
  void OnNotSupportedError() override;
  void OnAbortCalled() override;
  void OnAppListReady() override;
  void OnCompleteCalled() override;
  void OnMinimalUIReady() override;
  void OnUIDisplayed() override;

  // Resets the event waiter for a given |event| or |event_sequence|.
  void ResetEventWaiterForSingleEvent(TestEvent event);
  void ResetEventWaiterForEventSequence(std::list<TestEvent> event_sequence);

  // Wait for the event(s) passed to ResetEventWaiter*() to occur.
  void WaitForObservedEvent();

  autofill::AutofillProfile CreateAndAddAutofillProfile();
  void AddAutofillProfile(const autofill::AutofillProfile& profile);
  autofill::CreditCard CreateAndAddCreditCardForProfile(
      const autofill::AutofillProfile& profile);
  void AddCreditCard(const autofill::CreditCard& card);
  autofill::CreditCard CreatCreditCardForProfile(
      const autofill::AutofillProfile& profile);

  // Looks for the "supportedMethods" URL and removes its port number.
  std::string ClearPortNumber(const std::string& may_contain_method_url);

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  PaymentRequestTestController* test_controller() { return &test_controller_; }

 private:
  std::unique_ptr<EventWaiter> event_waiter_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  PaymentRequestTestController test_controller_;
};

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_PLATFORM_BROWSERTEST_BASE_H_

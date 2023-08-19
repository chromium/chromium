// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "components/payments/core/android_app_description.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

// Cross-platform test support for Android payment app communication. On Chrome
// OS, this connects to the Android subsystem.
//
// The test expectations are platform-specific. For example, on Chrome OS,
// expectations are setup in the mock Mojo IPC service.
class AndroidAppCommunicationTestSupport {
 public:
  // The object that initializes the ability to invoke Android apps in tests.
  // For example, on Chrome OS, this object creates a mock Mojo IPC connection
  // for the Android subsystem. This is meant to be placed on the stack, so it
  // can perform clean up in its destructor.
  class ScopedInitialization {
   public:
    ScopedInitialization() = default;
    virtual ~ScopedInitialization() = default;

    ScopedInitialization(const ScopedInitialization& other) = delete;
    ScopedInitialization& operator=(const ScopedInitialization& other) = delete;
  };

  // Defined in platform-specific files.
  static std::unique_ptr<AndroidAppCommunicationTestSupport> Create();

  virtual ~AndroidAppCommunicationTestSupport() = default;

  // Disallow copy and assign.
  AndroidAppCommunicationTestSupport(
      const AndroidAppCommunicationTestSupport& other) = delete;
  AndroidAppCommunicationTestSupport& operator=(
      const AndroidAppCommunicationTestSupport& other) = delete;

  // Whether this platform supports Android apps. Used in tests to determine the
  // expected outcome when attempting to query and invoke Android payment apps.
  virtual bool AreAndroidAppsSupportedOnThisPlatform() const = 0;

  // Creates the object that initializes the ability to invoke Android apps
  // in tests. Places this on the stack, so it can perform clean up in its
  // destructor. It's also useful to have a test case that does not invoke this
  // method, so the code path that handles inability to invoke Android apps is
  // tested.
  virtual std::unique_ptr<ScopedInitialization>
  CreateScopedInitialization() = 0;

  // Sets up the expectation that the test case will not query the list of
  // Android payment apps. This can happen, for example, when there is no
  // ScopedInitialization object in scope.
  virtual void ExpectNoListOfPaymentAppsQuery() = 0;

  // Sets up the expectation that the test case will not query an
  // IS_READY_TO_PAY service.
  virtual void ExpectNoIsReadyToPayQuery() = 0;

  // Sets up the expectation that the test case will not invoke a PAY activity.
  virtual void ExpectNoPaymentAppInvoke() = 0;

  // Sets up the expectation that the test case will query the list of Android
  // payment apps. When that happens, the given list of |apps| will be used for
  // the response.
  virtual void ExpectQueryListOfPaymentAppsAndRespond(
      std::vector<std::unique_ptr<AndroidAppDescription>> apps) = 0;

  // Sets up the expectation that the test case will query an IS_READY_TO_PAY
  // service. When that happens, the service will reply with the given
  // |is_ready_to_pay| answer.
  virtual void ExpectQueryIsReadyToPayAndRespond(bool is_ready_to_pay) = 0;

  // Sets up the expectation that the test case will invoke a PAY activity. When
  // that happens, the activity will reply with the given parameters.
  virtual void ExpectInvokePaymentAppAndRespond(
      bool is_activity_result_ok,
      const std::string& payment_method_identifier,
      const std::string& stringified_details) = 0;

  // Sets up the expectation that the test case will invoke a PAY activity, and
  // then subsequently abort that payment. The invoke callback will be called
  // when the payment is aborted with an error result, and then the abort will
  // be reported as successful.
  virtual void ExpectInvokeAndAbortPaymentApp() = 0;

  // Sets up the expectation that the test case will not abort any payment
  // flows.
  virtual void ExpectNoAbortPaymentApp() = 0;

  // Returns the browser context to use.
  virtual content::BrowserContext* context() = 0;

  // Returns the expected error string when the communication cannot connected
  // to the instance. For Lacros, this string will be different to specify that
  // the connection is failed at the Lacros to Ash connection.
  virtual std::string GetNoInstanceExpectedErrorString() = 0;

 protected:
  AndroidAppCommunicationTestSupport() = default;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_APP_COMMUNICATION_TEST_SUPPORT_H_

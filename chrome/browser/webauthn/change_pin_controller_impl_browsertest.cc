// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// These tests are disabled under MSAN. The enclave subprocess is written in
// Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/463858650).
#if !defined(MEMORY_SANITIZER)

namespace {

class ModelObserver : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit ModelObserver(AuthenticatorRequestDialogModel* model)
      : model_(model) {
    model_->observers.AddObserver(this);
  }

  ~ModelObserver() override {
    if (model_) {
      model_->observers.RemoveObserver(this);
      model_ = nullptr;
    }
  }

  void SetStepToObserve(AuthenticatorRequestDialogModel::Step step) {
    ASSERT_FALSE(run_loop_);
    step_ = step;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForStep() {
    if (model_->step() == step_) {
      run_loop_.reset();
      return;
    }
    ASSERT_TRUE(run_loop_);
    run_loop_->Run();
    // When waiting for `kClosed` the model is deleted at this point.
    if (step_ != AuthenticatorRequestDialogModel::Step::kClosed) {
      CHECK_EQ(step_, model_->step());
    }
    Reset();
  }

  void OnStepTransition() override {
    if (run_loop_ && step_ == model_->step()) {
      run_loop_->QuitWhenIdle();
    }
  }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

  void Reset() {
    step_ = AuthenticatorRequestDialogModel::Step::kNotStarted;
    run_loop_.reset();
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step step_ =
      AuthenticatorRequestDialogModel::Step::kNotStarted;
  std::unique_ptr<base::RunLoop> run_loop_;
};

std::string GetDeviceLog() {
  return device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"fido",
      /*max_level=*/device_event_log::LOG_LEVEL_EVENT, /*max_events=*/0);
}

}  // namespace

class ChangePinControllerBrowserTest : public EnclaveAuthenticatorTestBase {
 public:
  ChangePinControllerBrowserTest() = default;
  ~ChangePinControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    EnclaveAuthenticatorTestBase::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  }

  ChangePinControllerImpl* GetController() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChangePinControllerImpl::GetOrCreateForCurrentDocument(
        web_contents->GetPrimaryMainFrame());
  }
};

IN_PROC_BROWSER_TEST_F(ChangePinControllerBrowserTest, ChangePin) {
  SimulateSuccessfulGpmPinCreation("123456");

  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);

  ModelObserver observer(controller->model_for_testing());
  base::HistogramTester histogram_tester;

  base::test::TestFuture<bool> change_pin_future;
  controller->StartChangePin(change_pin_future.GetCallback());

  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset);
  observer.WaitForStep();

  controller->OnGPMReauthComplete("rapt");

  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMChangePin);
  observer.WaitForStep();

  controller->OnGPMPinEntered(u"654321");

  EXPECT_TRUE(change_pin_future.Get());

  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kFlowStartedFromSettings, 1);
  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kReauthCompleted, 1);
  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kNewPinEntered, 1);
  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kCompletedSuccessfully, 1);

  // Verify the PIN claim key was redacted.
  EXPECT_THAT(GetDeviceLog(),
              testing::HasSubstr("\"pin_claim_key\": \"[redacted]\""));
}

IN_PROC_BROWSER_TEST_F(ChangePinControllerBrowserTest,
                       IsChangePinFlowAvailable_NoPin) {
  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);
  base::test::TestFuture<bool> future;
  controller->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

IN_PROC_BROWSER_TEST_F(ChangePinControllerBrowserTest,
                       IsChangePinFlowAvailable_HasPin) {
  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);

  SimulateSuccessfulGpmPinCreation("123456");

  base::test::TestFuture<bool> future;
  controller->IsChangePinFlowAvailable(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_F(ChangePinControllerBrowserTest,
                       ChangePin_ReauthCancelled) {
  SimulateSuccessfulGpmPinCreation("123456");

  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);

  ModelObserver observer(controller->model_for_testing());
  base::HistogramTester histogram_tester;

  base::test::TestFuture<bool> change_pin_future;
  controller->StartChangePin(change_pin_future.GetCallback());

  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset);
  observer.WaitForStep();

  controller->OnGPMRecoverSecurityDomainClosed();

  // The flow should have failed because the reauth was cancelled.
  EXPECT_FALSE(change_pin_future.Get());

  EXPECT_EQ(controller->model_for_testing()->step(),
            AuthenticatorRequestDialogModel::Step::kNotStarted);

  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kFlowStartedFromSettings, 1);
  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.ChangePinEventsV2",
      EnclaveChangePinEvent::kReauthCancelled, 1);
}

// Regression test for https://crbug.com/466163490.
IN_PROC_BROWSER_TEST_F(ChangePinControllerBrowserTest,
                       ChangePin_EnclaveManagerBecomesUnregistered) {
  SimulateSuccessfulGpmPinCreation("123456");
  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);

  // Simulating the concurrent unregistration of Enclave Manager.
  enclave_manager().ClearRegistrationForTesting();

  base::test::TestFuture<bool> change_pin_future;
  controller->StartChangePin(change_pin_future.GetCallback());
  EXPECT_FALSE(change_pin_future.Get());
}

// Regression test for https://crbug.com/466163490.
IN_PROC_BROWSER_TEST_F(
    ChangePinControllerBrowserTest,
    ChangePin_EnclaveManagerBecomesUnregistered_AfterReauth) {
  SimulateSuccessfulGpmPinCreation("123456");
  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);
  ModelObserver observer(controller->model_for_testing());

  controller->StartChangePin(base::DoNothing());
  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset);
  observer.WaitForStep();

  // Simulating the concurrent unregistration of Enclave Manager.
  enclave_manager().ClearRegistrationForTesting();

  controller->OnGPMReauthComplete("rapt");
  observer.SetStepToObserve(AuthenticatorRequestDialogModel::Step::kGPMError);
  observer.WaitForStep();
}

// Regression test for https://crbug.com/466163490.
IN_PROC_BROWSER_TEST_F(
    ChangePinControllerBrowserTest,
    ChangePin_EnclaveManagerBecomesUnregistered_AfterEnteringPin) {
  SimulateSuccessfulGpmPinCreation("123456");
  ChangePinControllerImpl* controller = GetController();
  ASSERT_TRUE(controller);
  ModelObserver observer(controller->model_for_testing());

  controller->StartChangePin(base::DoNothing());
  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset);
  observer.WaitForStep();
  controller->OnGPMReauthComplete("rapt");
  observer.SetStepToObserve(
      AuthenticatorRequestDialogModel::Step::kGPMChangePin);
  observer.WaitForStep();

  // Simulating the concurrent unregistration of Enclave Manager.
  enclave_manager().ClearRegistrationForTesting();

  observer.SetStepToObserve(AuthenticatorRequestDialogModel::Step::kGPMError);
  controller->OnGPMPinEntered(u"123456");
  observer.WaitForStep();
}

#endif  // !defined(MEMORY_SANITIZER)

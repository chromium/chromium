// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// Tests that WebXR sessions can be created when the "Allow" button is pressed.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentAllowCreatesSession) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("generic_webxr_page"));

  t->EnterSessionWithUserGestureOrFail();

  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that a session is not created if the user explicitly clicks the
// cancel button.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentCancelFailsSessionCreation) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickCancelButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_consent"));
  t->EnterSessionWithUserGesture();
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null");
  t->RunJavaScriptOrFail("verifySessionConsentError(sessionTypes.IMMERSIVE)");
  t->AssertNoJavaScriptErrors();

  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that a session is not created if the user explicitly closes the
// dialog.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentCloseFailsSessionCreation) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kCloseDialog);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_consent"));
  t->EnterSessionWithUserGesture();
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null");
  t->RunJavaScriptOrFail("verifySessionConsentError(sessionTypes.IMMERSIVE)");
  t->AssertNoJavaScriptErrors();

  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that requesting a session with the same required level of consent
// without a page reload, only prompts once.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentPersistsSameLevel) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("generic_webxr_page"));

  t->EnterSessionWithUserGestureOrFail();
  t->EndSessionOrFail();

  // Since the consent from the earlier prompt should be persisted, requesting
  // an XR session a second time should not prompt the user, but should create
  // a valid session.
  t->EnterSessionWithUserGestureOrFail();

  // Validate that the consent prompt has only been shown once since the start
  // of this test.
  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 1u)
      << "Consent Dialog should have only been shown once";
}

// Verify that inline with no session parameters doesn't prompt for consent.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentNotNeededForInline) {
  // This ensures that we have a fresh consent manager with a new count.
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_consent"));
  t->RunJavaScriptOrFail("requestMagicWindowSession()");

  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // Validate that the consent prompt has not been shown since this test began.
  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 0u)
      << "Consent Dialog should not have been shown";
}

// Verify that if a higher level of consent is granted (e.g. bounded), that the
// lower level does not re-prompt for consent.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentPersistsLowerLevel) {
  // This ensures that we have a fresh consent manager with a new count.
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_consent"));

  // Setup to ensure that we request a session that requires a high level of
  // consent.
  t->RunJavaScriptOrFail("setupImmersiveSessionToRequestBounded()");

  t->EnterSessionWithUserGestureOrFail();
  t->EndSessionOrFail();

  // Since the (higher) consent from the earlier prompt should be persisted,
  // requesting an XR session a second time, with a lower level of permissions
  // expected should not prompt the user, but should create a valid session.
  t->RunJavaScriptOrFail("setupImmersiveSessionToRequestHeight()");
  t->EnterSessionWithUserGestureOrFail();

  // Validate that the consent prompt has only been shown once since the start
  // of this test.
  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 1u)
      << "Consent Dialog should have only been shown once";
}

// Tests that if a higher level of consent than was previously granted is needed
// that the user is re-prompted.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentRepromptsHigherLevel) {
  // This ensures that we have a fresh consent manager with a new count.
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_webxr_consent"));

  // First request an immersive session with a medium level of consent.
  t->RunJavaScriptOrFail("setupImmersiveSessionToRequestHeight()");
  t->EnterSessionWithUserGestureOrFail();
  t->EndSessionOrFail();

  // Now set up to request a session with a higher level of consent than
  // previously granted.  This should cause the prompt to appear again, and then
  // a new session be created.
  t->RunJavaScriptOrFail("setupImmersiveSessionToRequestBounded()");
  t->EnterSessionWithUserGestureOrFail();

  // Validate that both request sessions showed a consent prompt.
  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 2u)
      << "Consent Dialog should have been shown twice";
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestConsentRepromptsAfterReload) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("generic_webxr_page"));

  t->EnterSessionWithUserGestureOrFail();
  t->EndSessionOrFail();

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("generic_webxr_page"));

  t->EnterSessionWithUserGestureOrFail();

  // Validate that both request sessions showed a consent prompt.
  ASSERT_EQ(t->fake_consent_manager_->ShownCount(), 2u)
      << "Consent Dialog should have been shown twice";
}

}  // namespace vr

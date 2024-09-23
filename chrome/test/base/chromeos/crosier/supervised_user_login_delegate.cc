// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/supervised_user_login_delegate.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/test_accounts.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/chromeos/crosier/gaia_host_util.h"
#endif

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Constants for use in test scripts.
const char kEmailRegEx[] =
    "new RegExp(button.getAttribute('data-email').replace(/\\*/g, '.'));";
const char kGetEmailElementList[] = "document.querySelectorAll('[data-email]')";
const char kGetPasswordElement[] =
    "document.querySelector('input[type=password]')";
const char kAssertPasswordElement[] =
    "!!document.querySelector('input[type=password]')";

// Waits for parent email to appear in the GAIA frame, accounting for
// obfuscation that may be done by the GAIA server.
void WaitForParentEmail(std::string& parent_email) {
  crosier::GaiaFrameJS()
      .CreateWaiter(base::StrCat(
          {"[...", kGetEmailElementList, "].some(", "(button) => {",
           "const email = '", parent_email, "';", "const regex = ", kEmailRegEx,
           "return regex.test(email);})"}))
      ->Wait();
}

// Clicks on the parent email, accounting for obfuscation that may be done by
// the GAIA server.
void ClickParentEmail(std::string& parent_email) {
  crosier::GaiaFrameJS().Evaluate(base::StrCat(
      {"[...", kGetEmailElementList, "].some(", "(button) => {",
       "const email = '", parent_email, "';", "const regex = ", kEmailRegEx,
       "const match = regex.test(email);", "if (match) {button.click();}",
       "return match;})"}));
}

void TypePassword(std::string& password) {
  crosier::GaiaFrameJS()
      .CreateWaiter(base::StrCat({kAssertPasswordElement, " && ",
                                  kGetPasswordElement, ".value === ''"}))
      ->Wait();
  crosier::GaiaFrameJS().Evaluate(
      base::StrCat({kGetPasswordElement, ".value=\"", password, "\""}));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace

SupervisedUserLoginDelegate::SupervisedUserLoginDelegate() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  {
    // Allows reading account pool json file.
    base::ScopedAllowBlockingForTesting allow_blocking;

    test_data_ = crosier::GetFamilyTestData();
  }
#endif
}

void SupervisedUserLoginDelegate::DoCustomGaiaLogin(std::string& username) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Skip to login screen.
  crosier::SkipToGaiaScreenAndWait();

  std::string child_email;
  std::string child_password;

  std::string parent_email;
  std::string parent_password;

  switch (user_type_) {
    case (SupervisedUserType::kUnicorn):
      child_email = test_data_.unicorn.email;
      child_password = test_data_.unicorn.password;
      break;
    case (SupervisedUserType::kGeller):
      child_email = test_data_.geller.email;
      child_password = test_data_.geller.password;
      break;
    case (SupervisedUserType::kGriffin):
      child_email = test_data_.griffin.email;
      child_password = test_data_.griffin.password;
      break;
  }

  username = child_email;
  parent_email = test_data_.parent.email;
  parent_password = test_data_.parent.password;

  // Enter child email.
  crosier::GaiaFrameJS()
      .CreateWaiter("!!document.querySelector('#identifierId')")
      ->Wait();
  crosier::GaiaFrameJS().Evaluate(base::StrCat(
      {"document.querySelector('#identifierId').value=\"", child_email, "\""}));
  ash::test::OobeJS().Evaluate("Oobe.clickGaiaPrimaryButtonForTesting()");

  // Enter child password
  TypePassword(child_password);
  ash::test::OobeJS().Evaluate("Oobe.clickGaiaPrimaryButtonForTesting()");

  // Choose parent email for authentication
  WaitForParentEmail(parent_email);
  ClickParentEmail(parent_email);

  // Enter parent password.
  crosier::GaiaFrameJS().CreateWaiter(kAssertPasswordElement)->Wait();
  TypePassword(parent_password);
  ash::test::OobeJS().Evaluate("Oobe.clickGaiaPrimaryButtonForTesting()");

  // Agree to ToS for supervised account.
  crosier::GaiaFrameJS()
      .CreateWaiter(
          "document.querySelector('#headingText') && "
          "document.querySelector('#headingText').textContent === 'Privacy and "
          "Terms'")
      ->Wait();
  crosier::GaiaFrameJS().Evaluate(R"(
      const button = document.querySelector('button');
      if (button.textContent === 'I agree') {
        button.click();
      }
  )");

  // Skip post login steps.
  ash::WizardController::default_controller()->SkipPostLoginScreensForTesting();
#else
  CHECK(false) << "Gaia login is only supported in branded build.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

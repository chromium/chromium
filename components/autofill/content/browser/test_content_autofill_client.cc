// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/test_content_autofill_client.h"

#include <memory>

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"

namespace autofill {

TestContentAutofillClient::TestContentAutofillClient(
    content::WebContents* web_contents)
    : TestAutofillClientTemplate<
          ContentAutofillClient>::TestAutofillClientTemplate(web_contents),
      otp_field_detector_(this) {}

TestContentAutofillClient::~TestContentAutofillClient() = default;

std::unique_ptr<AutofillManager> TestContentAutofillClient::CreateManager(
    base::PassKey<ContentAutofillDriver> pass_key,
    ContentAutofillDriver& driver) {
  return std::make_unique<BrowserAutofillManager>(&driver);
}

credential_management::ContentCredentialManager*
TestContentAutofillClient::GetContentCredentialManager() {
  return nullptr;
}

OtpFieldDetector* TestContentAutofillClient::GetOtpFieldDetector() {
  return &otp_field_detector_;
}

}  // namespace autofill

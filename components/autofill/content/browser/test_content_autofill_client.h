// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// A variant of TestAutofillClient that can be associated with a
// content::WebContents.
//
// Consider using TestAutofillClientInjector to inject the client correctly,
// especially in browser tests.
class TestContentAutofillClient
    : public TestAutofillClientTemplate<ContentAutofillClient> {
 public:
  explicit TestContentAutofillClient(content::WebContents* web_contents);
  ~TestContentAutofillClient() override;

  // ContentAutofillClient:
  std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) override;
  credential_management::ContentCredentialManager* GetContentCredentialManager()
      override;
  OtpFieldDetector* GetOtpFieldDetector() override;

 private:
  OtpFieldDetector otp_field_detector_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_

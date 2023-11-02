// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"

#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

scoped_refptr<PasswordProtectionRequestContent> CreateDummyRequest(
    content::WebContents* web_contents) {
  std::unique_ptr<safe_browsing::MockPasswordProtectionService>
      password_protection_service =
          std::make_unique<safe_browsing::MockPasswordProtectionService>();
  scoped_refptr<PasswordProtectionRequestContent> request =
      base::MakeRefCounted<PasswordProtectionRequestContent>(
          web_contents, GURL(), GURL(), GURL(),
          web_contents->GetContentsMimeType(), "",
          PasswordType::PASSWORD_TYPE_UNKNOWN,
          std::vector<password_manager::MatchingReusedCredential>(),
          LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true,
          password_protection_service.get(), 0);
  request->set_request_outcome(RequestOutcome::UNKNOWN);
  return request;
}

}  // namespace safe_browsing

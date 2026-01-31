// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_otp_phish_guard_delegate.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#endif

namespace autofill {

ChromeOtpPhishGuardDelegate::ChromeOtpPhishGuardDelegate(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

ChromeOtpPhishGuardDelegate::~ChromeOtpPhishGuardDelegate() = default;

void ChromeOtpPhishGuardDelegate::StartOtpPhishGuardCheck(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (auto* client =
          ChromePasswordManagerClient::FromWebContents(&web_contents_.get())) {
    if (safe_browsing::PasswordProtectionService* pps =
            client->GetPasswordProtectionService()) {
      pps->MaybeStartOtpPhishingRequest(&web_contents_.get(), url,
                                        std::move(callback));
      return;
    }
  }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

  std::move(callback).Run(false);
}

}  // namespace autofill

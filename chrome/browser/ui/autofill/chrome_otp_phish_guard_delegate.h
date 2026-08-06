// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/safe_browsing/buildflags.h"

namespace content {
class WebContents;
}

namespace autofill {

class OtpFillingSafeBrowsingCheckerClient;

class ChromeOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  explicit ChromeOtpPhishGuardDelegate(content::WebContents* web_contents);
  ~ChromeOtpPhishGuardDelegate() override;

  // OtpPhishGuardDelegate:
  void StartOtpPhishGuardCheck(
      const GURL& main_frame_url,
      const GURL& frame_to_fill_url,
      base::OnceCallback<void(bool)> callback) override;

 private:
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  void OnSafeBrowsingCheckComplete(const GURL& main_frame_url,
                                   base::OnceCallback<void(bool)> callback,
                                   bool is_malicious);
#endif

  const raw_ref<content::WebContents> web_contents_;
  std::unique_ptr<OtpFillingSafeBrowsingCheckerClient>
      safe_browsing_checker_client_;

  base::WeakPtrFactory<ChromeOtpPhishGuardDelegate> weak_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_

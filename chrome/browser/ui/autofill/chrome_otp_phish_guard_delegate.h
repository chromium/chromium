// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"

namespace content {
class WebContents;
}

namespace autofill {

class ChromeOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  explicit ChromeOtpPhishGuardDelegate(content::WebContents* web_contents);
  ~ChromeOtpPhishGuardDelegate() override;

  // OtpPhishGuardDelegate:
  void StartOtpPhishGuardCheck(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) override;

 private:
  const raw_ref<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_OTP_PHISH_GUARD_DELEGATE_H_

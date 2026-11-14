// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_FLOW_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_FLOW_H_

namespace one_time_tokens {

// The product flow that requests a Gmail OTP. Selects the histogram variant of
// the sender domain match metrics.
enum class GmailOtpFlow {
  // Autofill offers the OTP as a suggestion on the OTP form.
  kAutofill,
  // An agent task fills the OTP on the user's behalf.
  kActor,
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_GMAIL_OTP_FLOW_H_

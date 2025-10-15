// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_H_

#include "base/functional/callback.h"

namespace autofill {

// The OtpManager helps the BrowserAutofillManager filling OTPs into webforms.
//
// One instance per frame, owned by the BrowserAutofillManager.
class OtpManager {
 public:
  using GetOtpSuggestionsCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  OtpManager() = default;
  virtual ~OtpManager() = default;

  // Invokes `callback` with the OTP value suggestions. This function returns
  // previously received OTPs or waits for a pending OTP retrieval to finish
  // before invoking the callback. This is the UI-facing function invoked by
  // autofill UI. Concrete implementations of `OtpManager` should retrieve OTPs
  // already when it becomes clear that the website asks for an OTP. It should
  // not wait until the user focuses the field.
  //
  // TODO(crbug.com/415273270): This function delays the `callback` until
  // an OTP arrived. That's bad for cooperative behavior. Instead the callback
  // should return immediately when no OTPs are cached but the Autofill UI
  // should be updated once OTPs arrive.
  virtual void GetOtpSuggestions(GetOtpSuggestionsCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_H_

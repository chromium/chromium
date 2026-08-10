// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/web/web_local_frame_observer.h"

namespace blink {
class WebFormElement;
}  // namespace blink

namespace autofill {

class AutofillAgent;

// Handles storing email verification tokens received from the browser process
// and injecting them into the form when the form is submitted.
class EmailVerificationHandler : public blink::WebLocalFrameObserver {
 public:
  explicit EmailVerificationHandler(AutofillAgent* agent);
  EmailVerificationHandler(const EmailVerificationHandler&) = delete;
  EmailVerificationHandler& operator=(const EmailVerificationHandler&) = delete;
  ~EmailVerificationHandler() override;

  void StoreEmailVerificationToken(FieldRendererId email_field_id,
                                   const std::string& email,
                                   FieldRendererId token_field_id,
                                   const std::string& token);

  // blink::WebLocalFrameObserver:
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void OnFrameDetached() override {}

  void Reset() { email_verification_tokens_.clear(); }

 private:
  struct TokenInfo {
    std::string token;
    FieldRendererId email_field_id;
    std::string email;
  };

  const raw_ptr<AutofillAgent> agent_;
  base::flat_map<FieldRendererId, TokenInfo> email_verification_tokens_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_

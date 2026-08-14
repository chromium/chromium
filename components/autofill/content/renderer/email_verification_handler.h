// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/web/web_local_frame_observer.h"

namespace blink {
class WebFormElement;
}  // namespace blink

namespace autofill {

class AutofillAgent;

// Handles querying nonces and storing email verification tokens received from
// the browser process and injecting them into the form when the form is
// submitted.
class EmailVerificationHandler : public blink::WebLocalFrameObserver {
 public:
  using GetNonceForEmailVerificationCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;

  explicit EmailVerificationHandler(AutofillAgent* agent);
  EmailVerificationHandler(const EmailVerificationHandler&) = delete;
  EmailVerificationHandler& operator=(const EmailVerificationHandler&) = delete;
  ~EmailVerificationHandler() override;

  void GetNonceForEmailVerification(
      FieldRendererId email_field_id,
      GetNonceForEmailVerificationCallback callback);

  void StoreEmailVerificationToken(FieldRendererId email_field_id,
                                   const std::string& email,
                                   const std::string& token);

  // blink::WebLocalFrameObserver:
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void OnFrameDetached() override {}

  void Reset() { email_verification_tokens_.clear(); }

 private:
  struct TokenInfo {
    std::string token;
    FieldRendererId token_field_id;
    std::string email;
  };

  const raw_ptr<AutofillAgent> agent_;
  // Maps the FieldRendererId of the verified email input field to its
  // stored verification token and metadata (TokenInfo).
  base::flat_map<FieldRendererId, TokenInfo> email_verification_tokens_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_EMAIL_VERIFICATION_HANDLER_H_

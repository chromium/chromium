// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/email_verification_handler.h"

#include <optional>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

EmailVerificationHandler::EmailVerificationHandler(AutofillAgent* agent)
    : blink::WebLocalFrameObserver(agent->unsafe_render_frame()->GetWebFrame()),
      agent_(agent) {}

EmailVerificationHandler::~EmailVerificationHandler() = default;

void EmailVerificationHandler::StoreEmailVerificationToken(
    FieldRendererId email_field_id,
    const std::string& email,
    FieldRendererId token_field_id,
    const std::string& token) {
  email_verification_tokens_[token_field_id] = TokenInfo{
      .token = token, .email_field_id = email_field_id, .email = email};
}

void EmailVerificationHandler::WillSendSubmitEvent(
    const blink::WebFormElement& form) {
  if (email_verification_tokens_.empty() || form.IsNull()) {
    return;
  }

  for (const auto& [field_id, info] : email_verification_tokens_) {
    blink::WebFormControlElement element =
        form_util::GetFormControlByRendererId(field_id);
    if (element && element.GetOwningFormForAutofill() == form) {
      // To prevent sharing an Email Verification Token (EVT) generated for a
      // different email address (e.g., if the user edited the email field,
      // cleared it, or selected a different email address after the token was
      // sent to the renderer), verify that the email field's current value
      // still matches the email address used during verification.
      blink::WebFormControlElement email_element =
          form_util::GetFormControlByRendererId(info.email_field_id);
      if (email_element) {
        std::u16string current_email = email_element.Value().Utf16();
        std::u16string original_email = base::UTF8ToUTF16(info.email);
        if (base::i18n::FoldCase(
                base::TrimWhitespace(current_email, base::TRIM_ALL)) !=
            base::i18n::FoldCase(
                base::TrimWhitespace(original_email, base::TRIM_ALL))) {
          continue;
        }
      } else {
        continue;
      }

      element.SetValue(blink::WebString::FromUtf8(info.token));

      if (auto* driver = agent_->unsafe_autofill_driver()) {
        if (std::optional<FormData> form_data = form_util::ExtractFormData(
                form.GetDocument(), form, agent_->field_data_manager(),
                agent_->GetCallTimerState(
                    CallTimerState::CallSite::
                        kFormWithEmailVerificationTokenSubmitted),
                agent_->button_titles_cache())) {
          driver->FormWithEmailVerificationTokenSubmitted(*form_data, field_id);
        }
      }
      return;
    }
  }
}

}  // namespace autofill

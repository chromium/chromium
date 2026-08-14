// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/email_verification_handler.h"

#include <optional>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

namespace {

// Finds the hidden email verification token field associated with the given
// email field.
// It looks for a hidden input field within the same form that has the
// "email-verification-token" autocomplete attribute.
blink::WebFormControlElement FindTokenFieldForEmailField(
    FieldRendererId email_field_id) {
  // Get the email element and its owning form.
  blink::WebFormControlElement email_element =
      form_util::GetFormControlByRendererId(email_field_id);
  if (!email_element) {
    return blink::WebFormControlElement();
  }
  blink::WebFormElement form_element = email_element.GetOwningFormForAutofill();

  // Scan all control elements in the form to find the hidden token field.
  for (const auto& control_element : form_util::GetOwnedFormControls(
           email_element.GetDocument(), form_element)) {
    // We are only interested in hidden input fields.
    if (control_element.FormControlTypeForAutofill() !=
        blink::mojom::FormControlType::kInputHidden) {
      continue;
    }
    // Check if the field has the "email-verification-token" autocomplete
    // attribute.
    std::string autocomplete =
        form_util::GetAutocompleteAttribute(control_element);
    std::optional<AutocompleteParsingResult> parsed =
        ParseAutocompleteAttribute(autocomplete);
    if (parsed && parsed->email_verification_token) {
      return control_element;
    }
  }
  return blink::WebFormControlElement();
}

}  // namespace

EmailVerificationHandler::EmailVerificationHandler(AutofillAgent* agent)
    : blink::WebLocalFrameObserver(agent->unsafe_render_frame()->GetWebFrame()),
      agent_(agent) {}

EmailVerificationHandler::~EmailVerificationHandler() = default;

void EmailVerificationHandler::GetNonceForEmailVerification(
    FieldRendererId email_field_id,
    GetNonceForEmailVerificationCallback callback) {
  blink::WebFormControlElement token_element =
      FindTokenFieldForEmailField(email_field_id);
  if (token_element) {
    std::move(callback).Run(token_element.GetAttribute("nonce").Utf8());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void EmailVerificationHandler::StoreEmailVerificationToken(
    FieldRendererId email_field_id,
    const std::string& email,
    const std::string& token) {
  if (token.empty()) {
    return;
  }

  blink::WebFormControlElement token_element =
      FindTokenFieldForEmailField(email_field_id);
  if (!token_element) {
    return;
  }

  email_verification_tokens_[email_field_id] =
      TokenInfo{.token = token,
                .token_field_id = form_util::GetFieldRendererId(token_element),
                .email = email};

  blink::WebInputElement input_element =
      form_util::GetFormControlByRendererId(email_field_id)
          .DynamicTo<blink::WebInputElement>();
  if (!input_element) {
    return;
  }
  input_element.SetEmailVerificationState(
      blink::EmailVerificationState::kVerified);
}

void EmailVerificationHandler::WillSendSubmitEvent(
    const blink::WebFormElement& form) {
  if (email_verification_tokens_.empty() || form.IsNull()) {
    return;
  }

  for (const auto& [email_field_id, info] : email_verification_tokens_) {
    blink::WebFormControlElement email_element =
        form_util::GetFormControlByRendererId(email_field_id);
    if (email_element && email_element.GetOwningFormForAutofill() == form) {
      // To prevent sharing an Email Verification Token (EVT) generated for a
      // different email address (e.g., if the user edited the email field,
      // cleared it, or selected a different email address after the token was
      // sent to the renderer), verify that the email field's current value
      // still matches the email address used during verification.
      std::u16string current_email = email_element.Value().Utf16();
      std::u16string original_email = base::UTF8ToUTF16(info.email);
      if (base::i18n::FoldCase(
              base::TrimWhitespace(current_email, base::TRIM_ALL)) !=
          base::i18n::FoldCase(
              base::TrimWhitespace(original_email, base::TRIM_ALL))) {
        continue;
      }

      blink::WebFormControlElement element =
          form_util::GetFormControlByRendererId(info.token_field_id);
      if (!element) {
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
          driver->FormWithEmailVerificationTokenSubmitted(*form_data,
                                                          email_field_id);
        }
      }
      return;
    }
  }
}

}  // namespace autofill

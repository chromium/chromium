// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/email_verifier_delegate.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"

namespace autofill {

namespace {

content::webid::EmailVerifier* GetOrCreateEmailVerifier(
    AutofillClient& client,
    const LocalFrameToken& frame_token) {
  content::RenderFrameHost* rfh = FindRenderFrameHostByToken(
      *static_cast<ContentAutofillClient&>(client).web_contents(), frame_token);
  if (!rfh) {
    return nullptr;
  }

  // EmailVerificationProtocol is enabled by default in the browser process,
  // but must also be enabled on the blink side (e.g. via Origin Trial token).
  content::RuntimeFeatureStateDocumentData* document_data =
      content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh);
  if (!document_data || !document_data->runtime_feature_state_read_context()
                             .IsEmailVerificationProtocolEnabled()) {
    return nullptr;
  }

  return content::webid::EmailVerifier::GetOrCreateForFrame(rfh);
}

const AutofillField* FindField(
    const std::vector<std::unique_ptr<AutofillField>>& fields,
    base::FunctionRef<bool(const AutofillField&)> predicate) {
  auto iter = std::ranges::find_if(
      fields, [&](const std::unique_ptr<AutofillField>& field) {
        return predicate(*field);
      });
  return iter != std::ranges::end(fields) ? iter->get() : nullptr;
}

}  // namespace

EmailVerifierDelegate::EmailVerifierDelegate(AutofillClient* client) {
  observation_.Observe(client);
}

EmailVerifierDelegate::~EmailVerifierDelegate() = default;

void EmailVerifierDelegate::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }

  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  const AutofillProfile* const* profile =
      std::get_if<const AutofillProfile*>(&filling_payload);

  if (!profile) {
    return;
  }

  const FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  const std::vector<std::unique_ptr<AutofillField>>& fields = form->fields();
  const AutofillField* email_field =
      FindField(fields, [&](const AutofillField& field) {
        return field.autofilled_type() == EMAIL_ADDRESS &&
               filled_field_ids.contains(field.global_id());
      });

  if (!email_field) {
    return;
  }

  const AutofillField* challenge_field =
      FindField(fields, [](const AutofillField& field) {
        return field.parsed_autocomplete() &&
               field.parsed_autocomplete()->email_verification_token &&
               !field.challenge().empty();
      });

  if (!challenge_field) {
    return;
  }

  content::webid::EmailVerifier* verifier =
      GetOrCreateEmailVerifier(manager.client(), email_field->host_frame());
  if (!verifier) {
    return;
  }

  // TODO(crbug.com/446288895): Currently, when filling a form, the browser
  // notifies observers via OnFillOrPreviewForm() **before** it sends the fill
  // request to the renderer and **before** it updates its own cache with the
  // newly filled values. Because of this timing, if we try to read
  // email_field->value() inside the callback, we will get the old value
  // (before filling), not the new email address. So, we extract it manually
  // from the profile instead.
  // We should introduce OnFilledOrPreviewedForm and move the notification to a
  // later stage (specifically after the renderer confirms the fill and the
  // browser updates its cache) so we can use the email_field->value() instead.
  std::u16string email = (*profile)->GetRawInfo(EMAIL_ADDRESS);

  verifier->Verify(
      base::UTF16ToUTF8(email), base::UTF16ToUTF8(challenge_field->challenge()),
      base::BindOnce(
          [](base::WeakPtr<AutofillManager> manager, FieldGlobalId field_id,
             std::optional<std::string> presentation_token) {
            if (!manager || !presentation_token) {
              return;
            }

            manager->driver().SendEmailVerificationToken(field_id,
                                                         *presentation_token);
          },
          manager.GetWeakPtr(), challenge_field->global_id()));
}

}  // namespace autofill

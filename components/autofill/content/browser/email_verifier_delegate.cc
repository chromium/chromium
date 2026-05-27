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
#include "net/base/schemeful_site.h"

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

  std::optional<bool> overridden_state =
      base::FeatureList::GetStateIfOverridden(
          ::features::kEmailVerificationProtocol);
  if (overridden_state == std::make_optional(false)) {
    // If the flag is overridden to be disabled (e.g. via Finch), respect that.
    return nullptr;
  }

  if (overridden_state == std::make_optional(true)) {
    // If the flag is overridden to enabled, we enable no matter what the
    // OT status is.
    return content::webid::EmailVerifier::GetOrCreateForFrame(rfh);
  }

  // In the non-overridden experiment state, EVT is enabled if the feature
  // is enabled globally (via the default state) and the web site opts in
  // via Origin trial token.
  bool globally_enabled =
      base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol);
  content::RuntimeFeatureStateDocumentData* document_data =
      content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh);
  bool enabled_for_page =
      document_data && document_data->runtime_feature_state_read_context()
                           .IsEmailVerificationProtocolEnabled();
  if (!globally_enabled || !enabled_for_page) {
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
    FieldGlobalId trigger_field_id,
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

  // Only trigger verification if the email field itself was the trigger for the
  // autofill action, rather than as a side-effect of autofilling another field
  // (e.g. a name field).
  const AutofillField* triggering_email_field =
      form->GetFieldById(trigger_field_id);
  if (!triggering_email_field ||
      triggering_email_field->autofilled_type() != EMAIL_ADDRESS) {
    return;
  }

  // TODO(crbug.com/446288895): Currently, when filling a form, the browser
  // notifies observers via `OnFillOrPreviewForm()` **before** it sends the fill
  // request to the renderer and **before** it updates its own cache with the
  // newly filled values. Because of this timing, if we try to read
  // `email_field->value()` inside the callback, we will get the old value
  // (before filling), not the new email address. So, we extract it manually
  // from the profile instead.
  // We should introduce `OnFilledOrPreviewedForm` and move the notification to
  // a later stage (specifically after the renderer confirms the fill and the
  // browser updates its cache) so we can use the `email_field->value()`
  // instead.
  std::u16string email = (*profile)->GetRawInfo(EMAIL_ADDRESS);
  TriggerVerification(manager, *form, *triggering_email_field, email);
}

void EmailVerifierDelegate::OnFillOrPreviewField(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    mojom::ActionPersistence action_persistence,
    const std::u16string& value,
    std::optional<FieldType> field_type_used) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }

  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  const FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  const AutofillField* triggering_email_field = form->GetFieldById(field_id);
  if (!triggering_email_field || field_type_used != EMAIL_ADDRESS) {
    return;
  }

  TriggerVerification(manager, *form, *triggering_email_field, value);
}

void EmailVerifierDelegate::TriggerVerification(
    AutofillManager& manager,
    const FormStructure& form,
    const AutofillField& email_field,
    const std::u16string& email_value) {
  const std::vector<std::unique_ptr<AutofillField>>& fields = form.fields();
  const AutofillField* nonce_field =
      FindField(fields, [&](const AutofillField& field) {
        return field.parsed_autocomplete() &&
               field.parsed_autocomplete()->email_verification_token &&
               !field.nonce().empty() &&
               field.host_form_id() == email_field.host_form_id();
      });

  if (!nonce_field) {
    return;
  }

  content::webid::EmailVerifier* verifier =
      GetOrCreateEmailVerifier(manager.client(), email_field.host_frame());
  if (!verifier) {
    return;
  }

  verifier->Verify(
      base::UTF16ToUTF8(email_value), base::UTF16ToUTF8(nonce_field->nonce()),
      base::BindOnce(
          [](base::WeakPtr<AutofillManager> manager,
             FieldGlobalId email_field_id, FieldGlobalId nonce_field_id,
             gfx::RectF email_field_bounds, std::u16string email,
             std::optional<content::webid::EmailVerifier::Result> result) {
            if (!manager || !result) {
              return;
            }
            manager->client().ShowEmailVerificationPopup(
                email_field_bounds, result->issuer_site, email,
                base::BindOnce(
                    [](base::WeakPtr<AutofillManager> manager,
                       FieldGlobalId email_field_id, std::string email,
                       FieldGlobalId token_field_id, std::string token,
                       bool confirmed) {
                      if (!confirmed || !manager) {
                        return;
                      }
                      manager->driver().SendEmailVerificationToken(
                          email_field_id, email, token_field_id, token);
                    },
                    manager, email_field_id, base::UTF16ToUTF8(email),
                    nonce_field_id, std::move(result->verification)));
          },
          manager.GetWeakPtr(), email_field.global_id(),
          nonce_field->global_id(), email_field.bounds(), email_value));
}

}  // namespace autofill

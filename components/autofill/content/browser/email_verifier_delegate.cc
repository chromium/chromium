// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/email_verifier_delegate.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"

namespace autofill {

EmailVerifierDelegate::EmailVerifierDelegate(AutofillClient* client)
    : EmailVerifierDelegate(
          client,
          base::BindRepeating([](AutofillManager& manager) {
            ContentAutofillDriver& content_driver =
                static_cast<ContentAutofillDriver&>(manager.driver());
            content::RenderFrameHost* rfh = content_driver.render_frame_host();
            return content::webid::EmailVerifier::GetOrCreateForFrame(rfh);
          })) {}

EmailVerifierDelegate::EmailVerifierDelegate(AutofillClient* client,
                                             EmailVerifierBuilder builder)
    : email_verifier_builder_(std::move(builder)) {
  observation_.Observe(client);
}

EmailVerifierDelegate::~EmailVerifierDelegate() = default;

void EmailVerifierDelegate::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  if (!base::FeatureList::IsEnabled(::features::kFedCmDelegation)) {
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

  FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  const std::vector<std::unique_ptr<AutofillField>>& fields = form->fields();
  auto it = std::ranges::find_if(
      fields, [&](const std::unique_ptr<AutofillField>& field) {
        return !field->nonce().empty() &&
               field->autofilled_type() == EMAIL_ADDRESS &&
               filled_field_ids.contains(field->global_id());
      });

  if (it == std::ranges::end(fields)) {
    return;
  }

  const AutofillField& email_field = *it->get();

  // TODO(crbug.com/446288895): Use email_field->value() when we fix the
  // OnFillOrPreviewForm callback.
  std::u16string email = (*profile)->GetRawInfo(EMAIL_ADDRESS);

  content::webid::EmailVerifier* verifier =
      email_verifier_builder_.Run(manager);
  verifier->Verify(
      base::UTF16ToUTF8(email), base::UTF16ToUTF8(email_field.nonce()),
      base::BindOnce(
          [](base::WeakPtr<AutofillManager> manager, FieldGlobalId field_id,
             std::optional<std::string> presentation_token) {
            if (!manager) {
              return;
            }

            if (!presentation_token) {
              return;
            }

            manager->client().ShowEmailVerifiedToast();
            manager->driver().DispatchEmailVerifiedEvent(field_id,
                                                         *presentation_token);
          },
          manager.GetWeakPtr(), email_field.global_id()));
}

}  // namespace autofill

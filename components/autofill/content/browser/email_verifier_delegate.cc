// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/email_verifier_delegate.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/strike_databases/email_verification_strike_database.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strike_database/history_clearable_strike_database.h"
#include "components/strike_database/strike_database.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace autofill {

namespace {

content::webid::EmailVerifier* GetOrCreateEmailVerifier(
    AutofillClient& client,
    content::RenderFrameHost* rfh) {
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

void EmailVerifierDelegate::Verify(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    std::string email_utf8,
    FieldGlobalId token_field_id,
    const std::string& nonce,
    const content::webid::EmailVerifier::Result& result) {
  content::RenderFrameHost* rfh = FindRenderFrameHostByToken(
      *static_cast<ContentAutofillClient&>(manager->client()).web_contents(),
      email_field_id.frame_token);
  content::webid::EmailVerifier* verifier =
      GetOrCreateEmailVerifier(manager->client(), rfh);
  if (!verifier) {
    return;
  }
  verifier->Verify(
      result, nonce,
      base::BindOnce(
          [](base::WeakPtr<EmailVerifierDelegate> delegate,
             base::WeakPtr<AutofillManager> manager,
             FieldGlobalId email_field_id, std::string email,
             FieldGlobalId token_field_id, net::SchemefulSite issuer_site,
             std::optional<std::string> token) {
            if (!manager || !token) {
              return;
            }
            if (delegate) {
              delegate->issuers_[token_field_id] = issuer_site.GetURL();
            }
            manager->driver().SendEmailVerificationToken(
                email_field_id, email, token_field_id, *token);
          },
          weak_ptr_factory_.GetWeakPtr(), manager, email_field_id, email_utf8,
          token_field_id, result.issuer_site));
}

void EmailVerifierDelegate::OnEmailVerificationDecision(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    std::string email_utf8,
    FieldGlobalId token_field_id,
    std::string nonce,
    content::webid::EmailVerifier::Result result,
    AutofillClient::EmailVerificationPermissionUiResult ui_result) {
  if (!manager) {
    return;
  }

  PrefService* prefs = manager->client().GetPrefs();
  switch (ui_result) {
    case AutofillClient::EmailVerificationPermissionUiResult::kAccepted: {
      if (prefs) {
        // Remember that the user allows this email address.
        ScopedDictPrefUpdate update(prefs,
                                    prefs::kAutofillEmailVerificationState);
        base::DictValue email_dict;
        if (const base::DictValue* existing =
                prefs->GetDict(prefs::kAutofillEmailVerificationState)
                    .FindDict(email_utf8)) {
          email_dict = existing->Clone();
        }
        email_dict.Set("allowed", true);
        email_dict.Set("issuer_site", result.issuer_site.Serialize());
        email_dict.Set("timestamp", base::TimeToValue(base::Time::Now()));
        update->Set(email_utf8, std::move(email_dict));
      }

      Verify(manager, email_field_id, email_utf8, token_field_id, nonce,
             result);

      if (manager->client().GetStrikeDatabase()) {
        EmailVerificationStrikeDatabase strike_db(
            manager->client().GetStrikeDatabase());
        strike_db.ClearStrikes(
            EmailVerificationStrikeDatabase::GetId(email_utf8));
      }
      break;
    }
    case AutofillClient::EmailVerificationPermissionUiResult::kDeclined: {
      if (manager->client().GetStrikeDatabase()) {
        EmailVerificationStrikeDatabase strike_db(
            manager->client().GetStrikeDatabase());
        strike_db.AddStrike(EmailVerificationStrikeDatabase::GetId(email_utf8));
      }
      break;
    }
    case AutofillClient::EmailVerificationPermissionUiResult::kIgnored: {
      break;
    }
  }
}

void EmailVerifierDelegate::OnIsVerifiable(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    FieldGlobalId nonce_field_id,
    gfx::RectF email_field_bounds,
    std::u16string email,
    std::string nonce,
    bool already_allowed,
    std::optional<content::webid::EmailVerifier::Result> result) {
  if (!manager || !result) {
    return;
  }

  if (already_allowed) {
    Verify(manager, email_field_id, base::UTF16ToUTF8(email), nonce_field_id,
           nonce, *result);
    return;
  }

  net::SchemefulSite issuer_site = result->issuer_site;
  manager->client().ShowEmailVerificationPopup(
      email_field_bounds, issuer_site, email,
      base::BindOnce(&EmailVerifierDelegate::OnEmailVerificationDecision,
                     weak_ptr_factory_.GetWeakPtr(), manager, email_field_id,
                     base::UTF16ToUTF8(email), nonce_field_id, nonce,
                     std::move(*result)));
}

EmailVerifierDelegate::EmailVerifierDelegate(AutofillClient* client) {
  observation_.Observe(client);
  if (auto* content_client = static_cast<ContentAutofillClient*>(client)) {
    Observe(content_client->web_contents());
  }
}

EmailVerifierDelegate::~EmailVerifierDelegate() = default;

void EmailVerifierDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    // `HasCommitted` returns true even for same document commits, e.g.
    // if the state is cleared on pushState() or #anchor navigations.
    // We clear the issuers_ map on these navigations too.
    issuers_.clear();
  }
}

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

  const PrefService* prefs = manager.client().GetPrefs();

  const AutofillProfile* const* profile =
      std::get_if<const AutofillProfile*>(&filling_payload);
  const FormStructure* form = manager.FindCachedFormById(form_id);

  if (!prefs || !prefs->GetBoolean(prefs::kAutofillEmailVerificationEnabled) ||
      action_persistence != mojom::ActionPersistence::kFill || !profile ||
      !form) {
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
  if (!triggering_email_field ||
      (field_type_used != EMAIL_ADDRESS &&
       triggering_email_field->Type().GetAddressType() != EMAIL_ADDRESS)) {
    return;
  }

  TriggerVerification(manager, *form, *triggering_email_field, value);
}

void EmailVerifierDelegate::OnEmailVerificationTokenShared(
    AutofillManager& manager,
    FieldGlobalId field_id) {
  if (auto it = issuers_.find(field_id); it != issuers_.end()) {
    GURL issuer_url = it->second;
    issuers_.erase(it);
    manager.client().ShowEmailVerifiedToast(issuer_url);
  }
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

  content::RenderFrameHost* rfh = FindRenderFrameHostByToken(
      *static_cast<ContentAutofillClient&>(manager.client()).web_contents(),
      email_field.host_frame());
  content::webid::EmailVerifier* verifier =
      GetOrCreateEmailVerifier(manager.client(), rfh);
  if (!verifier) {
    return;
  }

  // Record that the page has triggered the email verification protocol and
  // the feature is active. We record this use counter when verification is
  // first triggered on a page that successfully opted into the origin trial.
  // Note that the use counter shouldn't depend on the status of strike or
  // verify. It only represents whether the API is triggered on the website.
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      rfh, blink::mojom::WebFeature::kEmailVerificationProtocol);

  std::string email_utf8 = base::UTF16ToUTF8(email_value);

  if (manager.client().GetStrikeDatabase()) {
    EmailVerificationStrikeDatabase strike_db(
        manager.client().GetStrikeDatabase());
    if (strike_db.ShouldBlockFeature(
            EmailVerificationStrikeDatabase::GetId(email_utf8))) {
      return;
    }
  }

  PrefService* prefs = manager.client().GetPrefs();
  bool already_allowed = false;
  if (prefs) {
    const base::DictValue& state =
        prefs->GetDict(prefs::kAutofillEmailVerificationState);
    const base::DictValue* email_data = state.FindDict(email_utf8);
    already_allowed =
        email_data && email_data->FindBool("allowed").value_or(false);
  }

  verifier->CheckIfVerifiable(
      email_utf8,
      base::BindOnce(&EmailVerifierDelegate::OnIsVerifiable,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr(),
                     email_field.global_id(), nonce_field->global_id(),
                     email_field.bounds(), email_value,
                     base::UTF16ToUTF8(nonce_field->nonce()), already_allowed));
}

}  // namespace autofill

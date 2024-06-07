// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/metrics/plus_address_submission_logger.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace plus_addresses::metrics {

namespace {

using autofill::AutofillField;
using autofill::FieldGlobalId;
using autofill::FormFieldData;
using autofill::FormGlobalId;
using autofill::FormStructure;
using autofill::SuggestionType;

// A bucketed count of plus addresses of the profile.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PlusAddressCountBucket {
  kNoPlusAddress = 0,
  kOneToThreePlusAddresses = 1,
  kMoreThanThreePlusAddresses = 2,
  kMaxValue = kMoreThanThreePlusAddresses
};

PlusAddressCountBucket ToPlusAddressCountBucket(size_t count) {
  if (count == 0) {
    return PlusAddressCountBucket::kNoPlusAddress;
  } else if (count <= 3) {
    return PlusAddressCountBucket::kOneToThreePlusAddresses;
  } else {
    return PlusAddressCountBucket::kMoreThanThreePlusAddresses;
  }
}

bool IsCartOrCheckoutUrl(const GURL& url) {
  return commerce_heuristics::IsVisitCheckout(url) ||
         commerce_heuristics::IsVisitCart(url);
}

}  // namespace

PlusAddressSubmissionLogger::PlusAddressSubmissionLogger(
    signin::IdentityManager* identity_manager,
    PlusAddressVerifier plus_address_verifier)
    : identity_manager_(CHECK_DEREF(identity_manager)),
      plus_address_verifier_(std::move(plus_address_verifier)) {}

PlusAddressSubmissionLogger::~PlusAddressSubmissionLogger() = default;

void PlusAddressSubmissionLogger::OnPlusAddressSuggestionShown(
    autofill::AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field,
    autofill::AutofillPlusAddressDelegate::SuggestionContext suggestion_context,
    autofill::AutofillClient::PasswordFormType form_type,
    SuggestionType suggestion_type,
    size_t plus_address_count) {
  const CoreAccountInfo core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    return;
  }
  // TODO: crbug.com/343124027 - Re-evaluate what to do during paused signin
  // status.
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfo(core_account_info);

  FormStructure* form_structure = manager.FindCachedFormById(form);
  if (!form_structure) {
    return;
  }
  auto it =
      base::ranges::find_if(form_structure->fields(),
                            [&field](const std::unique_ptr<AutofillField>& f) {
                              return f->global_id() == field;
                            });
  if (it == form_structure->fields().end()) {
    return;
  }
  FormGlobalId renderer_form_id = (*it)->renderer_form_id();

  if (!records_.contains(&manager)) {
    managers_observation_.AddObservation(&manager);
  }

  const size_t field_count_in_renderer_form = base::ranges::count_if(
      form_structure->fields(),
      [renderer_form_id](
          const std::unique_ptr<autofill::AutofillField>& field) {
        return field->renderer_form_id() == renderer_form_id;
      });
  auto record = std::make_unique<Record>(manager.client().GetUkmSourceId());
  record
      ->SetCheckoutOrCartPage(IsCartOrCheckoutUrl(
          manager.client().GetLastCommittedPrimaryMainFrameURL()))
      .SetFieldCountBrowserForm(ukm::GetExponentialBucketMinForCounts1000(
          form_structure->fields().size()))
      .SetFieldCountRendererForm(ukm::GetExponentialBucketMinForCounts1000(
          field_count_in_renderer_form))
      .SetManagedProfile(account_info.IsManaged())
      .SetNewlyCreatedPlusAddress(suggestion_type ==
                                  SuggestionType::kCreateNewPlusAddress)
      .SetPasswordFormType(base::to_underlying(form_type))
      .SetPlusAddressCount(
          base::to_underlying(ToPlusAddressCountBucket(plus_address_count)))
      .SetSuggestionContext(base::to_underlying(suggestion_context));
  records_[&manager].insert_or_assign(field, std::move(record));
}

void PlusAddressSubmissionLogger::OnAutofillManagerDestroyed(
    autofill::AutofillManager& manager) {
  RemoveManagerObservation(manager);
}

void PlusAddressSubmissionLogger::OnAutofillManagerReset(
    autofill::AutofillManager& manager) {
  RemoveManagerObservation(manager);
}

void PlusAddressSubmissionLogger::OnFormSubmitted(
    autofill::AutofillManager& manager,
    const autofill::FormData& form) {
  const CoreAccountInfo core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    return;
  }

  bool gaia_email_submitted = false;
  bool plus_address_submitted = false;
  for (const FormFieldData& field : form.fields) {
    // TODO: crbug.com/343124027 - Consider removing whitespace.
    const std::string normalized_value = base::UTF16ToUTF8(
        autofill::RemoveDiacriticsAndConvertToLowerCase(field.value()));
    if (normalized_value == core_account_info.email) {
      gaia_email_submitted = true;
    } else if (plus_address_verifier_.Run(normalized_value)) {
      plus_address_submitted = true;
    }
  }
  if (!gaia_email_submitted && !plus_address_submitted) {
    // We could now delete the entries in `records_[&manager]` that correspond
    // to fields in this form, but since that happens automatically on every
    // page navigation (due to AutofillManager reset/destruction), it is not
    // worth the effort.
    return;
  }

  base::flat_map<FieldGlobalId, std::unique_ptr<Record>>& records_for_manager =
      records_[&manager];
  bool has_recorded_submission = false;
  for (const FormFieldData& field : form.fields) {
    auto it = records_for_manager.find(field.global_id());
    if (it == records_for_manager.end()) {
      continue;
    }
    // Ensure that only a single metric is recorded per form submission. In
    // general, there will be multiple fields for which suggestions were shown
    // and we pick an arbitrary one.
    if (!has_recorded_submission) {
      Record& record = *it->second;
      if (!plus_address_submitted) {
        record.SetNewlyCreatedPlusAddress(false);
      }
      record.SetSubmittedPlusAddress(plus_address_submitted);
      record.Record(manager.client().GetUkmRecorder());
      has_recorded_submission = true;

      // Record a subset of the data also in form of UMAs.
      base::UmaHistogramBoolean("PlusAddresses.Submission",
                                plus_address_submitted);
    }
    records_for_manager.erase(it);
  }
}

void PlusAddressSubmissionLogger::RemoveManagerObservation(
    autofill::AutofillManager& manager) {
  records_.erase(&manager);
  managers_observation_.RemoveObservation(&manager);
}

}  // namespace plus_addresses::metrics

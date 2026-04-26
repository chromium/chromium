// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/received_tab_forms_filler.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"

namespace send_tab_to_self {
namespace {

// TODO(crbug.com/485145029): Consider making this configurable.
constexpr base::TimeDelta kTimeout = base::Seconds(10);

// Returns the set of signatures that appear exactly once in the incoming tab's
// `fields`.
base::flat_set<PageContext::FormFieldAutofillSignature> ComputeUniqueSignatures(
    const std::vector<PageContext::FormField>& fields) {
  // Count occurrences of each valid signature.
  base::flat_map<PageContext::FormFieldAutofillSignature, size_t> counts;
  for (const PageContext::FormField& field : fields) {
    if (!field.autofill_signature.form_signature.value() ||
        !field.autofill_signature.field_signature.value()) {
      continue;
    }
    ++counts[field.autofill_signature];
  }

  // Filter for signatures that appeared exactly once.
  std::vector<PageContext::FormFieldAutofillSignature> unique_signatures;
  for (const auto& [signature, count] : counts) {
    if (count == 1) {
      unique_signatures.push_back(signature);
    }
  }
  return base::flat_set<PageContext::FormFieldAutofillSignature>(
      base::sorted_unique, std::move(unique_signatures));
}

// Returns the set of field signatures that are unique within `form`.
base::flat_set<autofill::FieldSignature> GetUniqueSignaturesInForm(
    const autofill::FormStructure& form) {
  if (form.form_signature().value() == 0) {
    return {};
  }
  // Count occurrences of each signature in the form.
  base::flat_map<autofill::FieldSignature, size_t> counts;
  for (const std::unique_ptr<autofill::AutofillField>& field : form.fields()) {
    autofill::FieldSignature signature = field->GetFieldSignature();
    if (signature.value() != 0) {
      ++counts[signature];
    }
  }

  // Collect signatures that appeared exactly once.
  std::vector<autofill::FieldSignature> unique_signatures;
  for (const auto& [signature, count] : counts) {
    if (count == 1) {
      unique_signatures.push_back(signature);
    }
  }
  return base::flat_set<autofill::FieldSignature>(base::sorted_unique,
                                                  std::move(unique_signatures));
}

}  // namespace

// static
void ReceivedTabFormsFiller::Start(
    autofill::AutofillClient& client,
    const url::Origin& origin,
    const PageContext::FormFieldInfo& form_field_info,
    base::OnceClosure on_completion_callback) {
  if (form_field_info.fields.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_completion_callback));
    return;
  }
  // This will manage its own lifetime.
  new ReceivedTabFormsFiller(client, origin, form_field_info,
                             std::move(on_completion_callback));
}

ReceivedTabFormsFiller::ReceivedTabFormsFiller(
    autofill::AutofillClient& client,
    const url::Origin& origin,
    const PageContext::FormFieldInfo& form_field_info,
    base::OnceClosure on_completion_callback)
    : origin_(origin),
      received_unique_signatures_(
          ComputeUniqueSignatures(form_field_info.fields)),
      pending_fields_(form_field_info.fields),
      on_completion_callback_for_test_(std::move(on_completion_callback)) {
  // Start a timer to self-destruct if filling takes too long. Using
  // base::Unretained() is safe because the destruction of `timeout_timer_`
  // (which is a member) guarantees that the task will not execute.
  timeout_timer_.Start(FROM_HERE, kTimeout,
                       base::BindOnce(&ReceivedTabFormsFiller::SelfDestruct,
                                      base::Unretained(this)));

  observation_.Observe(&client,
                       autofill::ScopedAutofillManagersObservation::
                           InitializationPolicy::kObservePreexistingManagers);

  for (autofill::AutofillDriver* driver :
       client.GetAutofillDriverFactory().GetExistingDrivers()) {
    switch (driver->GetLifecycleState()) {
      case autofill::AutofillDriver::LifecycleState::kActive:
        FillForms(driver->GetAutofillManager());
        break;
      case autofill::AutofillDriver::LifecycleState::kPendingDeletion:
      case autofill::AutofillDriver::LifecycleState::kPendingReset:
      case autofill::AutofillDriver::LifecycleState::kInactive:
        break;
    }
  }
}

ReceivedTabFormsFiller::~ReceivedTabFormsFiller() {
  std::move(on_completion_callback_for_test_).Run();
}

// static
ReceivedTabFormsFiller::FieldUniquenessKeyComparator::KeyTuple
ReceivedTabFormsFiller::FieldUniquenessKeyComparator::ToKey(
    const PageContext::FormField& field) {
  return {field.id_attribute, field.name_attribute, field.form_control_type};
}

bool ReceivedTabFormsFiller::FieldUniquenessKeyComparator::operator()(
    const PageContext::FormField& a,
    const PageContext::FormField& b) const {
  return ToKey(a) < ToKey(b);
}

bool ReceivedTabFormsFiller::FieldUniquenessKeyComparator::operator()(
    const PageContext::FormField& a,
    const KeyTuple& b) const {
  return ToKey(a) < b;
}

bool ReceivedTabFormsFiller::FieldUniquenessKeyComparator::operator()(
    const KeyTuple& a,
    const PageContext::FormField& b) const {
  return a < ToKey(b);
}

void ReceivedTabFormsFiller::OnAfterFormsSeen(
    autofill::AutofillManager& manager,
    base::span<const autofill::FormGlobalId> updated_forms,
    base::span<const autofill::FormGlobalId> removed_forms) {
  FillForms(manager);
}

void ReceivedTabFormsFiller::OnAutofillManagerStateChanged(
    autofill::AutofillManager& manager,
    autofill::AutofillManager::LifecycleState previous,
    autofill::AutofillManager::LifecycleState current) {
  switch (current) {
    case autofill::AutofillManager::LifecycleState::kActive:
      FillForms(manager);
      break;
    case autofill::AutofillManager::LifecycleState::kPendingDeletion:
    case autofill::AutofillManager::LifecycleState::kPendingReset:
    case autofill::AutofillManager::LifecycleState::kInactive:
      break;
  }
}

const PageContext::FormField* ReceivedTabFormsFiller::FindPendingFieldMatching(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field,
    const base::flat_set<autofill::FieldSignature>& form_unique_signatures) {
  // 1. Try strict match based on ID, Name, and Type.
  if (const PageContext::FormField* match =
          FindPendingFieldByIdNameAndType(field)) {
    return match;
  }

  // 2. Try fallback match using unique Autofill signatures. Return nullptr if
  // no match is found or the signature is not unique in the receiver form.
  const PageContext::FormFieldAutofillSignature signature{
      form.form_signature(), field.GetFieldSignature()};
  return FindPendingFieldBySignature(signature, form_unique_signatures);
}

const PageContext::FormField*
ReceivedTabFormsFiller::FindPendingFieldByIdNameAndType(
    const autofill::AutofillField& field) {
  auto it = pending_fields_.find(std::make_tuple(
      field.id_attribute(), field.name_attribute(),
      autofill::FormControlTypeToString(field.form_control_type())));
  return it != pending_fields_.end() ? &*it : nullptr;
}

const PageContext::FormField*
ReceivedTabFormsFiller::FindPendingFieldBySignature(
    const PageContext::FormFieldAutofillSignature& signature,
    const base::flat_set<autofill::FieldSignature>& form_unique_signatures) {
  // Check if the signature was unique in pending_fields_ initially.
  if (!received_unique_signatures_.contains(signature)) {
    return nullptr;
  }

  // Check if the signature is unique in the receiver form.
  if (!form_unique_signatures.contains(signature.field_signature)) {
    return nullptr;
  }

  // Since the signature was unique initially, the first match found is
  // returned.
  auto find_it = std::find_if(pending_fields_.begin(), pending_fields_.end(),
                              [&](const PageContext::FormField& pending) {
                                return pending.autofill_signature == signature;
                              });
  return find_it != pending_fields_.end() ? &*find_it : nullptr;
}

void ReceivedTabFormsFiller::FillForms(autofill::AutofillManager& manager) {
  manager.ForEachCachedForm([&](const autofill::FormStructure& form) {
    const base::flat_set<autofill::FieldSignature> form_unique_signatures =
        GetUniqueSignaturesInForm(form);

    for (const std::unique_ptr<autofill::AutofillField>& field :
         form.fields()) {
      if (field->origin() != origin_) {
        // Only same-origin fields are considered for security reasons.
        continue;
      }

      const PageContext::FormField* pending_field =
          FindPendingFieldMatching(form, *field, form_unique_signatures);
      if (!pending_field) {
        continue;
      }

      // TODO(crbug.com/485145029): Consider using a type distinguishable from
      // `kAutocompleteEntry`.
      manager.FillOrPreviewField(
          autofill::mojom::ActionPersistence::kFill,
          autofill::mojom::FieldActionType::kReplaceAll, form.ToFormData(),
          *field, pending_field->value,
          autofill::SuggestionType::kAutocompleteEntry, std::nullopt);
      pending_fields_.erase(*pending_field);
    }
  });
}

void ReceivedTabFormsFiller::SelfDestruct() {
  delete this;
}

}  // namespace send_tab_to_self

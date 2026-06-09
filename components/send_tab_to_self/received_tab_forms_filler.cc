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
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace send_tab_to_self {

using AutofillTypeSet = ReceivedTabFormsFiller::AutofillTypeSet;

namespace {

// TODO(crbug.com/519101926): Consider making this configurable.
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

// Returns the set of field signatures that are unique among the given fields.
base::flat_set<autofill::FieldSignature> GetUniqueSignaturesInForm(
    base::span<const autofill::AutofillField* const> fields) {
  // Count occurrences of each signature in the form.
  absl::flat_hash_map<autofill::FieldSignature, size_t> counts;
  for (const autofill::AutofillField* field : fields) {
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
  return base::flat_set<autofill::FieldSignature>(std::move(unique_signatures));
}

// Helper to extract the AutofillTypeSet for a given AutofillField.
AutofillTypeSet GetFieldProtoTypes(const autofill::AutofillField& field) {
  AutofillTypeSet proto_types;
  for (autofill::FieldType type : field.Type().GetTypes()) {
    if (std::optional<sync_pb::FormField_AutofillFieldType> proto_type =
            AutofillFieldTypeToProto(type)) {
      proto_types.insert(*proto_type);
    }
  }
  return proto_types;
}

// Returns the set of Autofill type sets that appear exactly once among the
// given fields.
base::flat_set<AutofillTypeSet> GetUniqueTypeSetsInForm(
    base::span<const autofill::AutofillField* const> fields) {
  base::flat_map<AutofillTypeSet, size_t> counts;
  for (const autofill::AutofillField* field : fields) {
    AutofillTypeSet type_set = GetFieldProtoTypes(*field);
    if (!type_set.empty()) {
      ++counts[type_set];
    }
  }
  std::vector<AutofillTypeSet> unique_type_sets;
  for (const auto& [type_set, count] : counts) {
    if (count == 1) {
      unique_type_sets.push_back(type_set);
    }
  }
  return base::flat_set<AutofillTypeSet>(base::sorted_unique,
                                         std::move(unique_type_sets));
}

// Returns the set of Autofill type sets that appear exactly once in the
// incoming tab's fields.
base::flat_set<AutofillTypeSet> ComputeUniqueTypeSets(
    const std::vector<PageContext::FormField>& fields) {
  base::flat_map<AutofillTypeSet, size_t> counts;
  for (const PageContext::FormField& field : fields) {
    if (!field.autofill_types.empty()) {
      ++counts[field.autofill_types];
    }
  }
  std::vector<AutofillTypeSet> unique_type_sets;
  for (const auto& [type_set, count] : counts) {
    if (count == 1) {
      unique_type_sets.push_back(type_set);
    }
  }
  return base::flat_set<AutofillTypeSet>(base::sorted_unique,
                                         std::move(unique_type_sets));
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
      received_unique_type_sets_(ComputeUniqueTypeSets(form_field_info.fields)),
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
        // Post task to avoid reentrancy check failures in the synchronous
        // AutofillManager observer list (see OnAfterFormsSeen() for details).
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&ReceivedTabFormsFiller::FillForms,
                           weak_ptr_factory_.GetWeakPtr(),
                           driver->GetAutofillManager().GetWeakPtr()));
        break;
      case autofill::AutofillDriver::LifecycleState::kPendingDeletion:
      case autofill::AutofillDriver::LifecycleState::kPendingReset:
      case autofill::AutofillDriver::LifecycleState::kInactive:
        break;
    }
  }
}

ReceivedTabFormsFiller::~ReceivedTabFormsFiller() {
  RecordFormFieldMatchOutcome(FormFieldMatchOutcome::kMatchedByIdNameAndType,
                              matched_id_name_type_count_);
  RecordFormFieldMatchOutcome(FormFieldMatchOutcome::kMatchedBySignature,
                              matched_signature_count_);
  RecordFormFieldMatchOutcome(FormFieldMatchOutcome::kMatchedByExactTypeSet,
                              matched_type_count_);
  // Record matching failures (kNoMatch) for received fields that were never
  // matched, avoiding noise from extra (unshared) fields present on the page.
  RecordFormFieldMatchOutcome(FormFieldMatchOutcome::kNoMatch,
                              pending_fields_.size());
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

// AutofillManager observer events are notified synchronously under a
// base::ObserverList that disallows reentrancy by default. Since FillForms()
// synchronously invokes AutofillManager::FillOrPreviewField() (which triggers
// its own synchronous observer notifications), executing this directly inside
// observer callbacks would trigger a reentrancy crash.
// To prevent this, we post a task to execute the filling asynchronously.
void ReceivedTabFormsFiller::OnAfterFormsSeen(
    autofill::AutofillManager& manager,
    base::span<const autofill::FormGlobalId> updated_forms,
    base::span<const autofill::FormGlobalId> removed_forms) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ReceivedTabFormsFiller::FillForms,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr()));
}

void ReceivedTabFormsFiller::OnAutofillManagerStateChanged(
    autofill::AutofillManager& manager,
    autofill::AutofillManager::LifecycleState previous,
    autofill::AutofillManager::LifecycleState current) {
  switch (current) {
    case autofill::AutofillManager::LifecycleState::kActive:
      // Post task to avoid reentrancy check failures in the synchronous
      // AutofillManager observer list (see OnAfterFormsSeen() for details).
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ReceivedTabFormsFiller::FillForms,
                         weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr()));
      break;
    case autofill::AutofillManager::LifecycleState::kPendingDeletion:
    case autofill::AutofillManager::LifecycleState::kPendingReset:
    case autofill::AutofillManager::LifecycleState::kInactive:
      break;
  }
}

// TODO(crbug.com/511137786): Evaluate if all these matching methods are
// really needed by looking at the UMA metrics. If fallbacks are not heavily
// used, they can be removed to simplify the code and the proto.
ReceivedTabFormsFiller::MatchResult
ReceivedTabFormsFiller::FindPendingFieldMatching(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field,
    const base::flat_set<autofill::FieldSignature>& form_unique_signatures,
    const base::flat_set<AutofillTypeSet>& form_unique_type_sets) const {
  // 1. Try strict match based on ID, Name, and Type.
  if (const PageContext::FormField* match =
          FindPendingFieldByIdNameAndType(field)) {
    return {match, FormFieldMatchOutcome::kMatchedByIdNameAndType};
  }

  // 2. Try fallback match using unique Autofill signatures.
  const PageContext::FormFieldAutofillSignature signature{
      form.form_signature(), field.GetFieldSignature()};
  if (const PageContext::FormField* match =
          FindPendingFieldBySignature(signature, form_unique_signatures)) {
    return {match, FormFieldMatchOutcome::kMatchedBySignature};
  }

  // 3. Try fallback match using Autofill types (exact set match).
  if (const PageContext::FormField* match =
          FindPendingFieldByExactTypeSet(field, form_unique_type_sets)) {
    return {match, FormFieldMatchOutcome::kMatchedByExactTypeSet};
  }

  return {};
}

const PageContext::FormField*
ReceivedTabFormsFiller::FindPendingFieldByIdNameAndType(
    const autofill::AutofillField& field) const {
  auto it = pending_fields_.find(std::make_tuple(
      field.id_attribute(), field.name_attribute(),
      autofill::FormControlTypeToString(field.form_control_type())));
  return it != pending_fields_.end() ? &*it : nullptr;
}

const PageContext::FormField*
ReceivedTabFormsFiller::FindPendingFieldBySignature(
    const PageContext::FormFieldAutofillSignature& signature,
    const base::flat_set<autofill::FieldSignature>& form_unique_signatures)
    const {
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

const PageContext::FormField*
ReceivedTabFormsFiller::FindPendingFieldByExactTypeSet(
    const autofill::AutofillField& field,
    const base::flat_set<AutofillTypeSet>& form_unique_type_sets) const {
  AutofillTypeSet local_type_set = GetFieldProtoTypes(field);
  if (local_type_set.empty()) {
    return nullptr;
  }

  // Check if the type set is unique in the receiver form.
  if (!form_unique_type_sets.contains(local_type_set)) {
    return nullptr;
  }

  // Check if the type set was unique in the pending fields.
  if (!received_unique_type_sets_.contains(local_type_set)) {
    return nullptr;
  }

  // Find the unique pending field that has this exact type set.
  auto find_it = std::find_if(pending_fields_.begin(), pending_fields_.end(),
                              [&](const PageContext::FormField& pending) {
                                return pending.autofill_types == local_type_set;
                              });
  return find_it != pending_fields_.end() ? &*find_it : nullptr;
}

void ReceivedTabFormsFiller::FillForms(
    base::WeakPtr<autofill::AutofillManager> manager) {
  if (!manager) {
    return;
  }
  manager->ForEachCachedForm([&](const autofill::FormStructure& form) {
    // Only same-origin fields are processed and filled for security reasons.
    // Pre-filter them here to perform the check once and avoid repeating
    // it in GetUniqueSignaturesInForm(), GetUniqueTypeSetsInForm(), and the
    // filling loop below.
    std::vector<const autofill::AutofillField*> same_origin_fields;
    for (const std::unique_ptr<autofill::AutofillField>& field :
         form.fields()) {
      if (field->origin() == origin_) {
        same_origin_fields.push_back(field.get());
      }
    }

    const base::flat_set<autofill::FieldSignature> form_unique_signatures =
        GetUniqueSignaturesInForm(same_origin_fields);
    const base::flat_set<AutofillTypeSet> form_unique_type_sets =
        GetUniqueTypeSetsInForm(same_origin_fields);

    for (const autofill::AutofillField* field : same_origin_fields) {
      const MatchResult match = FindPendingFieldMatching(
          form, *field, form_unique_signatures, form_unique_type_sets);
      if (!match.field) {
        // Fields on the page that don't match any pending field are ignored for
        // metrics purposes.
        continue;
      }

      switch (match.outcome) {
        case FormFieldMatchOutcome::kMatchedByIdNameAndType:
          ++matched_id_name_type_count_;
          break;
        case FormFieldMatchOutcome::kMatchedBySignature:
          ++matched_signature_count_;
          break;
        case FormFieldMatchOutcome::kMatchedByExactTypeSet:
          ++matched_type_count_;
          break;
        case FormFieldMatchOutcome::kNoMatch:
          NOTREACHED();
      }

      manager->FillOrPreviewField(
          autofill::mojom::ActionPersistence::kFill,
          autofill::mojom::FieldActionType::kReplaceAll, form.global_id(),
          field->global_id(), match.field->value,
          autofill::FillingProduct::kNone, std::nullopt);
      // Erase the successfully filled field immediately to prevent duplicate
      // fills or double-logging in the destructor.
      pending_fields_.erase(*match.field);
    }
  });
}

void ReceivedTabFormsFiller::SelfDestruct() {
  delete this;
}

}  // namespace send_tab_to_self

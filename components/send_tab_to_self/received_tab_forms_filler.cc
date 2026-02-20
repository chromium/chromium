// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/received_tab_forms_filler.h"

#include "base/check.h"
#include "base/check_deref.h"
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

namespace send_tab_to_self {
namespace {

// TODO(crbug.com/485145029): Consider making this configurable.
constexpr base::TimeDelta kTimeout = base::Seconds(10);

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

void ReceivedTabFormsFiller::FillForms(autofill::AutofillManager& manager) {
  manager.ForEachCachedForm([&](const autofill::FormStructure& form) {
    for (const std::unique_ptr<autofill::AutofillField>& field :
         form.fields()) {
      if (field->origin() != origin_) {
        // Only same-origin fields are considered for security reasons.
        continue;
      }

      const auto it = pending_fields_.find(std::make_tuple(
          field->id_attribute(), field->name_attribute(),
          autofill::FormControlTypeToString(field->form_control_type())));
      if (it == pending_fields_.end()) {
        continue;
      }

      // TODO(crbug.com/485145029): Consider using a type distinguishable from
      // `kAutocompleteEntry`.
      manager.FillOrPreviewField(autofill::mojom::ActionPersistence::kFill,
                                 autofill::mojom::FieldActionType::kReplaceAll,
                                 form.ToFormData(), *field, it->value,
                                 autofill::SuggestionType::kAutocompleteEntry,
                                 std::nullopt);
      pending_fields_.erase(it);
    }
  });
}

void ReceivedTabFormsFiller::SelfDestruct() {
  delete this;
}

}  // namespace send_tab_to_self

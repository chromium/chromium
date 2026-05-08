// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_RECEIVED_TAB_FORMS_FILLER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_RECEIVED_TAB_FORMS_FILLER_H_

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/send_tab_to_self/page_context.h"
#include "url/origin.h"

namespace autofill {
class AutofillClient;
}  // namespace autofill

namespace send_tab_to_self {

// Helper class to fill form field values from a FormFieldInfo to all frames
// managed by an AutofillClient using their AutofillManagers.
//
// This class manages its own lifetime and deletes itself, latest after a
// timeout.
class ReceivedTabFormsFiller : public autofill::AutofillManager::Observer {
 public:
  using AutofillTypeSet = base::flat_set<sync_pb::FormField_AutofillFieldType>;
  static void Start(
      autofill::AutofillClient& client,
      const url::Origin& origin,
      const PageContext::FormFieldInfo& form_field_info,
      base::OnceClosure on_completion_callback = base::DoNothing());

  ReceivedTabFormsFiller(const ReceivedTabFormsFiller&) = delete;
  ReceivedTabFormsFiller& operator=(const ReceivedTabFormsFiller&) = delete;

 private:
  struct FieldUniquenessKeyComparator {
    using is_transparent = void;
    // Each tuple contains (in order): id_attribute, name_attribute,
    // form_control_type.
    using KeyTuple =
        std::tuple<std::u16string_view, std::u16string_view, std::string_view>;

    static KeyTuple ToKey(const PageContext::FormField& field);

    bool operator()(const PageContext::FormField& a,
                    const PageContext::FormField& b) const;
    bool operator()(const PageContext::FormField& a, const KeyTuple& b) const;
    bool operator()(const KeyTuple& a, const PageContext::FormField& b) const;
  };

  ReceivedTabFormsFiller(autofill::AutofillClient& client,
                         const url::Origin& origin,
                         const PageContext::FormFieldInfo& form_field_info,
                         base::OnceClosure on_completion_callback);
  ~ReceivedTabFormsFiller() override;

  // AutofillManager::Observer:
  void OnAfterFormsSeen(
      autofill::AutofillManager& manager,
      base::span<const autofill::FormGlobalId> updated_forms,
      base::span<const autofill::FormGlobalId> removed_forms) override;
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillManager::LifecycleState previous,
      autofill::AutofillManager::LifecycleState current) override;

  void FillForms(autofill::AutofillManager& manager);
  void SelfDestruct();

  // Finds a matching field in `pending_fields_` for the given `field` in
  // `form`. Tries strict match first, then falls back to signature match if
  // unique, and finally to exact type set match if unique.
  const PageContext::FormField* FindPendingFieldMatching(
      const autofill::FormStructure& form,
      const autofill::AutofillField& field,
      const base::flat_set<autofill::FieldSignature>& form_unique_signatures,
      const base::flat_set<AutofillTypeSet>& form_unique_type_sets);

  // Tries to find a match based on strict ID, Name, and Type equality.
  const PageContext::FormField* FindPendingFieldByIdNameAndType(
      const autofill::AutofillField& field);

  // Tries to find a match using Autofill signatures as a fallback.
  // Ensures the signature is unique in both the receiver form and pending
  // fields.
  const PageContext::FormField* FindPendingFieldBySignature(
      const PageContext::FormFieldAutofillSignature& signature,
      const base::flat_set<autofill::FieldSignature>& form_unique_signatures);

  // Tries to find a match using Autofill types (sync enums) as a fallback.
  // The match is performed by comparing the exact set of types.
  // Restrictions to prevent false positives:
  // - Only valid, fillable types (that can be converted to the sync proto enum
  //   via AutofillFieldTypeToProto) are considered.
  // - The exact set of types must appear exactly once (unique) in both the
  //   incoming pending fields and the receiver form (filtered by origin).
  // - Partial matches (e.g. overlapping subsets of types) are not allowed.
  const PageContext::FormField* FindPendingFieldByExactTypeSet(
      const autofill::AutofillField& field,
      const base::flat_set<AutofillTypeSet>& form_unique_type_sets);

  const url::Origin origin_;
  // Signatures that appeared exactly once in the incoming tab's fields.
  const base::flat_set<PageContext::FormFieldAutofillSignature>
      received_unique_signatures_;
  // Type sets that appeared exactly once in the incoming tab's fields.
  const base::flat_set<AutofillTypeSet> received_unique_type_sets_;

  base::flat_set<PageContext::FormField, FieldUniquenessKeyComparator>
      pending_fields_;
  base::OneShotTimer timeout_timer_;
  base::OnceClosure on_completion_callback_for_test_;
  autofill::ScopedAutofillManagersObservation observation_{this};
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_RECEIVED_TAB_FORMS_FILLER_H_

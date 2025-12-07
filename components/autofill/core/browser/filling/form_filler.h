// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_H_

#include <optional>
#include <string>
#include <variant>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling/form_autofill_history.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"

namespace autofill {

class AutofillClient;
class AutofillProfile;
class BrowserAutofillManager;
class CreditCard;
class LogManager;
enum class FillingProduct;

// Denotes the reason for triggering a refill attempt.
// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this enum up to date with the one
// in tools/metrics/histograms/metadata/autofill/enums.xml.
enum class RefillTriggerReason {
  kFormChanged = 0,
  kSelectOptionsChanged = 1,
  kExpirationDateFormatted = 2,
  kMaxValue = kExpirationDateFormatted
};

using VerifiedProfile = std::map<FieldType, std::u16string>;

using FillingPayload = std::variant<const AutofillProfile*,
                                    const CreditCard*,
                                    const EntityInstance*,
                                    const VerifiedProfile*,
                                    const OtpFillData*>;

// Helper class responsible for [re]filling forms and fields.
//
// It is privately owned by the BrowserAutofillManager, which is the only
// component that talks to it.
//
// It receives cached data and is responsible for either filling directly or
// triggering a refill (which eventually results in a filling operation), and
// then sending the filled form to the renderer (via AutofillDriver).
//
// Additionally, it provides an API to determine which fields will be
// filled/skipped based on the given context.
//
// The class is directly responsible for modifying the cached fields in the form
// cache (BrowserAutofillManager::form_structures_) since it receives references
// to cached fields and modifies some attributes during filling.
//
// The class is also indirectly responsible for modifying blink forms, since
// after filling FormData objects it sends them to the renderer, which is
// directly responsible for filling the web forms.
//
// It holds any state that is only relevant for [re]filling.
class FormFiller {
 public:
  struct ValueAndType {
    std::u16string value;
    FieldType type = NO_SERVER_DATA;
  };

  explicit FormFiller(BrowserAutofillManager& manager);

  FormFiller(const FormFiller&) = delete;
  FormFiller& operator=(const FormFiller&) = delete;

  virtual ~FormFiller();

  class RefillOptions {
   public:
    static RefillOptions NotRefill();
    static RefillOptions Refill(DenseSet<FieldTypeGroup> originally_filled);

    bool is_refill() const;
    bool may_refill(const FieldTypeSet& field_type) const;

   private:
    RefillOptions();

    std::optional<DenseSet<FieldTypeGroup>> originally_filled_;
  };

  // Given `field`, the corresponding `autofill_field` to fill, and the
  // `trigger_field`, return the set of all reasons for that field to be skipped
  // for filling. If the field should not be skipped, an empty set is returned
  // (and not {FieldFillingSkipReason::kNotSkipped}).
  // `type_count` tracks the number of times a type of field has been filled.
  // `type_groups_originally_filled` denotes, in case of a refill, what groups
  // where filled in the initial filling.
  // `blocked_fields` are fields which must not be filled because another
  // filling product of higher priority claims them.
  // `filling_product` is the type of filling calling this function.
  // TODO(crbug.com/40281552): Make `type_groups_originally_filled` also a
  // FieldTypeSet.
  // TODO(crbug.com/40227496): Keep only one of 'field' and 'autofill_field'.
  static DenseSet<FieldFillingSkipReason> GetFillingSkipReasonsForField(
      const FormFieldData& field,
      const AutofillField& autofill_field,
      const AutofillField& trigger_field,
      const RefillOptions& refill_options,
      base::flat_map<FieldType, size_t>& type_count,
      const base::flat_set<FieldGlobalId>& blocked_fields,
      FillingProduct filling_product);

  // Resets states that FormFiller holds and maintains.
  void Reset();

  base::TimeDelta get_limit_before_refill() { return limit_before_refill_; }

  // Given a `form`, returns a map from each field's id to the skip reason for
  // that field. See additional comments in GetFieldFillingSkipReason.
  // TODO(crbug.com/40227496): Keep only one of 'form' and 'form_structure'.
  // TODO(crbug.com/40281552): Make `type_groups_originally_filled` also a
  // FieldTypeSet.
  static base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
  GetFieldFillingSkipReasons(base::span<const FormFieldData> fields,
                             const FormStructure& form_structure,
                             const AutofillField& trigger_field,
                             const RefillOptions& refill_options,
                             FillingProduct filling_product,
                             const AutofillClient& client);

  // Reverts the last autofill operation on `form` that affected
  // `trigger_field`. `renderer_action` denotes whether this is an actual
  // filling or a preview operation on the renderer side.
  // TODO(crbug.com/40227496): Keep only one of `form` and `form_structure`.
  void UndoAutofill(mojom::ActionPersistence action_persistence,
                    FormData form,
                    FormStructure& form_structure,
                    const FormFieldData& trigger_field,
                    FillingProduct filling_product);

  // Records filling information if possible and routes back to the renderer.
  void FillOrPreviewField(mojom::ActionPersistence action_persistence,
                          mojom::FieldActionType action_type,
                          const FormFieldData& field,
                          AutofillField* autofill_field,
                          const std::u16string& value,
                          FillingProduct filling_product,
                          std::optional<FieldType> field_type_used);

  // Fills or previews the data from `filling_payload` into `form`.
  // TODO(crbug.com/40227071): Clean up the API.
  void FillOrPreviewForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FillingPayload& filling_payload,
      FormStructure& form_structure,
      AutofillField& autofill_field,
      AutofillTriggerSource trigger_source,
      std::optional<RefillTriggerReason> refill_trigger_reason = std::nullopt);

  // May or may not trigger a refill operation on `form`. `field` and
  // `old_value` are only needed when `refill_trigger_reason` is
  // `RefillTriggerReason::kExpirationDateFormatted`, and in that case `field`
  // is the one that was reformatted and `old_value` is the value `field` had
  // before the reformatting.
  void MaybeTriggerRefill(
      const FormData& form,
      const FormStructure& form_structure,
      RefillTriggerReason refill_trigger_reason,
      AutofillTriggerSource trigger_source,
      base::optional_ref<const AutofillField> field = std::nullopt,
      base::optional_ref<const std::u16string> old_value = std::nullopt);

  base::WeakPtr<FormFiller> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  struct RefillContext;

 private:
  friend class FormFillerTestApi;
  friend class TestFormFiller;

  struct AugmentedFillingPayload;

  void SetRefillContext(FormGlobalId form_id,
                        std::unique_ptr<RefillContext> context);

  RefillContext* GetRefillContext(FormGlobalId form_id);

  // Schedules a call of TriggerRefill. Virtual for testing.
  virtual void ScheduleRefill(const FormData& form,
                              RefillContext& refill_context,
                              AutofillTriggerSource trigger_source,
                              RefillTriggerReason refill_trigger_reason);

  // Attempts to refill `form`.
  void TriggerRefill(const FormData& form,
                     AutofillTriggerSource trigger_source,
                     RefillTriggerReason refill_trigger_reason);

  struct ValueAndTypeAndOverride : public ValueAndType {
    bool value_is_an_override = false;
  };

  // Returns the value to fill along with the field type and if the value is an
  // override.
  ValueAndTypeAndOverride GetFieldFillingData(
      const AutofillField& autofill_field,
      const AugmentedFillingPayload& filling_payload,
      const std::map<FieldGlobalId, ValueAndType>& forced_fill_values,
      const FormFieldData& field_data,
      mojom::ActionPersistence action_persistence,
      std::string* failure_to_fill);

  // Fills `field_data` and modifies `autofill_field` given all other states.
  // Returns the FieldType of the value that was filled, or std::nullopt if no
  // value was filled. If the FieldType is not known, returns UNKNOWN_TYPE. The
  // return value is independent of whether the field was filled or autofilled
  // before. When `allow_suggestion_swapping` is true, the method still returns
  // the FieldType if the `autofill_field` is emptied.
  // TODO(crbug.com/40227071): Cleanup API and logic.
  std::optional<FieldType> FillField(
      AutofillField& autofill_field,
      const AugmentedFillingPayload& filling_payload,
      const std::map<FieldGlobalId, ValueAndType>& forced_fill_values,
      FormFieldData& field_data,
      mojom::ActionPersistence action_persistence,
      bool allow_suggestion_swapping,
      std::string* failure_to_fill);

  // Appends TriggerFillFieldLogEvent and FillFieldLogEvents to the relevant
  // fields in the `form_structure` if there was a filling operation.
  void AppendFillLogEvents(
      const FormData& form,
      FormStructure& form_structure,
      AutofillField& trigger_autofill_field,
      const base::flat_set<FieldGlobalId>& safe_field_ids,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
          skip_reasons,
      const FillingPayload& filling_payload,
      bool is_refill);

  LogManager* log_manager();

  // Container holding the history of Autofill filling operations. Used to undo
  // some of the filling operations.
  FormAutofillHistory form_autofill_history_;

  // A map from FormGlobalId to RefillContext instances used to make refill
  // attempts for dynamic forms.
  std::map<FormGlobalId, std::unique_ptr<RefillContext>> refill_context_;

  // The maximum amount of time between a change in the form and the original
  // fill that triggers a refill. This value is only changed in browser tests,
  // where time cannot be mocked, to avoid flakiness.
  base::TimeDelta limit_before_refill_ = kLimitBeforeRefill;

  const raw_ref<BrowserAutofillManager> manager_;

  base::WeakPtrFactory<FormFiller> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_H_

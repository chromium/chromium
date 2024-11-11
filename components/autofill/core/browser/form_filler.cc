// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_filler.h"

#include <array>
#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_filling_payments_util.h"
#include "components/autofill/core/browser/field_filling_skip_reason.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// Time to wait after a dynamic form change before triggering a refill.
// This is used for sites that change multiple things consecutively.
constexpr base::TimeDelta kWaitTimeForDynamicForms = base::Milliseconds(200);

// Returns how many fields with type |field_type| may be filled in a form at
// maximum.
size_t TypeValueFormFillingLimit(FieldType field_type) {
  switch (field_type) {
    case CREDIT_CARD_NUMBER:
      return kCreditCardTypeValueFormFillingLimit;
    case ADDRESS_HOME_STATE:
      return kStateTypeValueFormFillingLimit;
    default:
      return kTypeValueFormFillingLimit;
  }
}

std::string_view ActionPersistenceToString(
    mojom::ActionPersistence action_persistence) {
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      return "fill";
    case mojom::ActionPersistence::kPreview:
      return "preview";
  }
}

// Returns true iff `field` should be skipped during filling because its
// non-empty initial value is considered to be meaningful.
bool ShouldSkipFieldBecauseOfMeaningfulInitialValue(const AutofillField& field,
                                                    bool is_trigger_field) {
  // Assume that the trigger field can always be overwritten.
  if (is_trigger_field) {
    return false;
  }
  // Select (list) elements are currently not supported.
  if (field.IsSelectElement()) {
    return false;
  }
  // By default, empty initial values are not considered to be meaningful. A
  // value only consisting of whitespace is considered empty.
  if (base::TrimWhitespace(field.value(ValueSemantics::kInitial),
                           base::TrimPositions::TRIM_ALL)
          .empty()) {
    return false;
  }
  // If the field's initial value coincides with the value of its placeholder
  // attribute, don't consider the initial value to be meaningful.
  if (field.value(ValueSemantics::kInitial) == field.placeholder()) {
    return false;
  }

  // If kAutofillOverwritePlaceholdersOnly is enabled:
  // Fields that are non-empty on page load are only overwritten if
  // crowdsourcing classified them as "placeholder" fields (meaning that users
  // typically modify the value).
  //
  // At this point the field is known to contain a non-empty initial value at
  // page load.
  if (field.may_use_prefilled_placeholder().has_value() &&
      base::FeatureList::IsEnabled(
          features::kAutofillOverwritePlaceholdersOnly)) {
    return !field.may_use_prefilled_placeholder().value();
  }

  // If kAutofillSkipPreFilledFields is enabled:
  // Fields that are non-empty on page load are not meant to be overwritten.
  //
  // At this point the field is known to contain a non-empty initial value at
  // page load.
  return base::FeatureList::IsEnabled(features::kAutofillSkipPreFilledFields);
}

bool AllowPaymentSwapping(const AutofillField& trigger_field,
                          const AutofillField& field,
                          bool is_refill) {
  return GroupTypeOfFieldType(trigger_field.Type().GetStorableType()) ==
             FieldTypeGroup::kCreditCard &&
         GroupTypeOfFieldType(field.Type().GetStorableType()) ==
             FieldTypeGroup::kCreditCard &&
         !is_refill &&
         base::FeatureList::IsEnabled(
             features::kAutofillEnablePaymentsFieldSwapping);
}

// Returns whether a filling action for `filling_product` should be included in
// the form autofill history.
bool ShouldRecordFillingHistory(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kCreditCard:
    case FillingProduct::kPlusAddresses:
      return true;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kStandaloneCvc:
    case FillingProduct::kPredictionImprovements:
      return false;
  }
  NOTREACHED();
}

}  // namespace

DenseSet<FieldFillingSkipReason> FormFiller::GetFillingSkipReasonsForField(
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const AutofillField& trigger_field,
    base::flat_map<FieldType, size_t>& type_count,
    const std::optional<DenseSet<FieldTypeGroup>> type_groups_originally_filled,
    FieldTypeSet field_types_to_fill,
    FillingProduct filling_product,
    bool skip_unrecognized_autocomplete_fields,
    bool is_refill,
    bool is_expired_credit_card) {
  DenseSet<FieldFillingSkipReason> skip_reasons;
  const bool is_trigger_field =
      autofill_field.global_id() == trigger_field.global_id();

  auto add_if = [&skip_reasons](bool condition, FieldFillingSkipReason reason) {
    if (condition) {
      skip_reasons.insert(reason);
    }
  };

  // Do not fill fields that are not part of the filled section, as this has
  // higher probability to be inaccurate (a second full name field probably
  // exists not to be filled with the same info as the first full name field).
  add_if(autofill_field.section() != trigger_field.section(),
         FieldFillingSkipReason::kNotInFilledSection);

  // Some fields are rationalized so that they are only filled when focuses
  // (since we allow for example multiple phone number fields to exist in the
  // same section). Therefore we skip those fields if they're not focused.
  add_if(autofill_field.only_fill_when_focused() && !is_trigger_field,
         FieldFillingSkipReason::kNotFocused);

  // Address fields with unrecognized autocomplete attribute) are only filled
  // when triggered through manual fallbacks.
  add_if(!is_trigger_field && skip_unrecognized_autocomplete_fields &&
             autofill_field.ShouldSuppressSuggestionsAndFillingByDefault(),
         FieldFillingSkipReason::kUnrecognizedAutocompleteAttribute);

  // Skip if the form has changed in the meantime, which may happen with
  // refills.
  add_if(autofill_field.global_id() != field.global_id(),
         FieldFillingSkipReason::kFormChanged);

  // Don't fill unfocusable fields, with the exception of <select> fields, for
  // the sake of filling the synthetic fields.
  add_if(!autofill_field.IsFocusable() && !autofill_field.IsSelectElement(),
         FieldFillingSkipReason::kInvisibleField);

  // Do not fill fields that have been edited by the user, except if the field
  // is empty and its initial value (= cached value) was empty as well. A
  // similar check is done in ForEachMatchingFormFieldCommon(), which
  // frequently has false negatives.
  add_if((field.properties_mask() & kUserTyped) &&
             !(field.value().empty() &&
               autofill_field.value(ValueSemantics::kInitial).empty()) &&
             !is_trigger_field,
         FieldFillingSkipReason::kUserFilledFields);

  // Don't fill previously autofilled fields except the initiating field or
  // when it's a refill or for credit card fields, when
  // `kAutofillEnablePaymentsFieldSwapping` is enabled.
  add_if(field.is_autofilled() && !is_trigger_field && !is_refill &&
             !AllowPaymentSwapping(trigger_field, autofill_field, is_refill),
         FieldFillingSkipReason::kAlreadyAutofilled);

  FieldTypeGroup field_group_type = autofill_field.Type().group();
  add_if(field_group_type == FieldTypeGroup::kNoGroup,
         FieldFillingSkipReason::kNoFillableGroup);

  // On a refill, only fill fields from type groups that were present during
  // the initial fill.
  add_if(is_refill && type_groups_originally_filled.has_value() &&
             !base::Contains(*type_groups_originally_filled, field_group_type),
         FieldFillingSkipReason::kRefillNotInInitialFill);

  FieldType field_type = autofill_field.Type().GetStorableType();
  // Don't fill expired cards expiration date.
  add_if(data_util::IsCreditCardExpirationType(field_type) &&
             is_expired_credit_card,
         FieldFillingSkipReason::kExpiredCards);

  // Only fill fields whose types are included in `field_types_to_fill`.
  add_if(!field_types_to_fill.contains(field_type),
         FieldFillingSkipReason::kFieldDoesNotMatchTargetFieldsSet);

  // A field with a specific type is only allowed to be filled a limited
  // number of times given by |TypeValueFormFillingLimit(field_type)|.
  add_if(++type_count[field_type] > TypeValueFormFillingLimit(field_type),
         FieldFillingSkipReason::kFillingLimitReachedType);

  // Usually, this should not happen because Autofill sectioning logic
  // separates address fields from credit card fields. However, autofill
  // respects the HTML `autocomplete` attribute when it is used to specify a
  // section, and so in some rare cases it might happen that a credit card
  // field is included in an address section or vice versa.
  // Note that autofilling using manual fallback does not use this logic flow,
  // otherwise this wouldn't be true.
  add_if((filling_product == FillingProduct::kAddress &&
          !IsAddressType(autofill_field.Type().GetStorableType())) ||
             (filling_product == FillingProduct::kCreditCard &&
              !FieldTypeGroupSet({FieldTypeGroup::kCreditCard,
                                  FieldTypeGroup::kStandaloneCvcField})
                   .contains(autofill_field.Type().group())),
         FieldFillingSkipReason::kFieldTypeUnrelated);

  // Don't fill meaningfully pre-filled fields but overwrite placeholders.
  add_if(ShouldSkipFieldBecauseOfMeaningfulInitialValue(autofill_field,
                                                        is_trigger_field),
         FieldFillingSkipReason::kValuePrefilled);

  return skip_reasons;
}

FormFiller::FillingContext::FillingContext(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card_ptr)
    : profile_or_credit_card(absl::visit(
          [](const auto* c) {
            return absl::variant<CreditCard, AutofillProfile>(*c);
          },
          profile_or_credit_card_ptr)),
      filled_field_id(field.global_id()),
      filled_field_signature(field.GetFieldSignature()),
      filled_origin(field.origin()),
      original_fill_time(base::TimeTicks::Now()) {}

FormFiller::FillingContext::~FillingContext() = default;

FormFiller::FormFiller(BrowserAutofillManager& manager,
                       LogManager* log_manager,
                       const std::string& app_locale)
    : app_locale_(app_locale), log_manager_(log_manager), manager_(manager) {}

FormFiller::~FormFiller() = default;

void FormFiller::Reset() {
  filling_context_.clear();
  form_autofill_history_.Reset();
}

std::optional<base::TimeTicks> FormFiller::GetOriginalFillingTime(
    FormGlobalId form_id) {
  FillingContext* filling_context = GetFillingContext(form_id);
  return filling_context ? std::optional(filling_context->original_fill_time)
                         : std::nullopt;
}

base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
FormFiller::GetFieldFillingSkipReasons(
    base::span<const FormFieldData> fields,
    const FormStructure& form_structure,
    const AutofillField& trigger_field,
    const FieldTypeSet& field_types_to_fill,
    std::optional<DenseSet<FieldTypeGroup>> type_groups_originally_filled,
    FillingProduct filling_product,
    bool skip_unrecognized_autocomplete_fields,
    bool is_refill,
    bool is_expired_credit_card) const {
  // Counts the number of times a type was seen in the section to be filled.
  // This is used to limit the maximum number of fills per value.
  base::flat_map<FieldType, size_t> type_count;
  type_count.reserve(form_structure.field_count());

  CHECK_EQ(fields.size(), form_structure.field_count());
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      base::MakeFlatMap<FieldGlobalId, DenseSet<FieldFillingSkipReason>>(
          form_structure, {}, [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(),
                                  DenseSet<FieldFillingSkipReason>{});
          });
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    // Log events when the fields on the form are filled by autofill suggestion.
    DenseSet<FieldFillingSkipReason> field_skip_reasons =
        GetFillingSkipReasonsForField(
            fields[i], *form_structure.field(i), trigger_field, type_count,
            type_groups_originally_filled, field_types_to_fill, filling_product,
            skip_unrecognized_autocomplete_fields, is_refill,
            is_expired_credit_card);

    // Usually, `skip_reasons[field_id].empty()` before executing the line
    // below. It may not be the case though because FieldGlobalIds may not be
    // unique among `FormData::fields_` (see crbug.com/41496988), so a previous
    // iteration may have added skip reasons for `field_id`. To err on the side
    // of caution we accumulate all skip reasons found in any iteration
    skip_reasons[form_structure.field(i)->global_id()].insert_all(
        field_skip_reasons);
  }
  return skip_reasons;
}

FillingProduct FormFiller::UndoAutofill(
    mojom::ActionPersistence action_persistence,
    FormData form,
    FormStructure& form_structure,
    const FormFieldData& trigger_field) {
  if (!form_autofill_history_.HasHistory(trigger_field.global_id())) {
    LOG_AF(log_manager_)
        << "Could not undo the filling operation on field "
        << trigger_field.global_id()
        << " because history was dropped upon reaching history limit of "
        << kMaxStorableFieldFillHistory;
    return FillingProduct::kNone;
  }
  FormAutofillHistory::FillOperation operation =
      form_autofill_history_.GetLastFillingOperationForField(
          trigger_field.global_id());

  std::vector<FormFieldData> fields = form.ExtractFields();
  base::flat_map<FieldGlobalId, AutofillField*> cached_fields =
      base::MakeFlatMap<FieldGlobalId, AutofillField*>(
          form_structure.fields(), {},
          [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(), field.get());
          });
  // Remove the fields to be skipped so that we only pass fields to be modified
  // by the renderer.
  std::erase_if(
      fields, [this, &operation, &cached_fields](const FormFieldData& field) {
        // Skip not-autofilled fields as undo only acts on autofilled fields.
        return !field.is_autofilled() ||
               // Skip fields whose last autofill operations is different than
               // the one of the trigger field.
               form_autofill_history_.GetLastFillingOperationForField(
                   field.global_id()) != operation ||
               // Skip fields that are not cached to avoid unexpected outcomes.
               !cached_fields.contains(field.global_id());
      });

  for (FormFieldData& field : fields) {
    AutofillField& autofill_field =
        CHECK_DEREF(cached_fields[field.global_id()]);
    const FormAutofillHistory::FieldFillingEntry& previous_state =
        operation.GetFieldFillingEntry(field.global_id());
    // Update the FormFieldData to be sent for the renderer.
    field.set_value(previous_state.value);
    field.set_is_autofilled(previous_state.is_autofilled);

    // Update the cached AutofillField in the browser.
    // TODO(crbug.com/40232021): Consider updating the value too.
    autofill_field.set_is_autofilled(previous_state.is_autofilled);
    autofill_field.set_autofill_source_profile_guid(
        previous_state.autofill_source_profile_guid);
    autofill_field.set_autofilled_type(previous_state.autofilled_type);
    autofill_field.set_filling_product(previous_state.filling_product);
  }
  form.set_fields(std::move(fields));

  // Do not attempt a refill after an Undo operation.
  if (GetFillingContext(form.global_id())) {
    SetFillingContext(form.global_id(), nullptr);
  }

  // Since Undo only affects fields that were already filled, and only sets
  // values to fields to something that already existed in it prior to the
  // filling, it is okay to bypass the filling security checks and hence passing
  // dummy values for `triggered_origin` and `field_type_map`.
  manager_->driver().ApplyFormAction(mojom::FormActionType::kUndo,
                                     action_persistence, form.fields(),
                                     url::Origin(),
                                     /*field_type_map=*/{});

  FillingProduct filling_product = operation.get_filling_product();
  if (action_persistence != mojom::ActionPersistence::kPreview) {
    // History is not cleared on previews as it might be used for future
    // previews or for the filling.
    form_autofill_history_.EraseFormFillEntry(std::move(operation));
  }
  return filling_product;
}

void FormFiller::FillOrPreviewField(mojom::ActionPersistence action_persistence,
                                    mojom::FieldActionType action_type,
                                    const FormFieldData& field,
                                    AutofillField* autofill_field,
                                    const std::u16string& value,
                                    FillingProduct filling_product,
                                    std::optional<FieldType> field_type_used) {
  if (autofill_field && action_persistence == mojom::ActionPersistence::kFill) {
    autofill_field->set_is_autofilled(true);
    autofill_field->set_autofilled_type(field_type_used);
    autofill_field->set_filling_product(filling_product);
    autofill_field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
        .fill_event_id = GetNextFillEventId(),
        .had_value_before_filling = ToOptionalBoolean(!field.value().empty()),
        .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
        .was_autofilled_before_security_policy = ToOptionalBoolean(true),
        .had_value_after_filling = ToOptionalBoolean(true),
        .filling_method = FillingMethod::kFieldByFieldFilling});

    if (ShouldRecordFillingHistory(filling_product)) {
      // TODO(crbug.com/40232021): Only use AutofillField.
      form_autofill_history_.AddFormFillEntry(
          std::to_array<const FormFieldData*>({&field}),
          std::to_array<const AutofillField*>({autofill_field}),
          filling_product,
          /*is_refill=*/false);
    }
  }
  manager_->driver().ApplyFieldAction(action_type, action_persistence,
                                      field.global_id(), value);
}

void FormFiller::FillOrPreviewFormWithPredictionImprovements(
    mojom::ActionPersistence action_persistence,
    const FieldTypeSet& field_types_to_fill,
    const DenseSet<FieldFillingSkipReason>& ignorable_skip_reasons,
    const FormData& form,
    const FormFieldData& trigger_field,
    FormStructure& form_structure,
    const AutofillField& autofill_trigger_field,
    const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill) {
  std::vector<FormFieldData> result_fields = form.fields();
  // Previously, the following if statement wasn't there and instead a CHECK
  // expecting equal number of fields in `form` and `form_structure`. However,
  // dynamic form changes can cause the numbers of fields to differ which caused
  // a crash when this method was called by Autofill prediction improvements.
  // Return early here to mitigate further crashes.
  // TODO(crbug.com/372026861): Properly handle this case.
  if (result_fields.size() != form_structure.field_count()) {
    return;
  }

  // `FormFiller::GetFieldFillingSkipReasons` returns for each field a generic
  // list of reason for skipping each field. Some of these reasons might not be
  // relevant for the current context (given `ignorable_skip_reasons`) so we
  // filter them out from the start.
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      base::MakeFlatMap<FieldGlobalId, DenseSet<FieldFillingSkipReason>>(
          GetFieldFillingSkipReasons(
              result_fields, form_structure, autofill_trigger_field,
              field_types_to_fill,
              /*type_groups_originally_filled=*/std::nullopt,
              FillingProduct::kPredictionImprovements,
              /*skip_unrecognized_autocomplete_fields=*/false,
              /*is_refill=*/false,
              /*is_expired_credit_card=*/false),
          {},
          [&ignorable_skip_reasons](
              const std::pair<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
                  field_id_and_skip_reasons) {
            auto [field_id, field_skip_reasons] = field_id_and_skip_reasons;
            field_skip_reasons.erase_all(ignorable_skip_reasons);
            return std::make_pair(field_id, field_skip_reasons);
          });

  for (size_t i = 0; i < result_fields.size(); ++i) {
    FormFieldData& result_field = result_fields[i];

    // Skip fields that don't have a value to fill.
    if (!values_to_fill.contains(result_field.global_id()) ||
        values_to_fill.at(result_field.global_id()).empty()) {
      skip_reasons[result_field.global_id()].insert(
          FieldFillingSkipReason::kNoValueToFill);
    }
    if (!skip_reasons[result_field.global_id()].empty()) {
      continue;
    }

    // Fill the field.
    result_field.set_value(values_to_fill.at(result_field.global_id()));
    result_field.set_is_autofilled(true);
    if (action_persistence == mojom::ActionPersistence::kFill) {
      // TODO(crbug.com/40227496): Set also `AutofillField::value_` here.
      AutofillField& autofill_field = *form_structure.field(i);
      autofill_field.set_is_autofilled(true);
      autofill_field.set_filling_product(
          FillingProduct::kPredictionImprovements);
    }

    const bool autofilled_value_did_not_change =
        form.fields()[i].is_autofilled() && result_field.is_autofilled() &&
        form.fields()[i].value() == result_field.value();
    if (autofilled_value_did_not_change) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kAutofilledValueDidNotChange);
    }
  }

  std::erase_if(result_fields, [&skip_reasons](const FormFieldData& field) {
    return !skip_reasons[field.global_id()].empty();
  });

  std::ignore = manager_->driver().ApplyFormAction(
      mojom::FormActionType::kFill, action_persistence, result_fields,
      trigger_field.origin(), {});
}

void FormFiller::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    FormStructure* form_structure,
    AutofillField* autofill_trigger_field,
    const AutofillTriggerDetails& trigger_details,
    bool is_refill) {
  FillingProduct filling_product =
      absl::holds_alternative<const CreditCard*>(profile_or_credit_card)
          ? FillingProduct::kCreditCard
          : FillingProduct::kAddress;

  DCHECK(form_structure);
  DCHECK(autofill_trigger_field);

  LogBuffer buffer(IsLoggingActive(log_manager_));
  LOG_AF(buffer) << "action_persistence: "
                 << ActionPersistenceToString(action_persistence);
  LOG_AF(buffer) << "is credit card section: "
                 << (filling_product == FillingProduct::kCreditCard) << Br{};
  LOG_AF(buffer) << "is refill: " << is_refill << Br{};
  LOG_AF(buffer) << *form_structure << Br{};
  LOG_AF(buffer) << Tag{"table"};

  // TODO(crbug/1203667#c9): Skip if the form has changed in the meantime, which
  // may happen with refills.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    base::UmaHistogramBoolean(
        "Autofill.SkippingFormFillDueToChangedFieldCount",
        form_structure->field_count() != form.fields().size());
  }
  if (form_structure->field_count() != form.fields().size()) {
    LOG_AF(buffer)
        << Tr{} << "*"
        << "Skipped filling of form because the number of fields to be "
           "filled differs from the number of fields registered in the form "
           "cache."
        << CTag{"table"};
    LOG_AF(log_manager_) << LoggingScope::kFilling
                         << LogMessage::kSendFillingData << Br{}
                         << std::move(buffer);
    return;
  }

  if (action_persistence == mojom::ActionPersistence::kFill && !is_refill) {
    form_structure->set_last_filling_timestamp(base::TimeTicks::Now());
    SetFillingContext(form_structure->global_id(),
                      std::make_unique<FillingContext>(*autofill_trigger_field,
                                                       profile_or_credit_card));
  }

  // Only record the types that are filled for an eventual refill if all the
  // following are satisfied:
  //  The form is already filled.
  //  A refill has not been attempted for that form yet.
  //  This fill is not a refill attempt.
  FillingContext* filling_context =
      GetFillingContext(form_structure->global_id());
  bool could_attempt_refill = filling_context != nullptr &&
                              !filling_context->attempted_refill && !is_refill;

  std::vector<FormFieldData> result_fields = form.fields();
  CHECK_EQ(result_fields.size(), form_structure->field_count());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    // On the renderer, the section is used regardless of the autofill status.
    result_fields[i].set_section(form_structure->field(i)->section());
  }

  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      GetFieldFillingSkipReasons(
          result_fields, *form_structure, *autofill_trigger_field,
          trigger_details.field_types_to_fill,
          filling_context ? filling_context->type_groups_originally_filled
                          : std::optional<DenseSet<FieldTypeGroup>>(),
          filling_product,
          /*skip_unrecognized_autocomplete_fields=*/
          trigger_details.trigger_source !=
              AutofillTriggerSource::kManualFallback,
          is_refill,
          filling_product == FillingProduct::kCreditCard &&
              absl::get<const CreditCard*>(profile_or_credit_card)
                  ->IsExpired(AutofillClock::Now()));

  // This loop sets the values to fill in the `result_fields`. The
  // `result_fields` are sent to the renderer, whereas the very similar
  // `form_structure->fields()` remains in the browser process.
  // The fill value is determined by FillForm().
  for (size_t i = 0; i < result_fields.size(); ++i) {
    AutofillField* autofill_field = form_structure->field(i);
    constexpr DenseSet<FieldFillingSkipReason> kPreUkmLoggingSkips{
        FieldFillingSkipReason::kNotInFilledSection,
        FieldFillingSkipReason::kFormChanged,
        FieldFillingSkipReason::kNotFocused};
    if (!kPreUkmLoggingSkips.contains_any(
            skip_reasons[autofill_field->global_id()]) &&
        !autofill_field->IsFocusable()) {
      manager_->form_interactions_ukm_logger()
          ->LogHiddenRepresentationalFieldSkipDecision(
              *form_structure, *autofill_field,
              !autofill_field->IsSelectElement());
    }
    if (!skip_reasons[autofill_field->global_id()].empty()) {
      const FieldFillingSkipReason skip_reason =
          *skip_reasons[autofill_field->global_id()].begin();
      LOG_AF(buffer) << Tr{} << base::StringPrintf("Field %zu", i)
                     << GetSkipFieldFillLogMessage(skip_reason);
      continue;
    }

    if (could_attempt_refill) {
      filling_context->type_groups_originally_filled.insert(
          autofill_field->Type().group());
    }
    std::string failure_to_fill;  // Reason for failing to fill.
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values =
        filling_context ? filling_context->forced_fill_values
                        : std::map<FieldGlobalId, std::u16string>();

    // Fill the non-empty value from `profile_or_credit_card` into the
    // `result_form` form, which will be sent to the renderer.
    // FillField() may also fill a field if it had been autofilled or manually
    // filled before, and also returns true in such a case; however, such fields
    // don't reach this code.
    const bool is_newly_autofilled =
        FillField(*autofill_field, profile_or_credit_card, forced_fill_values,
                  result_fields[i], action_persistence, &failure_to_fill);
    const bool autofilled_value_did_not_change =
        form.fields()[i].is_autofilled() && result_fields[i].is_autofilled() &&
        form.fields()[i].value() == result_fields[i].value();
    if (is_newly_autofilled && !autofilled_value_did_not_change) {
      // For credit card fields, override the autofilled field value if the
      // field is autofilled.
      if (AllowPaymentSwapping(*autofill_trigger_field, *autofill_field,
                               is_refill) &&
          form.fields()[i].is_autofilled() &&
          result_fields[i].is_autofilled() &&
          form.fields()[i].value() != result_fields[i].value()) {
        // Override the autofilled value.
        result_fields[i].set_force_override(true);
      }
    } else if (is_newly_autofilled) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kAutofilledValueDidNotChange);
    } else {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kNoValueToFill);
    }

    const bool has_value_before = !result_fields[i].value().empty();
    const bool has_value_after = !result_fields[i].value().empty();
    const bool is_autofilled_before = form.fields()[i].is_autofilled();
    const bool is_autofilled_after = result_fields[i].is_autofilled();
    LOG_AF(buffer)
        << Tr{}
        << base::StringPrintf(
               "Field %zu Fillable - has value: %d->%d; autofilled: %d->%d. %s",
               i, has_value_before, has_value_after, is_autofilled_before,
               is_autofilled_after, failure_to_fill.c_str());
  }
  if (could_attempt_refill) {
    filling_context->filled_form = form;
    filling_context->filled_form->set_fields(result_fields);
  }
  auto field_types = base::MakeFlatMap<FieldGlobalId, FieldType>(
      *form_structure, {}, [](const auto& field) {
        return std::make_pair(field->global_id(),
                              field->Type().GetStorableType());
      });
  // Remove fields that won't be filled. This includes:
  // - Fields that have a skip reason.
  // - Fields that don't have a cached equivalent, because those fields don't
  //   have skip reasons and yet won't be filled.
  std::erase_if(result_fields,
                [&skip_reasons, &form_structure](const FormFieldData& field) {
                  return !skip_reasons[field.global_id()].empty() ||
                         !form_structure->GetFieldById(field.global_id());
                });
  base::flat_set<FieldGlobalId> safe_filled_field_ids =
      manager_->driver().ApplyFormAction(
          mojom::FormActionType::kFill, action_persistence, result_fields,
          autofill_trigger_field->origin(), field_types);

  // This will hold the subset of fields of `result_fields` whose ids are in
  // `safe_filled_field_ids`
  struct {
    std::vector<const FormFieldData*> old_values;
    std::vector<const FormFieldData*> new_values;
    std::vector<const AutofillField*> cached;
  } safe_filled_fields;

  for (const FormFieldData& field : result_fields) {
    const FieldGlobalId field_id = field.global_id();
    if (safe_filled_field_ids.contains(field_id)) {
      // A safe field was filled. Both functions will not return a nullptr
      // because they passed the `FieldFillingSkipReason::kFormChanged`
      // condition.
      safe_filled_fields.old_values.push_back(
          form.FindFieldByGlobalId(field_id));
      safe_filled_fields.new_values.push_back([&] {
        auto fields_it = base::ranges::find(result_fields, field_id,
                                            &FormFieldData::global_id);
        return fields_it != result_fields.end() ? &*fields_it : nullptr;
      }());
      safe_filled_fields.cached.push_back(
          form_structure->GetFieldById(field_id));
    } else {
      auto it = base::ranges::find(form.fields(), field_id,
                                   &FormFieldData::global_id);
      CHECK(it != result_fields.end());
      std::string field_number =
          base::StringPrintf("Field %zu", it - result_fields.begin());
      LOG_AF(buffer) << Tr{} << field_number
                     << "Actually did not fill field because of the iframe "
                        "security policy.";
    }
  }

  // Save filling history to support undoing it later if needed.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    form_autofill_history_.AddFormFillEntry(safe_filled_fields.old_values,
                                            safe_filled_fields.cached,
                                            filling_product, is_refill);
  }

  LOG_AF(buffer) << CTag{"table"};
  LOG_AF(log_manager_) << LoggingScope::kFilling << LogMessage::kSendFillingData
                       << Br{} << std::move(buffer);

  if (filling_context) {
    // When a new preview/fill starts, previously forced_fill_values should be
    // ignored the operation could be for a different card or address.
    filling_context->forced_fill_values.clear();
  }

  manager_->OnDidFillOrPreviewForm(
      action_persistence, form, *form_structure, *autofill_trigger_field,
      safe_filled_fields.new_values, safe_filled_fields.cached,
      base::MakeFlatSet<FieldGlobalId>(result_fields, {},
                                       &FormFieldData::global_id),
      safe_filled_field_ids, skip_reasons, profile_or_credit_card,
      trigger_details, is_refill);
}

bool FormFiller::ShouldTriggerRefill(
    const FormStructure& form_structure,
    RefillTriggerReason refill_trigger_reason) {
  // Should not refill if a form with the same FormGlobalId that has not been
  // filled before.
  FillingContext* filling_context =
      GetFillingContext(form_structure.global_id());
  if (filling_context == nullptr) {
    return false;
  }

  // Confirm that the form changed by running a DeepEqual check on the filled
  // form and the received form. Other trigger reasons do not need this check
  // since they do not depend on the form changing.
  if (refill_trigger_reason == RefillTriggerReason::kFormChanged &&
      filling_context->filled_form &&
      FormData::DeepEqual(form_structure.ToFormData(),
                          *filling_context->filled_form)) {
    return false;
  }

  // TODO(crbug.com/41490871): Use form_structure.last_filling_timestamp()
  // instead of filling_context->original_fill_time
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - filling_context->original_fill_time;

  return !filling_context->attempted_refill && delta < limit_before_refill_;
}

void FormFiller::ScheduleRefill(const FormData& form,
                                const FormStructure& form_structure,
                                const AutofillTriggerDetails& trigger_details) {
  FillingContext* filling_context =
      GetFillingContext(form_structure.global_id());
  DCHECK(filling_context != nullptr);
  // If a timer for the refill was already running, it means the form
  // changed again. Stop the timer and start it again.
  if (filling_context->on_refill_timer.IsRunning()) {
    filling_context->on_refill_timer.Stop();
  }
  // Start a new timer to trigger refill.
  filling_context->on_refill_timer.Start(
      FROM_HERE, kWaitTimeForDynamicForms,
      base::BindRepeating(&FormFiller::TriggerRefill,
                          weak_ptr_factory_.GetWeakPtr(), form,
                          trigger_details));
}

void FormFiller::TriggerRefill(const FormData& form,
                               const AutofillTriggerDetails& trigger_details) {
  FormStructure* form_structure =
      manager_->FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  FillingContext* filling_context =
      GetFillingContext(form_structure->global_id());
  DCHECK(filling_context);

  // The refill attempt can happen from different paths, some of which happen
  // after waiting for a while. Therefore, although this condition has been
  // checked prior to calling TriggerRefill, it may not hold, when we get
  // here.
  if (filling_context->attempted_refill) {
    return;
  }
  filling_context->attempted_refill = true;

  // Try to find the field from which the original fill originated.
  // The precedence for the look up is the following:
  //  - focusable `filled_field_id`
  //  - focusable `filled_field_signature`
  //  - non-focusable `filled_field_id`
  //  - non-focusable `filled_field_signature`
  // and prefer newer renderer ids.
  auto comparison_attributes =
      [&](const std::unique_ptr<AutofillField>& field) {
        return std::make_tuple(
            field->origin() == filling_context->filled_origin,
            field->IsFocusable(),
            field->global_id() == filling_context->filled_field_id,
            field->GetFieldSignature() ==
                filling_context->filled_field_signature,
            field->renderer_id());
      };
  auto it =
      base::ranges::max_element(*form_structure, {}, comparison_attributes);
  AutofillField* autofill_field =
      it != form_structure->end() ? it->get() : nullptr;
  bool found_matching_element =
      autofill_field &&
      autofill_field->origin() == filling_context->filled_origin &&
      (autofill_field->global_id() == filling_context->filled_field_id ||
       autofill_field->GetFieldSignature() ==
           filling_context->filled_field_signature);
  if (!found_matching_element) {
    return;
  }
  absl::visit(
      [&](const auto& profile_or_credit_card) {
        FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                          &profile_or_credit_card, form_structure,
                          autofill_field, trigger_details,
                          /*is_refill=*/true);
      },
      filling_context->profile_or_credit_card);
}

void FormFiller::MaybeTriggerRefillForExpirationDate(
    const FormData& form,
    const FormFieldData& field,
    const FormStructure& form_structure,
    const std::u16string& old_value,
    const AutofillTriggerDetails& trigger_details) {
  // We currently support a single case of refilling credit card expiration
  // dates: If we filled the expiration date in a format "05/2023" and the
  // website turned it into "05 / 20" (i.e. it broke the year by cutting the
  // last two digits instead of stripping the first two digits).
  constexpr size_t kSupportedLength = std::string_view("MM/YYYY").size();
  if (old_value.length() != kSupportedLength) {
    return;
  }
  if (old_value == field.value()) {
    return;
  }
  static constexpr char16_t kFormatRegEx[] =
      uR"(^(\d\d)(\s?[/-]?\s?)?(\d\d|\d\d\d\d)$)";
  std::vector<std::u16string> old_groups;
  if (!MatchesRegex<kFormatRegEx>(old_value, &old_groups)) {
    return;
  }
  DCHECK_EQ(old_groups.size(), 4u);

  std::vector<std::u16string> new_groups;
  if (!MatchesRegex<kFormatRegEx>(field.value(), &new_groups)) {
    return;
  }
  DCHECK_EQ(new_groups.size(), 4u);

  int old_month, old_year, new_month, new_year;
  if (!base::StringToInt(old_groups[1], &old_month) ||
      !base::StringToInt(old_groups[3], &old_year) ||
      !base::StringToInt(new_groups[1], &new_month) ||
      !base::StringToInt(new_groups[3], &new_year) ||
      old_groups[3].size() != 4 || new_groups[3].size() != 2 ||
      old_month != new_month ||
      // We need to refill if the first two digits of the year were preserved.
      old_year / 100 != new_year) {
    return;
  }
  std::u16string refill_value = field.value();
  CHECK(refill_value.size() >= 2);
  refill_value[refill_value.size() - 1] = '0' + (old_year % 10);
  refill_value[refill_value.size() - 2] = '0' + ((old_year % 100) / 10);

  if (ShouldTriggerRefill(form_structure,
                          RefillTriggerReason::kExpirationDateFormatted)) {
    FillingContext* filling_context =
        GetFillingContext(form_structure.global_id());
    DCHECK(filling_context);  // This is enforced by ShouldTriggerRefill.
    filling_context->forced_fill_values[field.global_id()] = refill_value;
    ScheduleRefill(form, form_structure, trigger_details);
  }
}

void FormFiller::SetFillingContext(FormGlobalId form_id,
                                   std::unique_ptr<FillingContext> context) {
  filling_context_[form_id] = std::move(context);
}

FormFiller::FillingContext* FormFiller::GetFillingContext(
    FormGlobalId form_id) {
  auto it = filling_context_.find(form_id);
  return it != filling_context_.end() ? it->second.get() : nullptr;
}

FormFiller::FieldFillingData FormFiller::GetFieldFillingData(
    const AutofillField& autofill_field,
    const absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    const FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  auto it = forced_fill_values.find(field_data.global_id());
  bool value_is_an_override = it != forced_fill_values.end();
  const auto& [value_to_fill, filling_type] =
      value_is_an_override
          ? std::make_pair(it->second, autofill_field.Type().GetStorableType())
      : absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card)
          ? GetFillingValueAndTypeForProfile(
                *absl::get<const AutofillProfile*>(profile_or_credit_card),
                app_locale_, autofill_field.Type(), field_data,
                manager_->client().GetAddressNormalizer(), failure_to_fill)
          : std::make_pair(
                GetFillingValueForCreditCard(
                    *absl::get<const CreditCard*>(profile_or_credit_card),
                    app_locale_, action_persistence, autofill_field,
                    failure_to_fill),
                autofill_field.Type().GetStorableType());
  return {value_to_fill, filling_type, value_is_an_override};
}

bool FormFiller::FillField(
    AutofillField& autofill_field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  const FieldFillingData filling_content = GetFieldFillingData(
      autofill_field, profile_or_credit_card, forced_fill_values, field_data,
      action_persistence, failure_to_fill);

  // Do not attempt to fill empty values as it would skew the metrics.
  if (filling_content.value_to_fill.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "No value to fill available. ";
    }
    return false;
  }
  field_data.set_value(filling_content.value_to_fill);
  field_data.set_force_override(filling_content.value_is_an_override);

  if (failure_to_fill) {
    *failure_to_fill = "Decided to fill";
  }
  if (action_persistence == mojom::ActionPersistence::kFill) {
    // Mark the cached field as autofilled, so that we can detect when a
    // user edits an autofilled field (for metrics).
    autofill_field.set_is_autofilled(true);
    autofill_field.set_filling_product(
        absl::holds_alternative<const CreditCard*>(profile_or_credit_card)
            ? FillingProduct::kCreditCard
            : FillingProduct::kAddress);
    if (const AutofillProfile** profile =
            absl::get_if<const AutofillProfile*>(&profile_or_credit_card)) {
      autofill_field.set_autofill_source_profile_guid((*profile)->guid());
    }
    autofill_field.set_autofilled_type(filling_content.field_type);
  }
  // Mark the field as autofilled when a non-empty value is assigned to
  // it. This allows the renderer to distinguish autofilled fields from
  // fields with non-empty values, such as select-one fields.
  field_data.set_is_autofilled(true);
  return true;
}

}  // namespace autofill

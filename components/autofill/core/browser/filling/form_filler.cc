// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/form_filler.h"

#include <array>
#include <optional>
#include <utility>
#include <variant>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/overloaded.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/payments/field_filling_payments_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/per_fill_metrics.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// Time to wait after a dynamic form change before triggering a refill.
// This is used for sites that change multiple things consecutively.
constexpr base::TimeDelta kWaitTimeForDynamicForms = base::Milliseconds(200);

bool FillingProductSupportsRefills(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kCreditCard:
      return true;
    case FillingProduct::kAutocomplete:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCompose:
    case FillingProduct::kIban:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kIdentityCredential:
      return false;
    case FillingProduct::kPassword:
    case FillingProduct::kNone:
      NOTREACHED();
  }
}

FillingProduct GetFillingProductFromFillingPayload(
    const FillingPayload& filling_payload) {
  return std::visit(
      base::Overloaded{
          [](const AutofillProfile*) { return FillingProduct::kAddress; },
          [](const CreditCard*) { return FillingProduct::kCreditCard; },
          [](const EntityInstance*) { return FillingProduct::kAutofillAi; },
          [](const VerifiedProfile*) {
            return FillingProduct::kIdentityCredential;
          }},
      filling_payload);
}

// Given `filling_product`, returns the types supported for filling by this
// FillingProduct, or std::nullopt if `filling_product` is independent of field
// classifications.
std::optional<FieldTypeSet> GetFieldTypesToFillFromFillingProduct(
    FillingProduct filling_product) {
  FieldTypeSet field_types;
  switch (filling_product) {
    case FillingProduct::kAddress:
      for (FieldType field_type : kAllFieldTypes) {
        if (IsAddressType(field_type)) {
          field_types.insert(field_type);
        }
      }
      return field_types;
    case FillingProduct::kCreditCard:
      for (FieldType field_type : kAllFieldTypes) {
        if (FieldTypeGroupSet({FieldTypeGroup::kCreditCard,
                               FieldTypeGroup::kStandaloneCvcField})
                .contains(GroupTypeOfFieldType(field_type))) {
          field_types.insert(field_type);
        }
      }
      return field_types;
    case FillingProduct::kAutofillAi:
      static constexpr auto kAutofillAiFieldTypes = []() {
        DenseSet<FieldType> result;
        for (AttributeType type : DenseSet<AttributeType>::all()) {
          result.insert(type.field_type());
        }
        return result;
      }();
      return kAutofillAiFieldTypes;
    case FillingProduct::kPassword:
      for (FieldType field_type : kAllFieldTypes) {
        if (FieldTypeGroupSet({FieldTypeGroup::kUsernameField,
                               FieldTypeGroup::kPasswordField})
                .contains(GroupTypeOfFieldType(field_type))) {
          field_types.insert(field_type);
        }
      }
      return field_types;
    case FillingProduct::kMerchantPromoCode:
      return FieldTypeSet{MERCHANT_PROMO_CODE};
    case FillingProduct::kIban:
      return FieldTypeSet{IBAN_VALUE};
    case FillingProduct::kLoyaltyCard:
      return FieldTypeSet{LOYALTY_MEMBERSHIP_ID};
    case FillingProduct::kPlusAddresses:
      return FieldTypeSet{EMAIL_ADDRESS};
    case FillingProduct::kIdentityCredential:
      return FieldTypeSet{EMAIL_ADDRESS, NAME_FIRST, NAME_FULL};
    case FillingProduct::kAutocomplete:
    case FillingProduct::kCompose:
      return std::nullopt;
    case FillingProduct::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

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
  if (base::TrimWhitespace(field.initial_value(), base::TrimPositions::TRIM_ALL)
          .empty()) {
    return false;
  }
  // If the field's initial value coincides with the value of its placeholder
  // attribute, don't consider the initial value to be meaningful.
  if (field.initial_value() == field.placeholder()) {
    return false;
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
         !is_refill && IsPaymentsFieldSwappingEnabled();
}

// Returns whether a filling action for `filling_product` should be included in
// the form autofill history.
bool ShouldRecordFillingHistory(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCreditCard:
    case FillingProduct::kPlusAddresses:
      return true;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kIdentityCredential:
      return false;
  }
  NOTREACHED();
}

// Called by `FormFiller::MaybeTriggerRefill()` and constructs a refill value in
// case the website used JavaScript to reformat an expiration date like
// "05/2023" into "05 / 20" (i.e. it broke the year by cutting the last two
// digits instead of stripping the first two digits).
std::optional<std::u16string> GetRefillValueForExpirationDate(
    const FormFieldData& field,
    const std::u16string& old_value) {
  // We currently support a single case of refilling credit card expiration
  // dates: If we filled the expiration date in a format "05/2023" and the
  // website turned it into "05 / 20" (i.e. it broke the year by cutting the
  // last two digits instead of stripping the first two digits).
  constexpr size_t kSupportedLength = std::string_view("MM/YYYY").size();
  if (old_value.length() != kSupportedLength) {
    return std::nullopt;
  }
  if (old_value == field.value()) {
    return std::nullopt;
  }
  static constexpr char16_t kFormatRegEx[] =
      uR"(^(\d\d)(\s?[/-]?\s?)?(\d\d|\d\d\d\d)$)";
  std::vector<std::u16string> old_groups;
  if (!MatchesRegex<kFormatRegEx>(old_value, &old_groups)) {
    return std::nullopt;
  }
  DCHECK_EQ(old_groups.size(), 4u);

  std::vector<std::u16string> new_groups;
  if (!MatchesRegex<kFormatRegEx>(field.value(), &new_groups)) {
    return std::nullopt;
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
    return std::nullopt;
  }
  std::u16string refill_value = field.value();
  CHECK(refill_value.size() >= 2);
  refill_value[refill_value.size() - 1] = '0' + (old_year % 10);
  refill_value[refill_value.size() - 2] = '0' + ((old_year % 100) / 10);
  return refill_value;
}

}  // namespace

DenseSet<FieldFillingSkipReason> FormFiller::GetFillingSkipReasonsForField(
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const AutofillField& trigger_field,
    base::flat_map<FieldType, size_t>& type_count,
    const std::optional<DenseSet<FieldTypeGroup>> type_groups_originally_filled,
    const base::flat_set<FieldGlobalId>& blocked_fields,
    FillingProduct filling_product,
    bool is_refill) {
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

  // An address fields with unrecognized autocomplete attribute is only filled
  // when it is the field triggering the filling operation.
  add_if(!is_trigger_field &&
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
  add_if(
      (field.properties_mask() & kUserTyped) &&
          !(field.value().empty() && autofill_field.initial_value().empty()) &&
          !is_trigger_field,
      FieldFillingSkipReason::kUserFilledFields);

  // Don't fill previously autofilled fields except the initiating field or
  // when it's a refill or for credit card fields, when
  // `kAutofillPaymentsFieldSwapping` is enabled.
  add_if(field.is_autofilled() && !is_trigger_field && !is_refill &&
             !AllowPaymentSwapping(trigger_field, autofill_field, is_refill),
         FieldFillingSkipReason::kAlreadyAutofilled);

  FieldType field_type = autofill_field.Type().GetStorableType();
  std::optional<FieldType> autofill_ai_type =
      autofill_field.GetAutofillAiServerTypePredictions();

  // On a refill, only fill fields from type groups that were present during
  // the initial fill.
  add_if(is_refill && type_groups_originally_filled.has_value() &&
             !base::Contains(*type_groups_originally_filled,
                             GroupTypeOfFieldType(field_type)),
         FieldFillingSkipReason::kRefillNotInInitialFill);

  // A field with a specific type is only allowed to be filled a limited
  // number of times given by |TypeValueFormFillingLimit(field_type)|.
  add_if(++type_count[field_type] > TypeValueFormFillingLimit(field_type),
         FieldFillingSkipReason::kFillingLimitReachedType);

  std::optional<FieldTypeSet> supported_types =
      GetFieldTypesToFillFromFillingProduct(filling_product);
  // This ensures that a filling product only operates on fields of supported
  // types.
  add_if(
      supported_types && !supported_types->contains(field_type) &&
          (!autofill_ai_type || !supported_types->contains(*autofill_ai_type)),
      FieldFillingSkipReason::kFieldTypeUnrelated);

  // Don't fill meaningfully pre-filled fields but overwrite placeholders.
  add_if(ShouldSkipFieldBecauseOfMeaningfulInitialValue(autofill_field,
                                                        is_trigger_field),
         FieldFillingSkipReason::kValuePrefilled);

  // Do not fill fields that are blocked by another filling product.
  add_if(blocked_fields.contains(field.global_id()),
         FieldFillingSkipReason::kBlockedByOtherFillingProduct);

  return skip_reasons;
}

FormFiller::RefillContext::RefillContext(const AutofillField& field,
                                         const FillingPayload& filling_payload)
    : filled_field_id(field.global_id()),
      filled_field_signature(field.GetFieldSignature()),
      filled_origin(field.origin()),
      original_fill_time(base::TimeTicks::Now()) {
  profile_or_credit_card = std::visit(
      base::Overloaded{
          // Autofill with AI doesn't support refills.
          [](const EntityInstance*)
              -> std::variant<CreditCard, AutofillProfile> { NOTREACHED(); },
          // Verified Profiles doesn't support refills.
          [](const VerifiedProfile*)
              -> std::variant<CreditCard, AutofillProfile> { NOTREACHED(); },
          [](const auto* x) {
            return std::variant<CreditCard, AutofillProfile>(*x);
          }},
      filling_payload);
}

FormFiller::RefillContext::~RefillContext() = default;

FormFiller::FormFiller(BrowserAutofillManager& manager) : manager_(manager) {}

FormFiller::~FormFiller() = default;

LogManager* FormFiller::log_manager() {
  return manager_->client().GetCurrentLogManager();
}

void FormFiller::Reset() {
  refill_context_.clear();
  form_autofill_history_.Reset();
}

base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
FormFiller::GetFieldFillingSkipReasons(
    base::span<const FormFieldData> fields,
    const FormStructure& form_structure,
    const AutofillField& trigger_field,
    std::optional<DenseSet<FieldTypeGroup>> type_groups_originally_filled,
    FillingProduct filling_product,
    bool is_refill) const {
  // Counts the number of times a type was seen in the section to be filled.
  // This is used to limit the maximum number of fills per value.
  base::flat_map<FieldType, size_t> type_count;
  type_count.reserve(form_structure.field_count());

  base::flat_set<FieldGlobalId> blocked_fields;
  if (filling_product == FillingProduct::kAddress) {
    blocked_fields =
        GetFieldsFillableByAutofillAi(form_structure, manager_->client());
  }

  CHECK_EQ(fields.size(), form_structure.field_count());
  auto skip_reasons =
      base::MakeFlatMap<FieldGlobalId, DenseSet<FieldFillingSkipReason>>(
          form_structure, {}, [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(),
                                  DenseSet<FieldFillingSkipReason>{});
          });
  for (auto [field, autofill_field] :
       base::zip(fields, form_structure.fields())) {
    // Log events when the fields on the form are filled by autofill
    // suggestion.
    DenseSet<FieldFillingSkipReason> field_skip_reasons =
        GetFillingSkipReasonsForField(field, *autofill_field, trigger_field,
                                      type_count, type_groups_originally_filled,
                                      blocked_fields, filling_product,
                                      is_refill);

    // Usually, `skip_reasons[field_id].empty()` before executing the line
    // below. It may not be the case though because FieldGlobalIds may not be
    // unique among `FormData::fields_` (see crbug.com/41496988), so a previous
    // iteration may have added skip reasons for `field_id`. To err on the side
    // of caution we accumulate all skip reasons found in any iteration.
    skip_reasons[autofill_field->global_id()].insert_all(field_skip_reasons);
  }
  return skip_reasons;
}

void FormFiller::UndoAutofill(mojom::ActionPersistence action_persistence,
                              FormData form,
                              FormStructure& form_structure,
                              const FormFieldData& trigger_field,
                              FillingProduct filling_product) {
  if (!form_autofill_history_.HasHistory(trigger_field.global_id())) {
    LOG_AF(log_manager())
        << "Could not undo the filling operation on field "
        << trigger_field.global_id()
        << " because history was dropped upon reaching history limit of "
        << kMaxStorableFieldFillHistory;
    return;
  }

  const auto fill_operation_it =
      form_autofill_history_.GetLastFormFillingEntryForField(
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
  std::erase_if(fields, [&](const FormFieldData& field) {
    const auto field_fill_operation_it =
        form_autofill_history_.GetLastFormFillingEntryForField(
            field.global_id());
    return
        // Skip fields whose last autofill operation is different
        // than the one of the trigger field.
        field_fill_operation_it != fill_operation_it ||
        // Skip not-autofilled fields as undo only acts on autofilled
        // fields. Only exception is the fields that were emptied due to
        // suggestion swapping.
        // Note that `field_fill_operation` is guaranteed to have an entry for
        // `field.global_id()` because of the condition right above.
        (!field.is_autofilled() && !field.value().empty() &&
         field_fill_operation_it->at(field.global_id()).ignore_is_autofilled) ||
        // Skip fields that are not cached to avoid unexpected outcomes.
        !cached_fields.contains(field.global_id()) ||
        // Skip fields which have a different filling product than the trigger
        // field. This is to avoid modifying a field that was autofilled later
        // with a filling product that doesn't support Undo (e.g.,
        // Autocomplete).
        cached_fields[field.global_id()]->filling_product() != filling_product;
  });

  for (FormFieldData& field : fields) {
    AutofillField& autofill_field =
        CHECK_DEREF(cached_fields[field.global_id()]);
    auto it = fill_operation_it->find(field.global_id());
    // See comments in the `erase_if` block for why this is guaranteed.
    CHECK(it != fill_operation_it->end());
    const FormAutofillHistory::FieldFillingEntry& previous_state = it->second;

    // Update the FormFieldData to be sent for the renderer.
    field.set_value(previous_state.value);
    field.set_is_autofilled(previous_state.is_autofilled);

    // Update the cached AutofillField in the browser if the operation isn't a
    // preview.
    if (action_persistence == mojom::ActionPersistence::kFill) {
      autofill_field.set_is_autofilled(previous_state.is_autofilled);
      autofill_field.set_autofill_source_profile_guid(
          previous_state.autofill_source_profile_guid);
      autofill_field.set_autofilled_type(previous_state.autofilled_type);
      autofill_field.set_filling_product(previous_state.filling_product);

      // The filling history is not cleared on previews as it might be used for
      // future previews or for the filling. it is also cleared field by field
      // because some fields in the current entry might not be used now but
      // could still be valuable (see crbug.com/416019464).
      form_autofill_history_.EraseFieldFillingEntry(fill_operation_it,
                                                    field.global_id());
    }
  }
  form.set_fields(std::move(fields));

  // Do not attempt a refill after an Undo operation.
  if (GetRefillContext(form.global_id())) {
    SetRefillContext(form.global_id(), nullptr);
  }

  // Since Undo only affects fields that were already filled, and only sets
  // values to fields to something that already existed in it prior to the
  // filling, it is okay to bypass the filling security checks and hence passing
  // dummy values for `triggered_origin` and `field_type_map`.
  manager_->driver().ApplyFormAction(mojom::FormActionType::kUndo,
                                     action_persistence, form.fields(),
                                     url::Origin(),
                                     /*field_type_map=*/{});
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
        .had_value_after_filling = ToOptionalBoolean(true)});

    if (ShouldRecordFillingHistory(filling_product)) {
      // TODO(crbug.com/40232021): Only use AutofillField.
      form_autofill_history_.AddFormFillingEntry(
          std::to_array<const FormFieldData*>({&field}),
          std::to_array<const AutofillField*>({autofill_field}),
          filling_product,
          /*is_refill=*/false);
    }
  }
  manager_->driver().ApplyFieldAction(action_type, action_persistence,
                                      field.global_id(), value);
}

void FormFiller::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const FillingPayload& filling_payload,
    FormStructure& form_structure,
    AutofillField& autofill_trigger_field,
    AutofillTriggerSource trigger_source,
    std::optional<RefillTriggerReason> refill_trigger_reason) {
  FillingProduct filling_product =
      GetFillingProductFromFillingPayload(filling_payload);

  LogBuffer buffer(IsLoggingActive(log_manager()));
  LOG_AF(buffer) << "action_persistence: "
                 << ActionPersistenceToString(action_persistence) << Br{};
  LOG_AF(buffer) << "filling product: "
                 << FillingProductToString(filling_product) << Br{};
  LOG_AF(buffer) << "is refill: " << refill_trigger_reason.has_value() << Br{};
  LOG_AF(buffer) << form_structure << Br{};
  LOG_AF(buffer) << Tag{"table"};

  // TODO(crbug/1203667#c9): Skip if the form has changed in the meantime, which
  // may happen with refills.
  if (action_persistence == mojom::ActionPersistence::kFill) {
    base::UmaHistogramBoolean(
        "Autofill.SkippingFormFillDueToChangedFieldCount",
        form_structure.field_count() != form.fields().size());
  }
  if (form_structure.field_count() != form.fields().size()) {
    LOG_AF(buffer)
        << Tr{} << "*"
        << "Skipped filling of form because the number of fields to be "
           "filled differs from the number of fields registered in the form "
           "cache."
        << CTag{"table"};
    LOG_AF(log_manager()) << LoggingScope::kFilling
                          << LogMessage::kSendFillingData << Br{}
                          << std::move(buffer);
    return;
  }

  if (action_persistence == mojom::ActionPersistence::kFill &&
      !refill_trigger_reason) {
    form_structure.set_last_filling_timestamp(base::TimeTicks::Now());
    if (FillingProductSupportsRefills(filling_product)) {
      SetRefillContext(form_structure.global_id(),
                       std::make_unique<RefillContext>(autofill_trigger_field,
                                                       filling_payload));
    }
  }

  RefillContext* refill_context = GetRefillContext(form_structure.global_id());
  bool could_attempt_refill = FillingProductSupportsRefills(filling_product) &&
                              refill_context != nullptr &&
                              !refill_context->attempted_refill &&
                              !refill_trigger_reason;

  std::vector<FormFieldData> result_fields = form.fields();
  CHECK_EQ(result_fields.size(), form_structure.field_count());
  // TODO(crbug.com/40266549): Remove when Undo launches on iOS.
  for (auto [result_field, field] :
       base::zip(result_fields, form_structure.fields())) {
    // On the renderer, the section is used regardless of the autofill status.
    result_field.set_section(field->section());
  }

  // `FormFiller::GetFieldFillingSkipReasons` returns for each field a generic
  // list of reason for skipping each field.
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      GetFieldFillingSkipReasons(
          result_fields, form_structure, autofill_trigger_field,
          refill_context ? refill_context->type_groups_originally_filled
                         : std::optional<DenseSet<FieldTypeGroup>>(),
          filling_product, refill_trigger_reason.has_value());

  // This loop sets the values to fill in the `result_fields`. The
  // `result_fields` are sent to the renderer, whereas the very similar
  // `form_structure->fields()` remains in the browser process.
  // The fill value is determined by FillForm().
  for (size_t i = 0; i < result_fields.size(); ++i) {
    AutofillField& autofill_field = CHECK_DEREF(form_structure.field(i));
    constexpr DenseSet<FieldFillingSkipReason> kPreUkmLoggingSkips{
        FieldFillingSkipReason::kNotInFilledSection,
        FieldFillingSkipReason::kFormChanged,
        FieldFillingSkipReason::kNotFocused};
    if (!kPreUkmLoggingSkips.contains_any(
            skip_reasons[autofill_field.global_id()]) &&
        !autofill_field.IsFocusable()) {
      manager_->client()
          .GetFormInteractionsUkmLogger()
          .LogHiddenRepresentationalFieldSkipDecision(
              manager_->driver().GetPageUkmSourceId(), form_structure,
              autofill_field, !autofill_field.IsSelectElement());
    }
    if (!skip_reasons[autofill_field.global_id()].empty()) {
      const FieldFillingSkipReason skip_reason =
          *skip_reasons[autofill_field.global_id()].begin();
      LOG_AF(buffer) << Tr{} << base::StringPrintf("Field %zu", i)
                     << GetSkipFieldFillLogMessage(skip_reason);
      continue;
    }

    if (could_attempt_refill) {
      refill_context->type_groups_originally_filled.insert(
          autofill_field.Type().group());
    }
    std::string failure_to_fill;  // Reason for failing to fill.
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values =
        refill_context ? refill_context->forced_fill_values
                       : std::map<FieldGlobalId, std::u16string>();

    bool allow_suggestion_swapping =
        form.fields()[i].is_autofilled() &&
        AllowPaymentSwapping(autofill_trigger_field, autofill_field,
                             refill_trigger_reason.has_value());

    // Fill the data from `filling_payload` into `result_form`, which will be
    // sent to the renderer.
    // When `allow_suggestion_swapping` is true, the fields can also be emptied.
    // In that scenario, the `field->is_autofilled()` becomes false.
    const bool is_newly_autofilled_or_emptied = FillField(
        autofill_field, filling_payload, forced_fill_values, result_fields[i],
        action_persistence, allow_suggestion_swapping, &failure_to_fill);
    const bool autofilled_value_did_not_change =
        form.fields()[i].is_autofilled() && result_fields[i].is_autofilled() &&
        form.fields()[i].value() == result_fields[i].value();

    if (is_newly_autofilled_or_emptied && autofilled_value_did_not_change) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kAutofilledValueDidNotChange);
    } else if (!is_newly_autofilled_or_emptied) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kNoValueToFill);
    }

    const bool has_value_before = !form.fields()[i].value().empty();
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
    refill_context->filled_form = form;
    refill_context->filled_form->set_fields(result_fields);
  }
  // Remove fields that won't be filled. This includes:
  // - Fields that have a skip reason.
  // - Fields that don't have a cached equivalent, because those fields don't
  //   have skip reasons and yet won't be filled.
  std::erase_if(result_fields,
                [&skip_reasons, &form_structure](const FormFieldData& field) {
                  return !skip_reasons[field.global_id()].empty() ||
                         !form_structure.GetFieldById(field.global_id());
                });
  base::flat_set<FieldGlobalId> safe_filled_field_ids =
      manager_->driver().ApplyFormAction(
          mojom::FormActionType::kFill, action_persistence, result_fields,
          autofill_trigger_field.origin(),
          base::MakeFlatMap<FieldGlobalId, FieldType>(
              form_structure, {}, [](const auto& field) {
                return std::make_pair(field->global_id(),
                                      field->Type().GetStorableType());
              }));

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
        auto fields_it = std::ranges::find(result_fields, field_id,
                                           &FormFieldData::global_id);
        return fields_it != result_fields.end() ? &*fields_it : nullptr;
      }());
      safe_filled_fields.cached.push_back(
          form_structure.GetFieldById(field_id));
    } else {
      auto it =
          std::ranges::find(form.fields(), field_id, &FormFieldData::global_id);
      CHECK(it != result_fields.end());
      std::string field_number =
          base::StringPrintf("Field %zu", it - result_fields.begin());
      LOG_AF(buffer) << Tr{} << field_number
                     << "Actually did not fill field because of the iframe "
                        "security policy.";
    }
  }

  // Save filling history to support undoing it later if needed.
  if (action_persistence == mojom::ActionPersistence::kFill &&
      ShouldRecordFillingHistory(filling_product)) {
    form_autofill_history_.AddFormFillingEntry(
        safe_filled_fields.old_values, safe_filled_fields.cached,
        filling_product, refill_trigger_reason.has_value());
  }

  LOG_AF(buffer) << CTag{"table"};
  LOG_AF(log_manager()) << LoggingScope::kFilling
                        << LogMessage::kSendFillingData << Br{}
                        << std::move(buffer);

  if (refill_context) {
    // When a new preview/fill starts, previously forced_fill_values should be
    // ignored the operation could be for a different card or address.
    refill_context->forced_fill_values.clear();
  }

  manager_->OnDidFillOrPreviewForm(
      action_persistence, form, form_structure, autofill_trigger_field,
      safe_filled_fields.new_values, safe_filled_fields.cached,
      base::MakeFlatSet<FieldGlobalId>(result_fields, {},
                                       &FormFieldData::global_id),
      safe_filled_field_ids, skip_reasons, filling_payload, trigger_source,
      refill_trigger_reason);
}

void FormFiller::MaybeTriggerRefill(
    const FormData& form,
    const FormStructure& form_structure,
    RefillTriggerReason refill_trigger_reason,
    AutofillTriggerSource trigger_source,
    base::optional_ref<const FormFieldData> field,
    base::optional_ref<const std::u16string> old_value) {
  // Should not refill if a form with the same FormGlobalId has not been filled
  // before or if it has been refilled before.
  RefillContext* refill_context = GetRefillContext(form_structure.global_id());
  if (!refill_context || refill_context->attempted_refill) {
    return;
  }

  // Should not refill a form that has been filled a long time ago as the UX
  // would appear strange.
  // TODO(crbug.com/41490871): Use form_structure.last_filling_timestamp_
  // instead of filling_context->original_fill_time.
  if (base::TimeDelta delta =
          base::TimeTicks::Now() - refill_context->original_fill_time;
      delta > limit_before_refill_) {
    return;
  }

  switch (refill_trigger_reason) {
    case RefillTriggerReason::kFormChanged:
      // Confirm that the form actually changed between filling time and
      // parsing after filling time, and otherwise do not refill.
      if (refill_context->filled_form &&
          FormData::DeepEqual(form_structure.ToFormData(),
                              *refill_context->filled_form)) {
        return;
      }
      break;
    case RefillTriggerReason::kSelectOptionsChanged:
      break;
    case RefillTriggerReason::kExpirationDateFormatted:
      CHECK(field && old_value);
      if (std::optional<std::u16string> refill_value =
              GetRefillValueForExpirationDate(*field, *old_value)) {
        refill_context->forced_fill_values[field->global_id()] = *refill_value;
        break;
      }
      return;
  }
  ScheduleRefill(form, CHECK_DEREF(refill_context), trigger_source,
                 refill_trigger_reason);
}

void FormFiller::ScheduleRefill(const FormData& form,
                                RefillContext& refill_context,
                                AutofillTriggerSource trigger_source,
                                RefillTriggerReason refill_trigger_reason) {
  // If a timer for the refill was already running, it means the form
  // changed again. Stop the timer and start it again.
  if (refill_context.on_refill_timer.IsRunning()) {
    refill_context.on_refill_timer.Stop();
  }
  // Start a new timer to trigger refill.
  refill_context.on_refill_timer.Start(
      FROM_HERE, kWaitTimeForDynamicForms,
      base::BindRepeating(&FormFiller::TriggerRefill,
                          weak_ptr_factory_.GetWeakPtr(), form, trigger_source,
                          refill_trigger_reason));
}

void FormFiller::TriggerRefill(const FormData& form,
                               AutofillTriggerSource trigger_source,
                               RefillTriggerReason refill_trigger_reason) {
  FormStructure* form_structure =
      manager_->FindCachedFormById(form.global_id());
  if (!form_structure) {
    return;
  }
  RefillContext* refill_context = GetRefillContext(form_structure->global_id());
  DCHECK(refill_context);

  // The refill attempt can happen from different paths, some of which happen
  // after waiting for a while. Therefore, although this condition has been
  // checked prior to calling TriggerRefill, it may not hold, when we get
  // here.
  if (refill_context->attempted_refill) {
    return;
  }
  refill_context->attempted_refill = true;

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
            field->origin() == refill_context->filled_origin,
            field->IsFocusable(),
            field->global_id() == refill_context->filled_field_id,
            field->GetFieldSignature() ==
                refill_context->filled_field_signature,
            field->renderer_id());
      };
  auto it =
      std::ranges::max_element(*form_structure, {}, comparison_attributes);
  AutofillField* autofill_field =
      it != form_structure->end() ? it->get() : nullptr;
  bool found_matching_element =
      autofill_field &&
      autofill_field->origin() == refill_context->filled_origin &&
      (autofill_field->global_id() == refill_context->filled_field_id ||
       autofill_field->GetFieldSignature() ==
           refill_context->filled_field_signature);
  if (!found_matching_element) {
    return;
  }

  autofill_metrics::LogRefillTriggerReason(refill_trigger_reason);
  std::visit(
      [&](const auto& profile_or_credit_card) {
        FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                          &profile_or_credit_card, *form_structure,
                          *autofill_field, trigger_source,
                          refill_trigger_reason);
      },
      refill_context->profile_or_credit_card);
}

void FormFiller::SetRefillContext(FormGlobalId form_id,
                                  std::unique_ptr<RefillContext> context) {
  refill_context_[form_id] = std::move(context);
}

FormFiller::RefillContext* FormFiller::GetRefillContext(FormGlobalId form_id) {
  auto it = refill_context_.find(form_id);
  return it != refill_context_.end() ? it->second.get() : nullptr;
}

FormFiller::FieldFillingData FormFiller::GetFieldFillingData(
    const AutofillField& autofill_field,
    const FillingPayload& filling_payload,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    const FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  if (auto it = forced_fill_values.find(field_data.global_id());
      it != forced_fill_values.end()) {
    return {it->second, autofill_field.Type().GetStorableType(),
            /*value_is_an_override=*/true};
  }
  const auto& [value_to_fill, filling_type] = std::visit(
      base::Overloaded{
          [&](const AutofillProfile* profile)
              -> std::pair<std::u16string, std::optional<FieldType>> {
            return GetFillingValueAndTypeForProfile(
                CHECK_DEREF(profile), manager_->client().GetAppLocale(),
                autofill_field.Type(), field_data,
                manager_->client().GetAddressNormalizer(), failure_to_fill);
          },
          [&](const CreditCard* credit_card)
              -> std::pair<std::u16string, std::optional<FieldType>> {
            return {
                GetFillingValueForCreditCard(
                    CHECK_DEREF(credit_card), manager_->client().GetAppLocale(),
                    action_persistence, autofill_field, failure_to_fill),
                autofill_field.Type().GetStorableType()};
          },
          [&](const EntityInstance* entity)
              -> std::pair<std::u16string, std::optional<FieldType>> {
            // TODO(crbug.com/397620383): Which type should we return here?
            return {GetFillValueForEntity(
                        CHECK_DEREF(entity), autofill_field, action_persistence,
                        manager_->client().GetAppLocale(),
                        manager_->client().GetAddressNormalizer()),
                    std::nullopt};
          },
          [&](const VerifiedProfile* profile)
              -> std::pair<std::u16string, std::optional<FieldType>> {
            auto it = profile->find(autofill_field.Type().GetStorableType());
            std::u16string value = it == profile->end() ? u"" : it->second;
            return {value, autofill_field.Type().GetStorableType()};
          }},
      filling_payload);
  return {value_to_fill, filling_type, /*value_is_an_override=*/false};
}

bool FormFiller::FillField(
    AutofillField& autofill_field,
    const FillingPayload& filling_payload,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    bool allow_suggestion_swapping,
    std::string* failure_to_fill) {
  const FieldFillingData filling_content =
      GetFieldFillingData(autofill_field, filling_payload, forced_fill_values,
                          field_data, action_persistence, failure_to_fill);

  if (allow_suggestion_swapping) {
    field_data.set_value(filling_content.value_to_fill);
    field_data.set_force_override(true);
    field_data.set_is_autofilled(!filling_content.value_to_fill.empty());
    return true;
  }

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
    FillingProduct filling_product =
        GetFillingProductFromFillingPayload(filling_payload);
    autofill_field.set_filling_product(filling_product);
    if (filling_product == FillingProduct::kAddress) {
      autofill_field.set_autofill_source_profile_guid(
          std::get<const AutofillProfile*>(filling_payload)->guid());
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

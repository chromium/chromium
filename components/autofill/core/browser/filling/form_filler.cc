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
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
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
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Time to wait after a dynamic form change before triggering a refill.
// This is used for sites that change multiple things consecutively.
constexpr base::TimeDelta kWaitTimeForDynamicForms = base::Milliseconds(200);

FillDataType GetFillDataTypeFromFillingPayload(
    const FillingPayload& filling_payload) {
  return std::visit(
      absl::Overload{
          [](const AutofillProfile*) { return FillDataType::kAutofillProfile; },
          [](const CreditCard*) { return FillDataType::kCreditCard; },
          [](const EntityInstance*) { return FillDataType::kAutofillAi; },
          [](const VerifiedProfile*) { return FillDataType::kAutofillProfile; },
          [](const OtpFillData*) {
            return FillDataType::kOneTimePasswordValue;
          },
      },
      filling_payload);
}

// Given `filling_product`, returns the types supported for filling by this
// FillingProduct, or std::nullopt if `filling_product` is independent of field
// classifications.
std::optional<FieldTypeSet> GetFieldTypesToFillFromFillingProduct(
    FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress: {
      static constexpr FieldTypeSet kFieldTypes = []() {
        FieldTypeSet field_types;
        for (FieldType field_type : FieldTypeSet::all()) {
          if (IsAddressType(field_type)) {
            field_types.insert(field_type);
          }
        }
        return field_types;
      }();
      return kFieldTypes;
    }
    case FillingProduct::kCreditCard: {
      static constexpr FieldTypeSet kFieldTypes = []() {
        FieldTypeSet field_types;
        for (FieldType field_type : FieldTypeSet::all()) {
          if (FieldTypeGroupSet({FieldTypeGroup::kCreditCard,
                                 FieldTypeGroup::kStandaloneCvcField})
                  .contains(GroupTypeOfFieldType(field_type))) {
            field_types.insert(field_type);
          }
        }
        return field_types;
      }();
      return kFieldTypes;
    }
    case FillingProduct::kAutofillAi: {
      static constexpr auto kFieldTypes = []() {
        FieldTypeSet result;
        for (AttributeType type : DenseSet<AttributeType>::all()) {
          result.insert_all(type.field_subtypes());
        }
        return result;
      }();
      return kFieldTypes;
    }
    case FillingProduct::kPassword: {
      static constexpr FieldTypeSet kFieldTypes = []() {
        FieldTypeSet field_types;
        for (FieldType field_type : FieldTypeSet::all()) {
          if (FieldTypeGroupSet({FieldTypeGroup::kUsernameField,
                                 FieldTypeGroup::kPasswordField})
                  .contains(GroupTypeOfFieldType(field_type))) {
            field_types.insert(field_type);
          }
        }
        return field_types;
      }();
      return kFieldTypes;
    }
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
    case FillingProduct::kDataList:
    case FillingProduct::kPasskey:
      return std::nullopt;
    case FillingProduct::kOneTimePassword:
      return FieldTypeSet{ONE_TIME_CODE};
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
  // Since this function is about analysing the initial value, we should not
  // process fields that were modified, since those fields do not have their
  // initial values anymore.
  if (field.value() != field.initial_value() &&
      base::FeatureList::IsEnabled(
          features::kAutofillAllowFillingModifiedInitialValues)) {
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
  auto has_relevant_cc_field_type = [](const AutofillField& field) {
    const FieldType field_type = field.Type().GetCreditCardType();
    return field_type != UNKNOWN_TYPE &&
           field_type != CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
  };
  return has_relevant_cc_field_type(trigger_field) &&
         has_relevant_cc_field_type(field) && !is_refill &&
         IsPaymentsFieldSwappingEnabled();
}

// Returns whether a filling action for `filling_product` should be included in
// the form autofill history.
bool ShouldRecordFillingHistory(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCreditCard:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kOneTimePassword:
      return true;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPasskey:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
      return false;
  }
  NOTREACHED();
}

// Called by `FormFiller::MaybeTriggerRefill()` and constructs a refill value in
// case the website used JavaScript to reformat an expiration date like
// "05/2023" into "05 / 20" (i.e. it broke the year by cutting the last two
// digits instead of stripping the first two digits).
std::optional<FormFiller::ValueAndType> GetRefillValueForExpirationDate(
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
  return FormFiller::ValueAndType(refill_value,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
}

}  // namespace

// Like FillingPayload, but may carry additional data needed for filling.
struct FormFiller::AugmentedFillingPayload {
  using EntityPayload = std::pair<const EntityInstance*,
                                  std::vector<AutofillFieldWithAttributeType>>;
  using Variant = std::variant<const AutofillProfile*,
                               const CreditCard*,
                               EntityPayload,
                               const VerifiedProfile*,
                               const OtpFillData*>;

  AugmentedFillingPayload(const FillingPayload& filling_payload,
                          FormStructure& form_structure,
                          AutofillField& trigger_field)
      : variant(std::visit(
            absl::Overload{
                [](const AutofillProfile* autofill_profile) -> Variant {
                  return autofill_profile;
                },
                [](const CreditCard* credit_card) -> Variant {
                  return credit_card;
                },
                [&](const EntityInstance* entity) -> Variant {
                  return std::pair(
                      entity, RationalizeAndDetermineAttributeTypes(
                                  form_structure.fields(),
                                  trigger_field.section(), entity->type()));
                },
                [](const VerifiedProfile* verified_profile) -> Variant {
                  return verified_profile;
                },
                [](const OtpFillData* otp_filling_payload) -> Variant {
                  return otp_filling_payload;
                }},
            filling_payload)) {}

  FillingProduct filling_product() const {
    return std::visit(
        absl::Overload{
            [](const AutofillProfile*) { return FillingProduct::kAddress; },
            [](const CreditCard*) { return FillingProduct::kCreditCard; },
            [](const EntityPayload&) { return FillingProduct::kAutofillAi; },
            [](const VerifiedProfile*) {
              return FillingProduct::kIdentityCredential;
            },
            [](const OtpFillData*) {
              return FillingProduct::kOneTimePassword;
            }},
        variant);
  }

  bool supports_refills() const {
    switch (filling_product()) {
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
      case FillingProduct::kOneTimePassword:
        return false;
      case FillingProduct::kPasskey:
      case FillingProduct::kPassword:
      case FillingProduct::kDataList:
      case FillingProduct::kNone:
        NOTREACHED();
    }
  }

  Variant variant;
};

// Keeps track of the filling context for a form, used to make refill
// attempts.
struct FormFiller::RefillContext {
  // |filling_payload| contains the data used to perform the initial filling
  // operation.
  RefillContext(const AutofillField& field,
                const AugmentedFillingPayload& filling_payload)
      : filled_field_id(field.global_id()),
        filled_field_signature(field.GetFieldSignature()),
        filled_origin(field.origin()),
        original_fill_time(base::TimeTicks::Now()) {
    profile_or_credit_card = std::visit(
        absl::Overload{
            // Autofill with AI doesn't support refills.
            [](const AugmentedFillingPayload::EntityPayload&)
                -> std::variant<CreditCard, AutofillProfile> {
              // Beware that `EntityPayload::second` holds raw_refs to
              // AutofillFields. These references must no be stored in a
              // RefillContext because they would dangle.
              NOTREACHED();
            },
            // Verified Profiles doesn't support refills.
            [](const VerifiedProfile*)
                -> std::variant<CreditCard, AutofillProfile> { NOTREACHED(); },
            // OTP filling doesn't support refills.
            [](const OtpFillData*)
                -> std::variant<CreditCard, AutofillProfile> { NOTREACHED(); },
            [](const auto* x) {
              return std::variant<CreditCard, AutofillProfile>(*x);
            }},
        filling_payload.variant);
  }

  ~RefillContext() = default;

  // Whether a refill attempt was made.
  bool attempted_refill = false;
  // The profile or credit card that was used for the initial fill. This is
  // slightly different from `filling_payload` that is used by the filling
  // function: This contains actual objects because this needs to survive
  // potential storage mutation, and this only contains payloads that support
  // refills.
  std::variant<CreditCard, AutofillProfile> profile_or_credit_card;
  // Possible identifiers of the field that was focused when the form was
  // initially filled. A refill shall be triggered from the same field.
  const FieldGlobalId filled_field_id;
  const FieldSignature filled_field_signature;
  // The security origin from which the field was filled.
  url::Origin filled_origin;
  // The time at which the initial fill occurred.
  // TODO(crbug.com/41490871): Remove in favor of
  // FormStructure::last_filling_timestamp_.
  const base::TimeTicks original_fill_time;
  // The timer used to trigger a refill.
  base::OneShotTimer on_refill_timer;
  // The field type groups that were initially filled.
  DenseSet<FieldTypeGroup> type_groups_originally_filled;
  // If populated, this map determines which values will be filled into a
  // field (it does not matter whether the field already contains a value).
  std::map<FieldGlobalId, ValueAndType> forced_fill_values;
  // The form filled in the first attempt for filling. Used to check whether
  // a refill should be attempted upon parsing an updated FormData.
  std::optional<FormData> filled_form;
};

FormFiller::RefillOptions::RefillOptions() = default;

FormFiller::RefillOptions FormFiller::RefillOptions::NotRefill() {
  return {};
}

FormFiller::RefillOptions FormFiller::RefillOptions::Refill(
    DenseSet<FieldTypeGroup> originally_filled) {
  RefillOptions r;
  r.originally_filled_ = originally_filled;
  return r;
}

bool FormFiller::RefillOptions::is_refill() const {
  return originally_filled_.has_value();
}

bool FormFiller::RefillOptions::may_refill(
    const FieldTypeSet& field_types) const {
  CHECK(is_refill());
  return originally_filled_->contains_all(
      DenseSet<FieldTypeGroup>(field_types, &GroupTypeOfFieldType));
}

DenseSet<FieldFillingSkipReason> FormFiller::GetFillingSkipReasonsForField(
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const AutofillField& trigger_field,
    const RefillOptions& refill_options,
    base::flat_map<FieldType, size_t>& type_count,
    const base::flat_set<FieldGlobalId>& blocked_fields,
    FillingProduct filling_product) {
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
  add_if(field.is_autofilled() && !is_trigger_field &&
             !refill_options.is_refill() &&
             !AllowPaymentSwapping(trigger_field, autofill_field,
                                   refill_options.is_refill()),
         FieldFillingSkipReason::kAlreadyAutofilled);

  AutofillType autofill_type = autofill_field.Type();
  FieldTypeSet field_types = autofill_type.GetTypes();

  // On a refill, only fill fields from type groups that were present during
  // the initial fill.
  add_if(refill_options.is_refill() && !refill_options.may_refill(field_types),
         FieldFillingSkipReason::kRefillNotInInitialFill);

  // A field with a specific type is only allowed to be filled a limited
  // number of times given by |TypeValueFormFillingLimit(field_type)|.
  for (FieldType field_type : field_types) {
    add_if(++type_count[field_type] > TypeValueFormFillingLimit(field_type),
           FieldFillingSkipReason::kFillingLimitReachedType);
  }

  std::optional<FieldTypeSet> supported_types =
      GetFieldTypesToFillFromFillingProduct(filling_product);
  // This ensures that a filling product only operates on fields of supported
  // types.
  add_if(supported_types && !supported_types->contains_any(field_types),
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

FormFiller::FormFiller(BrowserAutofillManager& manager) : manager_(manager) {}

FormFiller::~FormFiller() = default;

LogManager* FormFiller::log_manager() {
  return manager_->client().GetCurrentLogManager();
}

void FormFiller::Reset() {
  refill_context_.clear();
  form_autofill_history_.Reset();
}

// static
base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
FormFiller::GetFieldFillingSkipReasons(base::span<const FormFieldData> fields,
                                       const FormStructure& form_structure,
                                       const AutofillField& trigger_field,
                                       const RefillOptions& refill_options,
                                       FillingProduct filling_product,
                                       const AutofillClient& client) {
  // Counts the number of times a type was seen in the section to be filled.
  // This is used to limit the maximum number of fills per value.
  base::flat_map<FieldType, size_t> type_count;
  type_count.reserve(form_structure.field_count());

  base::flat_set<FieldGlobalId> blocked_fields;
  if (filling_product == FillingProduct::kAddress) {
    blocked_fields = GetFieldsFillableByAutofillAi(form_structure, client);
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
                                      refill_options, type_count,
                                      blocked_fields, filling_product);

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
  manager_->driver().ApplyFormAction(
      mojom::FormActionType::kUndo, action_persistence, form.fields(),
      url::Origin(),
      /*field_type_map=*/{}, /*section_for_clear_form_on_ios=*/Section());
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
  const AugmentedFillingPayload augmented_filling_payload =
      AugmentedFillingPayload(filling_payload, form_structure,
                              autofill_trigger_field);

  LogBuffer buffer(IsLoggingActive(log_manager()));
  LOG_AF(buffer) << "action_persistence: "
                 << ActionPersistenceToString(action_persistence) << Br{};
  LOG_AF(buffer) << "filling product: "
                 << FillingProductToString(
                        augmented_filling_payload.filling_product())
                 << Br{};
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
    if (augmented_filling_payload.supports_refills()) {
      SetRefillContext(form_structure.global_id(),
                       std::make_unique<RefillContext>(
                           autofill_trigger_field, augmented_filling_payload));
    }
  }

  RefillContext* refill_context = GetRefillContext(form_structure.global_id());
  bool could_attempt_refill = augmented_filling_payload.supports_refills() &&
                              refill_context != nullptr &&
                              !refill_context->attempted_refill &&
                              !refill_trigger_reason;
  RefillOptions refill_options =
      refill_trigger_reason.has_value() && refill_context
          ? RefillOptions::Refill(refill_context->type_groups_originally_filled)
          : RefillOptions::NotRefill();

  std::vector<FormFieldData> result_fields = form.fields();
  CHECK_EQ(result_fields.size(), form_structure.field_count());

  std::vector<std::pair<FieldGlobalId, FieldType>> filled_field_types;

  // `FormFiller::GetFieldFillingSkipReasons` returns for each field a generic
  // list of reason for skipping each field.
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      GetFieldFillingSkipReasons(
          result_fields, form_structure, autofill_trigger_field, refill_options,
          augmented_filling_payload.filling_product(), manager_->client());

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

    std::string failure_to_fill;  // Reason for failing to fill.
    const std::map<FieldGlobalId, ValueAndType>& forced_fill_values =
        refill_context ? refill_context->forced_fill_values
                       : std::map<FieldGlobalId, ValueAndType>();

    bool allow_suggestion_swapping =
        form.fields()[i].is_autofilled() &&
        AllowPaymentSwapping(autofill_trigger_field, autofill_field,
                             refill_trigger_reason.has_value());

    // Fill the data from `augmented_filling_payload` into `result_form`, which
    // will be sent to the renderer. When `allow_suggestion_swapping` is true,
    // the fields can also be emptied. In that scenario, the
    // `field->is_autofilled()` becomes false.
    const std::optional<FieldType> filled_field_type =
        FillField(autofill_field, augmented_filling_payload, forced_fill_values,
                  result_fields[i], action_persistence,
                  allow_suggestion_swapping, &failure_to_fill);
    const bool is_newly_autofilled_or_emptied = filled_field_type.has_value();
    const bool autofilled_value_did_not_change =
        form.fields()[i].is_autofilled() && result_fields[i].is_autofilled() &&
        form.fields()[i].value() == result_fields[i].value();

    if (is_newly_autofilled_or_emptied && autofilled_value_did_not_change) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kAutofilledValueDidNotChange);
    } else if (!is_newly_autofilled_or_emptied) {
      skip_reasons[form.fields()[i].global_id()].insert(
          FieldFillingSkipReason::kNoValueToFill);
    } else if (could_attempt_refill) {
      refill_context->type_groups_originally_filled.insert_all(
          autofill_field.Type().GetGroups());
    }

    if (filled_field_type) {
      filled_field_types.emplace_back(result_fields[i].global_id(),
                                      *filled_field_type);
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
          base::flat_map<FieldGlobalId, FieldType>(
              std::move(filled_field_types)),
          /*section_for_clear_form_on_ios=*/autofill_trigger_field.section());

  // This will hold the subset of fields of `result_fields` whose ids are in
  // `safe_filled_field_ids`
  struct {
    std::vector<const FormFieldData*> old_values;
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

  if (action_persistence == mojom::ActionPersistence::kFill) {
    AppendFillLogEvents(form, form_structure, autofill_trigger_field,
                        safe_filled_field_ids, skip_reasons, filling_payload,
                        refill_trigger_reason.has_value());
    // Save filling history to support undoing it later if needed.
    if (ShouldRecordFillingHistory(
            augmented_filling_payload.filling_product())) {
      form_autofill_history_.AddFormFillingEntry(
          safe_filled_fields.old_values, safe_filled_fields.cached,
          augmented_filling_payload.filling_product(),
          refill_trigger_reason.has_value());
    }
  }

  LOG_AF(buffer) << CTag{"table"};
  LOG_AF(log_manager()) << LoggingScope::kFilling
                        << LogMessage::kSendFillingData << Br{}
                        << std::move(buffer);

  if (refill_context) {
    // When a new preview/fill starts, previously-set forced_fill_values should
    // be ignored, since the operation could be for a different card or address.
    refill_context->forced_fill_values.clear();
  }

  manager_->OnDidFillOrPreviewForm(
      action_persistence, form_structure, autofill_trigger_field,
      safe_filled_fields.cached,
      base::MakeFlatSet<FieldGlobalId>(result_fields, {},
                                       &FormFieldData::global_id),
      filling_payload, trigger_source, refill_trigger_reason);
}

void FormFiller::MaybeTriggerRefill(
    const FormData& form,
    const FormStructure& form_structure,
    RefillTriggerReason refill_trigger_reason,
    AutofillTriggerSource trigger_source,
    base::optional_ref<const AutofillField> field,
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
      // Only refill if the form actually changed since it was filled.
      // Since we won't schedule another refill, we should be cautious not to
      // prematurely schedule refills.
      if (refill_context->filled_form &&
          std::ranges::equal(
              refill_context->filled_form->fields(), form_structure.fields(),
              [](const FormFieldData& f,
                 const std::unique_ptr<AutofillField>& g) {
                return FormFieldData::IdenticalAndEquivalentDomElements(
                    f, *g, {FormFieldData::Exclusion::kValue});
              })) {
        return;
      }
      break;
    case RefillTriggerReason::kSelectOptionsChanged:
      if (const bool allow_refill =
              field && field->IsSelectElement() &&
              field->Type().GetGroups().contains_any(
                  refill_context->type_groups_originally_filled);
          !allow_refill && base::FeatureList::IsEnabled(
                               features::kAutofillFewerTrivialRefills)) {
        // The element in question is not fillable as a result of this signal.
        // Do not trigger a refill as it would most likely be a trivial one.
        return;
      }
      break;
    case RefillTriggerReason::kExpirationDateFormatted:
      CHECK(field && old_value);
      if (std::optional<ValueAndType> refill_value =
              GetRefillValueForExpirationDate(*field, *old_value)) {
        refill_context->forced_fill_values[field->global_id()] =
            *std::move(refill_value);
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

FormFiller::ValueAndTypeAndOverride FormFiller::GetFieldFillingData(
    const AutofillField& autofill_field,
    const AugmentedFillingPayload& filling_payload,
    const std::map<FieldGlobalId, ValueAndType>& forced_fill_values,
    const FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  if (auto it = forced_fill_values.find(field_data.global_id());
      it != forced_fill_values.end()) {
    return {it->second, /*value_is_an_override=*/true};
  }
  const auto& [value_to_fill, filled_field_type] = std::visit(
      absl::Overload{
          [&](const AutofillProfile* profile)
              -> std::pair<std::u16string, FieldType> {
            return GetFillingValueAndTypeForProfile(
                CHECK_DEREF(profile), manager_->client().GetAppLocale(),
                autofill_field.Type(), field_data,
                manager_->client().GetAddressNormalizer(), failure_to_fill);
          },
          [&](const CreditCard* credit_card)
              -> std::pair<std::u16string, FieldType> {
            return {
                GetFillingValueForCreditCard(
                    CHECK_DEREF(credit_card), manager_->client().GetAppLocale(),
                    action_persistence, autofill_field,
                    manager_->client().IsCvcSavingSupported(), failure_to_fill),
                autofill_field.Type().GetCreditCardType()};
          },
          [&](const AugmentedFillingPayload::EntityPayload&
                  entity_and_fields_and_types)
              -> std::pair<std::u16string, FieldType> {
            const EntityInstance& entity =
                CHECK_DEREF(entity_and_fields_and_types.first);
            const std::vector<AutofillFieldWithAttributeType>& fields =
                entity_and_fields_and_types.second;
            return {GetFillValueForEntity(
                        entity, fields, autofill_field, action_persistence,
                        manager_->client().GetAppLocale(),
                        manager_->client().GetAddressNormalizer()),
                    autofill_field.Type().GetAutofillAiType(entity.type())};
          },
          [&](const VerifiedProfile* profile)
              -> std::pair<std::u16string, FieldType> {
            const FieldType field_type =
                autofill_field.Type().GetIdentityCredentialType();
            auto it = profile->find(field_type);
            std::u16string value = it == profile->end() ? u"" : it->second;
            return {value, field_type};
          },
          [&](const OtpFillData* otp_fill_data)
              -> std::pair<std::u16string, FieldType> {
            auto it = otp_fill_data->find(field_data.global_id());
            const std::u16string& value =
                it == otp_fill_data->end() ? u"" : it->second;
            return {value, autofill_field.Type().GetPasswordManagerType()};
          }},
      filling_payload.variant);
  CHECK(filled_field_type != UNKNOWN_TYPE ||
            // The skip reasons lump all Autofill AI types together because
            // there is only a single FillingProduct for Autofill AI. Therefore,
            // when two Autofill AI FieldTypes of different entities appear in
            // the form, only the above std::visit() calls detects that the
            // value is not fillable and returns UNKNOWN_TYPE in that case.
            std::holds_alternative<AugmentedFillingPayload::EntityPayload>(
                filling_payload.variant),
        base::NotFatalUntil::M143);
  return {{value_to_fill, filled_field_type},
          /*value_is_an_override=*/false};
}

std::optional<FieldType> FormFiller::FillField(
    AutofillField& autofill_field,
    const AugmentedFillingPayload& filling_payload,
    const std::map<FieldGlobalId, ValueAndType>& forced_fill_values,
    FormFieldData& field_data,
    mojom::ActionPersistence action_persistence,
    bool allow_suggestion_swapping,
    std::string* failure_to_fill) {
  const ValueAndTypeAndOverride filling_content =
      GetFieldFillingData(autofill_field, filling_payload, forced_fill_values,
                          field_data, action_persistence, failure_to_fill);

  if (allow_suggestion_swapping) {
    field_data.set_value(filling_content.value);
    field_data.set_force_override(true);
    field_data.set_is_autofilled(!filling_content.value.empty());
    return filling_content.type;
  }

  // Do not attempt to fill empty values as it would skew the metrics.
  if (filling_content.value.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "No value to fill available. ";
    }
    return std::nullopt;
  }
  field_data.set_value(filling_content.value);
  field_data.set_force_override(filling_content.value_is_an_override);

  if (failure_to_fill) {
    *failure_to_fill = "Decided to fill";
  }
  if (action_persistence == mojom::ActionPersistence::kFill) {
    // Mark the cached field as autofilled, so that we can detect when a
    // user edits an autofilled field (for metrics).
    autofill_field.set_is_autofilled(true);
    autofill_field.set_filling_product(filling_payload.filling_product());
    if (filling_payload.filling_product() == FillingProduct::kAddress) {
      autofill_field.set_autofill_source_profile_guid(
          std::get<const AutofillProfile*>(filling_payload.variant)->guid());
    }
    autofill_field.set_autofilled_type(filling_content.type);
  }
  // Mark the field as autofilled when a non-empty value is assigned to
  // it. This allows the renderer to distinguish autofilled fields from
  // fields with non-empty values, such as select-one fields.
  field_data.set_is_autofilled(true);
  return filling_content.type;
}

void FormFiller::AppendFillLogEvents(
    const FormData& form,
    FormStructure& form_structure,
    AutofillField& trigger_autofill_field,
    const base::flat_set<FieldGlobalId>& safe_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
        skip_reasons,
    const FillingPayload& filling_payload,
    bool is_refill) {
  std::string country_code;
  if (const AutofillProfile* const* address =
          std::get_if<const AutofillProfile*>(&filling_payload)) {
    country_code =
        base::UTF16ToUTF8((*address)->GetRawInfo(ADDRESS_HOME_COUNTRY));
  }
  TriggerFillFieldLogEvent trigger_fill_field_log_event =
      TriggerFillFieldLogEvent{
          .data_type = GetFillDataTypeFromFillingPayload(filling_payload),
          .associated_country_code = country_code,
          .timestamp = base::Time::Now()};
  trigger_autofill_field.AppendLogEventIfNotRepeated(
      trigger_fill_field_log_event);
  FillEventId fill_event_id = trigger_fill_field_log_event.fill_event_id;

  for (auto [form_field, field] :
       base::zip(form.fields(), form_structure.fields())) {
    const FieldGlobalId field_id = field->global_id();
    const bool has_value_before = !form_field.value().empty();
    const FieldFillingSkipReason skip_reason =
        skip_reasons.at(field_id).empty() ? FieldFillingSkipReason::kNotSkipped
                                          : *skip_reasons.at(field_id).begin();
    if (!IsCheckable(field->check_status())) {
      if (skip_reason == FieldFillingSkipReason::kNotSkipped) {
        field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = fill_event_id,
            .had_value_before_filling = ToOptionalBoolean(has_value_before),
            .autofill_skipped_status = skip_reason,
            .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
            .had_value_after_filling =
                ToOptionalBoolean(safe_field_ids.contains(field_id)),
            .filling_prevented_by_iframe_security_policy =
                safe_field_ids.contains(field_id) ? OptionalBoolean::kFalse
                                                  : OptionalBoolean::kTrue,
            .was_refill = ToOptionalBoolean(is_refill),
        });
      } else {
        field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = fill_event_id,
            .had_value_before_filling = ToOptionalBoolean(has_value_before),
            .autofill_skipped_status = skip_reason,
            .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
            .had_value_after_filling = ToOptionalBoolean(has_value_before),
            .was_refill = ToOptionalBoolean(is_refill),
        });
      }
    }
  }
}

}  // namespace autofill

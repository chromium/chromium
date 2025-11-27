// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stdint.h>

#include <iterator>
#include <optional>
#include <ranges>
#include <variant>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

template <>
struct DenseSetTraits<FieldPrediction::Source>
    : EnumDenseSetTraits<FieldPrediction::Source,
                         FieldPrediction::Source_MIN,
                         FieldPrediction::Source_MAX> {};

namespace {

// This list includes pairs (heuristic_type, html_type) that express which
// heuristic predictions should be prioritized over HTML. The list is used for
// new field types that do not have a clear corresponding HTML type. In these
// cases, the local heuristics predictions will be used to determine the field
// overall type.
static constexpr auto kAutofillHeuristicsVsHtmlOverrides =
    base::MakeFixedFlatSet<std::pair<FieldType, HtmlFieldType>>(
        {{ADDRESS_HOME_ADMIN_LEVEL2, HtmlFieldType::kAddressLevel1},
         {ADDRESS_HOME_ADMIN_LEVEL2, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_APT_NUM, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_APT_NUM, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_BETWEEN_STREETS, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kOrganization},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel3},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_OVERFLOW_AND_LANDMARK, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_OVERFLOW, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_OVERFLOW, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_HOUSE_NUMBER, HtmlFieldType::kStreetAddress},
         {ADDRESS_HOME_STREET_NAME, HtmlFieldType::kStreetAddress},
         {NAME_LAST_PREFIX, HtmlFieldType::kAdditionalName},
         {NAME_LAST_PREFIX, HtmlFieldType::kAdditionalNameInitial},
         {NAME_LAST_CORE, HtmlFieldType::kFamilyName},
         {ALTERNATIVE_FAMILY_NAME, HtmlFieldType::kFamilyName},
         {ALTERNATIVE_GIVEN_NAME, HtmlFieldType::kGivenName},
         {ALTERNATIVE_FULL_NAME, HtmlFieldType::kName}});

// This list includes pairs (heuristic_type, server_type) that express which
// heuristics predictions should be prioritized over server predictions. The
// list is used for new field types that the server may have learned
// incorrectly. In these cases, the local heuristics predictions will be used to
// determine the field type.
// TODO(crbug.com/359768803): Remove overrides for alternative names once the
// feature is rolled out.
static constexpr auto kAutofillHeuristicsVsServerOverrides =
    base::MakeFixedFlatSet<std::pair<FieldType, FieldType>>(
        {{ADDRESS_HOME_ADMIN_LEVEL2, ADDRESS_HOME_CITY},
         {ADDRESS_HOME_HOUSE_NUMBER_AND_APT, ADDRESS_HOME_HOUSE_NUMBER},
         {ADDRESS_HOME_HOUSE_NUMBER_AND_APT, ADDRESS_HOME_APT_NUM},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_LINE3},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_HOUSE_NUMBER},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_LINE1},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_STREET_ADDRESS},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_STATE},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE3},
         {ADDRESS_HOME_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW_AND_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW, ADDRESS_HOME_LINE3},
         {ALTERNATIVE_FULL_NAME, NAME_FULL},
         {ALTERNATIVE_GIVEN_NAME, NAME_FIRST},
         {ALTERNATIVE_FAMILY_NAME, NAME_LAST},
         {ALTERNATIVE_FAMILY_NAME, NAME_LAST_SECOND},
         {ALTERNATIVE_FAMILY_NAME, NAME_LAST_CORE},
         {NAME_LAST_PREFIX, NAME_MIDDLE},
         {NAME_LAST_CORE, NAME_LAST}});

// Returns true, if the prediction is non-experimental and should be used by
// autofill or password manager.
// Note: A `NO_SERVER_DATA` prediction with `SOURCE_UNSPECIFIED` may also be a
// default prediction. We don't need to store it, because its meaning is that
// there is no default prediction.
bool IsDefaultPrediction(const FieldPrediction& prediction) {
  constexpr DenseSet<FieldPrediction::Source> default_sources = {
      FieldPrediction::SOURCE_AUTOFILL_DEFAULT,
      FieldPrediction::SOURCE_PASSWORDS_DEFAULT,
      FieldPrediction::SOURCE_OVERRIDE,
      FieldPrediction::SOURCE_MANUAL_OVERRIDE};
  return default_sources.contains(prediction.source());
}

bool IsAutofillAiPrediction(const FieldPrediction& prediction) {
  switch (prediction.source()) {
    case FieldPrediction::SOURCE_UNSPECIFIED:
    case FieldPrediction::SOURCE_AUTOFILL_DEFAULT:
    case FieldPrediction::SOURCE_PASSWORDS_DEFAULT:
    case FieldPrediction::SOURCE_OVERRIDE:
    case FieldPrediction::SOURCE_ALL_APPROVED_EXPERIMENTS:
    case FieldPrediction::SOURCE_FIELD_RANKS:
    case FieldPrediction::SOURCE_MANUAL_OVERRIDE:
    case FieldPrediction::SOURCE_AUTOFILL_COMBINED_TYPES:
      return false;
    case FieldPrediction::SOURCE_AUTOFILL_AI:
    case FieldPrediction::SOURCE_AUTOFILL_AI_CROWDSOURCING:
      return true;
  }
  // This is not using `NOTREACHED()` because the `FieldPrediction` may
  // originate from outside of Chrome and may not have been validated.
  return false;
}

// Returns true if for two consecutive events, the second event may be ignored.
// In that case, if `event1` is at the back of AutofillField::field_log_events_,
// `event2` is not supposed to be added.
bool AreCollapsibleLogEvents(const AutofillField::FieldLogEventType& event1,
                             const AutofillField::FieldLogEventType& event2) {
  return std::visit(
      [](const auto& e1, const auto& e2) {
        if constexpr (std::is_same_v<decltype(e1), decltype(e2)>) {
          return AreCollapsible(e1, e2);
        }
        return false;
      },
      event1, event2);
}

// Returns whether the `heuristic_type` should be preferred over the
// `html_type`. For certain field types that have been recently introduced, we
// want to prioritize local heuristics over the autocomplete type.
bool PreferHeuristicOverHtml(FieldType heuristic_type,
                             HtmlFieldType html_type) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableEmailOrLoyaltyCardsFilling) &&
      heuristic_type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID &&
      html_type == HtmlFieldType::kEmail) {
    return true;
  }

  return base::Contains(kAutofillHeuristicsVsHtmlOverrides,
                        std::make_pair(heuristic_type, html_type));
}

// Returns whether the `heuristic_type` should be preferred over the
// `server_type`. For certain field types that have been recently introduced, we
// want to prioritize the local heuristics predictions because they are more
// likely to be accurate. By prioritizing the local heuristics predictions, we
// can help the server to "learn" the correct classification for these fields.
bool PreferHeuristicOverServer(FieldType heuristic_type,
                               FieldType server_type,
                               FieldType password_ml_classification_type) {
  if (server_type == NO_SERVER_DATA) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableEmailOrLoyaltyCardsFilling) &&
      heuristic_type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID &&
      server_type == EMAIL_ADDRESS) {
    return true;
  }

  if (base::Contains(kAutofillHeuristicsVsServerOverrides,
                     std::make_pair(heuristic_type, server_type))) {
    return true;
  }

  // AutofillAI predictions overrule local heuristics unless
  // kAutofillAiPreferModelResponseOverHeuristics is disabled.
  if (heuristic_type != UNKNOWN_TYPE &&
      GroupTypeOfFieldType(server_type) == FieldTypeGroup::kAutofillAi &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAiPreferModelResponseOverHeuristics)) {
    return true;
  }

  // Sometimes the server and heuristics disagree on whether a name field
  // should be associated with an address or a credit card. There was a
  // decision to prefer the heuristics in these cases, but it looks like
  // it might be better to fix this server-side.
  // See http://crbug.com/429236 for background.
  if ((server_type == CREDIT_CARD_NAME_FULL && heuristic_type == NAME_FULL) ||
      (server_type == NAME_FULL && heuristic_type == CREDIT_CARD_NAME_FULL) ||
      (server_type == NAME_FIRST && heuristic_type == CREDIT_CARD_NAME_FIRST) ||
      (server_type == NAME_LAST && heuristic_type == CREDIT_CARD_NAME_LAST)) {
    return true;
  }

  // Either way, retain a preference for the CVC heuristic over the
  // server's password predictions (http://crbug.com/469007)
  if (GroupTypeOfFieldType(server_type) == FieldTypeGroup::kPasswordField &&
      heuristic_type == CREDIT_CARD_VERIFICATION_CODE) {
    return true;
  }

  // For structured last name tokens the heuristic predictions get precedence
  // over the server predictions.
  if (heuristic_type == NAME_LAST_SECOND || heuristic_type == NAME_LAST_FIRST) {
    return true;
  }

  // For structured address tokens the heuristic predictions get precedence
  // over the server predictions.
  if (heuristic_type == ADDRESS_HOME_STREET_NAME ||
      heuristic_type == ADDRESS_HOME_HOUSE_NUMBER) {
    return true;
  }

  // For merchant promo code fields the heuristic predictions get precedence
  // over the server predictions.
  if (heuristic_type == MERCHANT_PROMO_CODE) {
    return true;
  }

  // For international bank account number (IBAN) fields the heuristic
  // predictions get precedence over the server predictions.
  if (heuristic_type == IBAN_VALUE) {
    return true;
  }

  // For loyalty card fields the heuristic predictions get precedence over
  // `UNKNOWN_TYPE` server prediction.
  if (heuristic_type == LOYALTY_MEMBERSHIP_ID && server_type == UNKNOWN_TYPE) {
    return true;
  }

  // Password server predictions are ignored when the field is parsed to
  // be an OTP field with the clientside model, as incorrect server
  // password predictions are common in this case.
  if (password_ml_classification_type == ONE_TIME_CODE &&
      GroupTypeOfFieldType(server_type) == FieldTypeGroup::kPasswordField) {
    return true;
  }

  return false;
}

// Util function for `ComputedType`. Returns the values of HtmlFieldType that
// won't be overridden by heuristics or server predictions, up to a few
// exceptions. Check function `ComputedType` for more details.
DenseSet<HtmlFieldType> BelievedHtmlTypes(FieldType heuristic_prediction,
                                          FieldType server_prediction) {
  HtmlFieldTypeSet believed_html_types = HtmlFieldTypeSet::all();
  // We always override unspecified autocomplete attribute.
  believed_html_types.erase(HtmlFieldType::kUnspecified);
  auto is_street_name_or_house_number_type = [](FieldType field_type) {
    return field_type == ADDRESS_HOME_STREET_NAME ||
           field_type == ADDRESS_HOME_HOUSE_NUMBER;
  };
  // When the heuristics or server predict that an address is a street name or a
  // house number, we prioritize this over "address-line[1|2]" autocomplete
  // since those signals are usually stronger for this combination.
  if (is_street_name_or_house_number_type(heuristic_prediction) ||
      is_street_name_or_house_number_type(server_prediction)) {
    believed_html_types.erase_all(
        {HtmlFieldType::kAddressLine1, HtmlFieldType::kAddressLine2});
  }
  // Always override unrecognized autocomplete attributes.
  believed_html_types.erase(HtmlFieldType::kUnrecognized);
  return believed_html_types;
}

}  // namespace

// LINT.IfChange(PredictionSourceTranslation)

std::string_view AutofillPredictionSourceToStringView(
    AutofillPredictionSource source) {
  switch (source) {
    case AutofillPredictionSource::kHeuristics:
      return "Heuristics";
    case AutofillPredictionSource::kServerCrowdsourcing:
      return "ServerCrowdsourcing";
    case AutofillPredictionSource::kServerOverride:
      return "ServerOverride";
    case AutofillPredictionSource::kAutocomplete:
      return "AutocompleteAttribute";
    case AutofillPredictionSource::kRationalization:
      return "Rationalization";
  }
  NOTREACHED();
}

// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/histograms.xml:AutofillPredictionSources)

Section Section::FromAutocomplete(Section::Autocomplete autocomplete) {
  Section section;
  if (autocomplete.section.empty() &&
      autocomplete.mode == HtmlFieldMode::kNone) {
    return section;
  }
  section.value_ = std::move(autocomplete);
  return section;
}

Section Section::FromFieldIdentifier(
    const FormFieldData& field,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  Section section;
  // Set the section's value based on the field identifiers: the field's name,
  // mapped frame id, renderer id. We do not use LocalFrameTokens but instead
  // map them to consecutive integers using `frame_token_ids`, which uniquely
  // identify a frame within a given FormStructure. Since we do not intend to
  // compare sections from different FormStructures, this is sufficient.
  //
  // We intentionally do not include the LocalFrameToken in the section
  // because frame tokens should not be sent to a renderer.
  //
  // TODO(crbug.com/40200532): Remove special handling of FrameTokens.
  size_t generated_frame_id =
      frame_token_ids.emplace(field.host_frame(), frame_token_ids.size())
          .first->second;
  section.value_ = FieldIdentifier(base::UTF16ToUTF8(field.name()),
                                   generated_frame_id, field.renderer_id());
  return section;
}

Section::Section() = default;

Section::Section(const Section& section) = default;
Section& Section::operator=(const Section& section) = default;

Section::Section(Section&& section) = default;
Section& Section::operator=(Section&& section) = default;

Section::~Section() = default;

Section::operator bool() const {
  return !is_default();
}

bool Section::is_from_autocomplete() const {
  return std::holds_alternative<Autocomplete>(value_);
}

bool Section::is_from_fieldidentifier() const {
  return std::holds_alternative<FieldIdentifier>(value_);
}

bool Section::is_default() const {
  return std::holds_alternative<Default>(value_);
}

std::string Section::ToString() const {
  static constexpr char kDefaultSection[] = "-default";

  std::string section_name;
  if (const Autocomplete* autocomplete = std::get_if<Autocomplete>(&value_)) {
    // To prevent potential section name collisions, append `kDefaultSection`
    // suffix to fields without a `HtmlFieldMode`. Without this, 'autocomplete'
    // attribute values "section--shipping street-address" and "shipping
    // street-address" would have the same prefix.
    section_name = autocomplete->section +
                   (autocomplete->mode != HtmlFieldMode::kNone
                        ? "-" + HtmlFieldModeToString(autocomplete->mode)
                        : kDefaultSection);
  } else if (const FieldIdentifier* f = std::get_if<FieldIdentifier>(&value_)) {
    FieldIdentifier field_identifier = *f;
    section_name = base::StrCat(
        {field_identifier.field_name, "_",
         base::NumberToString(field_identifier.local_frame_id), "_",
         base::NumberToString(field_identifier.field_renderer_id.value())});
  }

  return section_name.empty() ? kDefaultSection : section_name;
}

LogBuffer& operator<<(LogBuffer& buffer, const Section& section) {
  return buffer << section.ToString();
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
  return os << section.ToString();
}

AutofillFormatString::AutofillFormatString() = default;

AutofillFormatString::AutofillFormatString(std::u16string v,
                                           FormatString_Type type)
    : value(std::move(v)), type(type) {
  DCHECK(IsValid(value, type));
}

AutofillFormatString::AutofillFormatString(const AutofillFormatString&) =
    default;

AutofillFormatString& AutofillFormatString::operator=(
    const AutofillFormatString&) = default;

AutofillFormatString::AutofillFormatString(AutofillFormatString&&) = default;

AutofillFormatString& AutofillFormatString::operator=(AutofillFormatString&&) =
    default;

AutofillFormatString::~AutofillFormatString() = default;

// static
bool AutofillFormatString::IsValid(std::u16string_view value,
                                   FormatString_Type type) {
  switch (type) {
    case FormatString_Type_DATE:
      return data_util::IsValidDateFormat(value);
    case FormatString_Type_AFFIX:
      return data_util::IsValidAffixFormat(value);
    case FormatString_Type_FLIGHT_NUMBER:
      return data_util::IsValidFlightNumberFormat(value);
    case FormatString_Type_ICU_DATE:
      // TODO(crbug.com/464004123): Add validation for ICU date format strings.
      return true;
  }
  // Graceful catch-all because the `type` may come from the server.
  return false;
}

AutofillField::AutofillField() {
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::AutofillField(FieldSignature field_signature) : AutofillField() {
  field_signature_ = field_signature;
}

AutofillField::AutofillField(const FormFieldData& field)
    : FormFieldData(field),
      field_signature_(
          CalculateFieldSignatureByNameAndType(name(), form_control_type())) {
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::AutofillField(AutofillField&&) = default;

AutofillField& AutofillField::operator=(AutofillField&&) = default;

AutofillField::~AutofillField() = default;

std::unique_ptr<AutofillField> AutofillField::CreateForPasswordManagerUpload(
    FieldSignature field_signature) {
  std::unique_ptr<AutofillField> field;
  field.reset(new AutofillField(field_signature));
  return field;
}

FieldType AutofillField::heuristic_type() const {
  return heuristic_type(GetActiveHeuristicSource());
}

FieldType AutofillField::heuristic_type(HeuristicSource s) const {
  // Special handling for ML model predictions.
  if (s == HeuristicSource::kAutofillMachineLearning) {
    FieldType regex_type =
        local_type_predictions_[static_cast<size_t>(HeuristicSource::kRegexes)];
    if (regex_type == FieldType::NO_SERVER_DATA) {
      regex_type = FieldType::UNKNOWN_TYPE;
    }
    FieldType model_type = local_type_predictions_[static_cast<size_t>(
        HeuristicSource::kAutofillMachineLearning)];
    // We fall back to regex heuristics in the following cases:
    // - The regex heuristics detected a type that the model does not support
    //   (e.g. IBAN).
    // - The model returned NO_SERVER_DATA, indicating that execution failed
    //   or that a confidence threshold was not reached.
    bool model_supports_regex_type =
        ml_supported_types_ && ml_supported_types_->contains(regex_type);
    if (!model_supports_regex_type || model_type == FieldType::NO_SERVER_DATA) {
      return regex_type;
    }
    return model_type;
  }

  FieldType type = local_type_predictions_[static_cast<size_t>(s)];
  // Guaranteed by construction of `local_type_predictions_`.
  DCHECK(ToSafeFieldType(type, MAX_VALID_FIELD_TYPE) != MAX_VALID_FIELD_TYPE);
  // `NO_SERVER_DATA` would mean that there is no heuristic type. Client code
  // presumes there is a prediction, therefore we coalesce to `UNKNOWN_TYPE`.
  // Shadow predictions however are not used and we care whether the type is
  // `UNKNOWN_TYPE` or whether we never ran the heuristics.
  return type != NO_SERVER_DATA || s != GetActiveHeuristicSource()
             ? type
             : UNKNOWN_TYPE;
}

FieldType AutofillField::server_type() const {
  return server_predictions_.empty()
             ? NO_SERVER_DATA
             : ToSafeFieldType(server_predictions_[0].type(), NO_SERVER_DATA);
}

bool AutofillField::server_type_prediction_is_override() const {
  return server_predictions_.empty() ? false
                                     : server_predictions_[0].override();
}

void AutofillField::set_heuristic_type(HeuristicSource s, FieldType type) {
  type = ToSafeFieldType(type, MAX_VALID_FIELD_TYPE);
  CHECK_NE(type, MAX_VALID_FIELD_TYPE, base::NotFatalUntil::M142) << type;
  local_type_predictions_[static_cast<size_t>(s)] = type;
  if (s == GetActiveHeuristicSource()) {
    overall_type_ = std::nullopt;
  }
}

void AutofillField::set_server_predictions(
    std::vector<FieldPrediction> predictions) {
  overall_type_ = std::nullopt;
  server_predictions_.clear();

  for (auto& prediction : predictions) {
    MaybeAddServerPrediction(std::move(prediction));
  }

  if (server_predictions_.empty()) {
    // Equivalent to a `NO_SERVER_DATA` prediction from `SOURCE_UNSPECIFIED`.
    server_predictions_.emplace_back();
  }
}

void AutofillField::MaybeAddServerPrediction(FieldPrediction prediction) {
  overall_type_ = std::nullopt;
  if (server_predictions_.size() == 1 &&
      server_predictions_[0].type() == NO_SERVER_DATA &&
      server_predictions_[0].source() == FieldPrediction::SOURCE_UNSPECIFIED) {
    // If the only existing "server prediction" is an empty one, remove it.
    server_predictions_.clear();
  }

  const FieldType field_type =
      ToSafeFieldType(prediction.type(), NO_SERVER_DATA);
  prediction.set_type(field_type);

  // LOYALTY_MEMBERSHIP_ID server predictions are only available for clients
  // with the flag `kAutofillEnableLoyaltyCardsFilling` enabled.
  if (field_type == LOYALTY_MEMBERSHIP_ID &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableLoyaltyCardsFilling)) {
    return;
  }

  if (!prediction.has_source()) {
    // TODO(crbug.com/40243028): captured tests store old autofill api
    // response recordings without `source` field. We need to maintain the old
    // behavior until these recordings will be migrated.
    server_predictions_.push_back(std::move(prediction));
    return;
  }

  if (prediction.source() == FieldPrediction::SOURCE_UNSPECIFIED) {
    // A prediction with `SOURCE_UNSPECIFIED` is one of two things:
    //   1. No prediction for default, a.k.a. `NO_SERVER_DATA`. The absence
    //      of a prediction may not be creditable to a particular prediction
    //      source.
    //   2. An experiment that is missing from the `PredictionSource` enum.
    //      Protobuf corrects unknown values to 0 when parsing.
    // Neither case is actionable.
    return;
  }

  if (IsDefaultPrediction(prediction)) {
    server_predictions_.push_back(std::move(prediction));
  } else if (IsAutofillAiPrediction(prediction)) {
    if (base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema)) {
      server_predictions_.push_back(std::move(prediction));
    }
  }
}

void AutofillField::SetHtmlType(HtmlFieldType type, HtmlFieldMode mode) {
  html_type_ = type;
  html_mode_ = mode;
  overall_type_ = std::nullopt;
}

void AutofillField::SetTypeTo(const AutofillType& type,
                              std::optional<AutofillPredictionSource> source) {
  DCHECK(!type.GetTypes().empty());
  overall_type_ = {type, source};
}

AutofillType AutofillField::ComputedType() const {
  return GetComputedPredictionResult().type;
}

AutofillType AutofillField::Type() const {
  return GetOverallPredictionResult().type;
}

std::optional<AutofillPredictionSource> AutofillField::PredictionSource()
    const {
  return GetOverallPredictionResult().source;
}

AutofillType AutofillField::MakeAutofillType(FieldType primary_field_type,
                                             bool is_country_code) const {
  // Indicates whether `ft` may be part of the union type.
  auto is_union_type_candidate = [](FieldType ft) {
    return GroupTypeOfFieldType(ft) == FieldTypeGroup::kAutofillAi &&
           base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema);
  };

  // Returns the union of
  // - `primary_field_type` and
  // - the types of the `predictions` that satisfy is_union_type_candidate().
  auto get_filtered_types = [&](base::span<const FieldPrediction> predictions) {
    FieldTypeSet field_types = {primary_field_type};
    for (const auto& prediction : predictions) {
      const FieldType ft = ToSafeFieldType(prediction.type(), NO_SERVER_DATA);
      if (is_union_type_candidate(ft)) {
        field_types.insert(ft);
      }
    }
    return field_types;
  };

  // Looks for the longest prefix of `server_predictions_` whose filtered
  // FieldTypes satisfy the AutofillType constraints.
  FieldTypeSet field_types;
  size_t prefix_length = server_predictions_.size();
  do {
    field_types = get_filtered_types(
        base::span(server_predictions_).first(prefix_length));
  } while (!AutofillType::TestConstraints(field_types) && prefix_length-- > 0);
  DCHECK(field_types.contains(primary_field_type));
  return AutofillType(field_types, is_country_code);
}

AutofillField::PredictionResult AutofillField::GetOverallPredictionResult()
    const {
  // Server Overrides are granted precedence unconditionally.
  if (server_type_prediction_is_override() && server_type() != NO_SERVER_DATA) {
    return {MakeAutofillType(server_type()),
            AutofillPredictionSource::kServerOverride};
  }
  if (!overall_type_) {
    overall_type_ = GetComputedPredictionResult();
  }
  return *overall_type_;
}

AutofillField::PredictionResult AutofillField::GetComputedPredictionResult()
    const {
  // Some of these (in particular, heuristic_type()) are slow to compute, so
  // cache them in local variables.
  const HtmlFieldType html_type_local = html_type();
  const FieldType server_type_local = server_type();
  const FieldType heuristic_type_local = heuristic_type();
  const FieldType password_ml_classification_type_local =
      heuristic_type(HeuristicSource::kPasswordManagerMachineLearning);

  // If autocomplete=tel/tel-* and server confirms it really is a phone field,
  // we always use the server prediction as html types are not very reliable.
  if (GroupTypeOfHtmlFieldType(html_type_local) == FieldTypeGroup::kPhone &&
      GroupTypeOfFieldType(server_type_local) == FieldTypeGroup::kPhone) {
    return {MakeAutofillType(server_type_local),
            AutofillPredictionSource::kServerCrowdsourcing};
  }

  // TODO(crbug.com/40266396) Delete this if-statement when
  // features::kAutofillEnableExpirationDateImprovements has launched. This
  // should be covered by
  // FormStructureRationalizer::RationalizeAutocompleteAttributes.
  //
  // If the explicit type is cc-exp and either the server or heuristics agree on
  // a 2 vs 4 digit specialization of cc-exp, use that specialization.
  if (html_type_local == HtmlFieldType::kCreditCardExp &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    if (server_type_local == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        server_type_local == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return {MakeAutofillType(server_type_local),
              AutofillPredictionSource::kServerCrowdsourcing};
    }
    if (heuristic_type_local == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        heuristic_type_local == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return {MakeAutofillType(heuristic_type_local),
              AutofillPredictionSource::kHeuristics};
    }
  }

  // In general, the autocomplete attribute has precedence over the other types
  // of field detection. Except for specific cases in PreferHeuristicOverHtml
  // and also those detailed in `BelievedHtmlTypes()`.
  if (PreferHeuristicOverHtml(heuristic_type_local, html_type_local)) {
    return {MakeAutofillType(heuristic_type_local),
            AutofillPredictionSource::kHeuristics};
  }

  if (BelievedHtmlTypes(heuristic_type_local, server_type_local)
          .contains(html_type_local)) {
    // The following is a hack. If we have relevant server predictions to add
    // to the AutofillType, we want to add them. We must not do that if
    // `html_type_local == kCountryCode` because in that case, `html_type_local`
    // and `HtmlFieldTypeToBestCorrespondingFieldType(html_type_local)` behave
    // differently (crbug.com/436013479). In all other cases, they are
    // identical, except for AutofillType::ToString().
    // TODO(crbug.com/436013479): Remove AutofillType::is_country_code().
    AutofillType type = MakeAutofillType(
        HtmlFieldTypeToBestCorrespondingFieldType(html_type_local),
        /*is_country_code=*/html_type_local == HtmlFieldType::kCountryCode);
    return {type, AutofillPredictionSource::kAutocomplete};
  }

  if (!PreferHeuristicOverServer(heuristic_type_local, server_type_local,
                                 password_ml_classification_type_local)) {
    return {MakeAutofillType(server_type_local),
            AutofillPredictionSource::kServerCrowdsourcing};
  }

  // If the field was classified as an OTP field by PasswordManager and
  // `server_type_local` and `html_type_local` did not contradict it,
  // return PasswordManager prediction.
  if (password_ml_classification_type_local == ONE_TIME_CODE) {
    return {AutofillType(password_ml_classification_type_local),
            AutofillPredictionSource::kHeuristics};
  }

  return {MakeAutofillType(heuristic_type_local),
          heuristic_type_local != UNKNOWN_TYPE
              ? std::optional(AutofillPredictionSource::kHeuristics)
              : std::nullopt};
}

const std::u16string& AutofillField::value_for_import() const {
  const bool should_consider_value_for_import =
      IsSelectElement() || initial_value() != value();
  if (!should_consider_value_for_import) {
    return base::EmptyString16();
  }
  if (base::optional_ref<const SelectOption> o = selected_option()) {
    return o->text;
  }
  return value();
}

FieldSignature AutofillField::GetFieldSignature() const {
  return field_signature_ ? *field_signature_
                          : CalculateFieldSignatureByNameAndType(
                                name(), form_control_type());
}

std::string AutofillField::FieldSignatureAsStr() const {
  return base::NumberToString(GetFieldSignature().value());
}

bool AutofillField::IsFieldFillable() const {
  return std::ranges::any_of(Type().GetTypes(), IsFillableFieldType);
}

bool AutofillField::HasExpirationDateType() const {
  static constexpr FieldTypeSet kExpirationDateTypes = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_4_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR};
  return Type().GetTypes().contains_any(kExpirationDateTypes);
}

bool AutofillField::ShouldSuppressSuggestionsAndFillingByDefault() const {
  return html_type_ == HtmlFieldType::kUnrecognized &&
         !server_type_prediction_is_override() && !IsCreditCardPrediction();
}

void AutofillField::SetPasswordRequirements(PasswordRequirementsSpec spec) {
  password_requirements_ = std::move(spec);
}

base::optional_ref<const AutofillFormatString> AutofillField::format_string()
    const {
  if (form_control_type() == FormControlType::kInputDate) {
    static const base::NoDestructor<AutofillFormatString> kFormat(
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE));
    return *kFormat;
  }
  if (form_control_type() == FormControlType::kInputMonth) {
    static const base::NoDestructor<AutofillFormatString> kFormat(
        AutofillFormatString(u"YYYY-MM", FormatString_Type_DATE));
    return *kFormat;
  }
  if (format_string_source_ == AutofillFormatStringSource::kUnset) {
    return std::nullopt;
  }
  return format_string_;
}

bool AutofillField::IsCreditCardPrediction() const {
  return GroupTypeOfFieldType(server_type()) == FieldTypeGroup::kCreditCard ||
         GroupTypeOfFieldType(heuristic_type()) == FieldTypeGroup::kCreditCard;
}

void AutofillField::AppendLogEventIfNotRepeated(
    const FieldLogEventType& log_event) {
  if (!field_log_events_) {
    return;
  }
  if (field_log_events_->empty() ||
      field_log_events_->back().index() != log_event.index() ||
      !AreCollapsibleLogEvents(field_log_events_->back(), log_event)) {
    if (field_log_events_->size() < kMaxLogEventsPerField) {
      field_log_events_->push_back(log_event);
    } else {
      // For fields that exceed the number of allowed events, we do not keep
      // track of any events to avoid memory regressions.
      field_log_events_ = std::nullopt;
    }
  }
}

bool AutofillField::WasAutofilledWithFallback() const {
  return autofilled_type_ &&
         (!overall_type_ ||
          !overall_type_->type.GetTypes().contains(*autofilled_type_));
}

}  // namespace autofill

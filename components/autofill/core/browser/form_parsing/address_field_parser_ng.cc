// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field_parser_ng.h"

#include <initializer_list>
#include <ostream>
#include <string_view>

#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {

namespace {

// Specify `--vmodule=address_field_parser_ng=1` to get insights into the
// classification process. It will produce output describing the recursive
// exploration of field type assignments.

constexpr FieldType kAddressLines[] = {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                       ADDRESS_HOME_LINE3};
constexpr FieldTypeSet kAddressLinesFieldTypeSet({ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_LINE2,
                                                  ADDRESS_HOME_LINE3});

// Adds a FormControlType to MatchParams.
MatchParams MatchParamsWithFieldType(MatchParams p,
                                     FormControlType field_type) {
  p.field_types.insert(field_type);
  return p;
}

// Removes a MatchAttribute from MatchParams.
MatchParams MatchParamsWithoutAttribute(MatchParams p,
                                        MatchAttribute attribute) {
  p.attributes.erase(attribute);
  return p;
}

std::string SequenceToScoreString(const ClassifiedFieldSequence& sequence) {
  return base::NumberToString(sequence.contained_types.size()) + "/" +
         base::NumberToString(sequence.score);
}

}  // namespace

// This class stores precalculated work for the address hierarchy of a
// specific country which should be precalculated to prevent repetitive work
// during form parsing.
class AddressFieldParserNG::FieldTypeInformation {
 public:
  explicit FieldTypeInformation(AddressCountryCode country_code);
  ~FieldTypeInformation();

  // Returns the set of field types supported by the address hierarchy of
  // the country passed in the constructor.
  FieldTypeSet supported_field_types() const { return supported_field_types_; }

  // Returns the set of field types that must not occur if `type` is already
  // assigned to a field:
  // - If a type T was assigned to a field, no ancestor or descendant of T
  //   should be assigned to one of the following fields in the parsing run
  //   because one would contain the other.
  //   E.g. a street name and a street address are incompatible because a
  //   a street name is contained in the street address.
  // - Each type T is incompatible with itself (we want to classify only one
  //   field as a type in a single parse run).
  //   E.g. we don't want to classify two postal code fields.
  // - For synthesized field types, we also consider two types T and T2 as
  //   incompatible if T and T2 share any descendants.
  //   E.g. a landmark+street-location is incompatible to a landmark+locality
  //   because they share a landmark.
  // - Structured address details (street name, house number, ...) are
  //   incompatible with address lines.
  // The function must only be called on `FieldType`s that exist in
  // `supported_field_types()`.
  FieldTypeSet incompatible_field_types(FieldType type) const {
    return incompatible_.at(type);
  }

 private:
  void InitializeFieldTypesAndDescendants(AddressComponent* node);
  void InitializeIncompatibilities();

  const bool is_custom_hierarchy_available_for_country = false;

  // All field types existing in the address model of the specified country.
  FieldTypeSet supported_field_types_;

  // All descendants of each field type, including the key field type itself.
  base::flat_map<FieldType, FieldTypeSet> descendants_and_self_;

  // All field types that are incompatible with the key field type meaning that
  // those field types must not be produced by the same execution of
  // `AddressFieldParserNG::Parse()` (e.g. because a ClassifiedFieldSequence
  // should not contain multiple instances of the same field type or because
  // address lines 1, 2, 3 should not co-exist with a landmark; see
  // `incompatible_field_types()`).
  base::flat_map<FieldType, FieldTypeSet> incompatible_;

  friend std::ostream& operator<<(std::ostream& os,
                                  const FieldTypeInformation& field_types);
};

AddressFieldParserNG::FieldTypeInformation::FieldTypeInformation(
    AddressCountryCode country_code)
    : is_custom_hierarchy_available_for_country(
          i18n_model_definition::IsCustomHierarchyAvailableForCountry(
              country_code)) {
  AddressComponentsStore model =
      i18n_model_definition::CreateAddressComponentModel(country_code);

  InitializeFieldTypesAndDescendants(model.Root());

  // Address lines 1, 2, 3 are not part of the `model` (they are derived from a
  // ADDRESS_HOME_STREET_ADDRESS). Therefore, they are not considered by
  // `InitializeFieldTypesAndDescendants()`. For the purpose of classifying
  // fields in an address form, they should still be listed in
  // `supported_field_types_` and `descendants_and_self_`.
  for (FieldType child_type : kAddressLines) {
    supported_field_types_.insert(child_type);
    descendants_and_self_[child_type].insert(child_type);
    descendants_and_self_[ADDRESS_HOME_STREET_ADDRESS].insert(child_type);
  }

  // The COMPANY_NAME is not part of the address model but classified by the
  // AddressFieldParser.
  supported_field_types_.insert(COMPANY_NAME);
  descendants_and_self_[COMPANY_NAME].insert(COMPANY_NAME);
  incompatible_[COMPANY_NAME] = FieldTypeSet({COMPANY_NAME});

  // UNKNOWN_TYPE is a non-standard field type that is repurposed for internal
  // logic. It is used to skip certain fields in the classification that may
  // occur at arbitrary locations in the address form but don't belong to the
  // address hierarchy. For example an email field belongs into this category.
  // UNKNOWN_TYPE is only used internally by the `AddressFieldParserNG` and
  // never returned to the caller of the parse function.
  supported_field_types_.insert(UNKNOWN_TYPE);
  descendants_and_self_[UNKNOWN_TYPE].insert(UNKNOWN_TYPE);

  for (FieldType t : supported_field_types_) {
    CHECK(descendants_and_self_.contains(t)) << FieldTypeToStringView(t);
  }

  InitializeIncompatibilities();

  DVLOG(1) << "FieldTypeInformation for " << country_code.value();
  DVLOG(1) << *this;
}

AddressFieldParserNG::FieldTypeInformation::~FieldTypeInformation() = default;

void AddressFieldParserNG::FieldTypeInformation::
    InitializeFieldTypesAndDescendants(AddressComponent* node) {
  // This function is a recursive descend through the address model tree for a
  // specific country to collect all field types that occur in the address model
  // in `supported_field_types_` and determine for each node which descendants
  // exist.
  FieldType field_type = node->GetStorageType();

  if (descendants_and_self_.contains(field_type)) {
    return;
  }
  descendants_and_self_[field_type].insert(field_type);
  supported_field_types_.insert(field_type);

  auto InitializeChild = [&](AddressComponent* child) {
    InitializeFieldTypesAndDescendants(child);
    // Invariant: All children have already updated their
    // `descendants_and_self_`.
    // Note: Don't inline the following line because two operator[]() calls lead
    // to undefined behavior. Each one may modify the underlying map and return
    // invalid references.
    FieldTypeSet values_of_child =
        descendants_and_self_[child->GetStorageType()];
    descendants_and_self_[field_type].insert_all(values_of_child);
  };

  for (AddressComponent* child : node->Subcomponents()) {
    InitializeChild(child);
  }

  for (AddressComponent* child : node->SynthesizedSubcomponents()) {
    InitializeChild(child);
  }
}

void AddressFieldParserNG::FieldTypeInformation::InitializeIncompatibilities() {
  // Each field type may be assigned only once and is therefore incompatible to
  // itself.
  for (FieldType field_type : supported_field_types_) {
    incompatible_[field_type] = FieldTypeSet();
  }

  // The descendants of a ADDRESS_HOME_STREET_ADDRESS in the address model are
  // (except for address lines 1, 2, 3) the structured street address components
  // like street name, house number, etc.

  // If either kAutofillStructuredFieldsDisableAddressLines is enabled or a
  // country is explicitly modeled for the i18n address hierarchy, these
  // structured address components are incompatible with address lines 1, 2, 3.
  // For the legacy model we allow an address line 2 to be paired with a street
  // name and house number.
  const bool autofill_structured_fields_disable_address_lines =
      base::FeatureList::IsEnabled(
          features::kAutofillStructuredFieldsDisableAddressLines) ||
      is_custom_hierarchy_available_for_country;

  // Because address lines 1, 2, 3 are already attached as children of
  // ADDRESS_HOME_STREET_ADDRESS, they need to be removed first to get true set
  // of structured address components.
  FieldTypeSet structured_address_components =
      descendants_and_self_[ADDRESS_HOME_STREET_ADDRESS];
  structured_address_components.erase_all(kAddressLinesFieldTypeSet);
  for (FieldType child_type : structured_address_components) {
    if (autofill_structured_fields_disable_address_lines) {
      for (FieldType address_line : kAddressLines) {
        incompatible_[child_type].insert(address_line);
        incompatible_[address_line].insert(child_type);
      }
    } else {
      if (supported_field_types_.contains(ADDRESS_HOME_STREET_NAME)) {
        incompatible_[ADDRESS_HOME_LINE1].insert(ADDRESS_HOME_STREET_NAME);
        incompatible_[ADDRESS_HOME_STREET_NAME].insert(ADDRESS_HOME_LINE1);
      }
      if (supported_field_types_.contains(ADDRESS_HOME_HOUSE_NUMBER)) {
        incompatible_[ADDRESS_HOME_LINE1].insert(ADDRESS_HOME_HOUSE_NUMBER);
        incompatible_[ADDRESS_HOME_HOUSE_NUMBER].insert(ADDRESS_HOME_LINE1);
      }
    }
  }

  // A field type T is incompatible with its ancestors and descendants. For
  // synthesized nodes it's also possible that a field type T is incompatible
  // with a type T2 that is neither an ancestor nor a descendant but shares
  // some descendants.
  for (FieldType c1 : supported_field_types_) {
    for (FieldType c2 : supported_field_types_) {
      // Comparing the underlying integer values of c1 and c2 is a speed
      // optimization to avoid redundant work that would happen due to symmetry.
      if (base::to_underlying(c1) > base::to_underlying(c2)) {
        continue;
      }
      // Note: Don't inline the following line because two operator[]() calls
      // lead to undefined behavior. Each one may modify the underlying map and
      // return invalid references.
      FieldTypeSet c2_values = descendants_and_self_[c2];
      if (descendants_and_self_[c1].contains_any(c2_values)) {
        incompatible_[c1].insert(c2);
        incompatible_[c2].insert(c1);
      }
    }
  }
}

std::ostream& operator<<(
    std::ostream& os,
    const AddressFieldParserNG::FieldTypeInformation& field_types) {
  os << "FieldTypeInformation::descendants_and_self_:\n";
  for (FieldType child_type : field_types.supported_field_types_) {
    if (!field_types.descendants_and_self_.at(child_type).empty()) {
      os << FieldTypeToStringView(child_type) << ":";
      for (FieldType desc : field_types.descendants_and_self_.at(child_type)) {
        os << " " << FieldTypeToStringView(desc);
      }
      os << "\n";
    }
  }
  os << "FieldTypeInformation::incompatible_:\n";
  for (FieldType child_type : field_types.supported_field_types_) {
    if (!field_types.incompatible_.at(child_type).empty()) {
      os << FieldTypeToStringView(child_type) << ":";
      for (FieldType incompatible : field_types.incompatible_.at(child_type)) {
        os << " " << FieldTypeToStringView(incompatible);
      }
      os << "\n";
    }
  }
  return os;
}

ClassifiedFieldSequence::ClassifiedFieldSequence() = default;
ClassifiedFieldSequence::~ClassifiedFieldSequence() = default;

bool ClassifiedFieldSequence::BetterThan(
    const ClassifiedFieldSequence& other) const {
  if (contained_types.size() != other.contained_types.size()) {
    return contained_types.size() > other.contained_types.size();
  }
  return score > other.score;
}

// TODO(crbug.com/328954153): This initialization of prepared work could be
// cached in a registry to prevent the repetitive creation effort.
AddressFieldParserNG::AddressFieldParserNG(AddressCountryCode client_country)
    : field_types_(std::make_unique<FieldTypeInformation>(client_country)) {}

AddressFieldParserNG::~AddressFieldParserNG() = default;

// static
std::unique_ptr<FormFieldParser> AddressFieldParserNG::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return nullptr;
  }

  size_t saved_cursor = scanner->SaveCursor();
  std::unique_ptr<AddressFieldParserNG> address_field(new AddressFieldParserNG(
      AddressCountryCode(context.client_country.value())));
  address_field->context_ = &context;
  address_field->scanner_ = scanner;
  address_field->initial_field_ = scanner->Cursor();

  DVLOG(1) << "Parse recursively starting at " << saved_cursor << " "
           << scanner->Cursor()->parseable_label();

  address_field->ParseRecursively();

  // These members are used during the parse run and should be cleared because
  // we cannot make any life-cycle assumptions on them beyond the call of Parse.
  address_field->context_ = nullptr;
  address_field->scanner_ = nullptr;
  address_field->initial_field_ = nullptr;

  // As per the contract of parse functions: If a viable classification was
  // found, set the cursor to the last classified field + 1, otherwise return
  // the scanner in the initial state.
  if (!address_field->best_classification_.assignments.empty()) {
    scanner->RewindTo(
        address_field->best_classification_.last_classified_field_index);
    scanner->Advance();
    return address_field;
  }
  scanner->RewindTo(saved_cursor);
  return nullptr;
}

void AddressFieldParserNG::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  for (auto [field_type, field_ptr] : best_classification_.assignments) {
    if (!field_ptr) {
      continue;
    }
    AddClassification(field_ptr, field_type, kBaseAddressParserScore,
                      field_candidates);
  }
}

base::span<const MatchPatternRef> AddressFieldParserNG::GetMatchPatterns(
    std::string_view name) {
  return ::autofill::GetMatchPatterns(name, context_->page_language,
                                      context_->pattern_file);
}

std::optional<double> AddressFieldParserNG::FindScoreOfBestMatchingRule(
    FieldType field_type) {
  // Naming convention: In the following code,
  //   auto r =
  // is a short cut for
  //   std::optional<double> result =
  // is used consistently to keep the code readable.

  // Give the label priority over the name to avoid misclassifications when the
  // name has a misleading value (e.g. in TR the province field is named "city",
  // in MX the input field for "Municipio/Delegación" is sometimes named "city"
  // even though that should be mapped to a "Ciudad"). The list of conditions is
  // currently hard-coded for simplicity and performance.
  // We may want to consider whether we unify this logic with the two if-blocks.
  // The first block is language-based, the second one is country based.
  // Currently, we don't always prefer labels if page_language ==
  // LanguageCode("es") here because Spanish is spoken in many countries and we
  // don't know whether such a change would be uniformly positive. At the same
  // time, limiting prefer_label in the first block to the Turkish geolocation
  // may restrict the behavior more than necessary.
  bool prefer_label = false;
  if (context_->page_language == LanguageCode("tr") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLabelPrecedenceForTurkishAddresses)) {
    prefer_label = true;
  } else if (context_->client_country == GeoIpCountryCode("MX")) {
    prefer_label = true;
  }

  auto MatchOnlyLabel = [](const MatchParams& p) {
    return MatchParamsWithoutAttribute(p, MatchAttribute::kName);
  };
  auto MatchOnlyName = [](const MatchParams& p) {
    return MatchParamsWithoutAttribute(p, MatchAttribute::kLabel);
  };
  // Returns `score` if the regex pattern identified by `pattern_name` matches
  // against the label or name of a field. In some countries we prefer matches
  // to labels over matches to field names; in other countries we prefer matches
  // to field names. If a match happens on the preferred attribute, the score is
  // boosted by 0.05.
  auto Match = [&](std::string_view pattern_name, double score,
                   MatchParams (*match_pattern_projection)(const MatchParams&) =
                       nullptr) -> std::optional<double> {
    // Helper function to consecutively match the regex against the label and
    // the name attribute in the desired order and adding a boost in case the
    // preferred attribute match.
    auto MatchAttribute = [&](bool match_label) -> std::optional<double> {
      if (FieldMatchesMatchPatternRef(
              *context_, GetMatchPatterns(pattern_name), *scanner_->Cursor(),
              pattern_name.data(),
              {match_label ? MatchOnlyLabel : MatchOnlyName,
               match_pattern_projection})) {
        return score + (match_label == prefer_label ? 0.05 : 0.0);
      }
      return std::nullopt;
    };
    if (prefer_label) {
      auto r = MatchAttribute(/*match_label*/ true);
      return r ? r : MatchAttribute(/*match_label*/ false);
    } else {
      auto r = MatchAttribute(/*match_label*/ false);
      return r ? r : MatchAttribute(/*match_label*/ true);
    }
  };

  // TOOD(crbug.com/328954153) Consider whether it makes sense to pull the
  // country specific rules out of this big switch statement.
  switch (field_type) {
    case UNKNOWN_TYPE:
      // The following are field types that may occur interspersed in an
      // address form but matches are ignored. Email fields are reported by a
      // different FormFieldParser. The other fields are just ignored.
      for (const char* type : {"ADDRESS_LOOKUP", "ADDRESS_NAME_IGNORED",
                               "EMAIL_ADDRESS", "ATTENTION_IGNORED"}) {
        if (Match(type, 10.0)) {
          return 10;
        }
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_ADDRESS:
      // The score is a bit higher than the score of an address line 1.
      // This ensures that
      // score(ADDRESS_HOME_STREET_ADDRESS) > score(ADDRESS_HOME_LINE1)
      // but
      // score(ADDRESS_HOME_STREET_ADDRESS) < score(ADDRESS_HOME_LINE1) +
      //                                      score(ADDRESS_HOME_LINE2)
      return Match("ADDRESS_LINE_1", 1.6, [](const MatchParams& p) {
        return MatchParamsWithFieldType(p, FormControlType::kTextArea);
      });
    case ADDRESS_HOME_LINE1:
      return Match("ADDRESS_LINE_1", 1.0);
    case ADDRESS_HOME_LINE2:
      // Address lines 2 can follow address lines 1 - and, if
      // kAutofillStructuredFieldsDisableAddressLines is disabled, a street name
      // and house number. If kAutofillStructuredFieldsDisableAddressLines is
      // enabled, `incompatible_` will suppress a combination of street
      // name/house number and address line 2.
      if (partial_classification_.contained_types.contains(
              ADDRESS_HOME_LINE1) ||
          partial_classification_.contained_types.contains_all(
              {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER})) {
        // If the country model does not contain support for an apartment
        // number, we treat a match for the apartment number regex as an
        // address line 2.
        if (!field_types_->supported_field_types().contains(
                ADDRESS_HOME_APT_NUM)) {
          if (auto r = Match("ADDRESS_HOME_APT_NUM", 1.0)) {
            return r;
          }
        }
        return Match("ADDRESS_LINE_2", 1.0);
      }
      return std::nullopt;
    case ADDRESS_HOME_LINE3:
      // An address line 3 can only directly follow an address line 2.
      if (partial_classification_.contained_types.contains_all(
              {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2}) &&
          partial_classification_.assignments[ADDRESS_HOME_LINE2]->rank() ==
              scanner_->Cursor()->rank() - 1) {
        if (auto r = Match("ADDRESS_LINE_2", 1.0)) {
          return r;
        }
        return Match("ADDRESS_LINE_EXTRA", 1.0);
      }
      return std::nullopt;
    case ADDRESS_HOME_APT_NUM:
      return Match("ADDRESS_HOME_APT_NUM", 1.0);
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_TYPE:
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      // ADDRESS_HOME_APT, ADDRESS_HOME_APT_TYPE and
      // ADDRESS_HOME_HOUSE_NUMBER_AND_APT are currently internal nodes of the
      // address hierarchy that only exist to parse and format an address. They
      // don't exist as recognized field types.
      return std::nullopt;
    case ADDRESS_HOME_CITY:
      return Match("CITY", 1.0);
    case ADDRESS_HOME_STATE:
      return Match("STATE", 1.0);
    case ADDRESS_HOME_ZIP:
      return Match("ZIP_CODE", 1.0);
      // TODO(crbug.com/328954153): ZIP4
    case ADDRESS_HOME_COUNTRY:
      // A bit >1.0 to prefer country over state in "country/region"
      if (auto r = Match("COUNTRY", 1.1)) {
        return r;
      }
      // The occasional page (e.g. google account registration page) calls
      // this a "location". However, this only makes sense for select tags, so
      // a different PatternRef is used.
      return Match("COUNTRY_LOCATION", 1.1);
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      // In India a special regex is used for the locality (dependent locality).
      if (context_->client_country == GeoIpCountryCode("IN")) {
        return Match("IN_DEPENDENT_LOCALITY", 1.0);
      }
      return Match("ADDRESS_HOME_DEPENDENT_LOCALITY", 1.0);
    case ADDRESS_HOME_STREET_NAME:
      // A bit >1.0 to prefer a street name over address line 1.
      return Match("ADDRESS_HOME_STREET_NAME", 1.1);
    case ADDRESS_HOME_HOUSE_NUMBER:
      return Match("ADDRESS_HOME_HOUSE_NUMBER", 1.1);
    case ADDRESS_HOME_STREET_LOCATION:
      // In India a special regex is used for the street location.
      if (context_->client_country == GeoIpCountryCode("IN")) {
        return Match("IN_STREET_LOCATION", 1.0);
      }
      // In most countries, street location is a combination of multiple
      // fields. Therefore, the score is higher than the score of each compound.
      return Match("ADDRESS_HOME_STREET_LOCATION", 1.5);
    case ADDRESS_HOME_LANDMARK:
      return Match("LANDMARK", 1.0);
    case ADDRESS_HOME_BETWEEN_STREETS:
      return Match("BETWEEN_STREETS", 1.5);
    case ADDRESS_HOME_BETWEEN_STREETS_1:
      // These are scored a big higher than ADDRESS_HOME_STREET_NAME to give
      // priority to ADDRESS_HOME_BETWEEN_STREETS_1/2. This is fine because the
      // regex is more specific than the regex for ADDRESS_HOME_STREET_NAME.
      return Match("BETWEEN_STREETS_LINE_1", 1.2);
    case ADDRESS_HOME_BETWEEN_STREETS_2:
      if (partial_classification_.contained_types.contains(
              ADDRESS_HOME_BETWEEN_STREETS_1)) {
        return Match("BETWEEN_STREETS_LINE_2", 1.2);
      }
      return std::nullopt;
    case ADDRESS_HOME_ADMIN_LEVEL2:
      // The score is a bit higher than city for MX because the term
      // "Municipio/Delegación" should take precedence.
      return Match("ADMIN_LEVEL_2", 1.1);
    case ADDRESS_HOME_OVERFLOW:
      return Match("OVERFLOW", 1.0);
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      // Higher score because the field needs to contain hints for both
      // between streets and landmark.
      return Match("BETWEEN_STREETS_OR_LANDMARK", 1.7);
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      // Higher score because the field needs to contain hints for both
      // overflow and landmark.
      return Match("OVERFLOW_AND_LANDMARK", 1.7);
    case COMPANY_NAME:
      // A bit less than 1.0 to prioritize an address line 2 interpretation.
      // score(street address) + score(company name) <
      //   score(address line 1) + score(address line 2)
      if (!Match("ADDRESS_LINE_1", 1.0) && !Match("ADDRESS_LINE_2", 1.0) &&
          !Match("ADDRESS_HOME_APT_NUM", 1.0)) {
        return Match("COMPANY_NAME", 0.8);
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_STREET_LOCATION", 1.0) &&
          Match("IN_DEPENDENT_LOCALITY", 1.0)) {
        return 1.5;
      }
      return std::nullopt;
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_STREET_LOCATION", 1.0) && Match("LANDMARK", 1.0)) {
        return 1.5;
      }
      // Some Location and landmark fields are labeled "Address". We give it
      // a 0.8 score to prefer a classification as a STREET_ADDRESS or
      // ADDRESS_LINE_1, but allow a higher score when combined with a
      // landmark.
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("ADDRESS_LINE_1", 1.0)) {
        return 0.8;
      }
      return std::nullopt;
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
      if (context_->client_country == GeoIpCountryCode("IN") &&
          Match("IN_DEPENDENT_LOCALITY", 1.0) && Match("LANDMARK", 1.0)) {
        return 1.5;
      }
      return std::nullopt;

    // Address related fields that we don't parse (yet).
    case DELIVERY_INSTRUCTIONS:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_SORTING_CODE:
      return std::nullopt;

    // Fields that are not processed by the AddressFieldParserNG.
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case EMAIL_ADDRESS:
    case USERNAME_AND_EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_WHOLE_NUMBER:
    case PHONE_HOME_EXTENSION:
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case IBAN_VALUE:
    case MERCHANT_PROMO_CODE:
    case USERNAME:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case SINGLE_USERNAME:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case NOT_PASSWORD:
    case NOT_USERNAME:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case ONE_TIME_CODE:
    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case PRICE:
    case NUMERIC_QUANTITY:
    case SEARCH_TERM:
    case IMPROVED_PREDICTION:
    case MAX_VALID_FIELD_TYPE:
      return std::nullopt;
  }
}

void AddressFieldParserNG::ParseRecursively() {
  const std::string log_prefix(scanner_->SaveCursor(), ' ');
  if (scanner_->IsEnd()) {
    DVLOG(1) << log_prefix << "END of input";
    DVLOG(1) << log_prefix
             << "score=" << SequenceToScoreString(partial_classification_)
             << ", best_score_so_far="
             << SequenceToScoreString(best_classification_)
             << ", plausible=" << IsClassificationPlausible();
    // Store classification if it's better.
    if (partial_classification_.BetterThan(best_classification_) &&
        IsClassificationPlausible()) {
      DVLOG(1) << log_prefix << "NEW BEST SOLUTION";
      best_classification_ = partial_classification_;
    }
    return;
  }

  // UNKNOWN_TYPE should always be the first element. If we have a match,
  // we skip all other field types.
  CHECK_EQ(*field_types_->supported_field_types().begin(), UNKNOWN_TYPE);

  // Whether any field type could be assigned for the current scanner position.
  bool found_extra_assignment = false;
  for (FieldType field_type : field_types_->supported_field_types()) {
    // Skip trying field_type if it's incompatible with already assigned types.
    if (partial_classification_.contained_types.contains_any(
            field_types_->incompatible_field_types(field_type))) {
      DVLOG(1) << log_prefix << "---- " << FieldTypeToStringView(field_type)
               << " conflict.";
      continue;
    }

    std::optional<double> extra_score = FindScoreOfBestMatchingRule(field_type);
    if (!extra_score) {
      DVLOG(1) << log_prefix << "---- " << FieldTypeToStringView(field_type)
               << " non-match.";
      continue;
    }

    found_extra_assignment = true;

    // Perform new assignment.
    const double old_score = partial_classification_.score;
    const size_t old_last_classified_field_index =
        partial_classification_.last_classified_field_index;
    if (field_type != UNKNOWN_TYPE) {
      partial_classification_.contained_types.insert(field_type);
      partial_classification_.assignments[field_type] = scanner_->Cursor();
      partial_classification_.last_classified_field_index =
          scanner_->SaveCursor();
    }
    partial_classification_.score += *extra_score;

    DVLOG(1) << log_prefix << "++++ " << FieldTypeToStringView(field_type)
             << " match. new score is " << partial_classification_.score;

    const size_t old_position = scanner_->SaveCursor();
    scanner_->Advance();
    ParseRecursively();
    scanner_->RewindTo(old_position);

    // Revert new assignment.
    if (field_type != UNKNOWN_TYPE) {
      partial_classification_.contained_types.erase(field_type);
      partial_classification_.assignments[field_type] = nullptr;
      partial_classification_.last_classified_field_index =
          old_last_classified_field_index;
    }
    partial_classification_.score = old_score;

    // If we had a match on UNKNOWN_TYPE (i.e. an email field, address lookup
    // field, etc.), we don't want to try other field types. E.g. "address" is
    // a substring of "email address" and should not be considered.
    if (field_type == UNKNOWN_TYPE && extra_score) {
      break;
    }
  }
  if (!found_extra_assignment) {
    DVLOG(1) << log_prefix << "END did not find another classification.";
    DVLOG(1) << log_prefix
             << "score=" << SequenceToScoreString(partial_classification_)
             << ", best_score_so_far="
             << SequenceToScoreString(best_classification_)
             << ", plausible=" << IsClassificationPlausible();
    if (partial_classification_.BetterThan(best_classification_) &&
        IsClassificationPlausible()) {
      DVLOG(1) << log_prefix << "NEW BEST SOLUTION";
      best_classification_ = partial_classification_;
    }
  }
}

bool AddressFieldParserNG::IsClassificationPlausible() const {
  // The house number is easy to guess wrong (e.g. to mix up with a CC number).
  // Therefore, we require extra evidence.
  const FieldTypeSet& contained_types = partial_classification_.contained_types;
  if (contained_types.contains(ADDRESS_HOME_HOUSE_NUMBER) &&
      !contained_types.contains_any(
          {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_OVERFLOW,
           ADDRESS_HOME_LANDMARK, ADDRESS_HOME_OVERFLOW_AND_LANDMARK})) {
    return false;
  }
  if (contained_types.contains(ADDRESS_HOME_APT_NUM) &&
      !contained_types.contains_all(
          {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER})) {
    return false;
  }
  return true;
}

}  // namespace autofill

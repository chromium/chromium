// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::i18n_model_definition {

namespace {
using i18n_model_definition::kAutofillFormattingRulesMap;
using i18n_model_definition::kAutofillModelRules;
using i18n_model_definition::kAutofillParsingRulesMap;

// Adjacency mapping, stores for each field type X the list of field types
// which are children of X.
using TreeDefinition =
    base::flat_map<ServerFieldType, base::span<const ServerFieldType>>;

std::unique_ptr<I18nAddressComponent> BuildSubTree(
    const TreeDefinition& tree_def,
    ServerFieldType root) {
  std::vector<std::unique_ptr<I18nAddressComponent>> children;
  // Leaf nodes do not have an entry in the tree_def.
  if (tree_def.contains(root)) {
    children.reserve(tree_def.at(root).size());
    for (ServerFieldType child_type : tree_def.at(root)) {
      children.push_back(BuildSubTree(tree_def, child_type));
    }
  }
  return std::make_unique<I18nAddressComponent>(root, std::move(children),
                                                MergeMode::kDefault);
}


}  // namespace

std::unique_ptr<AddressComponent> CreateAddressComponentModel(
    std::string_view country_code) {
  auto* it = kAutofillModelRules.find(country_code);

  // If the entry is not defined, use the legacy rules.
  auto tree_edges = it == kAutofillModelRules.end()
                        ? kAutofillModelRules.find("XX")->second
                        : it->second;

  // Convert the list of node properties into an adjacency lookup table.
  // For each field type it stores the list of children of the field type.
  TreeDefinition tree_def =
      base::MakeFlatMap<ServerFieldType, base::span<const ServerFieldType>>(
          tree_edges, {}, [](const auto& item) {
            return std::make_pair(item.field_type, item.children);
          });

  auto result = BuildSubTree(tree_def, ADDRESS_HOME_ADDRESS);

  if (!country_code.empty()) {
    // Set the address model country to the one requested.
    result->SetValueForType(ADDRESS_HOME_COUNTRY,
                            base::UTF8ToUTF16(country_code),
                            VerificationStatus::kObserved);
  }
  return result;
}

std::u16string GetFormattingExpression(ServerFieldType field_type,
                                       std::string_view country_code) {
  auto* it = kAutofillFormattingRulesMap.find({country_code, field_type});
  // If `country_code` is specified return a custom formatting expression if
  // exist.
  if (!country_code.empty() && it != kAutofillFormattingRulesMap.end()) {
    return std::u16string(it->second);
  }

  // Otherwise return a legacy formatting expression that exists.
  auto* legacy_it = kAutofillFormattingRulesMap.find({"XX", field_type});
  return legacy_it != kAutofillFormattingRulesMap.end()
             ? std::u16string(legacy_it->second)
             : u"";
}

i18n_model_definition::ValueParsingResults ParseValueByI18nRegularExpression(
    std::string_view value,
    ServerFieldType field_type,
    std::string_view country_code) {
  auto* it = kAutofillParsingRulesMap.find({country_code, field_type});
  // If `country_code` is specified, attempt to parse the `value` using a custom
  // parsing structure (if exist).
  if (!country_code.empty() && it != kAutofillParsingRulesMap.end()) {
    return it->second->Parse(value);
  }

  // Otherwise try using a legacy parsing expression (if exist).
  auto* legacy_it = kAutofillParsingRulesMap.find({"XX", field_type});
  return legacy_it != kAutofillParsingRulesMap.end()
             ? legacy_it->second->Parse(value)
             : absl::nullopt;
}

}  // namespace autofill::i18n_model_definition

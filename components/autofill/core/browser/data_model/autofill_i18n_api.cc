// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::i18n_model_definition {

namespace {
using i18n_model_definition::kAutofillFormattingRulesMap;
using i18n_model_definition::kAutofillModelRules;

// Adjacency mapping, stores for each field type X the list of field types
// which are children of X.
using TreeDefinition =
    base::flat_map<ServerFieldType, base::span<const ServerFieldType>>;

ServerFieldType GetRootFieldType(AutofillModelType model_type) {
  switch (model_type) {
    case AutofillModelType::kAddressModel:
      return ADDRESS_HOME_ADDRESS;
    case AutofillModelType::kNameModel:
      return NAME_FULL;
  }
  NOTREACHED_NORETURN();
}

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

std::unique_ptr<AddressComponent> GetLegacyHierarchy(
    AutofillModelType model_type) {
  switch (model_type) {
    case AutofillModelType::kAddressModel:
      return std::make_unique<AddressNode>();
    case AutofillModelType::kNameModel:
      if (HonorificPrefixEnabled()) {
        return std::make_unique<NameFullWithPrefix>();
      }
      return std::make_unique<NameFull>();
  }
  NOTREACHED_NORETURN();
}

}  // namespace

std::unique_ptr<AddressComponent> CreateAddressComponentModel(
    AutofillModelType model_type,
    std::string_view country_code) {
  auto* it = kAutofillModelRules.find(country_code);
  if (it == kAutofillModelRules.end()) {
    return GetLegacyHierarchy(model_type);
  }

  // Convert the list of node properties into an adjacency lookup table.
  // For each field type it stores the list of children of the field type.
  TreeDefinition tree_def =
      base::MakeFlatMap<ServerFieldType, base::span<const ServerFieldType>>(
          it->second, {}, [](const auto& item) {
            return std::make_pair(item.field_type, item.children);
          });

  auto result = BuildSubTree(tree_def, GetRootFieldType(model_type));

  if (model_type == AutofillModelType::kAddressModel) {
    // Set the address model country to the one requested.
    result->SetValueForType(ADDRESS_HOME_COUNTRY,
                            base::UTF8ToUTF16(country_code),
                            VerificationStatus::kObserved);
  }
  return result;
}

std::u16string_view GetFormattingExpression(ServerFieldType field_type,
                                            std::string_view country_code) {
  auto* it = kAutofillFormattingRulesMap.find({country_code, field_type});
  return it != kAutofillFormattingRulesMap.end() ? it->second : u"";
}

}  // namespace autofill::i18n_model_definition

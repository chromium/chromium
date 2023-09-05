// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include <string>
#include <type_traits>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::i18n_model_definition {

namespace {

// Checks that the AddressComponent graph has no cycles.
bool IsTree(AddressComponent* node, ServerFieldTypeSet* visited_types) {
  if (visited_types->contains(node->GetStorageType()) ||
      visited_types->contains_any(node->GetAdditionalSupportedFieldTypes())) {
    // Repeated types exist in the tree.
    return false;
  }
  visited_types->insert(node->GetStorageType());
  visited_types->insert_all(node->GetAdditionalSupportedFieldTypes());
  if (node->Subcomponents().empty()) {
    return true;
  }
  return base::ranges::all_of(node->Subcomponents(),
                              [&visited_types](AddressComponent* child) {
                                return IsTree(child, visited_types);
                              });
}
}  // namespace

TEST(AutofillI18nApi, GetAddressComponentModel_ReturnsNonEmptyModel) {
  for (const auto& [country_code, properties] : kAutofillModelRules) {
      // Make sure that the process of building the model finishes and returns a
      // non empty hierarchy.
      std::unique_ptr<AddressComponent> model =
          CreateAddressComponentModel(country_code);

      ASSERT_TRUE(model);
      ServerFieldTypeSet field_type_set;
      model->GetSupportedTypes(&field_type_set);
      EXPECT_FALSE(field_type_set.empty());
      EXPECT_FALSE(field_type_set.contains_any(
          {NO_SERVER_DATA, UNKNOWN_TYPE, EMPTY_TYPE}));

      EXPECT_EQ(model->GetRootNodeForTesting().GetStorageType(),
                ADDRESS_HOME_ADDRESS);
    }
}

TEST(AutofillI18nApi, GetAddressComponentModel_ReturnedModelIsTree) {
  for (const auto& [country_code, tree_def] : kAutofillModelRules) {
    // Currently, the model for kAddressModel should comprise all the nodes in
    // the rules.
    std::unique_ptr<AddressComponent> root =
        CreateAddressComponentModel(country_code);

    ServerFieldTypeSet supported_types;
    EXPECT_TRUE(IsTree(root.get(), &supported_types));

    // Test that all field types in the country rules are accessible through the
    // root (i.e. the tree is connected).
    for (const auto& [node_type, children_types] : tree_def) {
      EXPECT_TRUE(root->GetNodeForTypeForTesting(node_type));

      for (ServerFieldType child_type : children_types) {
        EXPECT_TRUE(root->GetNodeForTypeForTesting(child_type));
      }
    }
  }
}

TEST(AutofillI18nApi, GetAddressComponentModel_CountryNodeHasValue) {
  for (const auto& [country_code, tree_def] : kAutofillModelRules) {
    std::unique_ptr<AddressComponent> model =
        CreateAddressComponentModel(country_code);
    EXPECT_EQ(model->GetValueForType(ADDRESS_HOME_COUNTRY),
              base::UTF8ToUTF16(country_code));
  }
}

TEST(AutofillI18nApi, GetLegacyAddressHierarchy) {
  // "Countries that have not been migrated to the new Autofill i18n model
  // should use the legacy hierarchy (stored in a dummy country XX)."

  // Set up expected legacy hierarchy for non-migrated country.
  ASSERT_FALSE(kAutofillModelRules.contains("ES"));
  auto legacy_address_hierarchy_es = CreateAddressComponentModel("ES");

  auto legacy_address_hierarchy_xx = CreateAddressComponentModel("XX");
  legacy_address_hierarchy_xx->SetValueForType(ADDRESS_HOME_COUNTRY, u"ES",
                                               VerificationStatus::kObserved);
  EXPECT_TRUE(
      legacy_address_hierarchy_xx->SameAs(*legacy_address_hierarchy_es.get()));
}

TEST(AutofillI18nApi, GetFormattingExpressions) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();
  for (const std::string& country_code : country_data_map->country_codes()) {
    for (std::underlying_type_t<ServerFieldType> i = 0;
         i < MAX_VALID_FIELD_TYPE; ++i) {
      if (ServerFieldType field_type = ToSafeServerFieldType(i, NO_SERVER_DATA);
          field_type != NO_SERVER_DATA) {
        auto* it = kAutofillFormattingRulesMap.find({country_code, field_type});
        // The expected value is contained in `kAutofillFormattingRulesMap`. If
        // no entry is found, it is expected to fallback to the legacy string
        // (country XX).
        if (it != kAutofillFormattingRulesMap.end()) {
          EXPECT_EQ(GetFormattingExpression(field_type, country_code),
                    std::u16string(it->second));
        } else {
          auto* legacy_it =
              kAutofillFormattingRulesMap.find({"XX", field_type});
          std::u16string_view expected =
              legacy_it != kAutofillFormattingRulesMap.end() ? legacy_it->second
                                                             : u"";
          EXPECT_EQ(GetFormattingExpression(field_type, country_code),
                    expected);
        }
      }
    }
  }
}

TEST(AutofillI18nApi, ParseValueByI18nRegularExpression) {
  std::string apt_str = "sala 10";
  auto* it = kAutofillParsingRulesMap.find({"BR", ADDRESS_HOME_APT_NUM});

  ASSERT_TRUE(it != kAutofillParsingRulesMap.end());
  EXPECT_EQ(
      ParseValueByI18nRegularExpression(apt_str, ADDRESS_HOME_APT_NUM, "BR"),
      it->second->Parse(apt_str));

  std::string street_address = "street no 123 apt 10";

  // Parsing expression for address street in not available for Germany.
  ASSERT_TRUE(
      kAutofillParsingRulesMap.find({"DE", ADDRESS_HOME_STREET_ADDRESS}) ==
      kAutofillParsingRulesMap.end());
  // In that case the legacy expression is used (if available).
  EXPECT_EQ(ParseValueByI18nRegularExpression(
                street_address, ADDRESS_HOME_STREET_ADDRESS, "DE"),
            ParseValueByI18nRegularExpression(
                street_address, ADDRESS_HOME_STREET_ADDRESS, "XX"));
}

// Tests that the i18n address API returns by default a hierarchy equivalent to
// that of the legacy address model (autofill structured addresses).
TEST(AutofillI18nApi, CheckApiDefaultVsLegacyHierarchy) {
  std::unique_ptr<AddressComponent> default_hierarchy =
      CreateAddressComponentModel("");
  EXPECT_TRUE(default_hierarchy->SameAs(AddressNode()));
}

}  // namespace autofill::i18n_model_definition

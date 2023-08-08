// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
using i18n_model_definition::AutofillModelType;
using i18n_model_definition::kAutofillFormattingRulesMap;
using i18n_model_definition::kAutofillModelRules;

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
    for (AutofillModelType model_type :
         {AutofillModelType::kAddressModel, AutofillModelType::kNameModel}) {
      // Make sure that the process of building the model finishes and returns a
      // non empty hierarchy.
      std::unique_ptr<AddressComponent> model =
          CreateAddressComponentModel(model_type, country_code);

      ASSERT_TRUE(model);
      ServerFieldTypeSet field_type_set;
      model->GetSupportedTypes(&field_type_set);
      EXPECT_FALSE(field_type_set.empty());
      EXPECT_FALSE(field_type_set.contains_any(
          {NO_SERVER_DATA, UNKNOWN_TYPE, EMPTY_TYPE}));

      // Assert root nodes.
      switch (model_type) {
        case AutofillModelType::kAddressModel:
          EXPECT_EQ(model->GetRootNodeForTesting().GetStorageType(),
                    ADDRESS_HOME_ADDRESS);
          break;
        case AutofillModelType::kNameModel:
          EXPECT_EQ(model->GetRootNodeForTesting().GetStorageType(), NAME_FULL);
          break;
      }
    }
  }
}

TEST(AutofillI18nApi, GetAddressComponentModel_ReturnedModelIsTree) {
  for (const auto& [country_code, tree_def] : kAutofillModelRules) {
    // Currently, the model for kAddressModel should comprise all the nodes in
    // the rules.
    std::unique_ptr<AddressComponent> root = CreateAddressComponentModel(
        AutofillModelType::kAddressModel, country_code);

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
    std::unique_ptr<AddressComponent> model = CreateAddressComponentModel(
        AutofillModelType::kAddressModel, country_code);
    EXPECT_EQ(model->GetValueForType(ADDRESS_HOME_COUNTRY),
              base::UTF8ToUTF16(country_code));
  }
}

TEST(AutofillI18nApi, GetLegacy_FullName) {
  // "Countries that have not been migrated to the new Autofill i18n model
  // should use the legacy hierarchy."
  ASSERT_FALSE(kAutofillModelRules.contains("CA"));
  EXPECT_TRUE(CreateAddressComponentModel(AutofillModelType::kNameModel, "CA")
                  ->SameAs(NameFull()));
}

TEST(AutofillI18nApi, GetLegacy_FullNameWithPrefix) {
  base::test::ScopedFeatureList structured_name_feature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  // "Countries that have not been migrated to the new Autofill i18n model
  // should use the legacy hierarchy."
  ASSERT_FALSE(kAutofillModelRules.contains("DE"));
  EXPECT_TRUE(CreateAddressComponentModel(AutofillModelType::kNameModel, "DE")
                  ->SameAs(NameFullWithPrefix()));
}

TEST(AutofillI18nApi, GetLegacy_AddressNode) {
  // "Countries that have not been migrated to the new Autofill i18n model
  // should use the legacy hierarchy."
  ASSERT_FALSE(kAutofillModelRules.contains("ES"));
  EXPECT_TRUE(
      CreateAddressComponentModel(AutofillModelType::kAddressModel, "ES")
          ->SameAs(AddressNode()));
}

TEST(AutofillI18nApi, GetFormattingExpressions) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();
  for (const std::string& country_code : country_data_map->country_codes()) {
    for (int i = 0; i < MAX_VALID_FIELD_TYPE; ++i) {
      if (ServerFieldType raw_value = static_cast<ServerFieldType>(i);
          ToSafeServerFieldType(raw_value, NO_SERVER_DATA) != NO_SERVER_DATA) {
        auto* it = kAutofillFormattingRulesMap.find({country_code, raw_value});
        // The expected value is contained in `kAutofillFormattingRulesMap`. If
        // no entry is found, an empty string is expected.
        std::u16string_view expected =
            it != kAutofillFormattingRulesMap.end() ? it->second : u"";

        EXPECT_EQ(i18n_model_definition::GetFormattingExpression(raw_value,
                                                                 country_code),
                  expected);
      }
    }
  }
}

}  // namespace autofill

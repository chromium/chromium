// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

template <typename... Ts>
  requires(sizeof...(Ts) == 0 || (std::same_as<Ts, FieldType> && ...))
auto HasTypes(Ts... types) {
  return ResultOf(
      "AutofillType::GetTypes()",
      [](const AutofillType& t) { return t.GetTypes(); },
      UnorderedElementsAre(types...));
}

template <typename... Ts>
  requires(sizeof...(Ts) == 0 || (std::same_as<Ts, FieldTypeGroup> && ...))
auto HasGroups(Ts... groups) {
  return ResultOf(
      "AutofillType::GetGroups()",
      [](const AutofillType& t) { return t.GetGroups(); },
      UnorderedElementsAre(groups...));
}

template <typename... Ts>
  requires(sizeof...(Ts) == 0 || (std::same_as<Ts, FormType> && ...))
auto HasFormTypes(Ts... form_types) {
  return ResultOf(
      "AutofillType::GetFormTypes()",
      [](const AutofillType& t) { return t.GetFormTypes(); },
      UnorderedElementsAre(form_types...));
}

// TODO(crbug.com/40276395): Consolidate the prediction matchers used in
// different files and move them to a central location.
Matcher<FieldPrediction> EqualsPrediction(FieldType prediction) {
  return AllOf(Property("type", &FieldPrediction::type, prediction),
               Property("source", &FieldPrediction::source,
                        FieldPrediction::SOURCE_AUTOFILL_DEFAULT));
}

class AutofillTypeServerPredictionTest : public ::testing::Test {
 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
};

TEST_F(AutofillTypeServerPredictionTest, PredictionFromAutofillField) {
  AutofillField field = AutofillField(test::CreateTestFormField(
      "label", "name", "value", /*type=*/FormControlType::kInputText));
  field.set_server_predictions(
      {test::CreateFieldPrediction(FieldType::EMAIL_ADDRESS),
       test::CreateFieldPrediction(FieldType::USERNAME)});

  AutofillType::ServerPrediction prediction(field);
  EXPECT_THAT(prediction.server_predictions,
              ElementsAre(EqualsPrediction(FieldType::EMAIL_ADDRESS),
                          EqualsPrediction(FieldType::USERNAME)));
}

// Tests the constraints, which govern which FieldTypes may occur with another.
TEST(AutofillTypeTest, TestConstraints) {
  auto tc = [](FieldTypeSet s) { return AutofillType::TestConstraints(s); };

  // Singleton sets always meet the AutofillType constraints.
  EXPECT_TRUE(tc({NO_SERVER_DATA}));
  EXPECT_TRUE(tc({UNKNOWN_TYPE}));
  EXPECT_TRUE(tc({NAME_FIRST}));
  EXPECT_TRUE(tc({USERNAME}));
  EXPECT_TRUE(tc({PASSWORD}));
  EXPECT_TRUE(tc({PHONE_HOME_WHOLE_NUMBER}));
  for (FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message() << FieldTypeToStringView(field_type));
    EXPECT_TRUE(tc({field_type}));
  }

  // Explicitly allowed pairs of types.
  EXPECT_TRUE(tc({NO_SERVER_DATA, UNKNOWN_TYPE}));
  EXPECT_TRUE(tc({UNKNOWN_TYPE, EMPTY_TYPE}));
  EXPECT_TRUE(tc({NAME_FULL, CREDIT_CARD_NAME_FULL}));
  EXPECT_TRUE(tc({DRIVERS_LICENSE_REGION, PASSPORT_NUMBER}));
  EXPECT_TRUE(tc({DRIVERS_LICENSE_REGION, ADDRESS_HOME_COUNTRY}));
  EXPECT_TRUE(tc({EMAIL_ADDRESS, USERNAME}));
  EXPECT_TRUE(tc({LOYALTY_MEMBERSHIP_ID, ADDRESS_HOME_STATE}));

  // Some examples of combinations that must not occur together.
  EXPECT_FALSE(tc({NAME_FULL, ADDRESS_HOME_ZIP}));
  EXPECT_FALSE(tc({NAME_FIRST, NAME_LAST}));
  EXPECT_FALSE(tc({NAME_FIRST, NAME_FULL}));
  EXPECT_FALSE(tc({CREDIT_CARD_NUMBER, CREDIT_CARD_NAME_FULL}));
  EXPECT_FALSE(tc({NAME_FULL, PASSPORT_NUMBER}));
  EXPECT_FALSE(tc({EMAIL_ADDRESS, LOYALTY_MEMBERSHIP_ID}));
  EXPECT_FALSE(tc({USERNAME, PASSWORD}));
  EXPECT_FALSE(tc({PHONE_HOME_WHOLE_NUMBER, PASSWORD}));
  EXPECT_FALSE(tc(kAllFieldTypes));
}

// Tests that GetTypes() returns the encapsulated types modulo normalization.
TEST(AutofillTypeTest, GetTypes) {
  // Special case 1: NO_SERVER_DATA is ignored.
  // In practice, we don't construct FieldTypes that contain NO_SERVER_DATA and
  // other FieldTypes.
  EXPECT_THAT(AutofillType(NO_SERVER_DATA), HasTypes());
  EXPECT_THAT(AutofillType({NAME_FIRST, NO_SERVER_DATA}), HasTypes(NAME_FIRST));

  // Special case 2: UNKNOWN_TYPE overrides all other predictions.
  // There are no strong reasons for this behavior. UNKNOWN_TYPE predictions are
  // most importantly used by server overrides to indicate that a field should
  // not be filled.
  EXPECT_THAT(AutofillType(UNKNOWN_TYPE), HasTypes(UNKNOWN_TYPE));
  EXPECT_THAT(AutofillType({NO_SERVER_DATA, UNKNOWN_TYPE}),
              HasTypes(UNKNOWN_TYPE));
  EXPECT_THAT(AutofillType({NAME_FIRST, UNKNOWN_TYPE}), HasTypes(UNKNOWN_TYPE));

  // Ordinary FieldTypes.
  EXPECT_THAT(AutofillType(NAME_FIRST), HasTypes(NAME_FIRST));
  EXPECT_THAT(AutofillType({NAME_FIRST, CREDIT_CARD_NAME_LAST}),
              HasTypes(NAME_FIRST, CREDIT_CARD_NAME_LAST));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_LINE1, LOYALTY_MEMBERSHIP_ID}),
              HasTypes(ADDRESS_HOME_LINE1, LOYALTY_MEMBERSHIP_ID));
  EXPECT_THAT(AutofillType({CREDIT_CARD_NUMBER, PASSPORT_NUMBER}),
              HasTypes(CREDIT_CARD_NUMBER, PASSPORT_NUMBER));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_ZIP, DRIVERS_LICENSE_REGION}),
              HasTypes(ADDRESS_HOME_ZIP, DRIVERS_LICENSE_REGION));
  EXPECT_THAT(AutofillType({DRIVERS_LICENSE_REGION, PASSPORT_NAME_TAG}),
              HasTypes(DRIVERS_LICENSE_REGION, PASSPORT_NAME_TAG));

  // HTML types:
  EXPECT_THAT(AutofillType(HtmlFieldType::kGivenName), HasTypes(NAME_FIRST));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryCode),
              HasTypes(ADDRESS_HOME_COUNTRY));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryName),
              HasTypes(ADDRESS_HOME_COUNTRY));
}

// Tests that GetGroups() maps to the right FieldTypeGroups and filters
// FieldTypeGroup::kNoGroup.
//
// Autofill's FieldType --> FieldTypeGroup mapping GroupTypeOfFieldType() is
// somewhat broken, which leads to some surprising results. See the comment at
// AutofillType::GetGroups().
TEST(AutofillTypeTest, GetGroups) {
  using enum FieldTypeGroup;
  EXPECT_THAT(AutofillType(NO_SERVER_DATA), HasGroups());
  EXPECT_THAT(AutofillType(UNKNOWN_TYPE), HasGroups());
  EXPECT_THAT(AutofillType(NAME_FIRST), HasGroups(kName));
  EXPECT_THAT(AutofillType({NAME_FIRST, NO_SERVER_DATA}), HasGroups(kName));
  EXPECT_THAT(AutofillType({NAME_FIRST, UNKNOWN_TYPE}), HasGroups());
  EXPECT_THAT(AutofillType({NAME_FIRST, CREDIT_CARD_NAME_LAST}),
              HasGroups(kName, kCreditCard));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_LINE1, LOYALTY_MEMBERSHIP_ID}),
              HasGroups(kAddress, kLoyaltyCard));
  EXPECT_THAT(AutofillType({CREDIT_CARD_NUMBER, PASSPORT_NUMBER}),
              HasGroups(kCreditCard, kAutofillAi));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_ZIP, DRIVERS_LICENSE_REGION}),
              HasGroups(kAddress, kAutofillAi));
  EXPECT_THAT(AutofillType({DRIVERS_LICENSE_REGION, PASSPORT_NAME_TAG}),
              HasGroups(kAutofillAi));
  EXPECT_THAT(AutofillType(HtmlFieldType::kGivenName), HasGroups(kName));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryCode), HasGroups(kAddress));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryName), HasGroups(kAddress));
}

// Tests that GetFormTypes() maps to the right FormTypes and filters
// FormType::kUnknownFormType.
//
// Autofill's FieldTypeGroup --> FormType mapping FieldTypeGroupToFormType() is
// somewhat broken, which leads to some surprising results. See the comment at
// AutofillType::GetFormTypes().
TEST(AutofillTypeTest, GetFormTypes) {
  using enum FormType;
  EXPECT_THAT(AutofillType(NO_SERVER_DATA), HasFormTypes());
  EXPECT_THAT(AutofillType(UNKNOWN_TYPE), HasFormTypes());
  EXPECT_THAT(AutofillType(NAME_FIRST), HasFormTypes(kAddressForm));
  EXPECT_THAT(AutofillType({NAME_FIRST, NO_SERVER_DATA}),
              HasFormTypes(kAddressForm));
  EXPECT_THAT(AutofillType({NAME_FIRST, UNKNOWN_TYPE}), HasFormTypes());
  EXPECT_THAT(AutofillType({NAME_FIRST, CREDIT_CARD_NAME_LAST}),
              HasFormTypes(kAddressForm, kCreditCardForm));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_LINE1, LOYALTY_MEMBERSHIP_ID}),
              HasFormTypes(kAddressForm, kLoyaltyCardForm));
  EXPECT_THAT(AutofillType({CREDIT_CARD_NUMBER, PASSPORT_NUMBER}),
              HasFormTypes(kCreditCardForm));
  EXPECT_THAT(AutofillType({ADDRESS_HOME_ZIP, DRIVERS_LICENSE_REGION}),
              HasFormTypes(kAddressForm));
  EXPECT_THAT(AutofillType({DRIVERS_LICENSE_REGION, PASSPORT_NAME_TAG}),
              HasFormTypes());
  EXPECT_THAT(AutofillType(HtmlFieldType::kGivenName),
              HasFormTypes(kAddressForm));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryCode),
              HasFormTypes(kAddressForm));
  EXPECT_THAT(AutofillType(HtmlFieldType::kCountryName),
              HasFormTypes(kAddressForm));
}

// This test confirms that the documentation of AutofillType::GetGroups() and
// AutofillType::GetFormTypes() is correct. If the test fails, update the
// documentation.
TEST(AutofillTypeTest, SurprisingMappings_UpdateDocumentationIfThisTestFails) {
  // For `t = AutofillType(NAME_FIRST)`, it is true that
  //   `has_autofill_ai_type && !has_autofill_ai_group`
  // where
  //   `bool has_autofill_ai_type = !t.GetAutofillAiTypes().empty()`
  //   `bool has_autofill_ai_group = t.GetGroups().contains(kAutofillAi)`
  {
    AutofillType t = AutofillType(NAME_FIRST);
    EXPECT_THAT(t.GetAutofillAiTypes(), Not(IsEmpty()));
    EXPECT_THAT(t.GetGroups(), Not(Contains(FieldTypeGroup::kAutofillAi)));
  }

  // For `t = AutofillType(EMAIL_ADDRESS)`, it is true that
  //   `has_loyalty_type && !has_loyalty_group`
  // where
  //   `bool has_loyalty_type = !t.GetLoyaltyCardType().empty()`
  //   `bool has_loyalty_group = t.GetGroups().contains(kLoyaltyCard)`
  {
    AutofillType t = AutofillType(EMAIL_ADDRESS);
    EXPECT_EQ(t.GetLoyaltyCardType(), EMAIL_ADDRESS);
    EXPECT_THAT(t.GetGroups(), Not(Contains(FieldTypeGroup::kLoyaltyCard)));
  }

  // For `t = AutofillType(EMAIL_ADDRESS)`, the following is both true:
  //   `t.GetLoyaltyCardType() == EMAIL_ADDRESS`
  //   `!t.GetFormTypes().contains(kLoyaltyCardForm)`
  {
    AutofillType t = AutofillType(EMAIL_ADDRESS);
    EXPECT_EQ(t.GetLoyaltyCardType(), EMAIL_ADDRESS);
    EXPECT_THAT(t.GetFormTypes(), Not(Contains(FormType::kLoyaltyCardForm)));
  }

  // For `t = AutofillType(PASSPORT_NUMBER)`, the following is both true:
  //   `GetAutofillAiTypes() == {PASSPORT_NUMBER}`
  //   `GetFormTypes().empty()`
  {
    AutofillType t = AutofillType(PASSPORT_NUMBER);
    EXPECT_THAT(t.GetAutofillAiTypes(), ElementsAre(PASSPORT_NUMBER));
    EXPECT_THAT(t.GetFormTypes(), IsEmpty());
  }
}

TEST(AutofillTypeTest, HtmlFieldTypes) {
  // Unknown type.
  AutofillType unknown(HtmlFieldType::kUnspecified);
  EXPECT_THAT(unknown.GetTypes(), ElementsAre(UNKNOWN_TYPE));
  EXPECT_THAT(unknown.GetGroups(), IsEmpty());

  // Type with group but no subgroup.
  AutofillType first(HtmlFieldType::kGivenName);
  EXPECT_THAT(first.GetTypes(), ElementsAre(NAME_FIRST));
  EXPECT_THAT(first.GetGroups(), ElementsAre(FieldTypeGroup::kName));

  // Type with group and subgroup.
  AutofillType phone(HtmlFieldType::kTel);
  EXPECT_THAT(phone.GetTypes(), ElementsAre(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_THAT(phone.GetGroups(), ElementsAre(FieldTypeGroup::kPhone));

  // Last value, to check any offset errors.
  AutofillType last(HtmlFieldType::kCreditCardExp4DigitYear);
  EXPECT_THAT(last.GetTypes(), ElementsAre(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_THAT(last.GetGroups(), ElementsAre(FieldTypeGroup::kCreditCard));
}

// Tests that GetAddressType() returns exactly the address types.
TEST(AutofillTypeTest, GetAddressType) {
  auto get_type = [](FieldType type) {
    return AutofillType(type).GetAddressType();
  };
  EXPECT_EQ(get_type(NO_SERVER_DATA), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(UNKNOWN_TYPE), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(NAME_FULL), NAME_FULL);
  EXPECT_EQ(get_type(CREDIT_CARD_NAME_FULL), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(PASSPORT_NAME_TAG), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(ADDRESS_HOME_ZIP), ADDRESS_HOME_ZIP);
  for (FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << FieldTypeToStringView(field_type));
    EXPECT_EQ(get_type(field_type) != UNKNOWN_TYPE, IsAddressType(field_type));
  }
}

// Tests that GetAutofillAiType() and GetAutofillAiTypes() return Autofill AI
// types. In particular, this tests the behavior for dynamically assigned
// AttributeTypes, i.e., name types. See DetermineAttributeTypes() for more on
// the Autofill AI's concept of "dynamic type assignment".
TEST(AutofillTypeTest, GetAutofillAiType) {
  EntityType kPassport = EntityType(EntityTypeName::kPassport);
  EXPECT_EQ(AutofillType(PASSPORT_NUMBER).GetAutofillAiType(kPassport),
            PASSPORT_NUMBER);
  EXPECT_EQ(AutofillType(NAME_FIRST).GetAutofillAiType(kPassport), NAME_FIRST);
  EXPECT_EQ(AutofillType({NAME_FIRST, USERNAME}).GetAutofillAiType(kPassport),
            NAME_FIRST);

  // Test that `*_TAG` types are ignored.
  EXPECT_EQ(AutofillType(PASSPORT_NAME_TAG).GetAutofillAiType(kPassport),
            UNKNOWN_TYPE);
  EXPECT_EQ(AutofillType({NAME_FIRST, PASSPORT_NAME_TAG})
                .GetAutofillAiType(kPassport),
            NAME_FIRST);
  EXPECT_THAT(AutofillType(PASSPORT_NAME_TAG).GetAutofillAiTypes(), IsEmpty());
  EXPECT_THAT(
      AutofillType({NAME_FIRST, PASSPORT_NAME_TAG}).GetAutofillAiTypes(),
      ElementsAre(NAME_FIRST));

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kAutofillAiNoTagTypes);
    // Test that `NAME_*` types are not ignored if `*_TAG` types are enabled.
    EXPECT_EQ(AutofillType(PASSPORT_NAME_TAG).GetAutofillAiType(kPassport),
              PASSPORT_NAME_TAG);
    EXPECT_EQ(AutofillType({NAME_FIRST, PASSPORT_NAME_TAG})
                  .GetAutofillAiType(kPassport),
              PASSPORT_NAME_TAG);
    EXPECT_THAT(AutofillType(PASSPORT_NAME_TAG).GetAutofillAiTypes(),
                ElementsAre(PASSPORT_NAME_TAG));
    EXPECT_THAT(
        AutofillType({NAME_FIRST, PASSPORT_NAME_TAG}).GetAutofillAiTypes(),
        ElementsAre(PASSPORT_NAME_TAG));
  }

  {
    // Test that GetAutofillAiTypes() is the union of GetAutofillAiType().
    FieldTypeSet hit1;
    FieldTypeSet hit2;
    for (EntityType entity : DenseSet<EntityType>::all()) {
      for (FieldType field_type : kAllFieldTypes) {
        AutofillType type = AutofillType(field_type);
        if (type.GetAutofillAiType(entity) != UNKNOWN_TYPE) {
          hit1.insert(field_type);
        }
        hit2.insert_all(type.GetAutofillAiTypes());
      }
    }
    EXPECT_EQ(hit1, hit2);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kAutofillAiNoTagTypes);
    // Test that GetAutofillAiTypes() is the union of GetAutofillAiType().
    FieldTypeSet hit1;
    FieldTypeSet hit2;
    for (EntityType entity : DenseSet<EntityType>::all()) {
      for (FieldType field_type : kAllFieldTypes) {
        AutofillType type = AutofillType(field_type);
        if (type.GetAutofillAiType(entity) != UNKNOWN_TYPE) {
          hit1.insert(field_type);
        }
        hit2.insert_all(type.GetAutofillAiTypes());
      }
    }
    EXPECT_EQ(hit1, hit2);
  }
}

// Tests that GetCreditCardType() returns exactly the address types.
TEST(AutofillTypeTest, GetCreditCardType) {
  auto get_type = [](FieldType type) {
    return AutofillType(type).GetCreditCardType();
  };
  EXPECT_EQ(get_type(NO_SERVER_DATA), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(UNKNOWN_TYPE), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(ADDRESS_HOME_ZIP), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(CREDIT_CARD_TYPE), CREDIT_CARD_TYPE);
  EXPECT_EQ(get_type(CREDIT_CARD_NUMBER), CREDIT_CARD_NUMBER);
  EXPECT_EQ(get_type(CREDIT_CARD_EXP_MONTH), CREDIT_CARD_EXP_MONTH);
  EXPECT_EQ(get_type(CREDIT_CARD_EXP_4_DIGIT_YEAR),
            CREDIT_CARD_EXP_4_DIGIT_YEAR);
  EXPECT_EQ(get_type(CREDIT_CARD_VERIFICATION_CODE),
            CREDIT_CARD_VERIFICATION_CODE);
  EXPECT_EQ(get_type(CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
            CREDIT_CARD_STANDALONE_VERIFICATION_CODE);
  EXPECT_EQ(get_type(PASSWORD), UNKNOWN_TYPE);
  EXPECT_EQ(get_type(USERNAME), UNKNOWN_TYPE);
  EXPECT_EQ(
      AutofillType({ADDRESS_HOME_ZIP, CREDIT_CARD_TYPE}).GetCreditCardType(),
      CREDIT_CARD_TYPE);
}

// Tests that GetIdentityCredentialType() returns exactly the address types.
TEST(AutofillTypeTest, GetIdentityCredentialType) {
  constexpr FieldTypeSet kPositive = {NAME_FIRST, NAME_FULL, EMAIL_ADDRESS,
                                      PHONE_HOME_WHOLE_NUMBER, PASSWORD};
  for (const FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << FieldTypeToStringView(field_type));
    const FieldType actual =
        AutofillType(field_type).GetIdentityCredentialType();
    if (kPositive.contains(field_type)) {
      EXPECT_EQ(actual, field_type);
    } else {
      EXPECT_EQ(actual, UNKNOWN_TYPE);
    }
  }
  EXPECT_EQ(AutofillType({NAME_FULL, CREDIT_CARD_NAME_FIRST})
                .GetIdentityCredentialType(),
            NAME_FULL);
  EXPECT_EQ(AutofillType({NAME_FIRST, CREDIT_CARD_NAME_FIRST})
                .GetIdentityCredentialType(),
            NAME_FIRST);
  EXPECT_EQ(AutofillType({NAME_LAST, CREDIT_CARD_NAME_FIRST})
                .GetIdentityCredentialType(),
            UNKNOWN_TYPE);
}

// Tests that GetLoyaltyCardType() returns exactly the address types.
TEST(AutofillTypeTest, GetLoyaltyCardType) {
  constexpr FieldTypeSet kPositive = {
      EMAIL_ADDRESS,
      LOYALTY_MEMBERSHIP_ID,
      LOYALTY_MEMBERSHIP_PROGRAM,
      LOYALTY_MEMBERSHIP_PROVIDER,
      EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
  };
  for (const FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << FieldTypeToStringView(field_type));
    const FieldType actual = AutofillType(field_type).GetLoyaltyCardType();
    if (kPositive.contains(field_type)) {
      EXPECT_EQ(actual, field_type);
    } else {
      EXPECT_EQ(actual, UNKNOWN_TYPE);
    }
  }
}

// Tests that GetPasswordManagerType() returns exactly the address types.
TEST(AutofillTypeTest, GetPasswordManagerType) {
  constexpr FieldTypeSet kPositive = {PASSWORD,
                                      ACCOUNT_CREATION_PASSWORD,
                                      NOT_ACCOUNT_CREATION_PASSWORD,
                                      NEW_PASSWORD,
                                      PROBABLY_NEW_PASSWORD,
                                      NOT_NEW_PASSWORD,
                                      CONFIRMATION_PASSWORD,
                                      NOT_PASSWORD,
                                      SINGLE_USERNAME,
                                      NOT_USERNAME,
                                      SINGLE_USERNAME_FORGOT_PASSWORD,
                                      SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES,
                                      USERNAME,
                                      ONE_TIME_CODE};
  for (const FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << FieldTypeToStringView(field_type));
    const FieldType actual = AutofillType(field_type).GetPasswordManagerType();
    if (kPositive.contains(field_type)) {
      EXPECT_EQ(actual, field_type);
    } else {
      EXPECT_EQ(actual, UNKNOWN_TYPE);
    }
  }
  EXPECT_EQ(AutofillType({USERNAME, EMAIL_ADDRESS}).GetPasswordManagerType(),
            USERNAME);
  EXPECT_EQ(
      AutofillType({ONE_TIME_CODE, ADDRESS_HOME_ZIP}).GetPasswordManagerType(),
      ONE_TIME_CODE);
}

// Tests that almost all FieldTypes are covered by some getter
// AutofillType::Get{Address,AutofillAi,CreditCard,...}Type().
//
// WHAT TO DO WHEN THIS TEST FAILS:
//
// If the test fails, that's probably due to a newly added FieldType.
// - Does the new FieldType logically belong to one of the getters in
//   AutofillType that return a single FieldType (e.g.,
//   AutofillType::GetAddressType())?
//
//   If yes:
//   Check the definition of the getter. Most likely, you need to update the
//   FieldTypeSet defined in autofill_type.cc (e.g., `kAddressFieldTypes`).
//
//   If no:
//   - Do you want a new getter in AutofillType? That probably means you're
//     creating a new Autofill integrator (something like Plus Addresses).
//
//     If yes:
//     Add the getter and update AutofillType::TestConstraints().
//     Also update this unit test.
//
//     If no:
//     Add the type to the `kNotCovered` set below.
TEST(AutofillTypeTest, AlmostAllFieldTypesAreCovered) {
  // These are the FieldTypes that are not covered by any getter.
  FieldTypeSet kNotCovered{NO_SERVER_DATA,      UNKNOWN_TYPE,
                           EMPTY_TYPE,          MERCHANT_EMAIL_SIGNUP,
                           MERCHANT_PROMO_CODE, AMBIGUOUS_TYPE,
                           SEARCH_TERM,         PRICE,
                           IBAN_VALUE,          NUMERIC_QUANTITY,
                           MAX_VALID_FIELD_TYPE};
  if (base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    kNotCovered.insert_all(
        {DRIVERS_LICENSE_NAME_TAG, PASSPORT_NAME_TAG, VEHICLE_OWNER_TAG});
  }

  for (FieldType field_type : kAllFieldTypes) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << FieldTypeToStringView(field_type));
    AutofillType t = AutofillType(field_type);
    EXPECT_EQ(t.GetAddressType() == UNKNOWN_TYPE &&
                  std::ranges::all_of(DenseSet<EntityType>::all(),
                                      [&t](EntityType entity) {
                                        return t.GetAutofillAiType(entity) ==
                                               UNKNOWN_TYPE;
                                      }) &&
                  t.GetCreditCardType() == UNKNOWN_TYPE &&
                  t.GetIdentityCredentialType() == UNKNOWN_TYPE &&
                  t.GetLoyaltyCardType() == UNKNOWN_TYPE &&
                  t.GetPasswordManagerType() == UNKNOWN_TYPE,
              kNotCovered.contains(field_type));
  }
}

class AutofillTypeTestForHtmlFieldTypes
    : public ::testing::TestWithParam<std::underlying_type_t<HtmlFieldType>> {
 public:
  HtmlFieldType html_field_type() const {
    return ToSafeHtmlFieldType(GetParam(), HtmlFieldType::kUnrecognized);
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillTypeTest,
    AutofillTypeTestForHtmlFieldTypes,
    testing::Range(base::to_underlying(HtmlFieldType::kMinValue),
                   base::to_underlying(HtmlFieldType::kMaxValue)));

TEST_P(AutofillTypeTestForHtmlFieldTypes, GroupsOfHtmlFieldTypes) {
  if (HtmlFieldTypeToBestCorrespondingFieldType(html_field_type()) ==
      UNKNOWN_TYPE) {
    return;
  }
  AutofillType t(html_field_type());
  SCOPED_TRACE(testing::Message()
               << "html_field_type=" << FieldTypeToStringView(html_field_type())
               << " "
               << "field_type="
               << base::JoinString(
                      base::ToVector(t.GetTypes(),
                                     [](FieldType field_type) {
                                       return FieldTypeToStringView(field_type);
                                     }),
                      ", "));
  EXPECT_EQ(t.GetGroups(),
            FieldTypeGroupSet(t.GetTypes(), &GroupTypeOfFieldType));
}

}  // namespace
}  // namespace autofill

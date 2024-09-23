// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

#include <map>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

// Creates an atomic name component for testing purposes.
class TestAtomicFirstNameAddressComponent : public AddressComponent {
 public:
  TestAtomicFirstNameAddressComponent()
      : AddressComponent(NAME_FIRST, {}, MergeMode::kDefault) {}
};

class TestAtomicMiddleNameAddressComponent : public AddressComponent {
 public:
  TestAtomicMiddleNameAddressComponent()
      : AddressComponent(NAME_MIDDLE, {}, MergeMode::kDefault) {}

  const FieldTypeSet GetAdditionalSupportedFieldTypes() const override {
    constexpr FieldTypeSet supported_types{NAME_MIDDLE_INITIAL};
    return supported_types;
  }

  void SetValueForOtherSupportedType(
      FieldType field_type,
      const std::u16string& value,
      const VerificationStatus& status) override {
    CHECK(IsSupportedType(field_type));
    SetValue(value, status);
  }

  std::u16string GetValueForOtherSupportedType(
      FieldType field_type) const override {
    CHECK(IsSupportedType(field_type));
    return GetValue().substr(0, 1);
  }
};

class TestAtomicLastNameAddressComponent : public AddressComponent {
 public:
  TestAtomicLastNameAddressComponent()
      : AddressComponent(NAME_LAST, {}, MergeMode::kDefault) {}
};

// Creates a compound name for testing purposes.
class TestCompoundNameAddressComponent : public AddressComponent {
 public:
  TestCompoundNameAddressComponent()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);
  }

  AddressComponent* GetFirstNameSubComponentForTesting() {
    return test_api(*this).GetNodeForType(NAME_FIRST);
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Creates a compound name for testing purposes that uses an expression to
// parse.
class TestCompoundNameRegExParsedAddressComponent : public AddressComponent {
 public:
  TestCompoundNameRegExParsedAddressComponent()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);

    expression1_ =
        BuildRegExFromPattern("(?P<NAME_FULL>(?P<NAME_MIDDLE>\\d*))");
    expression2_ = BuildRegExFromPattern("(?P<NAME_FULL>(?P<NAME_LAST>.*))");
  }

  std::vector<const RE2*> GetParseRegularExpressionsByRelevance()
      const override {
    // The first two expressions will fail and the last one will be
    // successful.
    return {nullptr, expression1_.get(), expression2_.get()};
  }

 private:
  std::unique_ptr<const RE2> expression1_;
  std::unique_ptr<const RE2> expression2_;

  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Creates a compound name with a custom format for testing purposes.
class TestCompoundNameCustomFormatAddressComponent : public AddressComponent {
 public:
  TestCompoundNameCustomFormatAddressComponent()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);
  }

  // Introduces a custom format with a leading last name.
  std::u16string GetFormatString() const override {
    return u"${NAME_LAST}, ${NAME_FIRST}";
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Creates a compound name with a custom format for testing purposes.
class TestCompoundNameCustomAffixedFormatAddressComponent
    : public AddressComponent {
 public:
  TestCompoundNameCustomAffixedFormatAddressComponent()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);
  }

  // Introduces a custom format with a leading last name.
  std::u16string GetFormatString() const override {
    return u"${NAME_LAST;Dr. ; MD}, ${NAME_FIRST}";
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Creates a fictional compound component with sub- and sub subcomponents.
class TestCompoundNameWithTitleAddressComponent : public AddressComponent {
 public:
  TestCompoundNameWithTitleAddressComponent()
      : AddressComponent(CREDIT_CARD_NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&name);
  }

 private:
  TestCompoundNameAddressComponent name;
};

// Creates a tree that is not proper in the sense that it contains the same type
// multiple times.
class TestNonProperFirstNameAddressComponent : public AddressComponent {
 public:
  TestNonProperFirstNameAddressComponent()
      : AddressComponent(NAME_FIRST, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
};

// Tests the merging of two atomic component with |type|, and values
// |older_values| and |newer_values| respectively, and |merge_modes|.
// If |is_mergeable| it is expected that the two components are mergeable.
// If |newer_was_more_recently_used| the newer component was also more recently
// used which is true by default.
void TestAtomMerging(FieldType type,
                     AddressComponentTestValues older_values,
                     AddressComponentTestValues newer_values,
                     AddressComponentTestValues merge_expectation,
                     bool is_mergeable,
                     int merge_modes,
                     bool newer_was_more_recently_used = true) {
  AddressComponent older(type, {}, merge_modes);
  AddressComponent newer(type, {}, merge_modes);

  SetTestValues(&older, older_values);
  SetTestValues(&newer, newer_values);

  TestMerging(&older, &newer, merge_expectation, is_mergeable, merge_modes,
              newer_was_more_recently_used);
}

void TestCompoundNameMerging(AddressComponentTestValues older_values,
                             AddressComponentTestValues newer_values,
                             AddressComponentTestValues merge_expectation,
                             bool is_mergeable,
                             int merge_modes,
                             bool newer_was_more_recently_used = true) {
  TestCompoundNameAddressComponent older;
  TestCompoundNameAddressComponent newer;

  SetTestValues(&older, older_values);
  SetTestValues(&newer, newer_values);

  TestMerging(&older, &newer, merge_expectation, is_mergeable, merge_modes,
              newer_was_more_recently_used);
}

class AutofillStructuredAddressAddressComponent : public testing::Test {
 public:
  AutofillStructuredAddressAddressComponent() = default;

 private:
  base::test::ScopedFeatureList features_{features::kAutofillUseINAddressModel};
};

// Tests that the destructor does not crash
TEST_F(AutofillStructuredAddressAddressComponent, ConstructAndDestruct) {
  AddressComponent* component =
      new AddressComponent(NAME_FULL, {}, MergeMode::kDefault);
  delete component;
  EXPECT_TRUE(true);
}

// Tests that a non-proper AddressComponent tree fails a DCHECK for
// |GetSupportedTypes()|.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestNonProperTreeDcheckFailure) {
  TestNonProperFirstNameAddressComponent non_proper_compound;
  FieldTypeSet supported_types;
  EXPECT_DCHECK_DEATH(non_proper_compound.GetSupportedTypes(&supported_types));
}

// Tests getting the root node.
TEST_F(AutofillStructuredAddressAddressComponent, TestGetRootNode) {
  TestCompoundNameAddressComponent compound_component;

  // The root node should return the root node.
  EXPECT_EQ(&compound_component, &(test_api(compound_component).GetRootNode()));

  // Get a pointer to a subcomponent, verify that it is not the root node and
  // check that it successfully retrieves the root node.
  AddressComponent* first_name_subcomponent_ptr =
      compound_component.GetFirstNameSubComponentForTesting();
  EXPECT_NE(&compound_component, first_name_subcomponent_ptr);
  EXPECT_EQ(&compound_component,
            &(test_api(*first_name_subcomponent_ptr).GetRootNode()));
}

// Tests that additional field types are correctly retrieved.
TEST_F(AutofillStructuredAddressAddressComponent, TestGetSupportedFieldType) {
  TestAtomicFirstNameAddressComponent first_name_component;
  TestAtomicMiddleNameAddressComponent middle_name_component;

  // The first name does not have an additional supported field type.
  EXPECT_EQ(first_name_component.GetAdditionalSupportedFieldTypes(),
            FieldTypeSet({}));

  // The middle name supports an initial.
  EXPECT_EQ(middle_name_component.GetAdditionalSupportedFieldTypes(),
            FieldTypeSet({NAME_MIDDLE_INITIAL}));
}

// Tests setting an additional field type.
TEST_F(AutofillStructuredAddressAddressComponent, TestSetFieldTypeValue) {
  TestCompoundNameAddressComponent compound_name;
  EXPECT_TRUE(compound_name.SetValueForType(NAME_MIDDLE_INITIAL, u"M",
                                            VerificationStatus::kObserved));

  EXPECT_EQ(compound_name.GetValueForType(NAME_MIDDLE), u"M");
}

// Tests retrieving an additional field type.
TEST_F(AutofillStructuredAddressAddressComponent, TestGetFieldTypeValue) {
  TestCompoundNameAddressComponent compound_name;
  EXPECT_TRUE(compound_name.SetValueForType(NAME_MIDDLE, u"Middle",
                                            VerificationStatus::kObserved));

  EXPECT_EQ(compound_name.GetValueForType(NAME_MIDDLE_INITIAL), u"M");
  EXPECT_EQ(compound_name.GetVerificationStatusForType(NAME_MIDDLE_INITIAL),
            VerificationStatus::kObserved);
}

// Tests retrieving a value for comparison for a field type.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestGetValueForComparisonForType) {
  TestCompoundNameAddressComponent compound_name;
  EXPECT_TRUE(compound_name.SetValueForType(NAME_FIRST, u"First1-First2",
                                            VerificationStatus::kObserved));
  EXPECT_TRUE(compound_name.SetValueForTypeAndResetSubstructure(
      NAME_MIDDLE, u"Middle", VerificationStatus::kObserved));
  EXPECT_TRUE(compound_name.SetValueForType(NAME_LAST, u"LAST",
                                            VerificationStatus::kObserved));
  EXPECT_TRUE(compound_name.CompleteFullTree());
  EXPECT_EQ(
      compound_name.GetValueForComparisonForType(NAME_FULL, compound_name),
      u"first1 first2 middle last");
  EXPECT_EQ(
      compound_name.GetValueForComparisonForType(NAME_FIRST, compound_name),
      u"first1 first2");
  EXPECT_EQ(compound_name.GetValueForComparisonForType(NAME_MIDDLE_INITIAL,
                                                       compound_name),
            u"m");
  EXPECT_TRUE(
      compound_name
          .GetValueForComparisonForType(ADDRESS_HOME_STREET_NAME, compound_name)
          .empty());
}

// Tests adding all supported types to the set.
TEST_F(AutofillStructuredAddressAddressComponent, TestGetSupportedTypes) {
  FieldTypeSet field_type_set;

  TestAtomicFirstNameAddressComponent first_name_component;
  TestAtomicMiddleNameAddressComponent middle_name_component;
  TestCompoundNameAddressComponent compound_name;

  // The first name only supports NAME_FIRST.
  first_name_component.GetSupportedTypes(&field_type_set);
  EXPECT_EQ(field_type_set, FieldTypeSet({NAME_FIRST}));

  // The middle name supports an initial.
  field_type_set.clear();
  middle_name_component.GetSupportedTypes(&field_type_set);
  EXPECT_EQ(field_type_set, FieldTypeSet({NAME_MIDDLE, NAME_MIDDLE_INITIAL}));

  // Verify that all types are added correctly in a compound structure.
  field_type_set.clear();
  compound_name.GetSupportedTypes(&field_type_set);
  EXPECT_EQ(field_type_set, FieldTypeSet({NAME_MIDDLE, NAME_MIDDLE_INITIAL,
                                          NAME_FIRST, NAME_LAST, NAME_FULL}));
}

// Tests adding all storable types to the set.
TEST_F(AutofillStructuredAddressAddressComponent, TestGetStorableTypes) {
  FieldTypeSet field_type_set;

  TestAtomicFirstNameAddressComponent first_name_component;
  TestAtomicMiddleNameAddressComponent middle_name_component;
  TestCompoundNameAddressComponent compound_name;

  // The first name only supports NAME_FIRST.
  first_name_component.GetStorableTypes(&field_type_set);
  EXPECT_EQ(field_type_set, FieldTypeSet({NAME_FIRST}));

  // The middle name supports an initial.
  field_type_set.clear();
  middle_name_component.GetStorableTypes(&field_type_set);
  EXPECT_EQ(field_type_set, FieldTypeSet({NAME_MIDDLE}));

  // Verify that all types are added correctly in a compound structure.
  field_type_set.clear();
  compound_name.GetStorableTypes(&field_type_set);
  EXPECT_EQ(field_type_set,
            FieldTypeSet({NAME_MIDDLE, NAME_FIRST, NAME_LAST, NAME_FULL}));
}

// Tests the comparison of the atoms of the same type.
TEST_F(AutofillStructuredAddressAddressComponent, TestComparison_Atom) {
  AddressComponent left(NAME_FIRST, {}, MergeMode::kReplaceEmpty);
  AddressComponent right(NAME_FIRST, {}, MergeMode::kReplaceEmpty);

  left.SetValue(u"some value", VerificationStatus::kParsed);
  right.SetValue(u"some other value", VerificationStatus::kFormatted);
  EXPECT_NE(left.GetValue(), right.GetValue());
  EXPECT_NE(left.GetVerificationStatus(), right.GetVerificationStatus());

  EXPECT_FALSE(left.SameAs(right));

  right.SetValue(u"some value", VerificationStatus::kParsed);

  EXPECT_TRUE(left.SameAs(right));
}

// Tests comparison of two different types.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestComparisonOperator_DifferentTypes) {
  AddressComponent type_a1(NAME_FIRST, {}, MergeMode::kReplaceEmpty);
  AddressComponent type_a2(NAME_FIRST, {}, MergeMode::kReplaceEmpty);
  AddressComponent type_b(NAME_LAST, {}, MergeMode::kReplaceEmpty);

  EXPECT_TRUE(type_a1.SameAs(type_a2));
  EXPECT_FALSE(type_a1.SameAs(type_b));
}

// Tests the comparison with itself.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestComparisonOperator_SelfComparison) {
  AddressComponent type_a(NAME_FIRST, {}, MergeMode::kReplaceEmpty);

  EXPECT_TRUE(type_a.SameAs(type_a));
}

// Tests the comparison operator.
TEST_F(AutofillStructuredAddressAddressComponent, TestComparison_Compound) {
  TestCompoundNameAddressComponent left;
  TestCompoundNameAddressComponent right;

  // Set left to a value and verify its state.
  left.SetValueForType(NAME_FULL, u"First Middle Last",
                       VerificationStatus::kObserved);
  EXPECT_TRUE(left.CompleteFullTree());
  EXPECT_EQ(left.GetValueForType(NAME_FULL), u"First Middle Last");
  EXPECT_EQ(left.GetValueForType(NAME_FIRST), u"First");
  EXPECT_EQ(left.GetValueForType(NAME_MIDDLE), u"Middle");
  EXPECT_EQ(left.GetValueForType(NAME_LAST), u"Last");
  EXPECT_EQ(left.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kObserved);
  EXPECT_EQ(left.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(left.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kParsed);
  EXPECT_EQ(left.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kParsed);

  // Set right to another value and verify its state.
  right.SetValueForType(NAME_FULL, u"The Dark Knight",
                        VerificationStatus::kUserVerified);
  EXPECT_TRUE(right.CompleteFullTree());
  EXPECT_EQ(right.GetValueForType(NAME_FULL), u"The Dark Knight");
  EXPECT_EQ(right.GetValueForType(NAME_FIRST), u"The");
  EXPECT_EQ(right.GetValueForType(NAME_MIDDLE), u"Dark");
  EXPECT_EQ(right.GetValueForType(NAME_LAST), u"Knight");
  EXPECT_EQ(right.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(right.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(right.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kParsed);
  EXPECT_EQ(right.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kParsed);

  EXPECT_FALSE(left.SameAs(right));

  // Set left to the same values as right and verify that it is now equal.
  TestCompoundNameAddressComponent same_right;
  same_right.SetValueForType(NAME_FULL, u"The Dark Knight",
                             VerificationStatus::kUserVerified);
  EXPECT_TRUE(same_right.CompleteFullTree());

  EXPECT_TRUE(right.SameAs(same_right));

  // Change one subcomponent and verify that it is not equal anymore.
  same_right.SetValueForType(NAME_LAST, u"Joker", VerificationStatus::kParsed);
  EXPECT_FALSE(right.SameAs(same_right));
}

// Tests the assignment operator.
TEST_F(AutofillStructuredAddressAddressComponent, TestAssignmentOperator_Atom) {
  AddressComponent left(NAME_FIRST, {}, MergeMode::kReplaceEmpty);
  AddressComponent right(NAME_FIRST, {}, MergeMode::kReplaceEmpty);

  left.SetValue(u"some value", VerificationStatus::kParsed);
  right.SetValue(u"some other value", VerificationStatus::kFormatted);
  EXPECT_FALSE(left.SameAs(right));

  left.SetValue(u"some other value", VerificationStatus::kFormatted);
  EXPECT_TRUE(left.SameAs(right));
}

// Tests the assignment operator when using the base class type.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestAssignmentOperator_Compound_FromBase) {
  TestCompoundNameAddressComponent left;
  TestCompoundNameAddressComponent right;

  left.SetValueForType(NAME_FULL, u"First Middle Last",
                       VerificationStatus::kObserved);
  left.RecursivelyCompleteTree();

  right.SetValueForType(NAME_FULL, u"The Dark Knight",
                        VerificationStatus::kParsed);
  right.RecursivelyCompleteTree();

  AddressComponent* left_base = &left;
  AddressComponent* right_base = &right;
  EXPECT_FALSE(left_base->SameAs(*right_base));

  // Use the assignment operators defined in the base.
  left_base->CopyFrom(*right_base);
  // But verify that the higher level classes are assigned correctly.
  EXPECT_TRUE(left.SameAs(right));
}

// Tests the assignment operator on a compound node.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestAssignmentOperator_Compound) {
  TestCompoundNameAddressComponent left;
  TestCompoundNameAddressComponent right;

  left.SetValueForType(NAME_FULL, u"First Middle Last",
                       VerificationStatus::kObserved);
  left.RecursivelyCompleteTree();

  right.SetValueForType(NAME_FULL, u"The Dark Knight",
                        VerificationStatus::kParsed);
  right.RecursivelyCompleteTree();

  EXPECT_FALSE(left.SameAs(right));

  left.CopyFrom(right);
  EXPECT_TRUE(left.SameAs(right));
}

// Tests that self-assignment does not break things.
TEST_F(AutofillStructuredAddressAddressComponent, SelfAssignment) {
  TestCompoundNameAddressComponent left;

  left.SetValueForType(NAME_FULL, u"First Middle Last",
                       VerificationStatus::kObserved);
  left.CopyFrom(*(&left));

  EXPECT_EQ(left.GetValueForType(NAME_FULL), u"First Middle Last");
}

// Tests that the correct storage types are returned.
TEST_F(AutofillStructuredAddressAddressComponent, GetStorageType) {
  EXPECT_EQ(TestAtomicFirstNameAddressComponent().GetStorageType(), NAME_FIRST);
  EXPECT_EQ(TestAtomicMiddleNameAddressComponent().GetStorageType(),
            NAME_MIDDLE);
  EXPECT_EQ(TestAtomicLastNameAddressComponent().GetStorageType(), NAME_LAST);
  EXPECT_EQ(TestCompoundNameAddressComponent().GetStorageType(), NAME_FULL);
}

// Tests that the correct storage type names are returned.
TEST_F(AutofillStructuredAddressAddressComponent, GetStorageTypeName) {
  EXPECT_EQ(TestAtomicFirstNameAddressComponent().GetStorageTypeName(),
            "NAME_FIRST");
  EXPECT_EQ(TestAtomicMiddleNameAddressComponent().GetStorageTypeName(),
            "NAME_MIDDLE");
  EXPECT_EQ(TestAtomicLastNameAddressComponent().GetStorageTypeName(),
            "NAME_LAST");
  EXPECT_EQ(TestCompoundNameAddressComponent().GetStorageTypeName(),
            "NAME_FULL");
}

// Tests that the correct atomicity is returned.
TEST_F(AutofillStructuredAddressAddressComponent, GetAtomicity) {
  EXPECT_TRUE(TestAtomicFirstNameAddressComponent().IsAtomic());
  EXPECT_TRUE(TestAtomicMiddleNameAddressComponent().IsAtomic());
  EXPECT_TRUE(TestAtomicLastNameAddressComponent().IsAtomic());
  EXPECT_FALSE(TestCompoundNameAddressComponent().IsAtomic());
}

// Tests directly setting and retrieving values.
TEST_F(AutofillStructuredAddressAddressComponent, DirectlyGetSetAndUnsetValue) {
  std::u16string test_value = u"test_value";

  // Create an atomic structured component and verify its initial unset state
  TestAtomicFirstNameAddressComponent first_name_component;
  EXPECT_EQ(first_name_component.GetValue(), std::u16string());
  EXPECT_FALSE(first_name_component.IsValueAssigned());
  EXPECT_EQ(first_name_component.GetVerificationStatus(),
            VerificationStatus::kNoStatus);

  // Set the value and the verification status and verify the set state.
  first_name_component.SetValue(test_value, VerificationStatus::kObserved);
  EXPECT_EQ(first_name_component.GetValue(), test_value);
  EXPECT_TRUE(first_name_component.IsValueAssigned());
  EXPECT_EQ(first_name_component.GetVerificationStatus(),
            VerificationStatus::kObserved);

  // Unset the value and verify the unset state again.
  first_name_component.UnsetValue();
  EXPECT_EQ(first_name_component.GetValue(), std::u16string());
  EXPECT_FALSE(first_name_component.IsValueAssigned());
  EXPECT_EQ(first_name_component.GetVerificationStatus(),
            VerificationStatus::kNoStatus);
}

// Tests recursively setting and retrieving values.
TEST_F(AutofillStructuredAddressAddressComponent,
       RecursivelySettingAndGettingValues) {
  std::u16string test_value = u"test_value";

  // Create a compound component that has a child of type NAME_FIRST.
  TestCompoundNameAddressComponent compound_component;

  // Set the value and verification status of a type not present in the tree and
  // verify the failure.
  bool success = compound_component.SetValueForType(
      ADDRESS_HOME_COUNTRY, test_value, VerificationStatus::kObserved);
  EXPECT_FALSE(success);

  // Set the value and verification status of a type that is a subcomponent of
  // the compound and verify the success.
  EXPECT_TRUE(compound_component.SetValueForType(
      NAME_FIRST, test_value, VerificationStatus::kObserved));

  // Retrieve the value and verification status, verify the success and
  // retrieved values.
  std::u16string retrieved_value =
      compound_component.GetValueForType(NAME_FIRST);
  VerificationStatus retrieved_status =
      compound_component.GetVerificationStatusForType(NAME_FIRST);
  EXPECT_EQ(retrieved_value, test_value);
  EXPECT_EQ(retrieved_status, VerificationStatus::kObserved);

  // Retrieve the value of a non-existing type and verify the failure.
  retrieved_value = compound_component.GetValueForType(ADDRESS_HOME_COUNTRY);
  retrieved_status =
      compound_component.GetVerificationStatusForType(ADDRESS_HOME_COUNTRY);
  EXPECT_EQ(retrieved_value, u"");
  EXPECT_EQ(retrieved_status, VerificationStatus::kNoStatus);
}

// Tests retrieving the subcomponents types.
TEST_F(AutofillStructuredAddressAddressComponent, GetSubcomponentTypes) {
  // Create a compound component that has the subcomponents
  // NAME_FIRST, NAME_MIDDLE, NAME_LAST.
  TestCompoundNameAddressComponent compound_component;

  // Get the subcomponent types and verify the expectation.
  auto sub_component_types =
      test_api(compound_component).GetSubcomponentTypes();
  std::vector<FieldType> expected_types{NAME_FIRST, NAME_MIDDLE, NAME_LAST};
  EXPECT_EQ(sub_component_types, expected_types);
}

// Tests getting the best format string for an atom.
TEST_F(AutofillStructuredAddressAddressComponent, GetBestFormatString_ForAtom) {
  TestAtomicFirstNameAddressComponent first_name_component;
  EXPECT_EQ(test_api(first_name_component).GetFormatString(), u"${NAME_FIRST}");
}

// Tests getting the best format string using the fallback mechanism.
TEST_F(AutofillStructuredAddressAddressComponent,
       GetBestFormatString_WithFallback) {
  // Create a compound component.
  TestCompoundNameAddressComponent compound_component;

  // Verify the retrieved default format string against the expectation.
  std::u16string expected_result = u"${NAME_FIRST} ${NAME_MIDDLE} ${NAME_LAST}";
  std::u16string actual_result = test_api(compound_component).GetFormatString();
  EXPECT_EQ(expected_result, actual_result);
}

// Tests getting the best format string using the fallback mechanism.
TEST_F(AutofillStructuredAddressAddressComponent,
       GetBestFormatString_WithCustomMethod) {
  // Create a compound component.
  TestCompoundNameCustomFormatAddressComponent compound_component;

  // Verify the retrieved custom format string against the expectation.
  std::u16string expected_result = u"${NAME_LAST}, ${NAME_FIRST}";
  std::u16string actual_result = test_api(compound_component).GetFormatString();
  EXPECT_EQ(expected_result, actual_result);
}

// Tests formatting the unstructured value from the subcomponents.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatValueFromSubcomponents) {
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";

  // Create a compound component and set the values.
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  test_api(compound_component).FormatValueFromSubcomponents();

  std::u16string expected_value = u"Winston O'Brien Smith";
  std::u16string actual_value = compound_component.GetValue();

  EXPECT_EQ(expected_value, actual_value);
}

// Creates a compound name for testing purposes.
class TestCompoundNameAddressComponentCustomFormatSeparator
    : public AddressComponent {
 public:
  TestCompoundNameAddressComponentCustomFormatSeparator()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);
  }

  // Introduces a custom format with multiple separators.
  std::u16string GetFormatString() const override {
    return u"${NAME_FIRST}, ${NAME_MIDDLE} .,${NAME_LAST}";
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Tests formatting the unstructured value from the subcomponents.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatValueFromSubcomponentsSeparators) {
  std::u16string first_name = u"First";
  std::u16string middle_name = u"Middle";
  std::u16string last_name = u"Last";

  // Create a compound component and set the values.
  TestCompoundNameAddressComponentCustomFormatSeparator compound_component;
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First, Middle .,Last");

  // Middle name is empty. The separator for middle name should be ignored.
  compound_component.SetValueForType(NAME_MIDDLE, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First .,Last");

  // Last name is also empty. Only the first name token should be shown, no
  // separators.
  compound_component.SetValueForType(NAME_LAST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First");

  // All tokens are dropped. The formatted string should be empty.
  compound_component.SetValueForType(NAME_FIRST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"");

  // Middle and last name are non-empty. Separator for middle name should be
  // ignored.
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"Middle .,Last");

  // Only last name is non-empty. All separators should be ignored.
  compound_component.SetValueForType(NAME_MIDDLE, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"Last");
}

// Creates a compound name for testing purposes with a formatting string that
// contains new line characters.
class TestCompoundNameAddressComponentCustomFormatNewLineSeparator
    : public AddressComponent {
 public:
  TestCompoundNameAddressComponentCustomFormatNewLineSeparator()
      : AddressComponent(NAME_FULL, {}, MergeMode::kDefault) {
    RegisterChildNode(&first_name);
    RegisterChildNode(&middle_name);
    RegisterChildNode(&last_name);
  }

  // Introduces a custom format with multiple separators.
  std::u16string GetFormatString() const override {
    return u"${NAME_FIRST}\n${NAME_MIDDLE} .,${NAME_LAST}";
  }

 private:
  TestAtomicFirstNameAddressComponent first_name;
  TestAtomicMiddleNameAddressComponent middle_name;
  TestAtomicLastNameAddressComponent last_name;
};

// Tests formatting the unstructured value from the subcomponents.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatValueFromSubcomponentsNewLineSeparators) {
  std::u16string first_name = u"First";
  std::u16string middle_name = u"Middle";
  std::u16string last_name = u"Last";

  // Create a compound component and set the values.
  TestCompoundNameAddressComponentCustomFormatNewLineSeparator
      compound_component;
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First\nMiddle .,Last");

  // Only middle name is empty.
  compound_component.SetValueForType(NAME_MIDDLE, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First\nLast");
  // Only last name is set.
  compound_component.SetValueForType(NAME_FIRST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"Last");

  // Only name last is empty.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First\nMiddle");

  // Only middle name is set.
  compound_component.SetValueForType(NAME_FIRST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"Middle");

  // Only first name is missing.
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"Middle .,Last");

  // Only first name is set.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, u"",
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, u"",
                                     VerificationStatus::kUserVerified);
  test_api(compound_component).FormatValueFromSubcomponents();
  EXPECT_EQ(compound_component.GetValue(), u"First");
}

// Tests that formatted values are correctly trimmed.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatAndTrimValueFromSubcomponents) {
  std::u16string first_name = u"";
  std::u16string middle_name = u"O'Brien   ";
  std::u16string last_name = u"Smith";
  // Create a compound component.
  TestCompoundNameAddressComponent compound_component;

  // Set the values of the components.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  test_api(compound_component).FormatValueFromSubcomponents();

  // Expect that the leading whitespace due to the missing first name and the
  // double white spaces after the middle name are correctly trimmed.
  std::u16string expected_value = u"O'Brien Smith";
  std::u16string actual_value = compound_component.GetValue();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       TestEquivalenceOfReplacePlaceholderImplementations) {
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  // Create a compound component.
  TestCompoundNameCustomFormatAddressComponent compound_component;

  // Set the values of the subcomponents.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);
}

// Tests the formatting of the unstructured value from the components with a
// type-specific format string.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatValueFromSubcomponentsWithTypeSpecificFormat) {
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  // Create a compound component.
  TestCompoundNameCustomFormatAddressComponent compound_component;

  // Set the values of the subcomponents.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  // Format the compound and verify the expectation.
  test_api(compound_component).FormatValueFromSubcomponents();
  std::u16string expected_value = u"Smith, Winston";
  std::u16string actual_value = compound_component.GetValue();

  EXPECT_EQ(expected_value, actual_value);
}

// Tests the formatting of the unstructured value from the components with a
// type-specific format string containing a prefix and a suffix.
TEST_F(AutofillStructuredAddressAddressComponent,
       FormatValueFromSubcomponentsWithTypeSpecificAffixedFormat) {
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  // Create a compound component.
  TestCompoundNameCustomAffixedFormatAddressComponent compound_component;

  // Set the values of the subcomponents.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  // Format the compound and verify the expectation.
  test_api(compound_component).FormatValueFromSubcomponents();
  std::u16string expected_value = u"Dr. Smith MD, Winston";
  std::u16string actual_value = compound_component.GetValue();

  EXPECT_EQ(expected_value, actual_value);
}

// Tests parsing of an empty value. Because, that's why.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestParseValueAndAssignSubcomponentsByFallbackMethod_EmptyString) {
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValue(std::u16string(), VerificationStatus::kObserved);
  compound_component.ParseValueAndAssignSubcomponents();

  EXPECT_EQ(compound_component.GetValue(), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());
}

// Tests parsing using a defined method.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestParseValueAndAssignSubcomponentsByRegEx) {
  TestCompoundNameRegExParsedAddressComponent compound_component;
  compound_component.SetValue(u"Dr. Strangelove",
                              VerificationStatus::kObserved);
  compound_component.ParseValueAndAssignSubcomponents();

  EXPECT_EQ(compound_component.GetValue(), u"Dr. Strangelove");
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), u"Dr. Strangelove");
}

// Go nuclear and parse the value of an atomic component.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestParseValueAndAssignSubcomponentsByFallbackMethod_Atom) {
  TestAtomicFirstNameAddressComponent atomic_component;
  atomic_component.SetValue(u"Dangerzone", VerificationStatus::kObserved);
  atomic_component.ParseValueAndAssignSubcomponents();

  // The parsing should not crash the browser and keep the initial value intact.
  EXPECT_EQ(u"Dangerzone", atomic_component.GetValue());
}

// Tests the fallback method to parse a value into its components if there are
// more space-separated tokens than components.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestParseValueAndAssignSubcomponentsByFallbackMethod) {
  std::u16string full_name = u"Winston O'Brien Hammer Smith";

  // Create a compound component, set the value and parse the value of the
  // subcomponents.
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);
  compound_component.ParseValueAndAssignSubcomponents();

  // Define the expectations, and verify the expectation.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Hammer Smith";
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);
}

// Tests the fallback method to parse a value into its components if there are
// less space-separated tokens than components.
TEST_F(AutofillStructuredAddressAddressComponent,
       ParseValueAndAssignSubcomponentsByFallbackMethod_WithFewTokens) {
  std::u16string full_name = u"Winston";

  // Create a compound component and assign a value.
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);

  // Parse the full name into its components by using the fallback method
  compound_component.ParseValueAndAssignSubcomponents();

  // Verify the expectation.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"";
  std::u16string last_name = u"";
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);
}

// Tests that a tree is regarded completable if and only if there if the
// maximum number of assigned nodes on a path from the root node to a leaf is
// exactly one.
TEST_F(AutofillStructuredAddressAddressComponent, IsTreeCompletable) {
  // Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component.
  TestCompoundNameAddressComponent compound_component;

  // This tree is completable because it has not a single assigned node.
  EXPECT_TRUE(compound_component.IsTreeCompletable());

  // Set the first name node of the tree.
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);

  // The tree should be completable because there is exactly one assigned node.
  EXPECT_TRUE(compound_component.IsTreeCompletable());

  // Set the middle-name node of the tree.
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);

  // The tree should still be completable because the first and middle name are
  // siblings and not in a direct line.
  EXPECT_TRUE(compound_component.IsTreeCompletable());

  // Set the full name node of the tree.
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);

  // Now, the tree is not completable anymore because there are multiple
  // assigned values on a path from the root to a leaf.
  EXPECT_FALSE(compound_component.IsTreeCompletable());
  EXPECT_FALSE(compound_component.CompleteFullTree());
}

// Tests that the tree is completed successfully from the root node down to the
// leafs.
TEST_F(AutofillStructuredAddressAddressComponent, TreeCompletion_TopToBottom) {
  // Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component and set the value of the root node.
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);

  // Verify that the are subcomponents empty.
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());

  // Complete the tree.
  EXPECT_TRUE(compound_component.CompleteFullTree());

  // Verify that the values for the subcomponents have been successfully parsed.
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);
}

// Tests that the tree is completed successfully from leaf nodes to the root.
TEST_F(AutofillStructuredAddressAddressComponent, TreeCompletion_BottomToTop) {
  // Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component and set the value of the first, middle and last
  // name.
  TestCompoundNameAddressComponent compound_component;
  compound_component.SetValueForType(NAME_FIRST, first_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_MIDDLE, middle_name,
                                     VerificationStatus::kUserVerified);
  compound_component.SetValueForType(NAME_LAST, last_name,
                                     VerificationStatus::kUserVerified);

  // Verify that the root node is empty.
  EXPECT_EQ(compound_component.GetValueForType(NAME_FULL), std::u16string());

  // Complete the tree.
  compound_component.CompleteFullTree();

  // Verify that the value for the root node was successfully formatted.
  EXPECT_EQ(compound_component.GetValueForType(NAME_FULL), full_name);
}

// Tests that the tree is completed successfully both upwards and downwards when
// a node with both subcomponents and a parent is set.
TEST_F(AutofillStructuredAddressAddressComponent,
       TreeCompletion_ToTopAndBottom) {
  // Define Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component.
  TestCompoundNameWithTitleAddressComponent compound_component;

  // Set the value of the root node.
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);
  // Verify that the are subcomponents empty.
  // CREDIT_CARD_NAME_FULL is a fictive type containing a title and a full name.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());

  // Complete the tree.
  compound_component.CompleteFullTree();

  // Verify that the values for the subcomponents have been successfully parsed
  // and the parent node was probably formatted.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            full_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);
}

// Test that values are invalidated correctly.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestSettingsValuesWithInvalidation) {
  // Define Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component.
  TestCompoundNameWithTitleAddressComponent compound_component;

  // Set the value of the root node.
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);
  // Verify that the are subcomponents empty.
  // CREDIT_CARD_NAME_FULL is a fictive type containing a title and a full name.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());

  // Complete the tree.
  compound_component.CompleteFullTree();

  // Verify that the values for the subcomponents have been successfully parsed
  // and the parent node was probably formatted.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            full_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);

  // Change the value of FULL_NAME and invalidate all child and ancestor nodes.
  compound_component.SetValueForTypeAndResetSubstructure(
      NAME_FULL, u"Oh' Brian", VerificationStatus::kObserved);
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            full_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());
}

// Test unsetting a value and its subcomponents.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestUnsettingAValueAndItsSubcomponents) {
  // Define Some values.
  std::u16string first_name = u"Winston";
  std::u16string middle_name = u"O'Brien";
  std::u16string last_name = u"Smith";
  std::u16string full_name = u"Winston O'Brien Smith";

  // Create a compound component.
  TestCompoundNameWithTitleAddressComponent compound_component;

  // Set the value of the root node.
  compound_component.SetValueForType(NAME_FULL, full_name,
                                     VerificationStatus::kUserVerified);

  // Verify that the are subcomponents empty.
  // CREDIT_CARD_NAME_FULL is a fictive type containing a title and a full name.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());

  // Complete the tree.
  compound_component.CompleteFullTree();

  // Verify that the values for the subcomponents have been successfully parsed
  // and the parent node was probably formatted.
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            full_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), first_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), middle_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), last_name);

  // Change the value of FULL_NAME and invalidate all child and ancestor nodes.
  compound_component.UnsetValueForTypeIfSupported(NAME_FULL);
  EXPECT_EQ(compound_component.GetValueForType(CREDIT_CARD_NAME_FULL),
            full_name);
  EXPECT_EQ(compound_component.GetValueForType(NAME_FIRST), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_MIDDLE), std::u16string());
  EXPECT_EQ(compound_component.GetValueForType(NAME_LAST), std::u16string());
}

// Tests that the tree is completed successfully both upwards and downwards when
// a node with both subcomponents and a parent is set.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestUnsettingParsedAndFormattedValues) {
  // Define Some values.

  TestCompoundNameWithTitleAddressComponent compound_component;

  // Set a value somewhere in the tree, complete and verify that another node is
  // assigned.
  compound_component.SetValueForType(NAME_FULL, u"Winston Brian Smith",
                                     VerificationStatus::kObserved);
  EXPECT_EQ(VerificationStatus::kObserved,
            compound_component.GetVerificationStatusForType(NAME_FULL));
  compound_component.CompleteFullTree();
  EXPECT_EQ(VerificationStatus::kParsed,
            compound_component.GetVerificationStatusForType(NAME_MIDDLE));
  EXPECT_EQ(
      VerificationStatus::kFormatted,
      compound_component.GetVerificationStatusForType(CREDIT_CARD_NAME_FULL));

  // Clear parsed and formatted values and verify the expectation.
  compound_component.UnsetParsedAndFormattedValuesInEntireTree();
  EXPECT_EQ(VerificationStatus::kObserved,
            compound_component.GetVerificationStatusForType(NAME_FULL));
  EXPECT_EQ(VerificationStatus::kNoStatus,
            compound_component.GetVerificationStatusForType(NAME_MIDDLE));
  EXPECT_EQ(
      VerificationStatus::kNoStatus,
      compound_component.GetVerificationStatusForType(CREDIT_CARD_NAME_FULL));
}

TEST_F(AutofillStructuredAddressAddressComponent,
       MergeAtomicComponentsWithDifferentValues) {
  TestAtomicFirstNameAddressComponent one;
  one.SetValue(u"Peter", VerificationStatus::kFormatted);

  TestAtomicFirstNameAddressComponent two;
  two.SetValue(u"Hook", VerificationStatus::kUserVerified);

  // |one| and |two| are note mergeable because they contain completely
  // different values.
  EXPECT_FALSE(one.MergeWithComponent(two));
  // Since |one| and |two| are not mergeable, it is expected that the value of
  // |one| is preserved.
  EXPECT_EQ(one.GetValue(), u"Peter");
  EXPECT_EQ(one.GetVerificationStatus(), VerificationStatus::kFormatted);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       MergeAtomicComponentsWithSameValue) {
  TestAtomicFirstNameAddressComponent one;
  one.SetValue(u"Peter", VerificationStatus::kFormatted);

  TestAtomicFirstNameAddressComponent two;
  two.SetValue(u"Peter", VerificationStatus::kUserVerified);

  EXPECT_TRUE(one.MergeWithComponent(two));
  EXPECT_EQ(one.GetValue(), u"Peter");

  // The actual action is that the higher verification status is picked.
  EXPECT_EQ(one.GetVerificationStatus(), VerificationStatus::kUserVerified);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       MergeAtomicComponentsSimilarValueThatContainsSameNormalizedValue) {
  TestAtomicFirstNameAddressComponent one;
  one.SetValue(u"müller", VerificationStatus::kFormatted);

  TestAtomicFirstNameAddressComponent two;
  two.SetValue(u"Muller", VerificationStatus::kUserVerified);

  // Should be mergeable because the values are the same after normalization.
  EXPECT_TRUE(one.MergeWithComponent(two));
  // The value should be Muller because of its higher validation status.
  EXPECT_EQ(one.GetValue(), u"Muller");

  // The actual action is that the higher verification status is picked.
  EXPECT_EQ(one.GetVerificationStatus(), VerificationStatus::kUserVerified);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       MergeAtomicComponentsWithPermutedValue) {
  TestAtomicFirstNameAddressComponent one;
  one.SetValue(u"Peter Pan", VerificationStatus::kFormatted);

  TestAtomicFirstNameAddressComponent two;
  two.SetValue(u"Pan Peter", VerificationStatus::kUserVerified);

  EXPECT_TRUE(one.MergeWithComponent(two));
  EXPECT_EQ(one.GetValue(), u"Pan Peter");
  EXPECT_EQ(one.GetVerificationStatus(), VerificationStatus::kUserVerified);

  // If the merging is applied the other way round, the value of two is not
  // altered because |two| has the higher validation status.
  one.SetValue(u"Peter Pan", VerificationStatus::kFormatted);
  EXPECT_TRUE(two.MergeWithComponent(one));
  EXPECT_EQ(two.GetValue(), u"Pan Peter");
  EXPECT_EQ(two.GetVerificationStatus(), VerificationStatus::kUserVerified);
}

// This test verifies the merging of the verification statuses that is
// applicable for combining variants.
TEST_F(AutofillStructuredAddressAddressComponent, MergeVerificationStatuses) {
  TestCompoundNameAddressComponent one;
  TestCompoundNameAddressComponent two;

  one.SetValueForType(NAME_FULL, u"A B C", VerificationStatus::kObserved);
  one.SetValueForType(NAME_FIRST, u"A", VerificationStatus::kObserved);
  one.SetValueForType(NAME_MIDDLE, u"B", VerificationStatus::kObserved);
  one.SetValueForType(NAME_LAST, u"C", VerificationStatus::kObserved);

  two.SetValueForType(NAME_FULL, u"A D C", VerificationStatus::kUserVerified);
  two.SetValueForType(NAME_FIRST, u"A", VerificationStatus::kUserVerified);
  two.SetValueForType(NAME_MIDDLE, u"D", VerificationStatus::kUserVerified);
  two.SetValueForType(NAME_LAST, u"C", VerificationStatus::kUserVerified);

  one.MergeVerificationStatuses(two);

  EXPECT_EQ(one.GetValueForType(NAME_FULL), u"A B C");
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), u"A");
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), u"B");
  EXPECT_EQ(one.GetValueForType(NAME_LAST), u"C");

  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kUserVerified);
}

// This tests verifies the formatted and observed values can be reset
// correctly.
TEST_F(AutofillStructuredAddressAddressComponent,
       ClearParsedAndFormattedValues) {
  TestCompoundNameAddressComponent one;
  TestCompoundNameAddressComponent two;

  one.SetValueForType(NAME_FULL, u"A B C", VerificationStatus::kFormatted);
  one.SetValueForType(NAME_FIRST, u"A", VerificationStatus::kObserved);
  one.SetValueForType(NAME_MIDDLE, u"B", VerificationStatus::kParsed);
  one.SetValueForType(NAME_LAST, u"C", VerificationStatus::kUserVerified);
  one.RecursivelyUnsetParsedAndFormattedValues();

  EXPECT_EQ(one.GetValueForType(NAME_FULL), u"");
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), u"A");
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), u"");
  EXPECT_EQ(one.GetValueForType(NAME_LAST), u"C");

  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kUserVerified);
}

// This test verifies that if two value-equal components are merged, the higher
// verification statuses are picked.
TEST_F(AutofillStructuredAddressAddressComponent,
       MergeTriviallyMergeableCompoundComponents) {
  TestCompoundNameAddressComponent one;
  TestCompoundNameAddressComponent two;

  EXPECT_TRUE(one.SetValueForType(NAME_FULL, u"First LastFirst LastSecond",
                                  VerificationStatus::kUserVerified));
  one.CompleteFullTree();

  EXPECT_EQ(one.GetValueForType(NAME_FIRST), u"First");
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), u"LastFirst");
  EXPECT_EQ(one.GetValueForType(NAME_LAST), u"LastSecond");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kParsed);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kParsed);

  EXPECT_TRUE(
      two.SetValueForType(NAME_FIRST, u"First", VerificationStatus::kObserved));
  EXPECT_TRUE(two.SetValueForType(NAME_LAST, u"LastFirst LastSecond",
                                  VerificationStatus::kObserved));
  two.CompleteFullTree();
  EXPECT_EQ(two.GetValueForType(NAME_FULL), u"First LastFirst LastSecond");
  EXPECT_EQ(two.GetValueForType(NAME_MIDDLE), u"");

  EXPECT_TRUE(one.MergeWithComponent(two));
  EXPECT_EQ(one.GetValueForType(NAME_FULL), u"First LastFirst LastSecond");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), u"First");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), u"");
  EXPECT_EQ(one.GetValueForType(NAME_LAST), u"LastFirst LastSecond");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
}

// This test verifies that the formatted value is successfully replaced by the
// user-verified value while the substructure is corrected by the observation.
TEST_F(AutofillStructuredAddressAddressComponent, MergePermutedComponent) {
  TestCompoundNameAddressComponent one;
  TestCompoundNameAddressComponent two;

  // The first component has the unstructured representation as the user
  // verified it, but a wrong componentization.
  EXPECT_TRUE(one.SetValueForType(NAME_FULL, u"Last First Middle",
                                  VerificationStatus::kUserVerified));
  EXPECT_TRUE(
      one.SetValueForType(NAME_FIRST, u"Last", VerificationStatus::kParsed));
  EXPECT_TRUE(
      one.SetValueForType(NAME_MIDDLE, u"First", VerificationStatus::kParsed));
  EXPECT_TRUE(
      one.SetValueForType(NAME_LAST, u"Middle", VerificationStatus::kParsed));

  // The second component has a correct componentization but not the
  // unstructured representation the user prefers.
  EXPECT_TRUE(two.SetValueForType(NAME_FULL, u"First Last Middle",
                                  VerificationStatus::kFormatted));
  EXPECT_TRUE(
      two.SetValueForType(NAME_FIRST, u"First", VerificationStatus::kObserved));
  EXPECT_TRUE(two.SetValueForType(NAME_MIDDLE, u"Middle",
                                  VerificationStatus::kObserved));
  EXPECT_TRUE(
      two.SetValueForType(NAME_LAST, u"Last", VerificationStatus::kObserved));

  TestCompoundNameAddressComponent copy_of_one;
  copy_of_one.CopyFrom(one);
  EXPECT_TRUE(one.MergeWithComponent(two));

  // As a result of the merging, the unstructured representation should be
  // maintained, but the substructure should be corrected
  EXPECT_EQ(one.GetValueForType(NAME_FULL), u"Last First Middle");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), u"First");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), u"Middle");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetValueForType(NAME_LAST), u"Last");
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);

  // The merging should work in both directions the same way.
  EXPECT_TRUE(two.MergeWithComponent(copy_of_one));
  EXPECT_EQ(two.GetValueForType(NAME_FULL), u"Last First Middle");
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(two.GetValueForType(NAME_FIRST), u"First");
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetValueForType(NAME_MIDDLE), u"Middle");
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetValueForType(NAME_LAST), u"Last");
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       SimpleReplacementBasedMergingStrategies) {
  // Create values for a fully populated name that serve as a superset to other
  // cases.
  AddressComponentTestValues superset = {
      {.type = NAME_FULL,
       .value = "Thomas Neo Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  // Create values for a subset component of superset.
  AddressComponentTestValues subset = {
      {.type = NAME_FULL,
       .value = "Thomas Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  // Create values for a component that is not a subset of superset.
  AddressComponentTestValues not_a_subset = {
      {.type = NAME_FULL,
       .value = "Agent Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Agent",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  // Create values for a component that has a substring of the superset in its
  // root.
  AddressComponentTestValues superset_substring = {
      {.type = NAME_FULL,
       .value = "Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  // Create values for a component that has a substring of the superset in its
  // root.
  AddressComponentTestValues not_superset_substring = {
      {.type = NAME_FULL,
       .value = "Smith",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST,
       .value = "Smith",
       .status = VerificationStatus::kObserved},
  };

  // Create a subset component.
  AddressComponentTestValues empty = {
      {.type = NAME_FULL, .value = "", .status = VerificationStatus::kNoStatus},
  };

  // Test the merging of subsets.
  TestCompoundNameMerging(superset, subset, superset, true,
                          MergeMode::kReplaceSubset);
  TestCompoundNameMerging(subset, superset, superset, true,
                          MergeMode::kReplaceSubset);

  TestCompoundNameMerging(superset, not_a_subset, superset, false,
                          MergeMode::kReplaceSubset);
  TestCompoundNameMerging(not_a_subset, superset, not_a_subset, false,
                          MergeMode::kReplaceSubset);

  // Test the merging of supersets.
  TestCompoundNameMerging(superset, subset, subset, true,
                          MergeMode::kReplaceSuperset);
  TestCompoundNameMerging(subset, superset, subset, true,
                          MergeMode::kReplaceSuperset);

  TestCompoundNameMerging(superset, not_a_subset, superset, false,
                          MergeMode::kReplaceSuperset);
  TestCompoundNameMerging(not_a_subset, superset, not_a_subset, false,
                          MergeMode::kReplaceSuperset);

  // Test the replacement of empty components.
  TestCompoundNameMerging(superset, empty, superset, true,
                          MergeMode::kReplaceEmpty);
  TestCompoundNameMerging(empty, superset, superset, true,
                          MergeMode::kReplaceEmpty);

  // Test the merging of substrings.
  TestCompoundNameMerging(superset, superset_substring, superset_substring,
                          true, MergeMode::kUseMostRecentSubstring);
  TestCompoundNameMerging(superset_substring, superset, superset, true,
                          MergeMode::kUseMostRecentSubstring);

  TestCompoundNameMerging(superset, superset_substring, superset, true,
                          MergeMode::kUseMostRecentSubstring,
                          /*newer_was_more_recently_used=*/false);
  TestCompoundNameMerging(superset_substring, superset, superset_substring,
                          true, MergeMode::kUseMostRecentSubstring,
                          /*newer_was_more_recently_used=*/false);

  TestCompoundNameMerging(superset, not_superset_substring, superset, false,
                          MergeMode::kUseMostRecentSubstring);
  TestCompoundNameMerging(not_superset_substring, superset,
                          not_superset_substring, false,
                          MergeMode::kUseMostRecentSubstring);

  // Test taking the newer component.
  TestCompoundNameMerging(superset, not_a_subset, not_a_subset, true,
                          MergeMode::kUseNewerIfDifferent);
  TestCompoundNameMerging(not_a_subset, superset, superset, true,
                          MergeMode::kUseNewerIfDifferent);
}

TEST_F(AutofillStructuredAddressAddressComponent, MergeChildsAndReformatRoot) {
  TestCompoundNameAddressComponent older;
  TestCompoundNameAddressComponent newer;
  TestCompoundNameAddressComponent unmergeable_newer;

  // Set the root node to merging mode which only merges the children and gets
  // reformatted afterwards.
  test_api(older).SetMergeMode(MergeMode::kMergeChildrenAndReformatIfNeeded);
  // Set the merge modes of the children to replace empty values and use
  // supersets.
  for (AddressComponent* subcomponent : older.Subcomponents()) {
    test_api(*subcomponent).SetMergeMode(kReplaceEmpty | kReplaceSubset);
  }

  AddressComponentTestValues older_values = {
      {.type = NAME_FULL,
       .value = "Thomas Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST,
       .value = "The One Anderson",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues newer_values = {
      {.type = NAME_FULL,
       .value = "T Neo",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues unmergeable_values = {
      {.type = NAME_FULL,
       .value = "Agent Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Agent",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues expectation = {
      {.type = NAME_FULL,
       .value = "Thomas Neo The One Anderson",
       .status = VerificationStatus::kFormatted},
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "The One Anderson",
       .status = VerificationStatus::kObserved},
  };

  SetTestValues(&older, older_values);
  SetTestValues(&newer, newer_values);
  SetTestValues(&unmergeable_newer, unmergeable_values);

  EXPECT_TRUE(older.IsMergeableWithComponent(newer));
  EXPECT_TRUE(older.MergeWithComponent(newer));

  VerifyTestValues(&older, expectation);

  // Reset the values of the older component.
  SetTestValues(&older, older_values);
  SetTestValues(&unmergeable_newer, unmergeable_values);
  EXPECT_FALSE(older.IsMergeableWithComponent(unmergeable_newer));
  EXPECT_FALSE(older.MergeWithComponent(unmergeable_newer));
  VerifyTestValues(&older, older_values);
}

// Tests the comparison of different Verification statuses.
TEST_F(AutofillStructuredAddressAddressComponent,
       TestIsLessSignificantVerificationStatus) {
  EXPECT_TRUE(IsLessSignificantVerificationStatus(
      VerificationStatus::kParsed, VerificationStatus::kFormatted));
  EXPECT_TRUE(IsLessSignificantVerificationStatus(
      VerificationStatus::kParsed, VerificationStatus::kServerParsed));
  EXPECT_TRUE(IsLessSignificantVerificationStatus(
      VerificationStatus::kServerParsed, VerificationStatus::kObserved));
  EXPECT_TRUE(IsLessSignificantVerificationStatus(
      VerificationStatus::kServerParsed, VerificationStatus::kUserVerified));
  EXPECT_FALSE(IsLessSignificantVerificationStatus(
      VerificationStatus::kServerParsed, VerificationStatus::kFormatted));
  EXPECT_FALSE(IsLessSignificantVerificationStatus(
      VerificationStatus::kServerParsed, VerificationStatus::kParsed));
  EXPECT_FALSE(IsLessSignificantVerificationStatus(
      VerificationStatus::kObserved, VerificationStatus::kServerParsed));
  EXPECT_FALSE(IsLessSignificantVerificationStatus(
      VerificationStatus::kUserVerified, VerificationStatus::kServerParsed));
}

// Tests getting the more significant VerificationStatus.
TEST_F(AutofillStructuredAddressAddressComponent,
       GetMoreSignificantVerificationStatus) {
  EXPECT_EQ(VerificationStatus::kFormatted,
            GetMoreSignificantVerificationStatus(VerificationStatus::kFormatted,
                                                 VerificationStatus::kParsed));
  EXPECT_EQ(VerificationStatus::kObserved,
            GetMoreSignificantVerificationStatus(
                VerificationStatus::kFormatted, VerificationStatus::kObserved));
  EXPECT_EQ(
      VerificationStatus::kUserVerified,
      GetMoreSignificantVerificationStatus(VerificationStatus::kUserVerified,
                                           VerificationStatus::kUserVerified));
}

// Tests merging using the MergeMode::KUseBetterOrMoreRecentIfDifferent|
TEST_F(AutofillStructuredAddressAddressComponent,
       TestUseBetterOfMoreRecentIfDifferentMergeStrategy) {
  AddressComponentTestValues old_values = {
      {.type = NAME_FIRST,
       .value = "first value",
       .status = VerificationStatus::kObserved}};
  AddressComponentTestValues newer_values = {
      {.type = NAME_FIRST,
       .value = "second value",
       .status = VerificationStatus::kObserved}};
  AddressComponentTestValues better_values = {
      {.type = NAME_FIRST,
       .value = "second value",
       .status = VerificationStatus::kUserVerified}};
  AddressComponentTestValues not_better_values = {
      {.type = NAME_FIRST,
       .value = "second value",
       .status = VerificationStatus::kParsed}};

  // Test that the newer values are used.
  TestAtomMerging(NAME_FIRST, old_values, newer_values, newer_values,
                  /*is_mergeable=*/true,
                  MergeMode::kUseBetterOrMostRecentIfDifferent);

  // Test that the better values are used.
  TestAtomMerging(NAME_FIRST, old_values, better_values, better_values,
                  /*is_mergeable=*/true,
                  MergeMode::kUseBetterOrMostRecentIfDifferent);
  // Should work equally in both directions.
  TestAtomMerging(NAME_FIRST, better_values, old_values, better_values,
                  /*is_mergeable=*/true,
                  MergeMode::kUseBetterOrMostRecentIfDifferent);

  // Test that the not better values are not used.
  TestAtomMerging(NAME_FIRST, old_values, not_better_values, old_values,
                  /*is_mergeable=*/true,
                  MergeMode::kUseBetterOrMostRecentIfDifferent);
  // Should work equally in both directions.
  TestAtomMerging(NAME_FIRST, not_better_values, old_values, old_values,
                  /*is_mergeable=*/true,
                  MergeMode::kUseBetterOrMostRecentIfDifferent);
}

TEST_F(AutofillStructuredAddressAddressComponent, TestFillTreeGaps) {
  NameFull name;

  AddressComponentTestValues name_filled_values = {
      {.type = NAME_FULL,
       .value = "Pablo Diego Ruiz y Picasso",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_FIRST,
       .value = "Ruiz",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "y",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_SECOND,
       .value = "Picasso",
       .status = VerificationStatus::kObserved}};

  AddressComponentTestValues expectation = {
      {.type = NAME_FULL,
       .value = "Pablo Diego Ruiz y Picasso",
       .status = VerificationStatus::kObserved},
      {.type = NAME_FIRST,
       .value = "Pablo Diego",
       .status = VerificationStatus::kParsed},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST,
       .value = "Ruiz y Picasso",
       .status = VerificationStatus::kParsed},
      {.type = NAME_LAST_FIRST,
       .value = "Ruiz",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "y",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_SECOND,
       .value = "Picasso",
       .status = VerificationStatus::kObserved},
  };

  SetTestValues(&name, name_filled_values);
  name.CompleteFullTree();
  VerifyTestValues(&name, expectation);
}

TEST_F(AutofillStructuredAddressAddressComponent,
       IsValueCompatibleWithAncestorsCompatible) {
  AddressComponentsStore store =
      i18n_model_definition::CreateAddressComponentModel();
  AddressComponent* root = store.Root();

  AddressComponentTestValues test_values = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "Apt 42, Floor 7, Tagore Road Hostel, 13, Hitech City Rd",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_FLOOR,
       .value = "7",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = "42",
       .status = VerificationStatus::kObserved}};

  AddressComponentTestValues expectation = {
      {.type = ADDRESS_HOME_STREET_ADDRESS,
       .value = "Apt 42, Floor 7, Tagore Road Hostel, 13, Hitech City Rd",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_SUBPREMISE,
       .value = "Apt. 42, Floor 7",
       .status = VerificationStatus::kFormatted},
      {.type = ADDRESS_HOME_FLOOR,
       .value = "7",
       .status = VerificationStatus::kObserved},
      {.type = ADDRESS_HOME_APT_NUM,
       .value = "42",
       .status = VerificationStatus::kObserved}};

  SetTestValues(root, test_values);
  root->CompleteFullTree();
  VerifyTestValues(root, expectation);
}

TEST_F(AutofillStructuredAddressAddressComponent, TestFillTreeGapsParsing) {
  NameFull name;

  AddressComponentTestValues name_filled_values = {
      {.type = NAME_FULL,
       .value = "Pablo Diego Ruiz y Picasso",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Ruiz y Picasso",
       .status = VerificationStatus::kObserved}};

  AddressComponentTestValues expectation = {
      {.type = NAME_FULL,
       .value = "Pablo Diego Ruiz y Picasso",
       .status = VerificationStatus::kObserved},
      {.type = NAME_FIRST,
       .value = "Pablo Diego",
       .status = VerificationStatus::kParsed},
      {.type = NAME_MIDDLE,
       .value = "",
       .status = VerificationStatus::kNoStatus},
      {.type = NAME_LAST,
       .value = "Ruiz y Picasso",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_FIRST,
       .value = "Ruiz",
       .status = VerificationStatus::kParsed},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "y",
       .status = VerificationStatus::kParsed},
      {.type = NAME_LAST_SECOND,
       .value = "Picasso",
       .status = VerificationStatus::kParsed},
  };

  SetTestValues(&name, name_filled_values);
  name.CompleteFullTree();
  VerifyTestValues(&name, expectation);
}

}  // namespace autofill

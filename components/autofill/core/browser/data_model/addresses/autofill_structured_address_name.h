// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/addresses/autofill_feature_guarded_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill {

// Atomic components that represents the first name.
class NameFirst : public AddressComponent {
 public:
  NameFirst();
  ~NameFirst() override;
};

// Atomic component that represents the middle name.
class NameMiddle : public AddressComponent {
 public:
  NameMiddle();
  ~NameMiddle() override;

  const FieldTypeSet GetAdditionalSupportedFieldTypes() const override;

 protected:
  // Implements support for getting the value for the |MIDDLE_NAME_INITIAL|
  // type.
  std::u16string GetValueForOtherSupportedType(
      FieldType field_type) const override;

  // Implements support for setting the |MIDDLE_NAME_INITIAL| type.
  void SetValueForOtherSupportedType(FieldType field_type,
                                     const std::u16string& value,
                                     const VerificationStatus& status) override;
};

// Atomic component that represents the first part of a last name.
class NameLastFirst : public AddressComponent {
 public:
  NameLastFirst();
  ~NameLastFirst() override;
};

// Atomic component that represents the conjunction in a Hispanic/Latinx
// surname.
class NameLastConjunction : public AddressComponent {
 public:
  NameLastConjunction();
  ~NameLastConjunction() override;
};

// Atomic component that represents the second part of a surname.
class NameLastSecond : public AddressComponent {
 public:
  NameLastSecond();
  ~NameLastSecond() override;
};

// Atomic component that represents the prefix of a surname.
// A prefix of a last name can be "van" in the Netherlands for example.
class NameLastPrefix : public AddressComponent {
 public:
  NameLastPrefix();
  ~NameLastPrefix() override;
};

// Compound that represents a last name core, which is the part of the last name
// without the prefix.
class NameLastCore : public AddressComponent {
 public:
  NameLastCore();
  ~NameLastCore() override;

 private:
  // As a fallback, write everything to the second last name.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  NameLastFirst last_first_;
  NameLastConjunction last_conjuntion_;
  NameLastSecond last_second_;
};

// Compound that represents a last name. It can contain multiple components,
// including prefixes and conjunctions *within* the last name itself.
// For example, in "Pablo von Ruiz y Picasso":
// - "von Ruiz y Picasso" is the last name.
// - "von" is a prefix within the last name.
// - "Ruiz y Picasso" is the core of the last name.
// - "Ruiz" is the first part of the last name.
// - "y" is the conjunction within the last name.
// - "Picasso" is the second part of the last name.
//
// Hyphenated last names like "Miller-Smith" are treated as a single unit and
// stored in the _CORE component. If the last name has only one component and no
// internal prefixes or conjunctions, it is stored in the _CORE and propagated
// down to the _SECOND component by default, so that NAME_LAST, NAME_LAST_CORE,
// and NAME_LAST_SECOND have the same value.
//
// A separate _PREFIX component is used for last name prefixes (e.g., "von",
// "de"). _FIRST and _CONJUNCTION are only used for Hispanic/Latinx names.
//
// The structure is as follows:
//
//                        +-------------+
//                        |  NAME_LAST  |
//                        +-------------+
//                         /           \
//                        /             \
//                       /               \
//                +---------+          +---------+
//                | _PREFIX |          |  _CORE  |
//                +---------+          +---------+
//                                     /     |     \
//                                    /      |      \
//                                   /       |       \
//                                  /        |        \
//                         +--------+ +--------------+ +---------+
//                         | _FIRST | | _CONJUNCTION | | _SECOND |
//                         +--------+ +--------------+ +---------+
//
class NameLast : public AddressComponent {
 public:
  NameLast();
  ~NameLast() override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

 private:
  // As a fallback, write everything to the last name core.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

  // TODO(crbug.com/386916943): Keep only these components after launching
  // kAutofillSupportLastNamePrefix.
  NameLastPrefix last_prefix_;
  NameLastCore last_core_;

  // TODO(crbug.com/386916943): Delete these components after launching
  // kAutofillSupportLastNamePrefix.
  NameLastFirst last_first_;
  NameLastConjunction last_conjuntion_;
  NameLastSecond last_second_;
};

// Compound that represents a full name. It contains a honorific, a first
// name, a middle name and a last name. The last name is a compound itself.
//
//                     +------------+
//                     | NAME_FULL  |
//                     +------------+
//                    /       |      \
//                   /        |       \
//                  /         |        \
//    +------------+  +-------------+   +-----------+
//    | NAME_FIRST |  | NAME_MIDDLE |   | NAME_LAST |
//    +------------+  +-------------+   +-----------+
//
class NameFull : public AddressComponent {
 public:
  NameFull();
  NameFull(const NameFull& other);
  ~NameFull() override;

  void MigrateLegacyStructure() override;

 protected:
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Returns the format string to create the full name from its subcomponents.
  std::u16string GetFormatString() const override;

  NameFirst first_;
  NameMiddle middle_;
  NameLast last_;
};

// A common class used by the alternative name nodes to implement their shared
// methods.
class AlternativeNameAddressComponent : public AddressComponent {
 public:
  // AddressComponent:
  AlternativeNameAddressComponent(FieldType storage_type,
                                  SubcomponentsList subcomponents,
                                  unsigned int merge_mode);

  bool SameAs(const AddressComponent& other) const override;

  // Returns the value with all Katakana characters converted to Hiragana.
  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;
};

// Atomic component that represents the first part of an alternative name(e.g.
// Japanese phonetic given name).
class AlternativeGivenName : public AlternativeNameAddressComponent {
 public:
  AlternativeGivenName();
  ~AlternativeGivenName() override;
};

// Atomic component that represents the last part of an alternative name(e.g.
// Japanese phonetic last name).
class AlternativeFamilyName : public AlternativeNameAddressComponent {
 public:
  AlternativeFamilyName();
  ~AlternativeFamilyName() override;
};

// Compound node that represents an alternative full name (e.g. full phonetic
// name in Japanese). It contains a given name and a family name.
// TODO(crbug.com/359768803): This class is currently mainly focused on
// supporting Japanese phonetic names, but its logic should be extended to
// handle alternative name expressions in different languages as well (e.g.
// latin names in Greek).
//
//                   +-----------------------+
//                   | ALTERNATIVE_FULL_NAME |
//                   +-----------------------+
//                    /                   \
//                   /                     \
//                  /                       \
//    +------------------------+     +-------------------------+
//    | ALTERNATIVE_GIVEN_NAME |     | ALTERNATIVE_FAMILY_NAME |
//    +------------------------+     +-------------------------+
//
class AlternativeFullName : public AlternativeNameAddressComponent {
 public:
  AlternativeFullName();
  AlternativeFullName(const AlternativeFullName& other);
  ~AlternativeFullName() override;

 protected:
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Returns the format string to create the full alternative name from its
  // subcomponents.
  std::u16string GetFormatString() const override;

  AlternativeGivenName given_name_;
  AlternativeFamilyName family_name_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

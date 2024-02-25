// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_feature_guarded_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
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

// Compound that represent a last name. It contains a first and second last name
// and a conjunction as it is used in Hispanic/Latinx names. Note, that compound
// family names like Miller-Smith are not supposed to be split up into two
// components. If a name contains only a single component, the component is
// stored in the second part by default.
//
//               +-------+
//               | _LAST |
//               +--------
//               /    |    \
//             /      |      \
//           /        |        \
// +--------+ +-----------+ +---------+
// | _FIRST | | _CONJUNC. | | _SECOND |
// +--------+ +-----------+ +---------+
//
class NameLast : public AddressComponent {
 public:
  NameLast();
  ~NameLast() override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

 private:
  // As the fallback, write everything to the second last name.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

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
//                                     /      |      \
//                                    /       |       \
//                                   /        |        \
//                                  /         |         \
//                          +--------+ +--------------+ +---------+
//                          | _FIRST | | _CONJUNCTION | | _SECOND |
//                          +--------+ +--------------+ +---------+
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

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

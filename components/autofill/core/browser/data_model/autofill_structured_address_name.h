// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill {
namespace structured_address {

// Atomic component that represents the honorific prefix of a name.
class NameHonorific : public AddressComponent {
 public:
  explicit NameHonorific(AddressComponent* parent);
  ~NameHonorific() override;
};

// Atomic components that represents the first name.
class NameFirst : public AddressComponent {
 public:
  explicit NameFirst(AddressComponent* parent);
  ~NameFirst() override;
};

// Atomic component that represents the middle name.
class NameMiddle : public AddressComponent {
 public:
  explicit NameMiddle(AddressComponent* parent);
  ~NameMiddle() override;

  void GetAdditionalSupportedFieldTypes(
      ServerFieldTypeSet* supported_types) const override;

 protected:
  // Implements support for getting the value for the |MIDDLE_NAME_INITIAL|
  // type.
  bool ConvertAndGetTheValueForAdditionalFieldTypeName(
      const std::string& type_name,
      std::u16string* value) const override;

  // Implements support for setting the |MIDDLE_NAME_INITIAL| type.
  bool ConvertAndSetValueForAdditionalFieldTypeName(
      const std::string& type_name,
      const std::u16string& value,
      const structured_address::VerificationStatus& status) override;
};

// Atomic component that represents the first part of a last name.
class NameLastFirst : public AddressComponent {
 public:
  explicit NameLastFirst(AddressComponent* parent);
  ~NameLastFirst() override;
};

// Atomic component that represents the conjunction in a Hispanic/Latinx
// surname.
class NameLastConjunction : public AddressComponent {
 public:
  explicit NameLastConjunction(AddressComponent* parent);
  ~NameLastConjunction() override;
};

// Atomic component that represents the second part of a surname.
class NameLastSecond : public AddressComponent {
 public:
  explicit NameLastSecond(AddressComponent* parent);
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
  explicit NameLast(AddressComponent* parent);
  ~NameLast() override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

 private:
  // As the fallback, write everything to the second last name.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

  NameLastFirst first_{this};
  NameLastConjunction conjunction_{this};
  NameLastSecond second_{this};
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
  explicit NameFull(AddressComponent* parent);
  NameFull(const NameFull& other);
  ~NameFull() override;

  void MigrateLegacyStructure(bool is_verified_profile) override;

 protected:
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Returns the format string to create the full name from its subcomponents.
  std::u16string GetBestFormatString() const override;

 private:
  NameFirst name_first_{this};
  NameMiddle name_middle_{this};
  NameLast name_last_{this};
};

// Atomic component that represents a honorific prefix.
class NameHonorificPrefix : public AddressComponent {
 public:
  explicit NameHonorificPrefix(AddressComponent* parent);
  ~NameHonorificPrefix() override;
};

// Compound that represent a full name and a honorific prefix.
//
//             +-----------------------+
//             | NAME_FULL_WITH_PREFIX |
//             +-----------------------+
//                   /            \
//                  /              \
//                 /                \
//                /                  \
//   +-------------------+      +------------+
//   | HONORIFIC_PREFIX  |      | NAME_FULL  |
//   +-------------------+      +------------+
//                             /       |      \
//                            /        |       \
//                           /         |        \
//             +------------+  +-------------+   +-----------+
//             | NAME_FIRST |  | NAME_MIDDLE |   | NAME_LAST |
//             +------------+  +-------------+   +-----------+
//                                              /      |      \
//                                             /       |       \
//                                            /        |        \
//                                           /         |         \
//                                   +--------+ +--------------+ +---------+
//                                   | _FIRST | | _CONJUNCTION | | _SECOND |
//                                   +--------+ +--------------+ +---------+
//
class NameFullWithPrefix : public AddressComponent {
 public:
  NameFullWithPrefix();
  explicit NameFullWithPrefix(AddressComponent* parent);
  NameFullWithPrefix(const NameFullWithPrefix& other);
  ~NameFullWithPrefix() override;

  void MigrateLegacyStructure(bool is_verified_profile) override;

 protected:
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  NameHonorificPrefix honorific_prefix_{this};
  NameFull name_full_{this};
};

}  // namespace structured_address

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_NAME_H_

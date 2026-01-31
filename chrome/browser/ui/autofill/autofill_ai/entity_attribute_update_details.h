// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_ENTITY_ATTRIBUTE_UPDATE_DETAILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_ENTITY_ATTRIBUTE_UPDATE_DETAILS_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill.autofill_ai
enum class EntityAttributeUpdateType {
  // A new attribute has been added to the entity. For example, the vehicle
  // make attribute (`kVehicleMake`) has been added to the vehicle info entity.
  kNewEntityAttributeAdded,
  // An existing attribute value has been overwritten. For example, the passport
  // expiration date (`kPassportExpirationDate`) has been updated in the
  // passport entity.
  kNewEntityAttributeUpdated,
  // The entity attribute has remained the same. This value is used for
  // unchanged entity attributes during the save/update flow.
  kNewEntityAttributeUnchanged,
};

std::ostream& operator<<(std::ostream& os,
                         EntityAttributeUpdateType update_type);

// Specifies for each attribute of a new instance whether the attribute is
// new, updated, or unchanged. Also includes updates of an old instance
// attribute that had its value changed.
class EntityAttributeUpdateDetails {
 public:
  EntityAttributeUpdateDetails();
  EntityAttributeUpdateDetails(
      std::u16string attribute_name,
      std::u16string attribute_value,
      std::optional<std::u16string> old_attribute_value,
      EntityAttributeUpdateType update_type);
  EntityAttributeUpdateDetails(const EntityAttributeUpdateDetails&);
  EntityAttributeUpdateDetails(EntityAttributeUpdateDetails&&);
  EntityAttributeUpdateDetails& operator=(const EntityAttributeUpdateDetails&);
  EntityAttributeUpdateDetails& operator=(EntityAttributeUpdateDetails&&);
  ~EntityAttributeUpdateDetails();

  bool operator==(const EntityAttributeUpdateDetails& other) const;

  static std::vector<EntityAttributeUpdateDetails> GetUpdatedAttributesDetails(
      const EntityInstance& new_entity,
      base::optional_ref<const EntityInstance> old_entity,
      const std::string& app_locale);

  const std::u16string& attribute_name() const { return attribute_name_; }
  const std::u16string& attribute_value() const { return attribute_value_; }
  const std::optional<std::u16string>& old_attribute_value() const {
    return old_attribute_value_;
  }
  EntityAttributeUpdateType update_type() const { return update_type_; }

 private:
  std::u16string attribute_name_;
  std::u16string attribute_value_;
  std::optional<std::u16string> old_attribute_value_;
  EntityAttributeUpdateType update_type_{};
};

// So we can compare EntityAttributeUpdateDetails with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os,
                         const EntityAttributeUpdateDetails& details);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_ENTITY_ATTRIBUTE_UPDATE_DETAILS_H_

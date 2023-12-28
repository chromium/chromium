// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_

#include <map>
#include <optional>
#include <string>

#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// This class provides the methods to update and get the field data (the pair
// of user typed value and field properties mask).
class FieldDataManager : public base::RefCounted<FieldDataManager> {
 public:
  using FieldDataMap =
      std::map<FieldRendererId,
               std::pair<std::optional<std::u16string>, FieldPropertiesMask>>;

  FieldDataManager();

  void ClearData();
  bool HasFieldData(FieldRendererId id) const;

  // Updates the field value associated with the key |element| in
  // |field_value_and_properties_map_|.
  // Flags in |mask| are added with bitwise OR operation.
  // If |value| is empty, kUserTyped and kAutofilled should be cleared.
  void UpdateFieldDataMap(FieldRendererId id,
                          std::u16string_view value,
                          FieldPropertiesMask mask);
  // Only update FieldPropertiesMask when value is null.
  void UpdateFieldDataMapWithNullValue(FieldRendererId id,
                                       FieldPropertiesMask mask);

  // Returns value that was either typed or manually autofilled into the field.
  std::u16string GetUserInput(FieldRendererId id) const;

  FieldPropertiesMask GetFieldPropertiesMask(FieldRendererId id) const;

  // Check if the string |value| is saved in |field_value_and_properties_map_|.
  bool FindMatchedValue(const std::u16string& value) const;

  bool DidUserType(FieldRendererId id) const;

  bool WasAutofilledOnUserTrigger(FieldRendererId id) const;

  const FieldDataMap& field_data_map() const {
    return field_value_and_properties_map_;
  }

 private:
  friend class base::RefCounted<FieldDataManager>;

  ~FieldDataManager();

  FieldDataMap field_value_and_properties_map_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_

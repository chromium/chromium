// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/field_data_manager.h"

#include "base/check.h"
#include "base/i18n/case_conversion.h"

namespace autofill {

FieldDataManager::FieldDataManager() = default;

void FieldDataManager::ClearData() {
  field_value_and_properties_map_.clear();
}

bool FieldDataManager::HasFieldData(FieldRendererId id) const {
  return field_value_and_properties_map_.find(id) !=
         field_value_and_properties_map_.end();
}

std::u16string FieldDataManager::GetUserInput(FieldRendererId id) const {
  DCHECK(HasFieldData(id));
  return field_value_and_properties_map_.at(id).first.value_or(
      std::u16string());
}

FieldPropertiesMask FieldDataManager::GetFieldPropertiesMask(
    FieldRendererId id) const {
  DCHECK(HasFieldData(id));
  return field_value_and_properties_map_.at(id).second;
}

bool FieldDataManager::FindMatchedValue(const std::u16string& value) const {
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  for (const auto& map_key : field_value_and_properties_map_) {
    const std::u16string typed_from_key =
        map_key.second.first.value_or(std::u16string());
    if (typed_from_key.empty())
      continue;
    if (typed_from_key.size() >= kMinMatchSize &&
        lowercase.find(base::i18n::ToLower(typed_from_key)) !=
            std::u16string::npos)
      return true;
  }
  return false;
}

void FieldDataManager::UpdateFieldDataMap(FieldRendererId id,
                                          std::u16string_view value,
                                          FieldPropertiesMask mask) {
  if (HasFieldData(id)) {
    field_value_and_properties_map_[id].first = std::u16string(value);
    field_value_and_properties_map_[id].second |= mask;
  } else {
    field_value_and_properties_map_[id] = {std::u16string(value), mask};
  }
  // Reset kUserTyped and kAutofilled flags if the value is empty.
  if (value.empty()) {
    field_value_and_properties_map_[id].second &=
        ~(FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled);
  }
}

void FieldDataManager::UpdateFieldDataMapWithNullValue(
    FieldRendererId id,
    FieldPropertiesMask mask) {
  if (HasFieldData(id)) {
    field_value_and_properties_map_[id].second |= mask;
  } else {
    field_value_and_properties_map_[id] = {std::nullopt, mask};
  }
}

bool FieldDataManager::DidUserType(FieldRendererId id) const {
  return HasFieldData(id) &&
         (GetFieldPropertiesMask(id) & FieldPropertiesFlags::kUserTyped);
}

bool FieldDataManager::WasAutofilledOnUserTrigger(FieldRendererId id) const {
  return HasFieldData(id) && (GetFieldPropertiesMask(id) &
                              FieldPropertiesFlags::kAutofilledOnUserTrigger);
}

FieldDataManager::~FieldDataManager() = default;

}  // namespace autofill

// Copyright 2018 The Chromium Authors. All rights reserved.
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

base::string16 FieldDataManager::GetUserInput(FieldRendererId id) const {
  DCHECK(HasFieldData(id));
  return field_value_and_properties_map_.at(id).first.value_or(
      base::string16());
}

FieldPropertiesMask FieldDataManager::GetFieldPropertiesMask(
    FieldRendererId id) const {
  DCHECK(HasFieldData(id));
  return field_value_and_properties_map_.at(id).second;
}

bool FieldDataManager::FindMachedValue(const base::string16& value) const {
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  for (const auto& map_key : field_value_and_properties_map_) {
    const base::string16 typed_from_key =
        map_key.second.first.value_or(base::string16());
    if (typed_from_key.empty())
      continue;
    if (typed_from_key.size() >= kMinMatchSize &&
        lowercase.find(base::i18n::ToLower(typed_from_key)) !=
            base::string16::npos)
      return true;
  }
  return false;
}

void FieldDataManager::UpdateFieldDataMap(FieldRendererId id,
                                          const base::string16& value,
                                          FieldPropertiesMask mask) {
  if (HasFieldData(id)) {
    field_value_and_properties_map_.at(id).first =
        base::Optional<base::string16>(value);
    field_value_and_properties_map_.at(id).second |= mask;
  } else {
    field_value_and_properties_map_[id] =
        std::make_pair(base::Optional<base::string16>(value), mask);
  }
  // Reset kUserTyped and kAutofilled flags if the value is empty.
  if (value.empty()) {
    field_value_and_properties_map_.at(id).second &=
        ~(FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled);
  }
}

void FieldDataManager::UpdateFieldDataMapWithNullValue(
    FieldRendererId id,
    FieldPropertiesMask mask) {
  if (HasFieldData(id))
    field_value_and_properties_map_.at(id).second |= mask;
  else
    field_value_and_properties_map_[id] = std::make_pair(base::nullopt, mask);
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

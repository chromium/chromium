// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_mapper.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"

namespace chromeos {
namespace onc {

Mapper::Mapper() = default;

Mapper::~Mapper() = default;

base::Value Mapper::MapValue(const OncValueSignature& signature,
                             const base::Value& onc_value,
                             bool* error) {
  switch (onc_value.type()) {
    case base::Value::Type::DICTIONARY: {
      if (signature.onc_type != base::Value::Type::DICTIONARY) {
        *error = true;
        return {};
      }
      return MapObject(signature, onc_value, error);
    }
    case base::Value::Type::LIST: {
      if (signature.onc_type != base::Value::Type::LIST) {
        *error = true;
        return {};
      }
      return MapArray(signature, onc_value, error);
    }
    default: {
      if ((signature.onc_type == base::Value::Type::DICTIONARY) ||
          (signature.onc_type == base::Value::Type::LIST)) {
        *error = true;
        return {};
      }
      return MapPrimitive(signature, onc_value, error);
    }
  }
}

base::Value Mapper::MapObject(const OncValueSignature& signature,
                              const base::Value& onc_object,
                              bool* error) {
  base::Value result(base::Value::Type::DICTIONARY);

  bool found_unknown_field = false;
  MapFields(signature, onc_object, &found_unknown_field, error, &result);
  if (found_unknown_field)
    *error = true;
  return result;
}

base::Value Mapper::MapPrimitive(const OncValueSignature& signature,
                                 const base::Value& onc_primitive,
                                 bool* error) {
  return onc_primitive.Clone();
}

void Mapper::MapFields(const OncValueSignature& object_signature,
                       const base::Value& onc_object,
                       bool* found_unknown_field,
                       bool* nested_error,
                       base::Value* result) {
  DCHECK(onc_object.is_dict());
  for (auto it : onc_object.DictItems()) {
    bool current_field_unknown = false;
    base::Value result_value = MapField(it.first, object_signature, it.second,
                                        &current_field_unknown, nested_error);

    if (current_field_unknown)
      *found_unknown_field = true;
    else if (!result_value.is_none())
      result->SetKey(it.first, std::move(result_value));
    else
      DCHECK(*nested_error);
  }
}

base::Value Mapper::MapField(const std::string& field_name,
                             const OncValueSignature& object_signature,
                             const base::Value& onc_value,
                             bool* found_unknown_field,
                             bool* error) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(object_signature, field_name);

  if (field_signature != nullptr) {
    DCHECK(field_signature->value_signature != nullptr)
        << "Found missing value signature at field '" << field_name << "'.";

    return MapValue(*field_signature->value_signature, onc_value, error);
  }
  DVLOG(1) << "Found unknown field name: '" << field_name << "'";
  *found_unknown_field = true;
  return {};
}

base::Value Mapper::MapArray(const OncValueSignature& array_signature,
                             const base::Value& onc_array,
                             bool* nested_error) {
  DCHECK(array_signature.onc_array_entry_signature != NULL)
      << "Found missing onc_array_entry_signature.";

  base::Value result_array(base::Value::Type::LIST);
  int original_index = 0;
  for (const auto& entry : onc_array.GetList()) {
    base::Value result_entry =
        MapEntry(original_index, *array_signature.onc_array_entry_signature,
                 entry, nested_error);
    if (!result_entry.is_none())
      result_array.Append(std::move(result_entry));
    else
      DCHECK(*nested_error);
    ++original_index;
  }
  return result_array;
}

base::Value Mapper::MapEntry(int index,
                             const OncValueSignature& signature,
                             const base::Value& onc_value,
                             bool* error) {
  return MapValue(signature, onc_value, error);
}

}  // namespace onc
}  // namespace chromeos

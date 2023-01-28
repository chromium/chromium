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
    case base::Value::Type::DICT: {
      if (signature.onc_type != base::Value::Type::DICT) {
        *error = true;
        return {};
      }
      return base::Value(MapObject(signature, onc_value.GetDict(), error));
    }
    case base::Value::Type::LIST: {
      if (signature.onc_type != base::Value::Type::LIST) {
        *error = true;
        return {};
      }
      return base::Value(MapArray(signature, onc_value.GetList(), error));
    }
    default: {
      if ((signature.onc_type == base::Value::Type::DICT) ||
          (signature.onc_type == base::Value::Type::LIST)) {
        *error = true;
        return {};
      }
      return MapPrimitive(signature, onc_value, error);
    }
  }
}

base::Value::Dict Mapper::MapObject(const OncValueSignature& signature,
                                    const base::Value::Dict& onc_object,
                                    bool* error) {
  base::Value::Dict result;

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
                       const base::Value::Dict& onc_object,
                       bool* found_unknown_field,
                       bool* nested_error,
                       base::Value::Dict* result) {
  for (const auto [field_name, onc_value] : onc_object) {
    bool current_field_unknown = false;
    base::Value result_value = MapField(field_name, object_signature, onc_value,
                                        &current_field_unknown, nested_error);

    if (current_field_unknown)
      *found_unknown_field = true;
    else if (!result_value.is_none())
      result->Set(field_name, std::move(result_value));
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

base::Value::List Mapper::MapArray(const OncValueSignature& array_signature,
                                   const base::Value::List& onc_array,
                                   bool* nested_error) {
  DCHECK(array_signature.onc_array_entry_signature != nullptr)
      << "Found missing onc_array_entry_signature.";

  base::Value::List result_array;
  int original_index = 0;
  for (const auto& entry : onc_array) {
    bool error = false;
    base::Value result_entry =
        MapEntry(original_index, *array_signature.onc_array_entry_signature,
                 entry, &error);
    if (!error) {
      result_array.Append(std::move(result_entry));
    }

    *nested_error |= error;
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

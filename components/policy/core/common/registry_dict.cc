// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/registry_dict.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/schema.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "base/win/win_util.h"

using base::win::RegistryKeyIterator;
using base::win::RegistryValueIterator;
#endif  // BUILDFLAG(IS_WIN)

namespace policy {

namespace {

// Validates that a key is numerical. Used for lists below.
bool IsKeyNumerical(const std::string& key) {
  int temp = 0;
  return base::StringToInt(key, &temp);
}

}  // namespace

std::optional<base::Value> ConvertRegistryValue(const base::Value& value,
                                                const Schema& schema) {
  if (!schema.valid()) {
    return value.Clone();
  }

  // If the type is good already, go with it.
  if (value.type() == schema.type()) {
    // Recurse for complex types.
    if (value.is_dict()) {
      base::Value::Dict result;
      for (auto entry : value.GetDict()) {
        std::optional<base::Value> converted =
            ConvertRegistryValue(entry.second, schema.GetProperty(entry.first));
        if (converted.has_value()) {
          result.Set(entry.first, std::move(converted.value()));
        }
      }
      return base::Value(std::move(result));
    } else if (value.is_list()) {
      base::Value::List result;
      for (const auto& entry : value.GetList()) {
        std::optional<base::Value> converted =
            ConvertRegistryValue(entry, schema.GetItems());
        if (converted.has_value()) {
          result.Append(std::move(converted.value()));
        }
      }
      return base::Value(std::move(result));
    }
    return value.Clone();
  }

  // Else, do some conversions to map windows registry data types to JSON types.
  int int_value = 0;
  switch (schema.type()) {
    case base::Value::Type::NONE: {
      return base::Value();
    }
    case base::Value::Type::BOOLEAN: {
      // Accept booleans encoded as either string or integer.
      if (value.is_int())
        return base::Value(value.GetInt() != 0);
      if (value.is_string() &&
          base::StringToInt(value.GetString(), &int_value)) {
        return base::Value(int_value != 0);
      }
      break;
    }
    case base::Value::Type::INTEGER: {
      // Integers may be string-encoded.
      if (value.is_string() &&
          base::StringToInt(value.GetString(), &int_value)) {
        return base::Value(int_value);
      }
      break;
    }
    case base::Value::Type::DOUBLE: {
      // Doubles may be string-encoded or integer-encoded.
      if (value.is_double() || value.is_int())
        return base::Value(value.GetDouble());
      double double_value = 0;
      if (value.is_string() &&
          base::StringToDouble(value.GetString(), &double_value)) {
        return base::Value(double_value);
      }
      break;
    }
    case base::Value::Type::LIST: {
      // Lists are encoded as subkeys with numbered value in the registry
      // (non-numerical keys are ignored).
      if (value.is_dict()) {
        base::Value::List result;
        for (auto it : value.GetDict()) {
          if (!IsKeyNumerical(it.first)) {
            continue;
          }
          std::optional<base::Value> converted =
              ConvertRegistryValue(it.second, schema.GetItems());
          if (converted.has_value()) {
            result.Append(std::move(converted.value()));
          }
        }
        return base::Value(std::move(result));
      }
      // Fall through in order to accept lists encoded as JSON strings.
      [[fallthrough]];
    }
    case base::Value::Type::DICT: {
      // Dictionaries may be encoded as JSON strings.
      if (value.is_string()) {
        std::optional<base::Value> result = base::JSONReader::Read(
            value.GetString(),
            base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
        if (result.has_value() && result.value().type() == schema.type()) {
          return std::move(result.value());
        }
      }
      break;
    }
    case base::Value::Type::STRING:
    case base::Value::Type::BINARY:
      // No conversion possible.
      break;
  }

  LOG(WARNING) << "Failed to convert " << value.type() << " to "
               << schema.type();
  return std::nullopt;
}

bool CaseInsensitiveStringCompare::operator()(const std::string& a,
                                              const std::string& b) const {
  return base::CompareCaseInsensitiveASCII(a, b) < 0;
}

RegistryDict::RegistryDict() = default;

RegistryDict::~RegistryDict() {
  ClearKeys();
  ClearValues();
}

RegistryDict* RegistryDict::GetKey(const std::string& name) {
  auto entry = keys_.find(name);
  return entry != keys_.end() ? entry->second.get() : nullptr;
}

const RegistryDict* RegistryDict::GetKey(const std::string& name) const {
  auto entry = keys_.find(name);
  return entry != keys_.end() ? entry->second.get() : nullptr;
}

void RegistryDict::SetKey(const std::string& name,
                          std::unique_ptr<RegistryDict> dict) {
  if (!dict) {
    RemoveKey(name);
    return;
  }

  keys_[name] = std::move(dict);
}

std::unique_ptr<RegistryDict> RegistryDict::RemoveKey(const std::string& name) {
  std::unique_ptr<RegistryDict> result;
  auto entry = keys_.find(name);
  if (entry != keys_.end()) {
    result = std::move(entry->second);
    keys_.erase(entry);
  }
  return result;
}

void RegistryDict::ClearKeys() {
  keys_.clear();
}

base::Value* RegistryDict::GetValue(const std::string& name) {
  auto entry = values_.find(name);
  return entry != values_.end() ? &entry->second : nullptr;
}

const base::Value* RegistryDict::GetValue(const std::string& name) const {
  auto entry = values_.find(name);
  return entry != values_.end() ? &entry->second : nullptr;
}

void RegistryDict::SetValue(const std::string& name, base::Value&& dict) {
  values_[name] = std::move(dict);
}

std::optional<base::Value> RegistryDict::RemoveValue(const std::string& name) {
  std::optional<base::Value> result;
  auto entry = values_.find(name);
  if (entry != values_.end()) {
    result = std::move(entry->second);
    values_.erase(entry);
  }
  return result;
}

void RegistryDict::ClearValues() {
  values_.clear();
}

void RegistryDict::Merge(const RegistryDict& other) {
  for (auto entry(other.keys_.begin()); entry != other.keys_.end(); ++entry) {
    std::unique_ptr<RegistryDict>& subdict = keys_[entry->first];
    if (!subdict)
      subdict = std::make_unique<RegistryDict>();
    subdict->Merge(*entry->second);
  }

  for (auto entry(other.values_.begin()); entry != other.values_.end();
       ++entry) {
    SetValue(entry->first, entry->second.Clone());
  }
}

void RegistryDict::Swap(RegistryDict* other) {
  keys_.swap(other->keys_);
  values_.swap(other->values_);
}

#if BUILDFLAG(IS_WIN)
void RegistryDict::ReadRegistry(HKEY hive, const std::wstring& root) {
  ClearKeys();
  ClearValues();

  // First, read all the values of the key.
  for (RegistryValueIterator it(hive, root.c_str()); it.Valid(); ++it) {
    const std::string name = base::WideToUTF8(it.Name());
    switch (it.Type()) {
      case REG_EXPAND_SZ:
        if (auto expanded_path = base::win::ExpandEnvironmentVariables(
                base::wcstring_view{it.Value(), wcslen(it.Value())})) {
          SetValue(name, base::Value(base::WideToUTF8(*expanded_path)));
          continue;
        }
        [[fallthrough]];
      case REG_SZ:
        SetValue(name, base::Value(base::WideToUTF8(it.Value())));
        continue;
      case REG_DWORD_LITTLE_ENDIAN:
      case REG_DWORD_BIG_ENDIAN:
        if (it.ValueSize() == sizeof(DWORD)) {
          auto value =
              // TODO(crbug.com/40284755): it.Value() should return a
              // wcstring_view which will be usable as a span directly. The
              // ValueSize() here is the number of non-NUL *bytes* in the
              // Value() string, so we cast the Value() to bytes which is what
              // we want in the end anyway.
              UNSAFE_TODO(
                  base::span(reinterpret_cast<const uint8_t*>(it.Value()),
                             it.ValueSize()))
                  .first<sizeof(DWORD)>();
          DWORD dword_value = it.Type() == REG_DWORD_BIG_ENDIAN
                                  ? base::U32FromBigEndian(value)
                                  : base::U32FromLittleEndian(value);
          SetValue(name, base::Value(static_cast<int>(dword_value)));
          continue;
        }
        [[fallthrough]];
      case REG_NONE:
      case REG_LINK:
      case REG_MULTI_SZ:
      case REG_RESOURCE_LIST:
      case REG_FULL_RESOURCE_DESCRIPTOR:
      case REG_RESOURCE_REQUIREMENTS_LIST:
      case REG_QWORD_LITTLE_ENDIAN:
        // Unsupported type, message gets logged below.
        break;
    }

    LOG(WARNING) << "Failed to read hive " << hive << " at " << root << "\\"
                 << name << " type " << it.Type();
  }

  // Recurse for all subkeys.
  for (RegistryKeyIterator it(hive, root.c_str()); it.Valid(); ++it) {
    std::string name(base::WideToUTF8(it.Name()));
    std::unique_ptr<RegistryDict> subdict(new RegistryDict());
    subdict->ReadRegistry(hive, root + L"\\" + it.Name());
    SetKey(name, std::move(subdict));
  }
}

std::optional<base::Value> RegistryDict::ConvertToJSON(
    const Schema& schema) const {
  base::Value::Type type =
      schema.valid() ? schema.type() : base::Value::Type::DICT;
  switch (type) {
    case base::Value::Type::DICT: {
      base::Value::Dict result;
      for (RegistryDict::ValueMap::const_iterator entry(values_.begin());
           entry != values_.end(); ++entry) {
        SchemaList matching_schemas =
            schema.valid() ? schema.GetMatchingProperties(entry->first)
                           : SchemaList();
        // Always try the empty schema if no other schemas exist.
        if (matching_schemas.empty())
          matching_schemas.push_back(Schema());
        for (const Schema& subschema : matching_schemas) {
          std::optional<base::Value> converted =
              ConvertRegistryValue(entry->second, subschema);
          if (converted.has_value()) {
            result.Set(entry->first, std::move(converted.value()));
            break;
          }
        }
      }
      for (RegistryDict::KeyMap::const_iterator entry(keys_.begin());
           entry != keys_.end(); ++entry) {
        SchemaList matching_schemas =
            schema.valid() ? schema.GetMatchingProperties(entry->first)
                           : SchemaList();
        // Always try the empty schema if no other schemas exist.
        if (matching_schemas.empty())
          matching_schemas.push_back(Schema());
        for (const Schema& subschema : matching_schemas) {
          std::optional<base::Value> converted =
              entry->second->ConvertToJSON(subschema);
          if (converted) {
            result.Set(entry->first, std::move(*converted));
            break;
          }
        }
      }
      return base::Value(std::move(result));
    }
    case base::Value::Type::LIST: {
      base::Value::List result;
      Schema item_schema = schema.valid() ? schema.GetItems() : Schema();
      for (RegistryDict::KeyMap::const_iterator entry(keys_.begin());
           entry != keys_.end(); ++entry) {
        if (!IsKeyNumerical(entry->first))
          continue;
        std::optional<base::Value> converted =
            entry->second->ConvertToJSON(item_schema);
        if (converted)
          result.Append(std::move(*converted));
      }
      for (RegistryDict::ValueMap::const_iterator entry(values_.begin());
           entry != values_.end(); ++entry) {
        if (!IsKeyNumerical(entry->first))
          continue;
        std::optional<base::Value> converted =
            ConvertRegistryValue(entry->second, item_schema);
        if (converted.has_value())
          result.Append(std::move(*converted));
      }
      return base::Value(std::move(result));
    }
    default:
      LOG(WARNING) << "Can't convert registry key to schema type " << type;
  }

  return std::nullopt;
}
#endif  // #if BUILDFLAG(IS_WIN)
}  // namespace policy

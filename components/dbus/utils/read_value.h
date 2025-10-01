// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_READ_VALUE_H_
#define COMPONENTS_DBUS_UTILS_READ_VALUE_H_

#include <utility>

#include "base/files/scoped_file.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/variant.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace dbus_utils {

// Deserializes a D-Bus type T from `reader`. Returns std::nullopt if the value
// could not be read. `reader` is advanced on success or failure.
template <typename T>
  requires IsSupportedDBusType<T>
std::optional<T> ReadValue(dbus::MessageReader& reader);

namespace internal {

template <typename T>
  requires IsSupportedDBusType<T>
std::optional<std::vector<T>> ReadArray(dbus::MessageReader& reader) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    return std::nullopt;
  }
  std::vector<T> result;
  while (array_reader.HasMoreData()) {
    auto element = ReadValue<T>(array_reader);
    if (!element) {
      return std::nullopt;
    }
    result.emplace_back(std::move(*element));
  }
  return result;
}

template <typename K, typename V>
  requires IsSupportedDBusType<K> && IsSupportedDBusType<V>
std::optional<std::map<K, V>> ReadMap(dbus::MessageReader& reader) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    return std::nullopt;
  }
  std::map<K, V> map;
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader)) {
      return std::nullopt;
    }
    auto key = ReadValue<K>(dict_entry_reader);
    if (!key) {
      return std::nullopt;
    }
    auto value = ReadValue<V>(dict_entry_reader);
    if (!value) {
      return std::nullopt;
    }
    map.emplace(std::move(*key), std::move(*value));
  }
  return map;
}

template <typename T>
  requires IsSupportedStruct<T>::value
std::optional<T> ReadStruct(dbus::MessageReader& reader) {
  dbus::MessageReader struct_reader(nullptr);
  if (!reader.PopStruct(&struct_reader)) {
    return std::nullopt;
  }
  T s;
  const bool success = std::apply(
      [&](auto&... members) {
        auto read_and_assign = [&](auto& member) {
          using MemberType = std::remove_cvref_t<decltype(member)>;
          if (auto value = ReadValue<MemberType>(struct_reader)) {
            member = std::move(*value);
            return true;
          }
          return false;
        };
        return (read_and_assign(members) && ...);
      },
      s);
  if (!success || struct_reader.HasMoreData()) {
    return std::nullopt;
  }
  return s;
}

}  // namespace internal

template <typename T>
  requires IsSupportedDBusType<T>
std::optional<T> ReadValue(dbus::MessageReader& reader) {
  auto read_primitive =
      [&](bool (dbus::MessageReader::*pop)(T*)) -> std::optional<T> {
    T v;
    if (!(reader.*pop)(&v)) {
      return std::nullopt;
    }
    return v;
  };

  if constexpr (std::is_same_v<T, int16_t>) {
    return read_primitive(&dbus::MessageReader::PopInt16);
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    return read_primitive(&dbus::MessageReader::PopUint16);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return read_primitive(&dbus::MessageReader::PopInt32);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return read_primitive(&dbus::MessageReader::PopUint32);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return read_primitive(&dbus::MessageReader::PopInt64);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    return read_primitive(&dbus::MessageReader::PopUint64);
  } else if constexpr (std::is_same_v<T, bool>) {
    return read_primitive(&dbus::MessageReader::PopBool);
  } else if constexpr (std::is_same_v<T, double>) {
    return read_primitive(&dbus::MessageReader::PopDouble);
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    return read_primitive(&dbus::MessageReader::PopByte);
  } else if constexpr (std::is_same_v<T, std::string>) {
    return read_primitive(&dbus::MessageReader::PopString);
  } else if constexpr (std::is_same_v<T, dbus::ObjectPath>) {
    return read_primitive(&dbus::MessageReader::PopObjectPath);
  } else if constexpr (std::is_same_v<T, base::ScopedFD>) {
    return read_primitive(&dbus::MessageReader::PopFileDescriptor);
  } else if constexpr (internal::IsSupportedArray<T>::value) {
    return internal::ReadArray<typename T::value_type>(reader);
  } else if constexpr (internal::IsSupportedMap<T>::value) {
    return internal::ReadMap<typename T::key_type, typename T::mapped_type>(
        reader);
  } else if constexpr (internal::IsSupportedStruct<T>::value) {
    return internal::ReadStruct<T>(reader);
  } else if constexpr (std::is_same_v<T, Variant>) {
    Variant variant;
    if (!variant.Read(reader)) {
      return std::nullopt;
    }
    return std::move(variant);
  } else {
    static_assert(false, "Unsupported type for D-Bus reading");
  }
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_READ_VALUE_H_

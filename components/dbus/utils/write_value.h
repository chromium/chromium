// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_WRITE_VALUE_H_
#define COMPONENTS_DBUS_UTILS_WRITE_VALUE_H_

#include <utility>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/variant.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace dbus_utils {

// Writes a D-Bus `value` to `writer`. `value` must not be (or contain) an empty
// Variant.
template <typename T>
  requires IsSupportedDBusType<T>
void WriteValue(dbus::MessageWriter& writer, const T& value);

namespace internal {

template <typename T>
  requires IsSupportedArray<T>::value
void WriteArray(dbus::MessageWriter& writer, const T& container) {
  using ElementType = typename T::value_type;
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray(GetDBusTypeSignature<ElementType>(), &array_writer);
  for (const auto& element : container) {
    WriteValue(array_writer, element);
  }
  writer.CloseContainer(&array_writer);
}

template <typename T>
  requires IsSupportedMap<T>::value
void WriteMap(dbus::MessageWriter& writer, const T& map_container) {
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray(UNSAFE_BUFFERS(GetDBusTypeSignature<T>() + 1),
                   &array_writer);

  for (const auto& pair : map_container) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    WriteValue(dict_entry_writer, pair.first);
    WriteValue(dict_entry_writer, pair.second);
    array_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&array_writer);
}

template <typename T>
  requires IsSupportedStruct<T>::value
void WriteStruct(dbus::MessageWriter& writer, const T& structure) {
  dbus::MessageWriter struct_writer(nullptr);
  writer.OpenStruct(&struct_writer);
  auto write_members = [&](const auto&... members) {
    ((WriteValue(struct_writer, members)), ...);
  };
  std::apply(write_members, structure);
  writer.CloseContainer(&struct_writer);
}

}  // namespace internal

template <typename T>
  requires IsSupportedDBusType<T>
void WriteValue(dbus::MessageWriter& writer, const T& value) {
  if constexpr (std::is_same_v<T, int16_t>) {
    writer.AppendInt16(value);
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    writer.AppendUint16(value);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    writer.AppendInt32(value);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    writer.AppendUint32(value);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    writer.AppendInt64(value);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    writer.AppendUint64(value);
  } else if constexpr (std::is_same_v<T, bool>) {
    writer.AppendBool(value);
  } else if constexpr (std::is_same_v<T, double>) {
    writer.AppendDouble(value);
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    writer.AppendByte(value);
  } else if constexpr (std::is_same_v<T, std::string>) {
    writer.AppendString(value);
  } else if constexpr (std::is_same_v<T, dbus::ObjectPath>) {
    writer.AppendObjectPath(value);
  } else if constexpr (std::is_same_v<T, base::ScopedFD>) {
    writer.AppendFileDescriptor(value.get());
  } else if constexpr (internal::IsSupportedArray<T>::value) {
    internal::WriteArray(writer, value);
  } else if constexpr (internal::IsSupportedMap<T>::value) {
    internal::WriteMap(writer, value);
  } else if constexpr (internal::IsSupportedStruct<T>::value) {
    internal::WriteStruct(writer, value);
  } else if constexpr (std::is_same_v<T, Variant>) {
    value.Write(writer);
  } else {
    static_assert(false, "Unsupported type for D-Bus writing");
  }
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_WRITE_VALUE_H_

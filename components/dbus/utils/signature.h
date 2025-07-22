// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_SIGNATURE_H_
#define COMPONENTS_DBUS_UTILS_SIGNATURE_H_

#include <dbus/dbus.h>

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "components/dbus/utils/types.h"
#include "dbus/object_path.h"

namespace dbus_utils {

class Variant;

namespace internal {

template <typename T>
struct DBusSignature {
  static_assert(false, "Unsupported type for D-Bus");
};

#define DEFINE_SIMPLE_SIGNATURE(type, signature)             \
  template <>                                                \
  struct DBusSignature<type> {                               \
    static constexpr auto kValue = std::to_array(signature); \
  };

DEFINE_SIMPLE_SIGNATURE(int16_t, DBUS_TYPE_INT16_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(uint16_t, DBUS_TYPE_UINT16_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(int32_t, DBUS_TYPE_INT32_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(uint32_t, DBUS_TYPE_UINT32_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(int64_t, DBUS_TYPE_INT64_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(uint64_t, DBUS_TYPE_UINT64_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(bool, DBUS_TYPE_BOOLEAN_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(double, DBUS_TYPE_DOUBLE_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(uint8_t, DBUS_TYPE_BYTE_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(std::string, DBUS_TYPE_STRING_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(dbus::ObjectPath, DBUS_TYPE_OBJECT_PATH_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(Variant, DBUS_TYPE_VARIANT_AS_STRING)
DEFINE_SIMPLE_SIGNATURE(base::ScopedFD, DBUS_TYPE_UNIX_FD_AS_STRING)

#undef DEFINE_SIMPLE_SIGNATURE

// StrJoin takes char[N]s and std::array<char, M>s, each of which is null
// terminated string, and returns the std::array<char, ...> which is the
// result of concat, also null terminated.
template <typename T>
struct IsStrJoinable : std::false_type {};
template <size_t N>
struct IsStrJoinable<char[N]> : std::true_type {};
template <size_t N>
struct IsStrJoinable<std::array<char, N>> : std::true_type {};

template <typename... Ts>
constexpr auto StrJoin(Ts&&... args) {
  static_assert((IsStrJoinable<std::remove_cvref_t<Ts>>::value && ...),
                "All types passed to StrJoin must be either char[N] or "
                "std::array<char, N>.");

  constexpr size_t kSize =
      (std::size(std::remove_cvref_t<Ts>{}) + ...) - sizeof...(args) + 1;
  std::array<char, kSize> result = {};
  size_t i = 0;
  for (auto span : {base::span<const char>(args)...}) {
    for (size_t j = 0; j < span.size(); ++j) {
      result[i + j] = span[j];
    }
    i += span.size() - 1;  // Excluding trailing '\0'.
  }
  return result;
}

template <>
constexpr auto StrJoin() {
  return std::array<char, 1>{0};
}

template <typename T>
  requires IsSupportedArray<T>::value
struct DBusSignature<T> {
  static constexpr auto kValue =
      StrJoin(DBUS_TYPE_ARRAY_AS_STRING,
              DBusSignature<typename T::value_type>::kValue);
};

template <typename T>
  requires IsSupportedMap<T>::value
struct DBusSignature<T> {
  using KeyType = typename T::key_type;
  using ValueType = typename T::mapped_type;
  static constexpr auto kValue = StrJoin(DBUS_TYPE_ARRAY_AS_STRING,
                                         DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING,
                                         DBusSignature<KeyType>::kValue,
                                         DBusSignature<ValueType>::kValue,
                                         DBUS_DICT_ENTRY_END_CHAR_AS_STRING);
};

template <typename... Ts>
  requires IsSupportedStruct<std::tuple<Ts...>>::value
struct DBusSignature<std::tuple<Ts...>> {
  static constexpr auto kValue = StrJoin(DBUS_STRUCT_BEGIN_CHAR_AS_STRING,
                                         StrJoin(DBusSignature<Ts>::kValue...),
                                         DBUS_STRUCT_END_CHAR_AS_STRING);
};

template <typename T>
constexpr const char* GetDBusTypeSignature() {
  return DBusSignature<T>::kValue.data();
}

template <size_t N>
struct StringLiteral {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr StringLiteral(const char (&str)[N]) {
    std::copy_n(str, N, value.data());
  }

  std::array<char, N> value;
};

}  // namespace internal
}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_SIGNATURE_H_

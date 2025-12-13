// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_TYPES_H_
#define COMPONENTS_DBUS_UTILS_TYPES_H_

#include <map>
#include <tuple>
#include <type_traits>
#include <vector>

#include "base/files/scoped_file.h"
#include "dbus/object_path.h"

namespace dbus_utils {

class Variant;

namespace internal {

// This helper is required for nested types (eg. std::vector<std::vector<T>>)
// because concepts cannot be forward declared.
template <typename T>
struct IsSupportedDBusTypeHelper : std::false_type {};

template <typename T>
concept IsSupportedPrimitive =
    std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
    std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t> ||
    std::is_same_v<T, bool> || std::is_same_v<T, double> ||
    std::is_same_v<T, uint8_t> || std::is_same_v<T, std::string> ||
    std::is_same_v<T, dbus::ObjectPath> || std::is_same_v<T, base::ScopedFD>;

template <typename T>
struct IsSupportedArray : std::false_type {};

template <typename E, typename A>
  requires IsSupportedDBusTypeHelper<E>::value
struct IsSupportedArray<std::vector<E, A>> : std::true_type {};

template <typename T>
struct IsSupportedMap : std::false_type {};

template <typename K, typename V, typename C, typename A>
  requires IsSupportedDBusTypeHelper<K>::value &&
           IsSupportedDBusTypeHelper<V>::value
struct IsSupportedMap<std::map<K, V, C, A>> : std::true_type {};

template <typename T>
struct IsSupportedStruct : std::false_type {};

template <typename... Ts>
  requires(IsSupportedDBusTypeHelper<Ts>::value && ...)
struct IsSupportedStruct<std::tuple<Ts...>> : std::true_type {};

template <typename T>
  requires IsSupportedPrimitive<T> || IsSupportedArray<T>::value ||
           IsSupportedMap<T>::value || IsSupportedStruct<T>::value ||
           std::is_same_v<T, Variant>
struct IsSupportedDBusTypeHelper<T> : std::true_type {};

}  // namespace internal

// A concept that indicates if T is a C++ type corresponding to a valid D-Bus
// type. All D-Bus types are supported, including primitives, arrays, maps, and
// structs.
template <typename T>
concept IsSupportedDBusType = internal::IsSupportedDBusTypeHelper<T>::value;

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_TYPES_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/types.h"

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_utils {
namespace {

TEST(DBusTypesTraitTest, IsSupportedDBusType) {
  // Primitives
  static_assert(IsSupportedDBusType<bool>);
  static_assert(IsSupportedDBusType<uint8_t>);
  static_assert(IsSupportedDBusType<int16_t>);
  static_assert(IsSupportedDBusType<uint16_t>);
  static_assert(IsSupportedDBusType<int32_t>);
  static_assert(IsSupportedDBusType<uint32_t>);
  static_assert(IsSupportedDBusType<int64_t>);
  static_assert(IsSupportedDBusType<uint64_t>);
  static_assert(IsSupportedDBusType<double>);
  static_assert(IsSupportedDBusType<std::string>);
  static_assert(IsSupportedDBusType<dbus::ObjectPath>);
  static_assert(IsSupportedDBusType<base::ScopedFD>);
  static_assert(IsSupportedDBusType<Variant>);

  // Invalid primitives
  static_assert(!IsSupportedDBusType<char>);
  static_assert(!IsSupportedDBusType<float>);
  static_assert(!IsSupportedDBusType<void*>);
  static_assert(!IsSupportedDBusType<const char*>);

  // Simple Containers
  static_assert(IsSupportedDBusType<std::vector<int32_t>>);
  static_assert(IsSupportedDBusType<std::vector<std::string>>);
  static_assert(IsSupportedDBusType<std::map<std::string, uint64_t>>);

  // Simple Tuples
  static_assert(IsSupportedDBusType<std::tuple<int32_t, std::string>>);
  static_assert(IsSupportedDBusType<std::tuple<>>);
  static_assert(IsSupportedDBusType<std::tuple<bool, std::vector<int32_t>>>);

  // Vector
  static_assert(IsSupportedDBusType<std::vector<std::vector<bool>>>);
  static_assert(
      IsSupportedDBusType<std::vector<std::map<std::string, uint32_t>>>);
  static_assert(IsSupportedDBusType<std::vector<std::tuple<int16_t, double>>>);
  static_assert(IsSupportedDBusType<std::vector<Variant>>);

  // Map
  static_assert(IsSupportedDBusType<std::map<int16_t, std::vector<double>>>);
  static_assert(
      IsSupportedDBusType<std::map<std::string, std::map<uint8_t, bool>>>);
  static_assert(IsSupportedDBusType<
                std::map<uint32_t, std::tuple<dbus::ObjectPath, int32_t>>>);
  static_assert(IsSupportedDBusType<std::map<std::string, Variant>>);

  // Tuple
  static_assert(IsSupportedDBusType<std::tuple<std::vector<int32_t>, bool>>);
  static_assert(IsSupportedDBusType<
                std::tuple<std::map<std::string, uint64_t>, int16_t>>);
  static_assert(
      IsSupportedDBusType<std::tuple<std::tuple<bool, double>, int32_t>>);
  static_assert(IsSupportedDBusType<std::tuple<Variant, int32_t>>);

  // Nested Types
  static_assert(IsSupportedDBusType<std::vector<std::vector<std::string>>>);
  static_assert(
      IsSupportedDBusType<std::vector<std::vector<std::vector<int32_t>>>>);
  static_assert(
      IsSupportedDBusType<std::map<std::string, std::vector<Variant>>>);
  static_assert(
      IsSupportedDBusType<std::tuple<
          std::string, std::vector<std::map<int32_t, std::string>>, bool>>);
  static_assert(
      IsSupportedDBusType<std::map<
          std::string, std::tuple<int32_t, std::vector<std::vector<bool>>>>>);

  // Invalid Containers
  static_assert(!IsSupportedDBusType<std::vector<void*>>);
  static_assert(!IsSupportedDBusType<std::map<std::string, void*>>);
  static_assert(!IsSupportedDBusType<std::map<int, void*>>);
  static_assert(!IsSupportedDBusType<std::map<void*, int>>);

  // Invalid Tuples
  static_assert(!IsSupportedDBusType<std::tuple<int32_t, void*>>);
  static_assert(!IsSupportedDBusType<std::tuple<float>>);

  // Invalid Nested Types
  static_assert(!IsSupportedDBusType<std::vector<std::vector<char>>>);
  static_assert(
      !IsSupportedDBusType<std::map<std::string, std::vector<const char*>>>);
  static_assert(!IsSupportedDBusType<std::tuple<std::vector<float>>>);
  static_assert(!IsSupportedDBusType<std::map<bool, std::map<char, bool>>>);
}

}  // namespace
}  // namespace dbus_utils

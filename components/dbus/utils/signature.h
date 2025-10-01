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

// An array of characters that can be used as non-type template parameters.
// Implicitly converts from const char*.
template <size_t N>
struct SignatureLiteral {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr SignatureLiteral(const char (&str)[N]) {
    std::copy_n(str, N, value.data());
  }

  constexpr explicit SignatureLiteral(const std::array<char, N>& arr)
      : value(arr) {}

  std::array<char, N> value;
};

namespace internal {

template <size_t K, size_t N>
  requires(K <= N)
constexpr SignatureLiteral<N - K> SignatureLiteralSubstring(
    SignatureLiteral<N> literal) {
  std::array<char, N - K> result{};
  std::copy_n(&literal.value[K], N - K, result.data());
  return SignatureLiteral<N - K>(result);
}

template <typename T>
struct DBusSignature {
  static_assert(false, "Unsupported type for D-Bus");
};

#define DEFINE_SIMPLE_SIGNATURE(type, signature)             \
  template <>                                                \
  struct DBusSignature<type> {                               \
    static constexpr auto kValue = std::to_array(signature); \
  }

DEFINE_SIMPLE_SIGNATURE(int16_t, DBUS_TYPE_INT16_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(uint16_t, DBUS_TYPE_UINT16_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(int32_t, DBUS_TYPE_INT32_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(uint32_t, DBUS_TYPE_UINT32_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(int64_t, DBUS_TYPE_INT64_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(uint64_t, DBUS_TYPE_UINT64_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(bool, DBUS_TYPE_BOOLEAN_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(double, DBUS_TYPE_DOUBLE_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(uint8_t, DBUS_TYPE_BYTE_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(std::string, DBUS_TYPE_STRING_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(dbus::ObjectPath, DBUS_TYPE_OBJECT_PATH_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(Variant, DBUS_TYPE_VARIANT_AS_STRING);
DEFINE_SIMPLE_SIGNATURE(base::ScopedFD, DBUS_TYPE_UNIX_FD_AS_STRING);

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

template <SignatureLiteral signature>
struct ParseLeadingOneType;

template <SignatureLiteral signature, typename... Ts>
struct ParseLeadingTypeList {
  using Type = std::tuple<Ts...>;
  static constexpr inline auto kRemaining = signature;
};

template <SignatureLiteral signature, typename... Ts>
  requires(signature.value[0] != '\0' &&
           signature.value[0] != DBUS_STRUCT_END_CHAR &&
           signature.value[0] != DBUS_DICT_ENTRY_END_CHAR)
struct ParseLeadingTypeList<signature, Ts...>
    : ParseLeadingTypeList<ParseLeadingOneType<signature>::kRemaining,
                           Ts...,
                           typename ParseLeadingOneType<signature>::Type> {};

#define DEFINE_SIMPLE_SIGNATURE_PARSER(type, sig) \
  template <SignatureLiteral signature>           \
    requires(signature.value[0] == sig)           \
  struct ParseLeadingOneType<signature> {         \
    using Type = type;                            \
    static constexpr inline auto kRemaining =     \
        SignatureLiteralSubstring<1>(signature);  \
  }

DEFINE_SIMPLE_SIGNATURE_PARSER(int16_t, DBUS_TYPE_INT16);
DEFINE_SIMPLE_SIGNATURE_PARSER(uint16_t, DBUS_TYPE_UINT16);
DEFINE_SIMPLE_SIGNATURE_PARSER(int32_t, DBUS_TYPE_INT32);
DEFINE_SIMPLE_SIGNATURE_PARSER(uint32_t, DBUS_TYPE_UINT32);
DEFINE_SIMPLE_SIGNATURE_PARSER(int64_t, DBUS_TYPE_INT64);
DEFINE_SIMPLE_SIGNATURE_PARSER(uint64_t, DBUS_TYPE_UINT64);
DEFINE_SIMPLE_SIGNATURE_PARSER(bool, DBUS_TYPE_BOOLEAN);
DEFINE_SIMPLE_SIGNATURE_PARSER(double, DBUS_TYPE_DOUBLE);
DEFINE_SIMPLE_SIGNATURE_PARSER(uint8_t, DBUS_TYPE_BYTE);
DEFINE_SIMPLE_SIGNATURE_PARSER(std::string, DBUS_TYPE_STRING);
DEFINE_SIMPLE_SIGNATURE_PARSER(dbus::ObjectPath, DBUS_TYPE_OBJECT_PATH);
DEFINE_SIMPLE_SIGNATURE_PARSER(Variant, DBUS_TYPE_VARIANT);
DEFINE_SIMPLE_SIGNATURE_PARSER(base::ScopedFD, DBUS_TYPE_UNIX_FD);

#undef DEFINE_SIMPLE_SIGNATURE_PARSER

template <SignatureLiteral signature>
  requires(signature.value[0] == DBUS_TYPE_ARRAY &&
           signature.value[1] != DBUS_DICT_ENTRY_BEGIN_CHAR)
struct ParseLeadingOneType<signature> {
  using Element = ParseLeadingOneType<SignatureLiteralSubstring<1>(signature)>;
  using Type = std::vector<typename Element::Type>;
  static constexpr inline auto kRemaining = Element::kRemaining;
};

template <SignatureLiteral signature>
  requires(signature.value[0] == DBUS_TYPE_ARRAY &&
           signature.value[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
struct ParseLeadingOneType<signature> {
  using Key = ParseLeadingOneType<SignatureLiteralSubstring<2>(signature)>;
  using Value = ParseLeadingOneType<Key::kRemaining>;
  using Type = std::map<typename Key::Type, typename Value::Type>;
  static_assert(Value::kRemaining.value[0] == DBUS_DICT_ENTRY_END_CHAR);
  static constexpr inline auto kRemaining =
      SignatureLiteralSubstring<1>(Value::kRemaining);
};

template <SignatureLiteral signature>
  requires(signature.value[0] == DBUS_STRUCT_BEGIN_CHAR)
struct ParseLeadingOneType<signature> {
  using Elements =
      ParseLeadingTypeList<SignatureLiteralSubstring<1>(signature)>;
  using Type = typename Elements::Type;
  static_assert(Elements::kRemaining.value[0] == DBUS_STRUCT_END_CHAR);
  static constexpr inline auto kRemaining =
      SignatureLiteralSubstring<1>(Elements::kRemaining);
};

template <SignatureLiteral signature>
struct ParseDBusSignatureInternal {
  using Parsed = ParseLeadingOneType<signature>;
  static_assert(Parsed::kRemaining.value[0] == '\0');
  using Type = typename Parsed::Type;
};

template <SignatureLiteral Signature>
struct ParseDBusSignaturePackInternal {
  using ParsedList = internal::ParseLeadingTypeList<Signature>;
  static_assert(ParsedList::kRemaining.value[0] == '\0');
  using Type = typename ParsedList::Type;
};

// Parses a single D-Bus type signature into the corresponding C++ type.
// The signature must represent exactly one type, e.g. "s", "a{si}", "(is)".
// It is a compile-time error to use this with a signature that represents
// zero or more than one type, or invalid signatures.
template <SignatureLiteral signature>
using ParseDBusSignature = ParseDBusSignatureInternal<signature>::Type;

// Parses a D-Bus type signature representing a sequence of types into the
// corresponding C++ types in a std::tuple. The signature may represent one
// or more types, e.g. "si", "a{si}(is)". This is intended to be used to parse
// method argument lists.
template <SignatureLiteral signature>
using ParseDBusSignaturePack = ParseDBusSignaturePackInternal<signature>::Type;

}  // namespace internal

// Returns the D-Bus signature for T as a null-terminated C string.
template <typename T>
  requires IsSupportedDBusType<T>
constexpr const char* GetDBusTypeSignature() {
  return internal::DBusSignature<T>::kValue.data();
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_SIGNATURE_H_

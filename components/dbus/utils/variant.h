// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_VARIANT_H_
#define COMPONENTS_DBUS_UTILS_VARIANT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace dbus_utils {

class COMPONENT_EXPORT(COMPONENTS_DBUS) Variant {
 public:
  Variant();

  Variant(Variant&&) noexcept;
  Variant& operator=(Variant&&) noexcept;

  Variant(const Variant&) = delete;
  Variant& operator=(const Variant&) = delete;

  ~Variant();

  bool operator==(const Variant& other) const;

  // Create a new Variant that wraps the given `value`, which is consumed.
  // The `Signature` template parameter is a required string literal that
  // must match the D-Bus signature of `value`. This is intended to prevent
  // ambiguity if `value` is a literal like "42", so an explicit type is
  // required.
  template <SignatureLiteral Signature>
  static Variant Wrap(internal::ParseDBusSignature<Signature> value) {
    return WrapImpl(std::move(value));
  }

  // Extract the contained value from this variant. Returns std::nullopt if the
  // variant is empty or if the contained type does not match the requested
  // type. The variant is reset to an empty state after Take().
  template <typename T>
    requires IsSupportedDBusType<T>
  std::optional<T> Take() && {
    auto consumed_state = std::move(state_);
    state_.emplace<std::monostate>();
    std::string signature = std::move(signature_);

    if (signature != GetDBusTypeSignature<T>()) {
      return std::nullopt;
    }

    if constexpr (internal::IsSupportedMap<T>::value) {
      CHECK(std::holds_alternative<Dictionary>(consumed_state));
      using K = typename T::key_type;
      using V = typename T::mapped_type;
      std::map<K, V> result;
      for (auto& pair : std::get<Dictionary>(consumed_state)) {
        auto key_opt = std::move(pair.first).Take<K>();
        auto val_opt = std::move(pair.second).Take<V>();
        if (!key_opt || !val_opt) {
          return std::nullopt;
        }
        result.emplace(std::move(*key_opt), std::move(*val_opt));
      }
      return result;
    } else if constexpr (internal::IsSupportedArray<T>::value) {
      CHECK(std::holds_alternative<Sequence>(consumed_state));
      using E = typename T::value_type;
      std::vector<E> result;
      auto& seq = std::get<Sequence>(consumed_state);
      result.reserve(seq.size());
      for (auto& elem_variant : seq) {
        auto elem_opt = std::move(elem_variant).Take<E>();
        if (!elem_opt) {
          return std::nullopt;
        }
        result.push_back(std::move(*elem_opt));
      }
      return result;
    } else if constexpr (internal::IsSupportedStruct<T>::value) {
      CHECK(std::holds_alternative<Sequence>(consumed_state));
      T result_struct{};
      auto& seq = std::get<Sequence>(consumed_state);
      if (std::tuple_size_v<T> != seq.size()) {
        return std::nullopt;
      }
      size_t i = 0;
      bool success = std::apply(
          [&](auto&... members) {
            return ([&](auto& member) {
              auto opt =
                  std::move(seq[i++]).Take<std::decay_t<decltype(member)>>();
              if (!opt) {
                return false;
              }
              member = std::move(*opt);
              return true;
            }(members) &&
                    ...);
          },
          result_struct);
      return success ? std::optional<T>(std::move(result_struct))
                     : std::nullopt;
    } else if constexpr (std::is_same_v<T, Variant>) {
      CHECK(std::holds_alternative<NestedVariant>(consumed_state));
      auto ptr = std::move(std::get<NestedVariant>(consumed_state));
      return ptr ? std::optional<T>(std::move(*ptr)) : std::nullopt;
    } else {
      CHECK(std::holds_alternative<T>(consumed_state));
      return std::move(std::get<T>(consumed_state));
    }
  }

  // Writes `this` to `writer`. The variant must not be empty.
  void Write(dbus::MessageWriter& writer) const;

  // Reads a variant from `reader` into `this`, and returns true on success.
  bool Read(dbus::MessageReader& reader);

  const std::string& signature() const { return signature_; }

 private:
  using Dictionary = std::vector<std::pair<Variant, Variant>>;
  // Either an array or a struct.
  using Sequence = std::vector<Variant>;
  using NestedVariant = std::unique_ptr<Variant>;

  // `T` has already been validated by `Wrap`.
  template <typename T>
  static Variant WrapImpl(T&& value) {
    Variant v;
    using DecayedT = std::decay_t<T>;
    v.signature_ = GetDBusTypeSignature<DecayedT>();

    if constexpr (std::is_same_v<DecayedT, Variant>) {
      v.state_.emplace<NestedVariant>(
          std::make_unique<Variant>(std::move(value)));
    } else if constexpr (internal::IsSupportedMap<DecayedT>::value) {
      Dictionary dict;
      dict.reserve(value.size());
      for (auto& pair : value) {
        dict.emplace_back(Variant::WrapImpl(std::move(pair.first)),
                          Variant::WrapImpl(std::move(pair.second)));
      }
      v.state_.emplace<Dictionary>(std::move(dict));
    } else if constexpr (internal::IsSupportedStruct<DecayedT>::value) {
      Sequence seq;
      seq.reserve(std::tuple_size_v<DecayedT>);
      auto member_wrapper = [&](auto&... members) {
        (seq.push_back(Variant::WrapImpl(std::move(members))), ...);
      };
      std::apply(member_wrapper, value);
      v.state_.emplace<Sequence>(std::move(seq));
    } else if constexpr (internal::IsSupportedArray<DecayedT>::value) {
      Sequence seq;
      seq.reserve(value.size());
      for (auto& elem : value) {
        seq.push_back(Variant::WrapImpl(std::move(elem)));
      }
      v.state_.emplace<Sequence>(std::move(seq));
    } else {
      v.state_.emplace<DecayedT>(std::move(value));
    }
    return v;
  }

  // Similar to `Write` and `Read`, but for the contained type.
  void WriteContent(dbus::MessageWriter& writer) const;
  bool ReadContent(dbus::MessageReader& reader);

  // An explicit signature is required for empty arrays or dictionaries.
  std::string signature_;

  std::variant<
      // Default-constructed or moved-from.
      std::monostate,
      // Primitive types.
      bool,
      uint8_t,
      int16_t,
      uint16_t,
      int32_t,
      uint32_t,
      int64_t,
      uint64_t,
      double,
      std::string,
      dbus::ObjectPath,
      base::ScopedFD,
      // Container types.
      Dictionary,
      Sequence,
      NestedVariant>
      state_;
};

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_VARIANT_H_

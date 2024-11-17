// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_DATA_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_DATA_H_

#include <concepts>
#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/is_instantiation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education::internal {

// Base already has an is_raw_ref, but there is no is_raw_ptr, so implement it
// here.
template <typename T>
struct is_raw_ptr : std::false_type {};
template <typename T, base::RawPtrTraits Traits>
struct is_raw_ptr<raw_ptr<T, Traits>> : std::true_type {};

// A value is cacheable if:
//  - it is moveable
//  - it is default-constructable
//  - it is not a pointer or pointer-like type
//    (pointers can lead to dangling references)
//
// To hold polymorphic or non-moveable objects, use std::unique_ptr.
template <typename T>
concept PreconditionCacheable =
    std::movable<T> && std::default_initializable<T> && !std::is_pointer_v<T> &&
    !is_raw_ptr<T>::value && !base::internal::is_raw_ref_v<T>;

template <typename T>
  requires PreconditionCacheable<T>
class TypedPreconditionData;

// Preconditions can cache and retrieve data; this ensures that computations
// aren't done multiple times, and that computed data can be retrieved from
// preconditions before they are removed from a queue and discarded.
//
// This is a base class that ensures proper polymorphism and cleanup in the
// typed implementation below.
class PreconditionData {
 public:
  using Identifier = ui::ElementIdentifier;
  template <typename T>
  using TypedIdentifier = ui::TypedIdentifier<T>;
  using Collection = std::map<Identifier, std::unique_ptr<PreconditionData>>;

  explicit PreconditionData(Identifier identifier) : identifier_(identifier) {}
  PreconditionData(const PreconditionData& other) = delete;
  void operator=(const PreconditionData& other) = delete;
  virtual ~PreconditionData() = default;

  Identifier identifier() const { return identifier_; }

  // Retrieves typed data from a data collection, or null if not found.
  template <typename T>
  static T* Get(Collection& coll, TypedIdentifier<T> id) {
    const auto it = coll.find(id.identifier());
    return it == coll.end() ? nullptr : &it->second->AsTyped(id).data();
  }

 private:
  // Retrieves this object as a typed object. The identifier must match.
  template <typename T>
  TypedPreconditionData<T>& AsTyped(TypedIdentifier<T> id);

  const Identifier identifier_;
};

// This represents typed cached data retrieved from preconditions.
// Use `PreconditionData::AsTyped<>()` to retrieve this version, then `data()`
// to access the data.
//
// Type is enforced by the use of a unique `TypedIdentifier<T>`, which must be
// supplied both to create and retrieve the corresponding typed data. This
// ensures a `PreconditionData` object is never cast to the wrong type of
// `TypedPreconditionData`.
template <typename T>
  requires PreconditionCacheable<T>
class TypedPreconditionData : public PreconditionData {
 public:
  using Type = T;

  template <typename U>
    requires std::same_as<T, U>
  explicit TypedPreconditionData(TypedIdentifier<U> identifier)
      : PreconditionData(identifier.identifier()) {}
  ~TypedPreconditionData() override = default;

  T& data() { return data_; }
  const T& data() const { return data_; }

 private:
  T data_ = T();
};

template <typename T>
TypedPreconditionData<T>& PreconditionData::AsTyped(TypedIdentifier<T> id) {
  CHECK_EQ(id.identifier(), identifier_);
  return *static_cast<TypedPreconditionData<T>*>(this);
}

}  // namespace user_education::internal

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_DATA_H_

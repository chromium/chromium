// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_TYPES_H_
#define COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_TYPES_H_

#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "ui/base/identifier/unique_identifier.h"

namespace user_education {

namespace internal {

// Opaque ID for required messages. Do not use directly.
DECLARE_UNIQUE_IDENTIFIER_TYPE(ProductMessageUniqueId);

static constexpr std::string_view kProductMessageUniqueIdSuffix = "UniqueId";

}  // namespace internal

// An enumeration of all known product message types, in general order of
// priority from lowest to highest.
enum class ProductMessageType {
  // This value is only for filtering and must be first on the list.
  kNone = 0,
  kLowPriorityForTesting,  // IN-TEST
  kAnchoredMessage,
  kLowPriorityIph,
  kHighPriorityIph,
  kLegalOrComplianceNotice,
  kHighPriorityForTesting,  // IN-TEST
  kMaxValue = kHighPriorityForTesting
};

// Used to uniquely identify a message and define its type.
// Opaque, but the type can be retrieved.
// Declare with the macros below.
class ProductMessageKey {
 public:
  consteval ProductMessageKey() = default;
  consteval ProductMessageKey(internal::ProductMessageUniqueId id,
                              ProductMessageType type)
      : id_(id), type_(type) {}

  explicit constexpr operator bool() const { return !!id_; }
  bool operator<(ProductMessageKey other) const;
  friend constexpr bool operator==(ProductMessageKey lhs,
                                   ProductMessageKey rhs) = default;

  std::string GetName() const;
  std::string ToString() const;

  internal::ProductMessageUniqueId id() const { return id_; }
  ProductMessageType type() const { return type_; }

 private:
  internal::ProductMessageUniqueId id_;
  ProductMessageType type_ = ProductMessageType::kNone;
};

// Use these macros to declare keys.

// Place this in a .h file:
#define DECLARE_PRODUCT_MESSAGE_KEY(name, type)                             \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(                                          \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId);  \
  inline constexpr ::user_education::ProductMessageKey name(name##UniqueId, \
                                                            type)

// Place this in a .cc file:
#define DEFINE_PRODUCT_MESSAGE_KEY(name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(        \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId)

// This can be used in tests to avoid name conflicts.
#define DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(name, type)                          \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(                                 \
      __FILE__, __LINE__, ::user_education::internal::ProductMessageUniqueId, \
      name##UniqueId);                                                        \
  static constexpr ::user_education::ProductMessageKey name(name##UniqueId,   \
                                                            type)

// This can be used to scope an identifier to a class; use this in the public
// part of the class definition.
#define DECLARE_CLASS_PRODUCT_MESSAGE_KEY(name, type)                      \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(                                   \
      ::user_education::internal::ProductMessageUniqueId, name##UniqueId); \
  static constexpr ::user_education::ProductMessageKey name {              \
    name##UniqueId, type                                                   \
  }

// Use this in the .cc file to define an identifier scoped to a class, this must
// be paired with the DECLARE macro above.
#define DEFINE_CLASS_PRODUCT_MESSAGE_KEY(Class, Name)            \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(                          \
      Class, ::user_education::internal::ProductMessageUniqueId, \
      Name##UniqueId)

// The status of a message.
enum class ProductMessageStatus {
  // The given message is not in the system. Either it has not been requested,
  // it was blocked, or it was made eligible/shown and then released.
  kNone,
  // The given message is waiting for eligibility.
  kWaiting,
  // The given message is has been given permission to show, but has not yet
  // been shown.
  kReady,
  // The given message is showing, as per ProductMessagingHandle::SetShown().
  kShowing
};

using ProductMessageStatusCallback =
    base::RepeatingCallback<void(ProductMessageKey, ProductMessageStatus)>;

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_TYPES_H_

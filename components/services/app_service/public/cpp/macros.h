// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_

#include <string>

#include "base/macros/concat.h"

namespace apps {

#define SET_OPTIONAL_VALUE(VALUE) \
  if (delta->VALUE.has_value()) { \
    state->VALUE = delta->VALUE;  \
  }

#define SET_ENUM_VALUE(VALUE, DEFAULT_VALUE) \
  if (delta->VALUE != DEFAULT_VALUE) {       \
    state->VALUE = delta->VALUE;             \
  }

#define GET_VALUE(VALUE)           \
  if (delta_ && delta_->VALUE()) { \
    return delta_->VALUE();        \
  }                                \
  if (state_ && state_->VALUE()) { \
    return state_->VALUE();        \
  }                                \
  return nullptr;

#define IS_VALUE_CHANGED(VALUE)       \
  return delta_ && delta_->VALUE() && \
         (!state_ || (delta_->VALUE() != state_->VALUE()));

#define GET_VALUE_WITH_FALLBACK(VALUE, FALLBACK_VALUE) \
  if (delta_ && delta_->VALUE.has_value()) {           \
    return delta_->VALUE.value();                      \
  }                                                    \
  if (state_ && state_->VALUE.has_value()) {           \
    return state_->VALUE.value();                      \
  }                                                    \
  return FALLBACK_VALUE;

#define GET_VALUE_WITH_DEFAULT_VALUE(VALUE, DEFAULT_VALUE) \
  if (delta_ && delta_->VALUE != (DEFAULT_VALUE)) {        \
    return delta_->VALUE;                                  \
  }                                                        \
  if (state_) {                                            \
    return state_->VALUE;                                  \
  }                                                        \
  return DEFAULT_VALUE;

#define IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(VALUE, DEFAULT_VALUE) \
  return delta_ && (delta_->VALUE != DEFAULT_VALUE) &&            \
         (!state_ || (delta_->VALUE != state_->VALUE));

#define GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(VALUE, CHECK, DEFAULT_RETURN) \
  if (delta_ && !delta_->VALUE.CHECK()) {                                     \
    return delta_->VALUE;                                                     \
  }                                                                           \
  if (state_ && !state_->VALUE.CHECK()) {                                     \
    return state_->VALUE;                                                     \
  }                                                                           \
  return DEFAULT_RETURN;

#define IS_VALUE_CHANGED_WITH_CHECK(VALUE, CHECK) \
  return delta_ && !delta_->VALUE.CHECK() &&      \
         (!state_ || (delta_->VALUE != state_->VALUE));

#define RETURN_OPTIONAL_VALUE_CHANGED(VALUE)    \
  return delta_ && delta_->VALUE.has_value() && \
         (!state_ || (delta_->VALUE != state_->VALUE));

#define PRINT_OPTIONAL_BOOL(VALUE) \
  (VALUE.has_value() ? (VALUE.value() ? "true" : "false") : "null")

#define IS_VECTOR_VALUE_EQUAL(VALUE)                \
  if (this->VALUE.size() != other.VALUE.size()) {   \
    return false;                                   \
  }                                                 \
  for (size_t i = 0; i < other.VALUE.size(); i++) { \
    if (this->VALUE[i] != other.VALUE[i]) {         \
      return false;                                 \
    }                                               \
  }

// Macros for enum

#define ARC_COUNT_(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   _14, _15, _16, N, ...)                                      \
  N
#define ARG_COUNT(...)                                                       \
  ARC_COUNT_(0, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, \
             3, 2, 1, 0)

// Go through all items in enum to generate code for each element.
#define DOARG1(FUNC, CLASSNAME, ELEM) FUNC(CLASSNAME, ELEM)
#define DOARG2(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG1(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG3(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG2(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG4(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG3(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG5(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG4(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG6(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG5(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG7(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG6(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG8(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG7(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG9(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG8(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG10(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG9(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG11(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG10(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG12(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG11(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG13(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG12(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG14(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG13(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG15(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG14(FUNC, CLASSNAME, __VA_ARGS__)
#define DOARG16(FUNC, CLASSNAME, ELEM1, ...) \
  DOARG1(FUNC, CLASSNAME, ELEM1) DOARG15(FUNC, CLASSNAME, __VA_ARGS__)

#define FOREACH_(FUNC, CLASSNAME, ...) \
  BASE_CONCAT(DOARG, ARG_COUNT(__VA_ARGS__))(FUNC, CLASSNAME, __VA_ARGS__)

#define GET_ELEM(N, ...) BASE_CONCAT(GET_ELEM, N)(__VA_ARGS__)
#define GET_ELEM1(_1, ...) _1
#define GET_ELEM2(_1, _2, ...) _2
#define GET_ELEM3(_1, _2, _3, ...) _3
#define GET_ELEM4(_1, _2, _3, _4, ...) _4
#define GET_ELEM5(_1, _2, _3, _4, _5, ...) _5
#define GET_ELEM6(_1, _2, _3, _4, _5, _6, ...) _6
#define GET_ELEM7(_1, _2, _3, _4, _5, _6, _7, ...) _7
#define GET_ELEM8(_1, _2, _3, _4, _5, _6, _7, _8, ...) _8
#define GET_ELEM9(_1, _2, _3, _4, _5, _6, _7, _8, _9, ...) _9
#define GET_ELEM10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, ...) _10
#define GET_ELEM11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, ...) _11
#define GET_ELEM12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, ...) _12
#define GET_ELEM13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   ...)                                                    \
  _13
#define GET_ELEM14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   _14, ...)                                               \
  _14
#define GET_ELEM15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   _14, _15, ...)                                          \
  _15
#define GET_ELEM16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   _14, _15, _16, ...)                                     \
  _16

// Get last argument.
#define GET_LAST(...) GET_ELEM(ARG_COUNT(__VA_ARGS__), __VA_ARGS__),

// Macros for enum definitions
#define ELEM(CLASSNAME, E) E,

// Macro to generate enum `CLASSNAME` for elements, and the definition for the
// `EnumToString`, e.g.:
//   enum class ClassName {
//     kUnknown,
//     kElement1,
//     kElement2,
//     kElement3,
//     kMaxValue = kElement3,
//  };
//  COMPONENT_EXPORT(APP_TYPES)
//  std::string EnumToString(ClassName input);
//
// Modify ARC_COUNT, GET_ELEMXX and DOARGXX to support more elements.
#define ENUM(CLASSNAME, ...)                                                 \
  enum class CLASSNAME {                                                     \
    FOREACH_(ELEM, CLASSNAME, __VA_ARGS__) kMaxValue = GET_LAST(__VA_ARGS__) \
  };                                                                         \
  COMPONENT_EXPORT(APP_TYPES)                                                \
  std::string EnumToString(CLASSNAME input);

#define ENUM_FOR_COMPONENT(COMPONENT, CLASSNAME, ...)              \
  enum class CLASSNAME { FOREACH_(ELEM, CLASSNAME, __VA_ARGS__) }; \
  COMPONENT_EXPORT(COMPONENT)                                      \
  std::string EnumToString(CLASSNAME input);

// Macros to print enum
#define PRINT_CLASSNAME_AND_ELEM(CLASSNAME, ELEM) \
  std::string(#CLASSNAME) + std::string("::") + std::string(#ELEM)

#define PRINT_ELEM(CLASSNAME, ELEM) \
  case CLASSNAME::ELEM:             \
    return PRINT_CLASSNAME_AND_ELEM(CLASSNAME, ELEM);

// Macro to generate the function `EnumToString` to print the enum `CLASSNAME`
// elements, e.g.:
//   std::string EnumToString(ClassName input) {
//     switch (input) {
//       case ClassName::kElement1:
//         return "ClassName::kElement1";
//       case ClassName::kElement2:
//         return "ClassName::kElement2";
//       case ClassName::kElement3:
//         return "ClassName::kElement1";
//    }
//  }
//
// Modify ARC_COUNT, GET_ELEMXX and DOARGXX to support more elements.
#define APP_ENUM_TO_STRING(CLASSNAME, ...)                          \
  std::string EnumToString(CLASSNAME input) {                       \
    switch (input) { FOREACH_(PRINT_ELEM, CLASSNAME, __VA_ARGS__) } \
  }

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_
